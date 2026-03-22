/**
 * @file display_manager.h
 * @brief Display Manager Interface - ESP-IDF
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    float temp[4];
    float humidity[4];
    bool valid[4];
    uint8_t num_sensors;
} display_sensor_data_t;

esp_err_t display_manager_init(void);
esp_err_t display_update_sensors(const display_sensor_data_t *data);
esp_err_t display_show_alert(const char *title, const char *message, uint32_t duration_ms);
esp_err_t display_clear_alert(void);
esp_err_t display_set_power(bool on);
bool display_is_available(void);
esp_err_t display_manager_deinit(void);

#endif