#include "ADC.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_spiffs.h"
#include "esp_log.h"

// For reading from SPIFFS
static const char *TAG = "ADC";

static sensor_data_t current_data; // Use the struct defined in ADC.h
FILE *sensor_data_file = NULL;

void adc_init() {
    // Initialize SPIFFS
    esp_vfs_spiffs_conf_t config = {
        .base_path = "/storage",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t result = esp_vfs_spiffs_register(&config);

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(result));
        return;
    }

    size_t total = 0, used = 0;
    result = esp_spiffs_info(config.partition_label, &total, &used);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get partition info (%s)", esp_err_to_name(result));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    // Initialize SPI, chip select pin, and open CSV file
    sensor_data_file = fopen("/storage/sensor_data.csv", "r");
    if (sensor_data_file == NULL) {
        printf("Failed to open CSV file.\n");
    } else {
        // Skip the header line that shows sensor type
        char header[256];
        fgets(header, sizeof(header), sensor_data_file);
    }
    printf("ADC set up complete. Reading test data from CSV file.\n");
}

// float adc_read_sensor(sensor_channel_t channel) {
//     // Simulate selecting different sensor channels based on the channel enum
//     switch (channel) {
//         case AIN1_AMMONIA:
//             return current_data.ammonia;
//         case AIN2_H2S:
//             return current_data.h2s;
//         case AIN3_CO2:
//             return current_data.co2;
//         case AIN4_METHANE:
//             return current_data.methane;
//         default:
//             return -1; // Invalid channel
//     }
// }

// Function to check chip select and print only the required sensor data
void chip_select(float mq_ammonia, float mq_methane, float mq_h2s, float mq_co2) {
    if (mq_ammonia == 1.0) {
        printf("Ammonia (NH3) Level: %.2f ppm\n", current_data.ammonia);
    }
    if (mq_methane == 1.0) {
        printf("Methane (CH4) Level: %.2f ppm\n", current_data.methane);
    }
    if (mq_h2s == 1.0) {
        printf("Hydrogen Sulfide (H2S) Level: %.2f ppm\n", current_data.h2s);
    }
    if (mq_co2 == 1.0) {
        printf("Carbon Dioxide (CO2) Level: %.2f ppm\n", current_data.co2);
    }
}

void read_sensor_data_csv() {
    char line[256];
    if (fgets(line, sizeof(line), sensor_data_file) != NULL) {
        float mq_ammonia, mq_methane, mq_h2s, mq_co2;
        
        // Read sensor data values from the CSV and store them in current_data structure
        sscanf(line, "%*[^,],%f,%f,%f,%f,%f,%f,%f,%f", 
               &current_data.ammonia, &current_data.methane, &current_data.co2, &current_data.h2s,
               &mq_ammonia, &mq_methane, &mq_h2s, &mq_co2);

        printf("New sensor data read from CSV:\n");
        
        // Use the chip select function to display relevant data based on sensor flags
        chip_select(mq_ammonia, mq_methane, mq_h2s, mq_co2);
    } else {
        printf("End of CSV file reached.\n");
        rewind(sensor_data_file);  // Restart reading from the beginning if at the end
    }
}

// Adding this function to fetch current data 
sensor_data_t get_current_data() {
    return current_data;
}