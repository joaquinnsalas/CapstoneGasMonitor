#include <stdio.h> //for basic printf commands
#include <string.h> //for handling strings

#include "freertos/FreeRTOS.h" //for delay,mutexs,semphrs rtos operations
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h" //esp_wifi_init functions and wifi operations
#include "esp_log.h" //for showing logs
#include "esp_mac.h"
#include "esp_event.h" //for wifi event
#include "esp_system.h" //esp_init funtions esp_err_t 

#include "lwip/sockets.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h" //light weight ip packets error handling
#include "lwip/sys.h" //system applications for light weight ip apps

#include "nvs_flash.h" //non volatile storage
#include "driver/i2c.h" // I2C communication
#include "si7021.h" // Custom Si7021 library
#include "ADC.h" // My ADC simulation

#define PORT 3333
#define EXAMPLE_ESP_WIFI_SSID "ESP32-Access-Point"
#define EXAMPLE_ESP_WIFI_PASS "joaquinsalas"
#define EXAMPLE_ESP_WIFI_CHANNEL    1
#define EXAMPLE_MAX_STA_CONN        4
#define LED_GPIO GPIO_NUM_2

static const char *TAG = "TCP_SOCKET_SERVER";

// Initialize sensor variables
float temperature;
float humidity;
float ammonia;
float h2s;
float co2;
float methane;

// Function to blink LED
void blink_led() {
    gpio_set_level(LED_GPIO, 1); // turn LED on
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    gpio_set_level(LED_GPIO, 0); // turn LED off
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}

// Event handler for WiFi
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d", MAC2STR(event->mac), event->aid, event->reason);
    }
}

// Setup Soft AP mode
void wifi_init_softap(void) {

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {

        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {.required = true},
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
                        EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

static void tcp_server_task(void *pvParameters) {
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = IPPROTO_IP;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        close(listen_sock);
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket accepted");

        while (1) {
            char data_to_send[128];
            snprintf(data_to_send, sizeof(data_to_send),
                     "Temp:%.2f,Humidity:%.2f,NH3:%.2f,H2S:%.2f,CO2:%.2f,CH4:%.2f",
                     temperature, humidity, ammonia, h2s, co2, methane);

            int err = send(sock, data_to_send, strlen(data_to_send), 0);
            if (err < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "Data sent: %s", data_to_send);
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }

    close(listen_sock);
    vTaskDelete(NULL);
}

// Need to create a task to read the sensor data
void sensor_task(void *pvParameters) {
    // Initialize the Si7021 sensor with the I2C port and pins
    esp_err_t ret = si7021_init(I2C_NUM_0, GPIO_NUM_21, GPIO_NUM_22, GPIO_PULLUP_ENABLE, GPIO_PULLUP_ENABLE);
    if (ret != ESP_OK) {
        ESP_LOGE("SI7021", "Failed to initialize Si7021 sensor, error code: %d", ret);
        vTaskDelete(NULL);
    }
    // Infinite loop to keep reading and printing sensor data
    while (1) {
        // Read temperature and humidity using the library functions
        float temperature_c = si7021_read_temperature();
        temperature = (temperature_c * 9.0 / 5.0) + 32.0; // Converting from Celsius to Farenheit
        float humidity = si7021_read_humidity();

        // Log the temperature and humidity values to the terminal
        ESP_LOGI("SI7021", "Temperature: %.2f°F, Humidity: %.2f%%", temperature, humidity);

        // Wait for 5 seconds before the next reading
        vTaskDelay(5000 / portTICK_PERIOD_MS);  // Adjust the delay time as needed
    }
}

// Task for reading sensor data from csv file
void csv_task(void *pvParameters) {
    //Infinite loop to continuously read from the CSV file
    while(1) {
        read_sensor_data_csv();

        // Get latest sensor data
        sensor_data_t data = get_current_data();

        // Update global variables with latest sensor readings
        ammonia = data.ammonia;
        h2s = data.h2s;
        co2 = data.co2;
        methane = data.methane;
        //ESP_LOGI("Analog Sensors", "nh3: %.2f, h2s: %.2f, co2: %.2f, ch4: %.2f", ammonia, h2s, co2, methane);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

// Main application entry point
void app_main() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize LED GPIO
    esp_rom_gpio_pad_select_gpio(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    wifi_init_softap();

    // Initialize sensors
    adc_init();  // Initialize CSV file reading for simulated sensors

    // Start TCP server task
    xTaskCreate(tcp_server_task, "tcp_server_task", 4096, (void *)AF_INET, 5, NULL);

    // Create separate tasks for reading sensors
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(csv_task, "csv_task", 4096, NULL, 5, NULL);

    // Blink LED to indicate successful initialization
    for (int i = 0; i < 50; i++) {
        blink_led();
    }
}
