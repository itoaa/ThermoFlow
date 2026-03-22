/**
 * @file fan_controller.h
 * @brief PWM Fan Controller Interface
 * 
 * Controls 1-2 PWM fans with fail-safe defaults
 * Implements IEC 62443 SR-009: Actuator Fail-Safe
 * 
 * @version 1.0.0
 * @date 2026-03-22
 */

#ifndef FAN_CONTROLLER_H
#define FAN_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fan identifiers
 */
typedef enum {
    FAN_1 = 0,
    FAN_2 = 1,
    FAN_MAX
} fan_id_t;

/**
 * @brief Fan speed levels
 */
typedef enum {
    FAN_SPEED_OFF = 0,
    FAN_SPEED_LOW = 30,
    FAN_SPEED_MEDIUM = 60,
    FAN_SPEED_HIGH = 100,
    FAN_SPEED_MAX = 100
} fan_speed_t;

/**
 * @brief Fan control modes
 */
typedef enum {
    FAN_MODE_OFF,           /**< Fan always off */
    FAN_MODE_MANUAL,      /**< Manual speed setting */
    FAN_MODE_THERMOSTAT,  /**< Speed based on temperature delta */
    FAN_MODE_PID,         /**< PID control (advanced) */
    FAN_MODE_FAILSAFE     /**< Fail-safe: forced off */
} fan_mode_t;

/**
 * @brief Fan status
 */
typedef struct {
    fan_id_t id;
    fan_mode_t mode;
    uint8_t speed_percent;  /**< 0-100 */
    bool enabled;
    bool fault_detected;
    uint32_t runtime_seconds;
    uint32_t rpm;           /**< 0 if not available (3-pin fans) */
} fan_status_t;

/**
 * @brief Fan configuration
 */
typedef struct {
    uint8_t gpio_fan1;          /**< PWM GPIO for fan 1 */
    uint8_t gpio_fan2;          /**< PWM GPIO for fan 2 (0 = disabled) */
    uint8_t gpio_tach1;         /**< Tachometer GPIO for fan 1 (0 = disabled) */
    uint8_t gpio_tach2;         /**< Tachometer GPIO for fan 2 (0 = disabled) */
    uint32_t pwm_freq_hz;       /**< PWM frequency (typically 25000) */
    uint8_t pwm_resolution;     /**< Bits (typically 8) */
    uint16_t pwm_max_duty;      /**< Max duty cycle value */
    bool fail_safe_default;     /**< Start in fail-safe mode */
} fan_controller_config_t;

/**
 * @brief Thermostat control parameters
 */
typedef struct {
    float temp_setpoint;        /**< Target temperature */
    float temp_hysteresis;      /**< Hysteresis for on/off control */
    float temp_min;             /**< Minimum temperature for fan start */
    float temp_max;             /**< Maximum temperature (100% speed) */
} fan_thermostat_params_t;

/**
 * @brief Initialize fan controller
 * 
 * @param config Configuration structure
 * @return ESP_OK on success
 * 
 * @note Fans start in OFF state (SR-009: fail-safe)
 */
esp_err_t fan_controller_init(void);

/**
 * @brief Deinitialize fan controller
 */
esp_err_t fan_controller_deinit(void);

/**
 * @brief Set fan speed
 * 
 * @param fan Fan identifier
 * @param speed_percent 0-100
 * @return ESP_OK on success
 * 
 * @note Will be clamped to valid range and ignored in fail-safe mode
 */
esp_err_t fan_controller_set_speed(fan_id_t fan, uint8_t speed_percent);

/**
 * @brief Get current fan speed
 * 
 * @param fan Fan identifier
 * @return Current speed 0-100, or 0 if error
 */
uint8_t fan_controller_get_speed(fan_id_t fan);

/**
 * @brief Set fan control mode
 * 
 * @param fan Fan identifier
 * @param mode Control mode
 * @return ESP_OK on success
 */
esp_err_t fan_controller_set_mode(fan_id_t fan, fan_mode_t mode);

/**
 * @brief Get fan control mode
 * 
 * @param fan Fan identifier
 * @return Current mode
 */
fan_mode_t fan_controller_get_mode(fan_id_t fan);

/**
 * @brief Get fan status
 * 
 * @param fan Fan identifier
 * @param[out] status Status structure to fill
 * @return ESP_OK on success
 */
esp_err_t fan_controller_get_status(fan_id_t fan, fan_status_t *status);

/**
 * @brief Set thermostat parameters
 * 
 * @param fan Fan identifier
 * @param params Thermostat parameters
 * @return ESP_OK on success
 */
esp_err_t fan_controller_set_thermostat_params(fan_id_t fan, const fan_thermostat_params_t *params);

/**
 * @brief Calculate fan speed from temperature (for external control)
 * 
 * @param current_temp Current temperature
 * @param setpoint Target temperature
 * @param temp_min Temperature for 0% speed
 * @param temp_max Temperature for 100% speed
 * @return Calculated speed 0-100
 */
uint8_t fan_controller_calc_speed_from_temp(float current_temp, float setpoint, 
                                               float temp_min, float temp_max);

/**
 * @brief Enter fail-safe mode
 * 
 * Immediately stops all fans. Requires explicit mode change to resume.
 * 
 * @param reason Reason code for logging
 * @return ESP_OK on success
 * 
 * @note SR-009: Actuator fail-safe requirement
 */
esp_err_t fan_controller_enter_failsafe(const char *reason);

/**
 * @brief Exit fail-safe mode
 * 
 * @return ESP_OK on success
 */
esp_err_t fan_controller_exit_failsafe(void);

/**
 * @brief Check if in fail-safe mode
 * 
 * @return true if fail-safe is active
 */
bool fan_controller_is_failsafe(void);

/**
 * @brief Check for fan faults
 * 
 * Monitors tachometer signals if available
 * 
 * @param fan Fan identifier
 * @return true if fault detected
 */
bool fan_controller_check_fault(fan_id_t fan);

/**
 * @brief Convert fan_speed_t enum to percentage
 * 
 * @param speed Speed enum value
 * @return Percentage 0-100
 */
static inline uint8_t fan_speed_to_percent(fan_speed_t speed) {
    return (uint8_t)speed;
}

#ifdef __cplusplus
}
#endif

#endif /* FAN_CONTROLLER_H */