/**
 * @file fan_controller.c
 * @brief PWM Fan Controller Implementation - ESP-IDF
 * 
 * Controls 1-2 PWM cooling fans with fail-safe defaults.
 * Implements IEC 62443 SR-009: Actuator Fail-Safe requirements.
 * 
 * Features:
 * - Thread-safe operation with mutex protection
 * - Fail-safe: all fans OFF on system boot
 * - Proportional speed control based on temperature
 * - Runtime tracking per fan
 * 
 * @author Ola Andersson
 * @version 1.0.0
 * @date 2026-03-22
 * 
 * @section changelog Change Log
 * - 1.0.0 (2026-03-22): Initial implementation
 *   - Basic PWM control stub (actual PWM to be implemented)
 *   - Fail-safe mode support
 *   - Thread-safe state management
 */

#include <string.h>                  /* memcpy */
#include "fan_controller.h"           /* Public interface */
#include "esp_log.h"                   /* ESP-IDF logging */
#include "esp_timer.h"                 /* Timer functions */
#include "freertos/FreeRTOS.h"         /* FreeRTOS core */
#include "freertos/semphr.h"           /* Semaphores for thread safety */

/* Logging tag - appears in log messages from this component */
static const char *TAG = "FAN_CTRL";

/* Maximum number of fans supported */
#define FAN_MAX_COUNT           2

/**
 * @brief Per-fan state structure
 * 
 * Tracks all runtime state for a single fan.
 */
typedef struct {
    bool enabled;                      /* Fan is enabled/configured */
    fan_mode_t mode;                   /* Current control mode */
    uint8_t speed_percent;           /* 0-100, actual speed */
    uint8_t target_speed;              /* Requested speed (may differ in fail-safe) */
    uint32_t runtime_seconds;          /* Total runtime */
    uint64_t last_update_time;         /* For runtime calculation */
    bool fault_detected;               /* Fault flag */
    fan_thermostat_params_t thermostat; /* Thermostat parameters */
} fan_state_t;

/**
 * @brief Fan controller global state
 * 
 * Thread-safe through mutex protection.
 */
typedef struct {
    bool initialized;                  /* Module initialized flag */
    fan_state_t fans[FAN_MAX_COUNT]; /* Per-fan state */
    bool fail_safe_active;           /* Fail-safe mode flag */
    SemaphoreHandle_t mutex;           /* Thread safety mutex */
} fan_controller_state_t;

/* Global state instance */
static fan_controller_state_t s_fan_ctrl = {
    .initialized = false,
    .fail_safe_active = false,
    .mutex = NULL
};

/* Internal function prototypes */
static void update_runtime(fan_id_t fan);
static uint8_t calc_thermostat_speed(fan_id_t fan, float current_temp);

/**
 * @brief Initialize fan controller
 * 
 * Sets up mutex and initializes all fans to OFF state.
 * Implements SR-009: Fail-safe defaults.
 *
 * @return ESP_OK on success, ESP_ERR_NO_MEM if mutex creation fails
 */
esp_err_t fan_controller_init(void)
{
    /* Create mutex for thread safety */
    s_fan_ctrl.mutex = xSemaphoreCreateMutex();
    if (s_fan_ctrl.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Take mutex during initialization */
    xSemaphoreTake(s_fan_ctrl.mutex, portMAX_DELAY);

    /* Initialize all fans to OFF state (fail-safe per SR-009) */
    for (int i = 0; i < FAN_MAX_COUNT; i++) {
        s_fan_ctrl.fans[i].enabled = true;
        s_fan_ctrl.fans[i].mode = FAN_MODE_OFF;
        s_fan_ctrl.fans[i].speed_percent = 0;
        s_fan_ctrl.fans[i].target_speed = 0;
        s_fan_ctrl.fans[i].runtime_seconds = 0;
        s_fan_ctrl.fans[i].last_update_time = 0;
        s_fan_ctrl.fans[i].fault_detected = false;
        memset(&s_fan_ctrl.fans[i].thermostat, 0, sizeof(fan_thermostat_params_t));
    }

    s_fan_ctrl.initialized = true;
    s_fan_ctrl.fail_safe_active = false;

    /* Release mutex */
    xSemaphoreGive(s_fan_ctrl.mutex);

    ESP_LOGI(TAG, "Fan controller initialized - all fans OFF (fail-safe)");
    return ESP_OK;
}

/**
 * @brief Deinitialize fan controller
 * 
 * Stops all fans and releases resources.
 *
 * @return ESP_OK on success
 */
esp_err_t fan_controller_deinit(void)
{
    if (!s_fan_ctrl.initialized) {
        return ESP_OK;  /* Already deinitialized */
    }

    xSemaphoreTake(s_fan_ctrl.mutex, portMAX_DELAY);

    /* Stop all fans */
    for (int i = 0; i < FAN_MAX_COUNT; i++) {
        s_fan_ctrl.fans[i].speed_percent = 0;
        s_fan_ctrl.fans[i].target_speed = 0;
        s_fan_ctrl.fans[i].enabled = false;
        s_fan_ctrl.fans[i].mode = FAN_MODE_OFF;
    }

    s_fan_ctrl.initialized = false;

    /* Release and delete mutex */
    xSemaphoreGive(s_fan_ctrl.mutex);
    vSemaphoreDelete(s_fan_ctrl.mutex);
    s_fan_ctrl.mutex = NULL;

    ESP_LOGI(TAG, "Fan controller deinitialized");
    return ESP_OK;
}

/**
 * @brief Update runtime statistics for a fan
 * 
 * Must be called with mutex held.
 *
 * @param fan Fan identifier
 */
static void update_runtime(fan_id_t fan)
{
    if (fan < 0 || fan >= FAN_MAX_COUNT) {
        return;
    }

    fan_state_t *f = &s_fan_ctrl.fans[fan];

    if (f->speed_percent > 0 && f->last_update_time > 0) {
        uint64_t now = esp_timer_get_time();  /* Current time in microseconds */
        uint64_t delta_us = now - f->last_update_time;
        f->runtime_seconds += (uint32_t)(delta_us / 1000000ULL);
    }

    f->last_update_time = esp_timer_get_time();
}

/**
 * @brief Set fan speed
 * 
 * Sets target speed for a fan. Actual speed may be overridden
 * if fail-safe mode is active.
 *
 * @param fan Fan identifier
 * @param speed_percent Target speed 0-100
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t fan_controller_set_speed(fan_id_t fan, uint8_t speed_percent)
{
    if (fan < 0 || fan >= FAN_MAX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_fan_ctrl.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Clamp speed to valid range */
    if (speed_percent > 100) {
        speed_percent = 100;
    }

    xSemaphoreTake(s_fan_ctrl.mutex, portMAX_DELAY);

    fan_state_t *f = &s_fan_ctrl.fans[fan];

    /* Update runtime tracking */
    update_runtime(fan);

    /* Store target speed */
    f->target_speed = speed_percent;

    /* Apply fail-safe override */
    if (s_fan_ctrl.fail_safe_active) {
        f->speed_percent = 0;
        ESP_LOGW(TAG, "Fan %d: Speed request ignored - fail-safe active", fan);
    } else {
        f->speed_percent = speed_percent;
        f->mode = FAN_MODE_MANUAL;

        /* Log significant speed changes */
        if (speed_percent == 0 || speed_percent == 100) {
            ESP_LOGI(TAG, "Fan %d: Speed set to %d%%", fan, speed_percent);
        }
    }

    xSemaphoreGive(s_fan_ctrl.mutex);

    return ESP_OK;
}

/**
 * @brief Get current fan speed
 * 
 * Returns the actual current speed (may differ from target in fail-safe).
 *
 * @param fan Fan identifier
 * @return Current speed 0-100, or 0 if error
 */
uint8_t fan_controller_get_speed(fan_id_t fan)
{
    if (fan < 0 || fan >= FAN_MAX_COUNT) {
        return 0;
    }

    if (!s_fan_ctrl.initialized) {
        return 0;
    }

    xSemaphoreTake(s_fan_ctrl.mutex, portMAX_DELAY);
    uint8_t speed = s_fan_ctrl.fans[fan].speed_percent;
    xSemaphoreGive(s_fan_ctrl.mutex);

    return speed;
}

/**
 * @brief Set fan control mode
 * 
 * Changes control mode for a fan.
 *
 * @param fan Fan identifier
 * @param mode Control mode
 * @return ESP_OK on success
 */
esp_err_t fan_controller_set_mode(fan_id_t fan, fan_mode_t mode)
{
    if (fan < 0 || fan >= FAN_MAX_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_fan_ctrl.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_fan_ctrl.mutex, portMAX_DELAY);

    fan_state_t *f = &s_fan_ctrl.fans[fan];

    /* Update runtime tracking before mode change */
    update_runtime(fan);

    f->mode = mode;

    /* Apply mode-specific actions */
    switch (mode) {
        case FAN_MODE_OFF:
            f->speed_percent = 0;
            f->target_speed = 0;
            break;

        case FAN_MODE_FAILSAFE:
            f->speed_percent = 0;
            f->target_speed = 0;
            break;

        case FAN_MODE_MANUAL:
            /* Keep current speed, expect manual set_speed calls */
            break;

        case FAN_MODE_THERMOSTAT:
            /* Will be updated in control loop based on temperature */
            break;

        case FAN_MODE_PID:
            /* Advanced mode - not yet implemented */
            ESP_LOGW(TAG, "PID mode not yet implemented");
            break;
    }

    ESP_LOGI(TAG, "Fan %d: Mode changed to %d", fan, mode);

    xSemaphoreGive(s_fan_ctrl.mutex);

    return ESP_OK;
}

/**
 * @brief Get fan control mode
 * 
 * @param fan Fan identifier
 * @return Current mode, or FAN_MODE_OFF if error
 */
fan_mode_t fan_controller_get_mode(fan_id_t fan)
{
    if (fan < 0 || fan >= FAN_MAX_COUNT) {
        return FAN_MODE_OFF;
    }

    if (!s_fan_ctrl.initialized) {
        return FAN_MODE_OFF;
    }

    xSemaphoreTake(s_fan_ctrl.mutex, portMAX_DELAY);
    fan_mode_t mode = s_fan_ctrl.fans[fan].mode;
    xSemaphoreGive(s_fan_ctrl.mutex);

    return mode;
}

/**
 * @brief Get fan status
 * 
 * Fills status structure with current fan state.
 *
 * @param fan Fan identifier
 * @param[out] status Status structure to fill
 * @return ESP_OK on success
 */
esp_err_t fan_controller_get_status(fan_id_t fan, fan_status_t *status)
{
    if (fan < 0 || fan >= FAN_MAX_COUNT || status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_fan_ctrl.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_fan_ctrl.mutex, portMAX_DELAY);

    fan_state_t *f = &s_fan_ctrl.fans[fan];

    /* Update runtime before returning */
    update_runtime(fan);

    status->id = fan;
    status->mode = f->mode;
    status->speed_percent = f->speed_percent;
    status->enabled = f->enabled;
    status->fault_detected = f->fault_detected;
    status->runtime_seconds = f->runtime_seconds;
    status->rpm = 0;  /* Tachometer not yet implemented */

    xSemaphoreGive(s_fan_ctrl.mutex);

    return ESP_OK;
}

/**
 * @brief Set thermostat parameters
 * 
 * Configures temperature-based control for a fan.
 *
 * @param fan Fan identifier
 * @param params Thermostat parameters
 * @return ESP_OK on success
 */
esp_err_t fan_controller_set_thermostat_params(fan_id_t fan, const fan_thermostat_params_t *params)
{
    if (fan < 0 || fan >= FAN_MAX_COUNT || params == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_fan_ctrl.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_fan_ctrl.mutex, portMAX_DELAY);
    memcpy(&s_fan_ctrl.fans[fan].thermostat, params, sizeof(fan_thermostat_params_t));
    xSemaphoreGive(s_fan_ctrl.mutex);

    ESP_LOGI(TAG, "Fan %d: Thermostat params set (setpoint=%.1f°C)", 
             fan, params->temp_setpoint);

    return ESP_OK;
}

/**
 * @brief Calculate fan speed from temperature
 * 
 * Linear interpolation between temp_min (0% speed) and temp_max (100% speed).
 *
 * @param current_temp Current temperature
 * @param setpoint Target temperature (unused in basic calc)
 * @param temp_min Temperature for 0% speed
 * @param temp_max Temperature for 100% speed
 * @return Calculated speed 0-100
 */
uint8_t fan_controller_calc_speed_from_temp(float current_temp, float setpoint, 
                                               float temp_min, float temp_max)
{
    (void)setpoint;  /* Unused in basic calculation */

    if (current_temp <= temp_min) {
        return 0;
    }
    if (current_temp >= temp_max) {
        return 100;
    }

    /* Linear interpolation */
    float ratio = (current_temp - temp_min) / (temp_max - temp_min);
    return (uint8_t)(ratio * 100.0f);
}

/**
 * @brief Calculate thermostat-based speed
 * 
 * Internal function - must be called with mutex held.
 *
 * @param fan Fan identifier
 * @param current_temp Current temperature
 * @return Calculated speed 0-100
 */
static uint8_t calc_thermostat_speed(fan_id_t fan, float current_temp)
{
    fan_state_t *f = &s_fan_ctrl.fans[fan];
    fan_thermostat_params_t *tp = &f->thermostat;

    /* Simple hysteresis control */
    if (current_temp < (tp->temp_setpoint - tp->temp_hysteresis)) {
        return 0;
    }
    if (current_temp > (tp->temp_setpoint + tp->temp_hysteresis)) {
        return fan_controller_calc_speed_from_temp(current_temp, tp->temp_setpoint,
                                                    tp->temp_min, tp->temp_max);
    }

    /* Within hysteresis - maintain current state */
    return f->speed_percent;
}

/**
 * @brief Enter fail-safe mode
 * 
 * Immediately stops all fans. Requires explicit exit to resume.
 *
 * @param reason Reason code for logging
 * @return ESP_OK on success
 */
esp_err_t fan_controller_enter_failsafe(const char *reason)
{
    if (!s_fan_ctrl.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_fan_ctrl.mutex, portMAX_DELAY);

    s_fan_ctrl.fail_safe_active = true;

    /* Stop all fans immediately */
    for (int i = 0; i < FAN_MAX_COUNT; i++) {
        update_runtime(i);
        s_fan_ctrl.fans[i].speed_percent = 0;
        s_fan_ctrl.fans[i].mode = FAN_MODE_FAILSAFE;
    }

    xSemaphoreGive(s_fan_ctrl.mutex);

    ESP_LOGW(TAG, "FAIL-SAFE MODE ENTERED: %s", reason ? reason : "no reason");

    return ESP_OK;
}

/**
 * @brief Exit fail-safe mode
 * 
 * @return ESP_OK on success
 */
esp_err_t fan_controller_exit_failsafe(void)
{
    if (!s_fan_ctrl.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_fan_ctrl.mutex, portMAX_DELAY);

    s_fan_ctrl.fail_safe_active = false;

    /* Restore fans to target speeds */
    for (int i = 0; i < FAN_MAX_COUNT; i++) {
        if (s_fan_ctrl.fans[i].mode == FAN_MODE_FAILSAFE) {
            s_fan_ctrl.fans[i].mode = FAN_MODE_MANUAL;
            s_fan_ctrl.fans[i].speed_percent = s_fan_ctrl.fans[i].target_speed;
        }
    }

    xSemaphoreGive(s_fan_ctrl.mutex);

    ESP_LOGI(TAG, "Fail-safe mode exited");

    return ESP_OK;
}

/**
 * @brief Check if in fail-safe mode
 * 
 * @return true if fail-safe is active
 */
bool fan_controller_is_failsafe(void)
{
    if (!s_fan_ctrl.initialized) {
        return false;
    }

    xSemaphoreTake(s_fan_ctrl.mutex, portMAX_DELAY);
    bool active = s_fan_ctrl.fail_safe_active;
    xSemaphoreGive(s_fan_ctrl.mutex);

    return active;
}

/**
 * @brief Check for fan faults
 * 
 * Monitors tachometer signals if available.
 *
 * @param fan Fan identifier
 * @return true if fault detected
 */
bool fan_controller_check_fault(fan_id_t fan)
{
    if (fan < 0 || fan >= FAN_MAX_COUNT) {
        return false;
    }

    if (!s_fan_ctrl.initialized) {
        return false;
    }

    /* Stub - actual fault detection requires tachometer implementation */
    /* TODO: Implement tachometer reading and stall detection */

    return false;
}
