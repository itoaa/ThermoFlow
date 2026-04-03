/**
 * @file heat_recovery.h
 * @brief Heat Recovery Calculations for Mini-FTX
 * 
 * Calculates heat recovery efficiency and energy savings
 * for a counter-flow heat exchanger in ventilation systems.
 * Includes frost protection and fan control.
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

/* Frost protection thresholds */
#define FTX_FROST_TEMP_THRESHOLD        2.0f    // °C - activate below this
#define FTX_FROST_RH_THRESHOLD          75.0f   // % - activate above this
#define FTX_FROST_HYSTERESIS_TEMP       4.0f    // °C - deactivate above this
#define FTX_FROST_HYSTERESIS_RH         70.0f   // % - deactivate below this
#define FTX_FROST_MIN_RUNTIME_MS        60000   // Min 60s frost protection

/* Fan control hysteresis */
#define FTX_FAN_HYSTERESIS_UP_C         12.0f   // °C - increase speed above
#define FTX_FAN_HYSTERESIS_DOWN_C       8.0f    // °C - decrease speed below
#define FTX_FAN_HYSTERESIS_UP_MAX_C     22.0f   // °C - max speed above
#define FTX_FAN_HYSTERESIS_DOWN_MAX_C   18.0f   // °C - reduce from max below

/* Rate limiting */
#define FTX_MIN_PUBLISH_INTERVAL_MS     5000    // Max 1 publish per 5 seconds

/* Sensor validation limits */
#define FTX_TEMP_MIN_VALID              -50.0f  // °C
#define FTX_TEMP_MAX_VALID              100.0f  // °C
#define FTX_RH_MIN_VALID                0.0f    // %
#define FTX_RH_MAX_VALID                100.0f  // %

/* Filter monitoring */
#define FILTER_PRESSURE_CLEAN_MAX       50.0f   // Pa
#define FILTER_PRESSURE_WARNING         150.0f  // Pa
#define FILTER_PRESSURE_CRITICAL        250.0f  // Pa

/**
 * @brief Heat recovery status
 */
typedef enum {
    FTX_STATUS_OK = 0,
    FTX_STATUS_CORE_FROST_RISK,      // Risk of freezing
    FTX_STATUS_CORE_FROST_ACTIVE,    // Frost protection active
    FTX_STATUS_FILTER_WARNING,       // Filter needs cleaning
    FTX_STATUS_FILTER_CRITICAL,      // Filter blocked
    FTX_STATUS_BYPASS_ACTIVE,        // Summer bypass mode
    FTX_STATUS_AIRFLOW_UNBALANCED,   // Supply/exhaust mismatch
    FTX_STATUS_SENSOR_ERROR,         // Invalid sensor reading
    FTX_STATUS_ERROR
} ftx_status_t;

/**
 * @brief Frost protection state
 */
typedef enum {
    FROST_STATE_IDLE = 0,            // No frost risk
    FROST_STATE_WARNING,             // Frost risk detected
    FROST_STATE_ACTIVE               // Frost protection active
} frost_protection_state_t;

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
    float airflow_balance_percent;       // Balance: supply vs exhaust
    
    // Filter monitoring
    float filter_pressure_pa;          // Pressure drop across filter
    uint32_t filter_runtime_hours;     // Hours since last filter change
    
    // Status
    ftx_status_t status;
    frost_protection_state_t frost_state;
    bool frost_protection_active;      // Pre-heater or defrost active
    bool bypass_active;                // Summer bypass engaged
    bool airflow_unbalanced;           // >10% difference
    
    // Rate limiting
    uint32_t last_sensor_update_ms;
    uint32_t last_publish_ms;
    
} heat_recovery_data_t;

/**
 * @brief Frost protection actions
 */
typedef enum {
    FROST_ACTION_NONE = 0,           // No action needed
    FROST_ACTION_REDUCE_SPEED,       // Reduce fan speed
    FROST_ACTION_PAUSE_FANS,         // Stop fans temporarily
    FROST_ACTION_PREHEAT             // Activate pre-heater
} frost_action_t;

/**
 * @brief Initialize heat recovery module
 * @param core_efficiency Heat exchanger efficiency (0.0-1.0)
 * @return ESP_OK on success
 */
int ftx_init(float core_efficiency);

/**
 * @brief Update sensor readings and calculate efficiency
 * @param data Pointer to heat recovery data structure
 * @return 0 on success, error code on failure
 */
int ftx_update(heat_recovery_data_t *data);

/**
 * @brief Calculate heat recovery efficiency with hysteresis
 * @param outdoor_temp Outside air temperature
 * @param supply_temp Supply air temperature (after HX)
 * @param exhaust_temp Exhaust air temperature (from room)
 * @param last_efficiency Previous efficiency for hysteresis
 * @return Efficiency in percent (0-100)
 */
float ftx_calc_efficiency_hysteresis(float outdoor_temp, 
                                          float supply_temp, 
                                          float exhaust_temp,
                                          float last_efficiency);

/**
 * @brief Calculate energy recovery in Watts
 * @param airflow_m3h Airflow rate (m³/h)
 * @param temp_diff Temperature difference across HX (°C)
 * @return Power recovered in Watts
 */
float ftx_calc_power(float airflow_m3h, float temp_diff);

/**
 * @brief Check for frost risk with hysteresis
 * @param outdoor_temp Outside temperature
 * @param exhaust_rh Exhaust air relative humidity
 * @param current_state Current frost protection state
 * @return Updated frost protection state
 */
frost_protection_state_t ftx_check_frost_hysteresis(
    float outdoor_temp, 
    float exhaust_rh,
    frost_protection_state_t current_state);

/**
 * @brief Activate frost protection and return action
 * @param data Pointer to heat recovery data
 * @return Action to take
 */
frost_action_t ftx_activate_frost_protection(heat_recovery_data_t *data);

/**
 * @brief Deactivate frost protection
 * @param data Pointer to heat recovery data
 */
void ftx_deactivate_frost_protection(heat_recovery_data_t *data);

/**
 * @brief Determine optimal fan speed with hysteresis
 * @param data Pointer to heat recovery data
 * @param current_speed Current fan speed for hysteresis
 * @return Recommended fan speed percentage (0-100)
 */
uint8_t ftx_recommend_fan_speed_hysteresis(
    const heat_recovery_data_t *data,
    uint8_t current_speed);

/**
 * @brief Check airflow balance between supply and exhaust
 * @param supply_flow Supply airflow (m³/h)
 * @param exhaust_flow Exhaust airflow (m³/h)
 * @return Balance percentage: 100% = perfect, <90% or >110% = unbalanced
 */
float ftx_check_airflow_balance(float supply_flow, float exhaust_flow);

/**
 * @brief Validate sensor readings
 * @param temp Temperature reading
 * @param rh Relative humidity reading
 * @return true if valid, false if out of range
 */
bool ftx_validate_sensor(float temp, float rh);

/**
 * @brief Check if enough time has passed since last publish (rate limiting)
 * @param last_publish_ms Timestamp of last publish
 * @param current_ms Current timestamp
 * @return true if OK to publish, false if too soon
 */
bool ftx_check_rate_limit(uint32_t last_publish_ms, uint32_t current_ms);

/**
 * @brief Get filter status based on pressure drop
 * @param pressure_pa Pressure drop across filter (Pa)
 * @return Filter status level
 */
ftx_status_t ftx_get_filter_status(float pressure_pa);

/**
 * @brief Get string representation of status
 * @param status Status code
 * @return Human readable status string
 */
const char* ftx_status_to_string(ftx_status_t status);

/**
 * @brief Calculate daily energy savings
 * @param avg_power_w Average recovered power (W)
 * @return Energy saved in kWh/day
 */
float ftx_daily_savings(float avg_power_w);

#ifdef __cplusplus
}
#endif

#endif // HEAT_RECOVERY_H
