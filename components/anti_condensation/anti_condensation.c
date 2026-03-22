/**
 * @file anti_condensation.c
 * @brief Anti-Condensation Protection - ESP-IDF
 *
 * Implements humidity monitoring with hysteresis to prevent
 * condensation on cooling surfaces. Triggers alerts when
 * relative humidity exceeds safe thresholds.
 *
 * Features:
 * - Configurable RH threshold and hysteresis
 * - Thread-safe state access
 * - Alert callbacks for integration
 * - Runtime statistics
 *
 * @author Ola Andersson
 * @version 1.0.0
 * @date 2026-03-22
 *
 * @section changelog Change Log
 * - 1.0.0 (2026-03-22): Initial implementation
 *   - Basic threshold monitoring with hysteresis
 *   - Alert callback support
 *   - Thread-safe operation
 */

#include "anti_condensation.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "string.h"

/* Logging tag */
static const char *TAG = "ANTI_COND";

/* Default configuration values */
#define DEFAULT_RH_THRESHOLD    90.0f   /* Alert above 90% RH */
#define DEFAULT_HYSTERESIS      5.0f    /* Clear at 85% RH */
#define DEFAULT_CHECK_INTERVAL  1000u   /* 1 second between checks */

/**
 * @brief Module state structure
 */
typedef struct {
    bool initialized;                    /* Module initialized flag */
    bool alert_active;                   /* Current alert state */
    float rh_threshold;                /* Alert trigger threshold (% RH) */
    float rh_hysteresis;               /* Hysteresis for clearing alert */
    uint32_t alert_count;                /* Total alerts triggered */
    uint64_t alert_start_time_us;        /* When current alert started */
    uint64_t total_alert_time_us;        /* Cumulative alert duration */
    condensation_alert_callback_t callback; /* Optional alert callback */
    void *callback_user_data;            /* Callback context */
    SemaphoreHandle_t mutex;             /* Thread safety */
} ac_state_t;

/* Global state instance */
static ac_state_t s_state = {0};

/**
 * @brief Initialize anti-condensation protection
 *
 * Sets up mutex and default thresholds.
 * Must be called before using other functions.
 *
 * @return ESP_OK on success, ESP_ERR_NO_MEM if mutex creation fails
 */
esp_err_t anti_condensation_init(void)
{
    /* Create mutex for thread safety */
    s_state.mutex = xSemaphoreCreateMutex();
    if (s_state.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    /* Take mutex during initialization */
    xSemaphoreTake(s_state.mutex, portMAX_DELAY);

    /* Clear all state */
    memset(&s_state, 0, sizeof(s_state));
    s_state.mutex = xSemaphoreCreateMutex();  /* Restore mutex after memset */

    /* Set defaults */
    s_state.rh_threshold = DEFAULT_RH_THRESHOLD;
    s_state.rh_hysteresis = DEFAULT_HYSTERESIS;
    s_state.initialized = true;

    xSemaphoreGive(s_state.mutex);

    ESP_LOGI(TAG, "Initialized: threshold=%.1f%%, hysteresis=%.1f%%",
             s_state.rh_threshold, s_state.rh_hysteresis);

    return ESP_OK;
}

/**
 * @brief Check humidity for condensation risk
 *
 * Implements hysteresis to prevent rapid toggling:
 * - Alert triggers when RH >= threshold
 * - Alert clears when RH <= (threshold - hysteresis)
 *
 * @param[in] rh Current relative humidity in percent (0-100)
 * @param[in] temp Current temperature in Celsius (for future dewpoint calc)
 * @return ESP_OK if no alert, ESP_ERR_INVALID_STATE if alert active
 */
esp_err_t anti_condensation_check(float rh, float temp)
{
    (void)temp;  /* Reserved for future dewpoint calculation */

    /* Validate initialization */
    if (!s_state.initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Clamp RH to valid range */
    if (rh < 0.0f) rh = 0.0f;
    if (rh > 100.0f) rh = 100.0f;

    xSemaphoreTake(s_state.mutex, portMAX_DELAY);

    esp_err_t result = ESP_OK;

    if (!s_state.alert_active) {
        /* Check if we should enter alert state */
        if (rh >= s_state.rh_threshold) {
            s_state.alert_active = true;
            s_state.alert_start_time_us = esp_timer_get_time();
            s_state.alert_count++;

            ESP_LOGW(TAG, "ALERT: RH %.1f%% exceeds threshold %.1f%%",
                     rh, s_state.rh_threshold);

            /* Invoke callback if registered */
            if (s_state.callback != NULL) {
                s_state.callback(true, rh, s_state.callback_user_data);
            }

            result = ESP_ERR_INVALID_STATE;  /* Signal alert condition */
        }
    } else {
        /* Check if we should exit alert state */
        float clear_threshold = s_state.rh_threshold - s_state.rh_hysteresis;

        if (rh <= clear_threshold) {
            s_state.alert_active = false;

            /* Calculate alert duration */
            uint64_t now = esp_timer_get_time();
            uint64_t duration = now - s_state.alert_start_time_us;
            s_state.total_alert_time_us += duration;

            ESP_LOGI(TAG, "Alert cleared: RH %.1f%% below %.1f%% (duration %llu ms)",
                     rh, clear_threshold, duration / 1000);

            /* Invoke callback if registered */
            if (s_state.callback != NULL) {
                s_state.callback(false, rh, s_state.callback_user_data);
            }
        }
    }

    xSemaphoreGive(s_state.mutex);
    return result;
}

/**
 * @brief Check if condensation alert is currently active
 *
 * @return true if alert condition exists, false otherwise
 */
bool anti_condensation_is_active(void)
{
    if (!s_state.initialized) {
        return false;
    }

    xSemaphoreTake(s_state.mutex, portMAX_DELAY);
    bool active = s_state.alert_active;
    xSemaphoreGive(s_state.mutex);

    return active;
}

/**
 * @brief Set alert callback function
 *
 * Called whenever alert state changes. Can be NULL to disable.
 *
 * @param[in] callback Function to call on alert state change
 * @param[in] user_data Context pointer passed to callback
 * @return ESP_OK on success
 */
esp_err_t anti_condensation_set_callback(condensation_alert_callback_t callback,
                                          void *user_data)
{
    if (!s_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_state.mutex, portMAX_DELAY);
    s_state.callback = callback;
    s_state.callback_user_data = user_data;
    xSemaphoreGive(s_state.mutex);

    ESP_LOGI(TAG, "Alert callback %s",
             callback ? "registered" : "unregistered");
    return ESP_OK;
}

/**
 * @brief Get current configuration
 *
 * @param[out] threshold Current RH threshold
 * @param[out] hysteresis Current hysteresis value
 * @return ESP_OK on success
 */
esp_err_t anti_condensation_get_config(float *threshold, float *hysteresis)
{
    if (!s_state.initialized || threshold == NULL || hysteresis == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_state.mutex, portMAX_DELAY);
    *threshold = s_state.rh_threshold;
    *hysteresis = s_state.rh_hysteresis;
    xSemaphoreGive(s_state.mutex);

    return ESP_OK;
}

/**
 * @brief Update configuration
 *
 * @param[in] threshold New RH threshold
 * @param[in] hysteresis New hysteresis value
 * @return ESP_OK on success
 */
esp_err_t anti_condensation_set_config(float threshold, float hysteresis)
{
    if (!s_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Validate parameters */
    if (threshold < 0.0f || threshold > 100.0f) {
        ESP_LOGE(TAG, "Invalid threshold: %.1f (must be 0-100)", threshold);
        return ESP_ERR_INVALID_ARG;
    }

    if (hysteresis < 0.0f || hysteresis >= threshold) {
        ESP_LOGE(TAG, "Invalid hysteresis: %.1f (must be 0-threshold)", hysteresis);
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_state.mutex, portMAX_DELAY);
    s_state.rh_threshold = threshold;
    s_state.rh_hysteresis = hysteresis;
    xSemaphoreGive(s_state.mutex);

    ESP_LOGI(TAG, "Config updated: threshold=%.1f%%, hysteresis=%.1f%%",
             threshold, hysteresis);
    return ESP_OK;
}

/**
 * @brief Get runtime statistics
 *
 * @param[out] alert_count Total number of alerts triggered
 * @param[out] total_alert_time_ms Total time spent in alert state
 * @return ESP_OK on success
 */
esp_err_t anti_condensation_get_stats(uint32_t *alert_count, uint64_t *total_alert_time_ms)
{
    if (!s_state.initialized || alert_count == NULL || total_alert_time_ms == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_state.mutex, portMAX_DELAY);
    *alert_count = s_state.alert_count;
    *total_alert_time_ms = s_state.total_alert_time_us / 1000;
    xSemaphoreGive(s_state.mutex);

    return ESP_OK;
}

/**
 * @brief Deinitialize module
 *
 * Clears alert if active and releases resources.
 *
 * @return ESP_OK on success
 */
esp_err_t anti_condensation_deinit(void)
{
    if (!s_state.initialized) {
        return ESP_OK;  /* Idempotent */
    }

    /* Clear any active alert */
    if (s_state.alert_active) {
        uint64_t now = esp_timer_get_time();
        s_state.total_alert_time_us += (now - s_state.alert_start_time_us);
        s_state.alert_active = false;

        if (s_state.callback != NULL) {
            s_state.callback(false, 0.0f, s_state.callback_user_data);
        }
    }

    s_state.initialized = false;

    /* Release mutex */
    if (s_state.mutex != NULL) {
        vSemaphoreDelete(s_state.mutex);
        s_state.mutex = NULL;
    }

    ESP_LOGI(TAG, "Deinitialized (total alerts: %lu)", s_state.alert_count);
    return ESP_OK;
}
