#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#define PORT 3333
#define SERVER_IP "192.168.4.1" // IP address of ESP32 #1 (server)
#define WIFI_SSID "ESP32-Access-Point"
#define WIFI_PASS "joaquinsalas"

static const char *TAG = "TCP_SOCKET_CLIENT";

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

//////////////////////////////////////// UART DRIVER ////////////////////////////////////////
#include "driver/gpio.h" //Controls GPIO pins 
#include "driver/uart.h"

#define UART_PORT_NUM      UART_NUM_1
#define UART_BAUD_RATE     115200
#define UART_TX_PIN        GPIO_NUM_17  // TX pin
#define UART_RX_PIN        GPIO_NUM_16  // RX pin

void uart_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    //Configure UART
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));

    //Set UART pins
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    //uart_set_pin(UART_NUM_1, GPIO_NUM_17, GPIO_NUM_16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    //uart_driver_install(UART_NUM_1, 1024, 0, 0, NULL, 0);
    // Setup UART buffered IO with an event queue
    // Install UART driver
    const int uart_buffer_size = (1024 * 2);
    QueueHandle_t uart_queue;
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));
}

void send_data_over_uart(const char *data) {
    uart_write_bytes(UART_PORT_NUM, data, strlen(data));
}
//////////////////////////////////////// UART DRIVER ////////////////////////////////////////

// Global variables for sensor data
float temperature;
float humidity;
float ammonia;
float h2s;
float co2;
float methane;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Reconnecting to the AP...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init_sta(void) {
    wifi_event_group = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

// Parsing and logging data for database
void parse_and_log_data(const char *data) {
    float parsed_temperature, parsed_humidity, parsed_ammonia, parsed_h2s, parsed_co2, parsed_methane;

    // Parse the data
    int result = sscanf(data, "Temp:%f,Humidity:%f,NH3:%f,H2S:%f,CO2:%f,CH4:%f",
                        &parsed_temperature, &parsed_humidity, &parsed_ammonia, &parsed_h2s, &parsed_co2, &parsed_methane);

    if (result == 6) { // Ensure all values are parsed successfully

        // Update global variables with parsed values
        temperature = parsed_temperature;
        humidity = parsed_humidity;
        ammonia = parsed_ammonia;
        h2s = parsed_h2s;
        co2 = parsed_co2;
        methane = parsed_methane;

        // Log parsed data
        ESP_LOGI(TAG, "Parsed Data - Temp: %.2f, Humidity: %.2f, NH3: %.2f, H2S: %.2f, CO2: %.2f, CH4: %.2f",
                 temperature, humidity, ammonia, h2s, co2, methane);

        // TODO: Add code here to forward data to the SQLite database on your computer
    } else {
        ESP_LOGE(TAG, "Failed to parse data: %s", data);
    }
}

// TCP Client Task
void tcp_client(void *pvParameters) {
    char rx_buffer[128];
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    while (1) {
        // Create socket
        int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Socket created, connecting to %s:%d", SERVER_IP, PORT);

        // Connect to server
        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            close(sock);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Successfully connected");

        // Send a request message to indicate data transfer
        char *request = "GET_SENSOR_DATA";
        send(sock, request, strlen(request), 0);

        while (1) {
            // Receive data from server
            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len < 0) {
                ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
                break;
            } else if (len == 0) {
                ESP_LOGI(TAG, "Connection closed by server");
                break;
            } else {
                rx_buffer[len] = '\0'; // Null-terminate the received data
                ESP_LOGI(TAG, "Received data: %s", rx_buffer);

                //Parse and log the data 
                parse_and_log_data(rx_buffer);

                // Forward the data via UART
                send_data_over_uart(rx_buffer);
            }
            vTaskDelay(5000 / portTICK_PERIOD_MS); // Adjust delay as needed
        }

        // Clean up
        if (sock != -1) {
            ESP_LOGI(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS); // Retry connection every 5 seconds if disconnected
    }
}

void app_main(void) {
    // Initialize global variables
    temperature = 0.0;
    humidity = 0.0;
    ammonia = 0.0;
    h2s = 0.0;
    co2 = 0.0;
    methane = 0.0;

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    //Initialize WiFi
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    //Testing UART
    uart_init();
    char uart_data[256];
    snprintf(uart_data, sizeof(uart_data),
            "Temp: %.2f, Humidity: %.2f, NH3: %.2f, H2S: %.2f, CO2: %.2f, CH4: %.2f\n",
            temperature, humidity, ammonia, h2s, co2, methane);
    send_data_over_uart(uart_data);

    // Wait for Wi-Fi to connect
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);

    // Start TCP client task
    xTaskCreate(tcp_client, "tcp_client", 4096, NULL, 5, NULL);
}