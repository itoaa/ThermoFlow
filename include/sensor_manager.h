/**
 * @file sensor_manager.h
 * @brief Sensor management interface
 * 
 * Manages SHT40 temperature and humidity sensors
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "thermoflow_config.h"

typedef struct {
    float temperature;    // Celsius
    float humidity;     // Percentage
    bool valid;         // Data valid flag
    uint32_t timestamp; // Last update time
    uint8_t sensor_id;  // Sensor identifier
} sensor_reading_t;

typedef struct {
    sensor_reading_t sensors[THERMOFLOW_NUM_SENSORS];
    uint8_t num_sensors;
    bool all_valid;
    uint32_t last_update;
} sensor_data_t;

// Initialize I2C bus
esp_err_t sensor_manager_init_i2c(void);

// Initialize all sensors
esp_err_t sensor_manager_init(void);

// Read all sensors
esp_err_t sensor_manager_read_all(sensor_data_t *data);

// Update global sensor data
void sensor_manager_update_data(const sensor_data_t *data);

// Get current sensor data
void sensor_manager_get_data(sensor_data_t *data);

// Check if sensor reading is valid (range check)
bool sensor_reading_is_valid(const sensor_reading_t *reading);

#endif // SENSOR_MANAGER_H
