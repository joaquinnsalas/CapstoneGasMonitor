#include "driver/i2c.h"       // I2C communication
#include "si7021.h" // Custom Si7021 library
#include "ADC.h" // My ADC simulation
#include <stdio.h> //for basic printf commands
#include <string.h> //for handling strings
#include "freertos/FreeRTOS.h" //for delay,mutexs,semphrs rtos operations
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h" //esp_wifi_init functions and wifi operations
#include "esp_event.h" //for wifi event
#include "esp_log.h" //for showing logs
#include "nvs_flash.h" //non volatile storage
#include "lwip/err.h" //light weight ip packets error handling
#include "lwip/sys.h" //system applications for light weight ip apps
#include "esp_system.h" //esp_init funtions esp_err_t 


//For Wifi at home
const char *ssid = "FoundingFathers";
const char *pass = "chrisavila";

int retry_num = 0;
static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data) {
    if(event_id == WIFI_EVENT_STA_START) {
    printf("WIFI CONNECTING....\n");
    }
    else if (event_id == WIFI_EVENT_STA_CONNECTED) {
    printf("WiFi CONNECTED\n");
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
    printf("WiFi lost connection\n");
    if(retry_num<5){esp_wifi_connect();retry_num++;printf("Retrying to Connect...\n");}
    }
    else if (event_id == IP_EVENT_STA_GOT_IP) {
    printf("Wifi got IP...\n\n");
    }
}

void wifi_connection() {
    esp_netif_init(); //network interdace initialization

    esp_event_loop_create_default(); //responsible for handling and dispatching events

    esp_netif_create_default_wifi_sta(); //sets up necessary data structs for wifi station interface

    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();//sets up wifi wifi_init_config struct with default values

    esp_wifi_init(&wifi_initiation); //wifi initialised with dafault wifi_initiation

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);//creating event handler register for wifi

    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);//creating event handler register for ip event

    wifi_config_t wifi_configuration ={ //struct wifi_config_t var wifi_configuration
    .sta= {
            .ssid = "",
            .password= "", /*we are sending a const char of ssid and password which we will strcpy in following line so leaving it blank*/ 
        }//also this part is used if you donot want to use Kconfig.projbuild
    };

    strcpy((char*)wifi_configuration.sta.ssid,"FoundingFathers"); // copy chars from hardcoded configs to struct

    strcpy((char*)wifi_configuration.sta.password,"chrisavila");

    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);//setting up configs when event ESP_IF_WIFI_STA

    esp_wifi_start();//start connection with configurations provided in funtion

    esp_wifi_set_mode(WIFI_MODE_STA);//station mode selected

    esp_wifi_connect(); //connect with saved ssid and pass

    printf( "wifi_init_softap finished. SSID:%s  password:%s", ssid, pass);
}

// Need to create a task to read the sensor data
void sensor_task(void *pvParameters) {
    // Initialize the Si7021 sensor with the I2C port and pins
    esp_err_t ret = si7021_init(I2C_NUM_0, GPIO_NUM_21, GPIO_NUM_22, GPIO_PULLUP_ENABLE, GPIO_PULLUP_ENABLE);
    if (ret != ESP_OK) {
        ESP_LOGE("SI7021", "Failed to initialize Si7021 sensor");
        vTaskDelete(NULL);
    }
    // Infinite loop to keep reading and printing sensor data
    while (1) {
        // Read temperature and humidity using the library functions
        float temperature_c = si7021_read_temperature();
        float temperature_f = (temperature_c * 9.0 / 5.0) + 32.0; // Converting from Celsius to Farenheit
        float humidity = si7021_read_humidity();

        // Log the temperature and humidity values to the terminal
        ESP_LOGI("SI7021", "Temperature: %.2f°F, Humidity: %.2f%%", temperature_f, humidity);

        // Wait for 5 seconds before the next reading
        vTaskDelay(5000 / portTICK_PERIOD_MS);  // Adjust the delay time as needed
    }
}

// Task for reading sensor data from csv file
void csv_task(void *pvParameters) {
    //Infinite loop to continuously read from the CSV file
    while(1) {
        read_sensor_data_csv();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

// Initialize I2C and Si7021 sensor and csv file reading
void app_main() {
    // Calling to initialize WiFi
    nvs_flash_init(); // this is important in wifi case to store configurations, code will not work work without this
    // Connect wifi
    wifi_connection();

    adc_init();

    //Start the web server after WiFi connection
    start_webserver(); //open browser and go to http://192.168.1.123/sensor

    //Create tasks for reading sensor data and reading file
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
    xTaskCreate(csv_task, "csv_task", 4096, NULL, 5, NULL);
}

/////////////////////////   FROM NOVEMBER 19th     /////////////////////////


// // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% Creating ESP32 Server %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// #define EXAMPLE_ESP_WIFI_SSID       "ESP32-Access-Point"
// #define EXAMPLE_ESP_WIFI_PASS       "joaquinsalas"
// #define EXAMPLE_ESP_WIFI_CHANNEL    1
// #define EXAMPLE_MAX_STA_CONN        4

// static const char *TAG = "wifi softAP";

// // Initialize my sensor variables
// float temperature;
// float humidity;
// float ammonia;
// float h2s;
// float co2;
// float methane;

// // Event handler for WiFi
// static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
//     if (event_id == WIFI_EVENT_AP_STACONNECTED) {
//         wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
//         ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
//     } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
//         wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
//         ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d", MAC2STR(event->mac), event->aid, event->reason);
//     }
// }
// // Setup Soft AP mode
// void wifi_init_softap(void) {

//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     esp_netif_create_default_wifi_ap();

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

//     wifi_config_t wifi_config = {

//         .ap = {
//             .ssid = EXAMPLE_ESP_WIFI_SSID,
//             .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
//             .channel = EXAMPLE_ESP_WIFI_CHANNEL,
//             .password = EXAMPLE_ESP_WIFI_PASS,
//             .max_connection = EXAMPLE_MAX_STA_CONN,
//             .authmode = WIFI_AUTH_WPA2_PSK,
//             .pmf_cfg = {.required = true},
//         },
//     };
//     if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
//         wifi_config.ap.authmode = WIFI_AUTH_OPEN;
//     }
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
//     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());

//     ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
//                         EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
// }
// // HTTP server handler functions
// esp_err_t get_temperature_handler(httpd_req_t *req)
// {
//     char temp_str[10];
//     snprintf(temp_str, sizeof(temp_str), "%.2f", temperature);
//     httpd_resp_send(req, temp_str, HTTPD_RESP_USE_STRLEN);
//     return ESP_OK;
// }

// esp_err_t get_humidity_handler(httpd_req_t *req)
// {
//     char hum_str[10];
//     snprintf(hum_str, sizeof(hum_str), "%.2f", humidity);
//     httpd_resp_send(req, hum_str, HTTPD_RESP_USE_STRLEN);
//     return ESP_OK;
// }

// esp_err_t get_sensor_data_handler(httpd_req_t *req)
// {
//     char sensor_data[100];
//     snprintf(sensor_data, sizeof(sensor_data),
//              "Temp: %.2f\nHumidity: %.2f\nNH3: %.2f\nH2S: %.2f\nCO2: %.2f\nCH4: %.2f",
//              temperature, humidity, ammonia, h2s, co2, methane);
//     httpd_resp_set_type(req, "text/plain"); // set content type
//     httpd_resp_send(req, sensor_data, HTTPD_RESP_USE_STRLEN);
//     return ESP_OK;
// }

// // Set up HTTP server with URI handlers
// void start_webserver(void)
// {
//     httpd_config_t config = HTTPD_DEFAULT_CONFIG();
//     httpd_handle_t server = NULL;

//     // Start the HTTP server
//     if (httpd_start(&server, &config) == ESP_OK) {
//         // URI handlers for temperature and humidity
//         httpd_uri_t temp_uri = {
//             .uri       = "/temperature",
//             .method    = HTTP_GET,
//             .handler   = get_temperature_handler,
//             .user_ctx  = NULL
//         };
//         httpd_register_uri_handler(server, &temp_uri);

//         httpd_uri_t hum_uri = {
//             .uri       = "/humidity",
//             .method    = HTTP_GET,
//             .handler   = get_humidity_handler,
//             .user_ctx  = NULL
//         };
//         httpd_register_uri_handler(server, &hum_uri);

//         httpd_uri_t sensor_data_uri = {
//             .uri       = "/sensor",
//             .method    = HTTP_GET,
//             .handler   = get_sensor_data_handler,
//             .user_ctx  = NULL
//         };
//         httpd_register_uri_handler(server, &sensor_data_uri);
//     }
// }
// // %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% Creating ESP32 Server %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// // Need to create a task to read the sensor data
// void sensor_task(void *pvParameters) {
//     // Initialize the Si7021 sensor with the I2C port and pins
//     esp_err_t ret = si7021_init(I2C_NUM_0, GPIO_NUM_21, GPIO_NUM_22, GPIO_PULLUP_ENABLE, GPIO_PULLUP_ENABLE);
//     if (ret != ESP_OK) {
//         ESP_LOGE("SI7021", "Failed to initialize Si7021 sensor, error code: %d", ret);
//         vTaskDelete(NULL);
//     }
//     // Infinite loop to keep reading and printing sensor data
//     while (1) {
//         // Read temperature and humidity using the library functions
//         float temperature_c = si7021_read_temperature();
//         temperature = (temperature_c * 9.0 / 5.0) + 32.0; // Converting from Celsius to Farenheit
//         float humidity = si7021_read_humidity();

//         // Log the temperature and humidity values to the terminal
//         ESP_LOGI("SI7021", "Temperature: %.2f°F, Humidity: %.2f%%", temperature, humidity);

//         // Wait for 5 seconds before the next reading
//         vTaskDelay(5000 / portTICK_PERIOD_MS);  // Adjust the delay time as needed
//     }
// }

// // Task for reading sensor data from csv file
// void csv_task(void *pvParameters) {
//     //Infinite loop to continuously read from the CSV file
//     while(1) {
//         read_sensor_data_csv();

//         // Get latest sensor data
//         sensor_data_t data = get_current_data();

//         // Update global variables with latest sensor readings
//         ammonia = data.ammonia;
//         h2s = data.h2s;
//         co2 = data.co2;
//         methane = data.methane;
//         vTaskDelay(5000 / portTICK_PERIOD_MS);
//     }
// }

// // Initialize I2C and Si7021 sensor and csv file reading
// void app_main() {

//     //Initialize NVS and WiFi
//     esp_err_t ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ret = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK(ret);
//     ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");

//     wifi_init_softap();

//     //Initialize sensors
// //    si7021_init(I2C_NUM_0, GPIO_NUM_21, GPIO_NUM_22, GPIO_PULLUP_ENABLE, GPIO_PULLUP_ENABLE);
//     adc_init();

//     //Start HTTP server
//     start_webserver();

//     //Create seperate tasks for the sensor readings
//     xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
//     xTaskCreate(csv_task, "csv_task", 4096, NULL, 5, NULL);
// }