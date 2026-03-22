/**
 * @file anti_condensation.h
 * @brief Anti-Condensation Protection
 */

#ifndef ANTI_CONDENSATION_H
#define ANTI_CONDENSATION_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Alert callback function type
 * 
 * Called when condensation risk changes state.
 * @param alert_active true if entering alert state, false if clearing
 * @param rh Current relative humidity percentage
 * @param user_data User-provided context pointer
 */
typedef void (*condensation_alert_callback_t)(bool alert_active, float rh, void *user_data);

esp_err_t anti_condensation_init(void);
esp_err_t anti_condensation_check(float rh, float temp);
bool anti_condensation_is_active(void);
esp_err_t anti_condensation_set_callback(condensation_alert_callback_t callback, void *user_data);
esp_err_t anti_condensation_set_thresholds(float threshold, float hysteresis);
esp_err_t anti_condensation_get_thresholds(float *threshold, float *hysteresis);
void anti_condensation_get_statistics(uint32_t *alert_count, uint64_t *total_alert_time_us);
esp_err_t anti_condensation_reset_statistics(void);
esp_err_t anti_condensation_deinit(void);

#endif
