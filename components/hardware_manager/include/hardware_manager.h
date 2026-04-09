/**
 * @file hardware_manager.h
 * @brief Hardware detection and simulation mode manager
 * 
 * Detects connected sensors and hardware at boot.
 * Falls back to simulation mode if hardware is missing.
 * Allows "bare" ESP32-S3 operation for testing and onboarding.
 * 
 * @version 1.1.0
 * @date 2026-04-09
 */

#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hardware component IDs */
typedef enum {
    HW_COMPONENT_NONE = 0,
    HW_COMPONENT_SHT40_SENSOR_1,    /* Tilluft sensor */
    HW_COMPONENT_SHT40_SENSOR_2,    /* Frånluft sensor */
    HW_COMPONENT_SHT40_SENSOR_3,    /* Avluft sensor */
    HW_COMPONENT_SHT40_SENSOR_4,    /* Uteluft sensor */
    HW_COMPONENT_OLED_DISPLAY,      /* SSD1306/SH1106 display */
    HW_COMPONENT_FAN_1,             /* PWM fan controller 1 */
    HW_COMPONENT_FAN_2,             /* PWM fan controller 2 */
    HW_COMPONENT_COUNT
} hw_component_t;

/* Hardware detection status */
typedef struct {
    bool detected[HW_COMPONENT_COUNT];
    bool simulation_mode;
    uint32_t detection_time_ms;
    char detected_components_str[256];
} hw_status_t;

/**
 * @brief Initialize hardware manager and detect all components
 * 
 * Runs at boot to probe I2C bus and GPIO pins for connected hardware.
 * If no sensors detected, automatically enables simulation mode.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t hardware_manager_init(void);

/**
 * @brief Check if specific hardware component is detected
 * 
 * @param component Component to check
 * @return true if detected, false otherwise
 */
bool hardware_is_detected(hw_component_t component);

/**
 * @brief Check if running in simulation mode
 * 
 * @return true if simulation mode active (no real sensors)
 */
bool hardware_is_simulation_mode(void);

/**
 * @brief Enable or disable simulation mode manually
 * 
 * @param enable true to enable simulation, false to disable
 * @return ESP_OK on success
 */
esp_err_t hardware_set_simulation_mode(bool enable);

/**
 * @brief Get full hardware status
 * 
 * @param[out] status Pointer to status structure to fill
 * @return ESP_OK on success
 */
esp_err_t hardware_get_status(hw_status_t *status);

/**
 * @brief Get human-readable detection summary
 * 
 * @return String describing detected hardware
 */
const char* hardware_get_summary(void);

/**
 * @brief Re-detect hardware (for runtime hot-plug support)
 * 
 * Useful if sensors are connected after boot.
 * 
 * @return ESP_OK if changes detected
 */
esp_err_t hardware_redetect(void);

/**
 * @brief Get count of detected sensors
 * 
 * @return Number of SHT40 sensors detected (0-4)
 */
uint8_t hardware_get_sensor_count(void);

/**
 * @brief Get count of detected fans
 * 
 * @return Number of PWM fans detected (0-2)
 */
uint8_t hardware_get_fan_count(void);

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_MANAGER_H */
