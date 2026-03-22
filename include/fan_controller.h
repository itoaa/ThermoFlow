/**
 * @file fan_controller.h
 * @brief Fan control interface
 * 
 * Security Requirements:
 * - SR-009: Actuator fail-safe (fans OFF on error)
 * - SR-010: Environmental limits
 */

#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include "thermoflow_config.h"

typedef enum {
    FAN_1 = 0,
    FAN_2 = 1,
    FAN_MAX
} fan_id_t;

typedef enum {
    FAN_SPEED_OFF = 0,
    FAN_SPEED_LOW = 77,      // 30%
    FAN_SPEED_MEDIUM = 153,  // 60%
    FAN_SPEED_HIGH = 255     // 100%
} fan_speed_t;

typedef struct {
    fan_speed_t speed;
    bool fault;
    uint32_t runtime_seconds;
    uint32_t last_update;
} fan_status_t;

// Initialize fan controller
esp_err_t fan_controller_init(void);

// Set fan speed
esp_err_t fan_controller_set_speed(fan_id_t fan, fan_speed_t speed);

// Get fan status
esp_err_t fan_controller_get_status(fan_id_t fan, fan_status_t *status);

// Emergency stop all fans
void fan_controller_emergency_stop(void);

// Calculate fan speeds based on sensor data
void fan_controller_calculate_speeds(const sensor_data_t *sensors, 
                                        fan_speed_t *fan1, fan_speed_t *fan2);

// Convert speed to percentage
static inline int fan_speed_to_percent(fan_speed_t speed) {
    return (speed * 100) / 255;
}

#endif // FAN_CONTROLLER_H
