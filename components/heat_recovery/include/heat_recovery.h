/**
 * @file heat_recovery.h
 * @brief Heat Recovery Calculations for Mini-FTX
 * 
 * Calculates heat recovery efficiency and energy savings
 * for a counter-flow heat exchanger in ventilation systems.
 */

#ifndef HEAT_RECOVERY_H
#define HEAT_RECOVERY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Heat recovery core efficiency (0.0 - 1.0)
 * Typical values: 0.75-0.95 for modern heat exchangers
 */
#define FTX_CORE_EFFICIENCY_DEFAULT     0.85f

/* Air properties */
#define AIR_DENSITY_KG_M3               1.225f
#define AIR_SPECIFIC_HEAT_J_KG_K        1005.0f

/* Filter pressure drop thresholds (Pa) */
#define FILTER_PRESSURE_CLEAN_MAX       50.0f
#define FILTER_PRESSURE_WARNING         150.0f
#define FILTER_PRESSURE_CRITICAL        250.0f

/**
 * @brief Heat recovery status
 */
typedef enum {
    FTX_STATUS_OK = 0,
    FTX_STATUS_CORE_FROST_RISK,      // Risk of freezing
    FTX_STATUS_FILTER_WARNING,       // Filter needs cleaning
    FTX_STATUS_FILTER_CRITICAL,      // Filter blocked
    FTX_STATUS_BYPASS_ACTIVE,        // Summer bypass mode
    FTX_STATUS_ERROR
} ftx_status_t;

/**
 * @brief Heat recovery data structure
 */
typedef struct {
    // Temperatures (°C)
    float outdoor_temp;              // Outside air (supply before HX)
    float supply_temp;               // Supply air (after HX)
    float exhaust_temp;              // Exhaust air (from room)
    float extract_temp;              // Extract air (before HX)
    
    // Humidity (%RH)
    float outdoor_rh;
    float supply_rh;
    float exhaust_rh;
    float extract_rh;
    
    // Calculated values
    float efficiency_percent;          // Heat recovery efficiency
    float energy_recovery_w;           // Power recovered (Watts)
    float energy_saving_kwh_day;       // Daily energy saving
    float frost_risk_temp;             // Temperature where frost forms
    
    // Airflow
    float airflow_supply_m3h;          // Supply airflow (m³/h)
    float airflow_exhaust_m3h;         // Exhaust airflow (m³/h)
    
    // Filter monitoring
    float filter_pressure_pa;          // Pressure drop across filter
    uint32_t filter_runtime_hours;     // Hours since last filter change
    
    // Status
    ftx_status_t status;
    bool bypass_active;                // Summer bypass engaged
    bool frost_protection_active;      // Pre-heater or defrost active
    
} heat_recovery_data_t;

/**
 * @brief Initialize heat recovery module
 * @param core_efficiency Heat exchanger efficiency (0.0-1.0)
 * @return ESP_OK on success
 */
esp_err_t heat_recovery_init(float core_efficiency);

/**
 * @brief Update sensor readings and calculate efficiency
 * @param data Pointer to heat recovery data structure
 * @return ESP_OK on success
 */
esp_err_t heat_recovery_update(heat_recovery_data_t *data);

/**
 * @brief Calculate heat recovery efficiency
 * @param outdoor_temp Outside air temperature
 * @param supply_temp Supply air temperature (after HX)
 * @param exhaust_temp Exhaust air temperature (from room)
 * @return Efficiency in percent (0-100)
 */
float heat_recovery_calculate_efficiency(float outdoor_temp, 
                                          float supply_temp, 
                                          float exhaust_temp);

/**
 * @brief Calculate energy recovery in Watts
 * @param airflow_m3h Airflow rate (m³/h)
 * @param temp_diff Temperature difference across HX (°C)
 * @return Power recovered in Watts
 */
float heat_recovery_calculate_power(float airflow_m3h, float temp_diff);

/**
 * @brief Check if frost protection should activate
 * @param outdoor_temp Outside temperature
 * @param exhaust_rh Exhaust air relative humidity
 * @return true if frost risk detected
 */
bool heat_recovery_check_frost_risk(float outdoor_temp, float exhaust_rh);

/**
 * @brief Determine optimal fan speed based on conditions
 * @param data Pointer to heat recovery data
 * @return Recommended fan speed percentage (0-100)
 */
uint8_t heat_recovery_recommend_fan_speed(const heat_recovery_data_t *data);

/**
 * @brief Get filter status based on pressure drop
 * @param pressure_pa Pressure drop across filter (Pa)
 * @return Filter status level
 */
ftx_status_t heat_recovery_get_filter_status(float pressure_pa);

/**
 * @brief Get string representation of status
 * @param status Status code
 * @return Human readable status string
 */
const char* heat_recovery_status_to_string(ftx_status_t status);

/**
 * @brief Convert airflow from PWM duty to m³/h
 * @param pwm_duty PWM duty cycle (0-100)
 * @param fan_max_m3h Maximum fan capacity (m³/h)
 * @return Calculated airflow (m³/h)
 */
float heat_recovery_pwm_to_airflow(uint8_t pwm_duty, uint16_t fan_max_m3h);

/**
 * @brief Calculate daily energy savings
 * @param avg_power_w Average recovered power (W)
 * @return Energy saved in kWh/day
 */
float heat_recovery_daily_savings(float avg_power_w);

#ifdef __cplusplus
}
#endif

#endif // HEAT_RECOVERY_H
