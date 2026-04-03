/**
 * @file heat_recovery.c
 * @brief Heat Recovery Calculations Implementation with All Fixes
 */

#include "heat_recovery.h"
#include "esp_timer.h"  // For esp_timer_get_time() in microseconds
#include <math.h>

static float s_core_efficiency = FTX_CORE_EFFICIENCY_DEFAULT;
static uint64_t s_frost_activation_time_us = 0;

int ftx_init(float core_efficiency) {
    if (core_efficiency > 0.0f && core_efficiency <= 1.0f) {
        s_core_efficiency = core_efficiency;
    }
    s_frost_activation_time_us = 0;
    return 0;
}

/**
 * @brief Validate sensor readings (FIX #4)
 */
bool ftx_validate_sensor(float temp, float rh) {
    // Check for NaN or infinity
    if (isnan(temp) || isinf(temp) || isnan(rh) || isinf(rh)) {
        return false;
    }
    
    // Check temperature range
    if (temp < FTX_TEMP_MIN_VALID || temp > FTX_TEMP_MAX_VALID) {
        return false;
    }
    
    // Check humidity range
    if (rh < FTX_RH_MIN_VALID || rh > FTX_RH_MAX_VALID) {
        return false;
    }
    
    return true;
}

/**
 * @brief Check frost risk with hysteresis (FIX #1 - proper state machine)
 */
frost_protection_state_t ftx_check_frost_hysteresis(
    float outdoor_temp, 
    float exhaust_rh,
    frost_protection_state_t current_state) {
    
    uint64_t current_time_us = esp_timer_get_time();
    
    switch (current_state) {
        case FROST_STATE_IDLE:
            // Activate if below threshold
            if (outdoor_temp < FTX_FROST_TEMP_THRESHOLD && 
                exhaust_rh > FTX_FROST_RH_THRESHOLD) {
                return FROST_STATE_WARNING;
            }
            return FROST_STATE_IDLE;
            
        case FROST_STATE_WARNING:
            // Move to active if conditions persist
            if (outdoor_temp < FTX_FROST_TEMP_THRESHOLD && 
                exhaust_rh > FTX_FROST_RH_THRESHOLD) {
                s_frost_activation_time_us = current_time_us;
                return FROST_STATE_ACTIVE;
            }
            // Return to idle if conditions improve
            if (outdoor_temp > FTX_FROST_HYSTERESIS_TEMP || 
                exhaust_rh < FTX_FROST_HYSTERESIS_RH) {
                return FROST_STATE_IDLE;
            }
            return FROST_STATE_WARNING;
            
        case FROST_STATE_ACTIVE:
            // Minimum runtime check (60 seconds)
            uint64_t elapsed_ms = (current_time_us - s_frost_activation_time_us) / 1000;
            if (elapsed_ms < FTX_FROST_MIN_RUNTIME_MS) {
                return FROST_STATE_ACTIVE;  // Keep active for minimum time
            }
            
            // Deactivate only with hysteresis
            if (outdoor_temp > FTX_FROST_HYSTERESIS_TEMP || 
                exhaust_rh < FTX_FROST_HYSTERESIS_RH) {
                return FROST_STATE_IDLE;
            }
            return FROST_STATE_ACTIVE;
    }
    
    return FROST_STATE_IDLE;
}

/**
 * @brief Activate frost protection and return action (FIX #1)
 */
frost_action_t ftx_activate_frost_protection(heat_recovery_data_t *data) {
    if (!data) return FROST_ACTION_NONE;
    
    data->frost_protection_active = true;
    data->frost_state = FROST_STATE_ACTIVE;
    data->status = FTX_STATUS_CORE_FROST_ACTIVE;
    
    // Determine action based on severity
    if (data->outdoor_temp < -5.0f) {
        // Severe cold - stop fans to prevent ice formation
        return FROST_ACTION_PAUSE_FANS;
    } else if (data->outdoor_temp < 0.0f) {
        // Moderate cold - reduce speed
        return FROST_ACTION_REDUCE_SPEED;
    } else {
        // Mild - just warning
        return FROST_ACTION_REDUCE_SPEED;
    }
}

/**
 * @brief Deactivate frost protection (FIX #1)
 */
void ftx_deactivate_frost_protection(heat_recovery_data_t *data) {
    if (!data) return;
    
    data->frost_protection_active = false;
    data->frost_state = FROST_STATE_IDLE;
    
    // Clear status if no other issues
    if (data->status == FTX_STATUS_CORE_FROST_ACTIVE) {
        data->status = FTX_STATUS_OK;
    }
}

/**
 * @brief Calculate efficiency with hysteresis (smoothing)
 */
float ftx_calc_efficiency_hysteresis(float t_out, float t_supply, float t_exhaust, float last_efficiency) {
    // Validate inputs first
    if (!ftx_validate_sensor(t_out, 50.0f) || 
        !ftx_validate_sensor(t_supply, 50.0f) || 
        !ftx_validate_sensor(t_exhaust, 50.0f)) {
        return last_efficiency;  // Return last valid value
    }
    
    if (fabsf(t_exhaust - t_out) < 0.1f) {
        return last_efficiency;
    }
    
    float efficiency = ((t_supply - t_out) / (t_exhaust - t_out)) * 100.0f;
    
    // Clamp to realistic values
    if (efficiency < 0.0f) efficiency = 0.0f;
    if (efficiency > 100.0f) efficiency = 100.0f;
    
    // Apply simple smoothing (exponential moving average)
    float alpha = 0.3f;  // 30% new value, 70% old
    float smoothed = (alpha * efficiency) + ((1.0f - alpha) * last_efficiency);
    
    return smoothed;
}

float ftx_calc_power(float airflow_m3h, float temp_diff) {
    // Validate inputs
    if (airflow_m3h < 0 || airflow_m3h > 1000) return 0.0f;
    if (temp_diff < -50 || temp_diff > 50) return 0.0f;
    
    float mass_flow = (airflow_m3h * AIR_DENSITY_KG_M3) / 3600.0f;
    float power = mass_flow * AIR_SPECIFIC_HEAT_J_KG_K * temp_diff;
    power *= s_core_efficiency;
    
    return power;
}

/**
 * @brief Recommend fan speed with hysteresis (FIX #2)
 */
uint8_t ftx_recommend_fan_speed_hysteresis(
    const heat_recovery_data_t *data,
    uint8_t current_speed) {
    
    if (!data) return current_speed;
    
    float temp_diff = fabsf(data->exhaust_temp - data->outdoor_temp);
    uint8_t target_speed = 30;  // Start with minimum
    
    // Determine target speed with hysteresis
    if (temp_diff > FTX_FAN_HYSTERESIS_UP_MAX_C) {
        target_speed = 90;
    } else if (temp_diff > FTX_FAN_HYSTERESIS_UP_C) {
        target_speed = 70;
    } else if (temp_diff > FTX_FAN_HYSTERESIS_DOWN_C) {
        target_speed = 50;
    } else {
        target_speed = 30;
    }
    
    // Apply hysteresis - don't change too quickly
    int8_t speed_diff = (int8_t)target_speed - (int8_t)current_speed;
    
    if (speed_diff > 20) {
        // Increase gradually
        return current_speed + 10;
    } else if (speed_diff < -20) {
        // Decrease gradually
        return current_speed - 10;
    } else if (abs(speed_diff) < 5) {
        // Small change - keep current to prevent oscillation
        return current_speed;
    }
    
    return target_speed;
}

/**
 * @brief NEW: Calculate maximum safe fan speed and reason (for UI/MQTT)
 */
void ftx_calculate_max_safe_speed(heat_recovery_data_t *data, uint8_t requested_speed) {
    if (!data) return;
    
    data->fan_speed_requested = requested_speed;
    data->fan_speed_max_safe = requested_speed;  // Start with requested
    data->fan_limit_reason = LIMIT_REASON_NONE;
    data->fan_limit_description = "Normal drift - ingen begränsning";
    
    // Check frost risk
    if (data->frost_state >= FROST_STATE_WARNING) {
        data->fan_speed_max_safe = 30;  // Max 30% vid frost-risk
        data->fan_limit_reason = LIMIT_REASON_FROST_RISK;
        data->fan_limit_description = "Frost-risk - begränsat till 30% för att skydda värmeväxlaren";
        if (requested_speed > 30) {
            data->fan_speed_current = 30;
        } else {
            data->fan_speed_current = requested_speed;
        }
        return;
    }
    
    // Check condensation risk
    if (data->condensation_risk) {
        data->fan_speed_max_safe = 50;
        data->fan_limit_reason = LIMIT_REASON_CONDENSATION;
        data->fan_limit_description = "Kondens-risk - begränsat till 50% för att förhindra vattenansamling";
        if (requested_speed > 50) {
            data->fan_speed_current = 50;
        } else {
            data->fan_speed_current = requested_speed;
        }
        return;
    }
    
    // Check high humidity
    if (data->exhaust_rh > 90.0f) {
        data->fan_speed_max_safe = 60;
        data->fan_limit_reason = LIMIT_REASON_HIGH_HUMIDITY;
        data->fan_limit_description = "Hög luftfuktighet - begränsat till 60%";
        if (requested_speed > 60) {
            data->fan_speed_current = 60;
        } else {
            data->fan_speed_current = requested_speed;
        }
        return;
    }
    
    // Check filter pressure
    if (data->filter_pressure_pa > FILTER_PRESSURE_WARNING) {
        data->fan_speed_max_safe = 70;
        data->fan_limit_reason = LIMIT_REASON_FILTER_PRESSURE;
        data->fan_limit_description = "Högt filtertryck - begränsat till 70%, byt filter snart";
        if (requested_speed > 70) {
            data->fan_speed_current = 70;
        } else {
            data->fan_speed_current = requested_speed;
        }
        return;
    }
    
    // No limits apply
    data->fan_speed_current = requested_speed;
    data->fan_speed_max_safe = 100;
}

/**
 * @brief NEW: Check for condensation risk
 */
bool ftx_check_condensation_risk(const heat_recovery_data_t *data) {
    if (!data) return false;
    
    // Calculate dew point of exhaust air
    // Simplified Magnus formula
    float a = 17.271f;
    float b = 237.7f;
    float alpha = ((a * data->exhaust_temp) / (b + data->exhaust_temp)) + logf(data->exhaust_rh / 100.0f);
    float dew_point = (b * alpha) / (a - alpha);
    
    // Risk if cold surface (outdoor side) is below dew point
    if (data->outdoor_temp < dew_point + 2.0f) {  // 2°C margin
        return true;
    }
    
    return false;
}

/**
 * @brief Check airflow balance (FIX #5)
 */
float ftx_check_airflow_balance(float supply_flow, float exhaust_flow) {
    if (supply_flow <= 0 || exhaust_flow <= 0) {
        return 0.0f;  // Invalid
    }
    
    // Calculate balance: 100% = perfect match
    // < 100% means supply < exhaust (underpressure)
    // > 100% means supply > exhaust (overpressure)
    float balance = (supply_flow / exhaust_flow) * 100.0f;
    
    return balance;
}

/**
 * @brief Check rate limit (FIX #3)
 */
bool ftx_check_rate_limit(uint32_t last_publish_ms, uint32_t current_ms) {
    if (last_publish_ms == 0) {
        return true;  // First publish
    }
    
    uint32_t elapsed_ms = current_ms - last_publish_ms;
    return (elapsed_ms >= FTX_MIN_PUBLISH_INTERVAL_MS);
}

ftx_status_t ftx_get_filter_status(float pressure_pa) {
    if (pressure_pa > FILTER_PRESSURE_CRITICAL) {
        return FTX_STATUS_FILTER_CRITICAL;
    } else if (pressure_pa > FILTER_PRESSURE_WARNING) {
        return FTX_STATUS_FILTER_WARNING;
    }
    return FTX_STATUS_OK;
}

const char* ftx_status_to_string(ftx_status_t status) {
    switch (status) {
        case FTX_STATUS_OK: return "OK";
        case FTX_STATUS_CORE_FROST_RISK: return "Frost Risk";
        case FTX_STATUS_CORE_FROST_ACTIVE: return "Frost Protection Active";
        case FTX_STATUS_FILTER_WARNING: return "Filter Warning";
        case FTX_STATUS_FILTER_CRITICAL: return "Filter Critical";
        case FTX_STATUS_BYPASS_ACTIVE: return "Bypass Active";
        case FTX_STATUS_AIRFLOW_UNBALANCED: return "Airflow Unbalanced";
        case FTX_STATUS_SENSOR_ERROR: return "Sensor Error";
        case FTX_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

/**
 * @brief NEW: Get string description of fan limit reason (for UI/MQTT)
 */
const char* ftx_limit_reason_to_string(fan_limit_reason_t reason) {
    switch (reason) {
        case LIMIT_REASON_NONE: return "Ingen begränsning";
        case LIMIT_REASON_FROST_RISK: return "Frost-risk";
        case LIMIT_REASON_CONDENSATION: return "Kondens-risk";
        case LIMIT_REASON_HIGH_HUMIDITY: return "Hög luftfuktighet";
        case LIMIT_REASON_FILTER_PRESSURE: return "Högt filtertryck";
        case LIMIT_REASON_USER_OVERRIDE: return "Användarbegränsning";
        case LIMIT_REASON_AUTO_OPTIMIZE: return "Energibesparing";
        default: return "Okänd anledning";
    }
}

float ftx_daily_savings(float avg_power_w) {
    return (avg_power_w * 24.0f) / 1000.0f;
}

/**
 * @brief Main update function with all validation
 */
int ftx_update(heat_recovery_data_t *data) {
    if (!data) return -1;
    
    // Get current time for rate limiting
    uint64_t current_time_us = esp_timer_get_time();
    uint32_t current_ms = (uint32_t)(current_time_us / 1000);
    
    // Validate all sensors (FIX #4)
    if (!ftx_validate_sensor(data->outdoor_temp, data->outdoor_rh) ||
        !ftx_validate_sensor(data->supply_temp, data->supply_rh) ||
        !ftx_validate_sensor(data->exhaust_temp, data->exhaust_rh)) {
        data->status = FTX_STATUS_SENSOR_ERROR;
        return -1;
    }
    
    // Check condensation risk (NEW)
    data->condensation_risk = ftx_check_condensation_risk(data);
    
    // Check frost protection with hysteresis (FIX #1)
    frost_protection_state_t new_frost_state = ftx_check_frost_hysteresis(
        data->outdoor_temp, 
        data->exhaust_rh,
        data->frost_state
    );
    
    if (new_frost_state == FROST_STATE_ACTIVE && 
        data->frost_state != FROST_STATE_ACTIVE) {
        // Just activated
        frost_action_t action = ftx_activate_frost_protection(data);
        // Apply action...
        (void)action;  // Mark as used
    } else if (new_frost_state == FROST_STATE_IDLE && 
               data->frost_state == FROST_STATE_ACTIVE) {
        // Just deactivated
        ftx_deactivate_frost_protection(data);
    }
    
    data->frost_state = new_frost_state;
    
    // Calculate max safe speed (NEW)
    ftx_calculate_max_safe_speed(data, data->fan_speed_requested);
    
    // Check airflow balance (FIX #5)
    float balance = ftx_check_airflow_balance(
        data->airflow_supply_m3h, 
        data->airflow_exhaust_m3h
    );
    data->airflow_balance_percent = balance;
    data->airflow_unbalanced = (balance < 90.0f || balance > 110.0f);
    
    if (data->airflow_unbalanced && data->status == FTX_STATUS_OK) {
        data->status = FTX_STATUS_AIRFLOW_UNBALANCED;
    }
    
    // Update last timestamp
    data->last_sensor_update_ms = current_ms;
    
    return 0;
}
