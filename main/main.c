/**
 * @file main.c
 * @brief ThermoFlow - ESP32-S3 Climate Monitoring and Control System
 */

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_task_wdt.h>
#include <nvs_flash.h>

#include "thermoflow_version.h"
#include "thermoflow_config.h"
#include "fan_controller.h"
#include "anti_condensation.h"
#include "wifi_manager.h"
#include "hardware_manager.h"
#include "sensor_manager.h"
#include "ota_manager.h"
#include "web_server.h"
#include "heat_recovery.h"
#include "mqtt_ftx.h"
#include "security_manager.h"
#include "audit_log.h"
#include "log_manager.h"
#include "rate_limiter.h"
#include "display_manager.h"
#include "device_profile.h"

static const char *TAG = "THERMOFLOW";

#define CONTROL_TASK_STACK_SIZE       6144u
#define CONTROL_TASK_PRIORITY         5
#define OTA_MONITOR_TASK_STACK_SIZE   4096u
#define OTA_MONITOR_TASK_PRIORITY     3
#define MAIN_LOOP_INTERVAL_MS         5000u
#define CONTROL_LOOP_INTERVAL_MS      1000u
#define OTA_MONITOR_INTERVAL_MS       30000u
#define OTA_HEALTH_DELAY_MS           120000u

#define SIM_MIN_TEMP_C                15.0f
#define SIM_MAX_TEMP_C                25.0f

static SemaphoreHandle_t g_sensor_mutex = NULL;

static heat_recovery_data_t g_ftx_data = {0};
static bool g_ftx_initialized = false;
static bool g_mqtt_started = false;
static bool g_ota_health_ok = false;
static uint32_t g_boot_time_ms = 0;

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG,
                 "NVS partition needs erase (%s) — WiFi and other saved settings will be cleared",
                 esp_err_to_name(ret));
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static void ota_event_callback(ota_state_t state, ota_error_t error, void *user_data)
{
    (void)user_data;

    switch (state) {
        case OTA_STATE_DOWNLOADING:
            audit_log_event(AUDIT_EVENT_OTA_START, AUDIT_SEVERITY_INFO, "OTA download started");
            break;
        case OTA_STATE_VERIFYING:
            ESP_LOGI(TAG, "OTA: Verifying firmware...");
            break;
        case OTA_STATE_READY:
            ESP_LOGI(TAG, "OTA: Update ready to apply");
            break;
        case OTA_STATE_APPLYING:
            ESP_LOGI(TAG, "OTA: Applying update...");
            break;
        case OTA_STATE_ROLLBACK:
            audit_log_event(AUDIT_EVENT_OTA_FAILURE, AUDIT_SEVERITY_WARNING, "OTA rollback");
            break;
        case OTA_STATE_ERROR:
            audit_log_event(AUDIT_EVENT_OTA_FAILURE, AUDIT_SEVERITY_ERROR,
                            "OTA error: %s", ota_manager_error_string(error));
            break;
        default:
            break;
    }
}

static esp_err_t init_ota(void)
{
    ota_config_t ota_config = {
        .use_https = true,
        .verify_signature = true,
        .verify_hash = true,
        .enable_anti_rollback = true,
        .min_security_version = THERMOFLOW_SECURITY_VERSION,
        .check_interval_ms = OTA_CHECK_INTERVAL_MS,
        .security_version = THERMOFLOW_SECURITY_VERSION,
        .auto_rollback = true,
        .event_cb = ota_event_callback,
    };

    strncpy(ota_config.update_url, OTA_SERVER_URL, sizeof(ota_config.update_url) - 1);

    esp_err_t ret = ota_manager_init(&ota_config);
    if (ret != ESP_OK) {
        return ret;
    }

    ota_status_t status = {0};
    ota_manager_get_status(&status);

    if (status.can_rollback) {
        ESP_LOGI(TAG, "OTA: Pending validation after update (deferred %lu ms)",
                 (unsigned long)OTA_HEALTH_DELAY_MS);
    }

    return ESP_OK;
}

static void populate_ftx_from_sensors(const sensor_manager_data_t *sensors, heat_recovery_data_t *ftx)
{
    if (!sensors || !ftx) {
        return;
    }

    if (sensors->num_sensors >= 4 && sensors->valid[3]) {
        ftx->outdoor_temp = sensors->temperature[3];
        ftx->outdoor_rh = sensors->humidity[3];
    }
    if (sensors->num_sensors >= 1 && sensors->valid[0]) {
        ftx->supply_temp = sensors->temperature[0];
        ftx->supply_rh = sensors->humidity[0];
    }
    if (sensors->num_sensors >= 2 && sensors->valid[1]) {
        ftx->extract_temp = sensors->temperature[1];
        ftx->extract_rh = sensors->humidity[1];
    }
    if (sensors->num_sensors >= 3 && sensors->valid[2]) {
        ftx->exhaust_temp = sensors->temperature[2];
        ftx->exhaust_rh = sensors->humidity[2];
    }

    ftx->airflow_supply_m3h = 120.0f;
    ftx->airflow_exhaust_m3h = 120.0f;
    ftx->last_sensor_update_ms = esp_log_timestamp();
}

static void apply_fan_policy(float temp_c, float rh_percent, uint8_t *fan1_speed, uint8_t *fan2_speed)
{
    *fan1_speed = 0;
    *fan2_speed = 0;

    device_control_state_t ctrl;
    device_profile_get_control(&ctrl);
    const device_profile_capabilities_t *cap = device_profile_get_active_capabilities();
    device_profile_t mode = device_profile_get();

    /* No PWM unless control active and mode has fans (or AC assist fans) */
    bool ac_assist = (mode == TF_MODE_AC_MONITOR) &&
                     (ctrl.ac_modules & TF_AC_MOD_ASSIST_FANS);
    if ((!device_profile_is_control_active() || !cap->pwm_control) && !ac_assist) {
        return;
    }
    if (mode == TF_MODE_SENSOR_ONLY) {
        return;
    }
    if (mode == TF_MODE_AC_MONITOR && !ac_assist) {
        return;
    }

    if (!hardware_is_detected(HW_COMPONENT_FAN_1)) {
        return;
    }

    if (ctrl.fan_mode == TF_FAN_MODE_OFF && mode != TF_MODE_AC_MONITOR) {
        return;
    }

    bool condensation = anti_condensation_is_active();

    /* ---- Mobil AC help-fans ---- */
    if (mode == TF_MODE_AC_MONITOR && ac_assist) {
        fan_controller_exit_failsafe();
        /* Cold-side RH is often the condensation driver for portable AC ducts */
        float cold_rh = rh_percent;
        bool high_cond = (cold_rh >= 85.0f && temp_c <= 16.0f) ||
                         (cold_rh >= 70.0f && temp_c <= 18.0f && condensation);

        if (ctrl.fan_mode == TF_FAN_MODE_OFF) {
            *fan1_speed = 0;
            *fan2_speed = 0;
            return;
        }

        if (ctrl.fan_mode == TF_FAN_MODE_MANUAL) {
            *fan1_speed = ctrl.fan1_speed;
            *fan2_speed = ctrl.fan2_speed;
        } else {
            /* AUTO: scale assist with humidity / mild cooling demand */
            uint8_t base = 20;
            if (cold_rh > 60.0f) {
                base = (uint8_t)(20 + (cold_rh - 60.0f) * 1.5f);
            }
            if (base > 70) {
                base = 70;
            }
            *fan1_speed = base;
            *fan2_speed = base;
        }

        /* Condensation policy overrides (SR-010 style for AC duct risk) */
        if (high_cond) {
            switch (ctrl.ac_cond_action) {
                case TF_AC_COND_BOOST_ASSIST:
                    if (*fan1_speed < 85) {
                        *fan1_speed = 85;
                    }
                    if (*fan2_speed < 85) {
                        *fan2_speed = 85;
                    }
                    break;
                case TF_AC_COND_REQUEST_FAN_ONLY:
                    device_profile_note_ac_command("fan_only_request");
                    /* Keep assist elevated while we "request" milder AC mode */
                    if (*fan1_speed < 60) {
                        *fan1_speed = 60;
                    }
                    *fan2_speed = *fan1_speed;
                    break;
                case TF_AC_COND_OBSERVE:
                default:
                    break;
            }
        }

        if (!hardware_is_detected(HW_COMPONENT_FAN_2)) {
            *fan2_speed = 0;
        }
        return;
    }

    if (ctrl.fan_mode == TF_FAN_MODE_MANUAL) {
        *fan1_speed = ctrl.fan1_speed;
        if (cap->dual_fan_independent) {
            *fan2_speed = ctrl.fan2_speed;
        } else {
            *fan2_speed = *fan1_speed;
        }
        /* SR-010: Mini-FTX may force full flow at high RH even in manual */
        if (mode == TF_MODE_MINI_FTX && condensation) {
            *fan1_speed = 100;
            *fan2_speed = 100;
        }
        if (mode == TF_MODE_HEAT_EXCHANGER && condensation) {
            *fan1_speed = 0;
            *fan2_speed = 0;
            fan_controller_enter_failsafe("SR-010 high RH");
        } else {
            fan_controller_exit_failsafe();
        }
        return;
    }

    /* AUTO */
    switch (mode) {
        case TF_MODE_HEAT_EXCHANGER:
            if (condensation) {
                *fan1_speed = 0;
                *fan2_speed = 0;
                fan_controller_enter_failsafe("SR-010 high RH");
            } else {
                fan_controller_exit_failsafe();
                *fan1_speed = fan_controller_calc_speed_from_temp(temp_c, 20.0f, SIM_MIN_TEMP_C, SIM_MAX_TEMP_C);
                if (*fan1_speed < 10) {
                    *fan1_speed = 10;
                }
                if (*fan1_speed > 60) {
                    *fan1_speed = 60;
                }
                /* Independent second fan: same auto curve for now (balance) */
                *fan2_speed = *fan1_speed;
            }
            break;

        case TF_MODE_MINI_FTX:
        default:
            fan_controller_exit_failsafe();
            if (g_ftx_initialized) {
                uint8_t recommended = ftx_recommend_fan_speed_hysteresis(&g_ftx_data,
                                                                          fan_controller_get_speed(FAN_1));
                ftx_calculate_max_safe_speed(&g_ftx_data, recommended);
                *fan1_speed = g_ftx_data.fan_speed_current;
                if (condensation) {
                    *fan1_speed = 100;
                }
            } else if (condensation) {
                *fan1_speed = 100;
            } else {
                *fan1_speed = fan_controller_calc_speed_from_temp(temp_c, 20.0f, SIM_MIN_TEMP_C, SIM_MAX_TEMP_C);
            }
            *fan2_speed = *fan1_speed;
            /* Phase placeholder for UI until reverse/spjäll hardware exists */
            {
                uint32_t half = (ctrl.cycle_period_s > 0 ? ctrl.cycle_period_s : 60) / 2;
                if (half < 5) {
                    half = 5;
                }
                uint32_t tick = (esp_log_timestamp() / 1000) % (half * 2);
                device_profile_set_cycle_phase(tick < half ? TF_CYCLE_PHASE_EXHAUST
                                                          : TF_CYCLE_PHASE_INTAKE);
            }
            break;
    }

    if (!hardware_is_detected(HW_COMPONENT_FAN_2)) {
        *fan2_speed = 0;
    } else if (!cap->dual_fan_independent && mode != TF_MODE_HEAT_EXCHANGER) {
        *fan2_speed = *fan1_speed;
    }
}

static void publish_mqtt_if_connected(void)
{
    if (!g_mqtt_started || !mqtt_ftx_is_connected()) {
        return;
    }

    if (!ftx_check_rate_limit(g_ftx_data.last_publish_ms, esp_log_timestamp())) {
        return;
    }

    ftx_sensor_data_t sensors = {
        .outdoor_temp = g_ftx_data.outdoor_temp,
        .outdoor_rh = g_ftx_data.outdoor_rh,
        .supply_temp = g_ftx_data.supply_temp,
        .supply_rh = g_ftx_data.supply_rh,
        .exhaust_temp = g_ftx_data.exhaust_temp,
        .exhaust_rh = g_ftx_data.exhaust_rh,
        .extract_temp = g_ftx_data.extract_temp,
        .extract_rh = g_ftx_data.extract_rh,
    };

    ftx_efficiency_data_t efficiency = {
        .efficiency_percent = g_ftx_data.efficiency_percent,
        .power_recovered_w = g_ftx_data.energy_recovery_w,
        .airflow_m3h = g_ftx_data.airflow_supply_m3h,
    };

    ftx_status_flags_t status = {
        .frost_protection_active = g_ftx_data.frost_protection_active,
        .bypass_active = g_ftx_data.bypass_active,
        .high_humidity_alert = g_ftx_data.condensation_risk,
    };

    mqtt_ftx_publish_full_state(&sensors, &efficiency, &status);
    g_ftx_data.last_publish_ms = esp_log_timestamp();
}

static void try_start_mqtt(void)
{
    if (g_mqtt_started) {
        mqtt_ftx_loop();
        return;
    }

    if (!wifi_manager_is_connected()) {
        return;
    }

    if (MQTT_BROKER_DEFAULT[0] == '\0') {
        return;
    }

    esp_err_t ret = mqtt_ftx_configure_tls(MQTT_BROKER_DEFAULT, MQTT_PORT_DEFAULT, false);
    if (ret == ESP_OK) {
        ret = mqtt_ftx_connect();
    }

    if (ret == ESP_OK) {
        g_mqtt_started = true;
        mqtt_ftx_register_log_sink();
        audit_log_event(AUDIT_EVENT_MQTT_CONNECT, AUDIT_SEVERITY_INFO, "MQTT connected");
        TF_LOG_INFO(TF_LOG_CAT_MQTT, TAG, "MQTT connected — remote log sink enabled");
        mqtt_ftx_ha_discovery();
    }
}

static void control_task(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "Control task started");
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    float temp_c = 20.0f;
    float rh_percent = 50.0f;

    while (1) {
        ESP_ERROR_CHECK(esp_task_wdt_reset());

        sensor_manager_data_t sensor_data;
        esp_err_t err = sensor_manager_read_all(&sensor_data);

        if (err == ESP_OK && sensor_data.num_sensors > 0) {
            for (int i = 0; i < sensor_data.num_sensors; i++) {
                if (sensor_data.valid[i]) {
                    temp_c = sensor_data.temperature[i];
                    rh_percent = sensor_data.humidity[i];
                    break;
                }
            }
        }

        if (g_sensor_mutex && xSemaphoreTake(g_sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sensor_manager_update_data(&sensor_data);
            xSemaphoreGive(g_sensor_mutex);
        }

        anti_condensation_check(rh_percent, temp_c);

        if (g_ftx_initialized) {
            populate_ftx_from_sensors(&sensor_data, &g_ftx_data);
            g_ftx_data.efficiency_percent = ftx_calc_efficiency_hysteresis(
                g_ftx_data.outdoor_temp,
                g_ftx_data.supply_temp,
                g_ftx_data.exhaust_temp,
                g_ftx_data.efficiency_percent);
            g_ftx_data.energy_recovery_w = ftx_calc_power(
                g_ftx_data.airflow_supply_m3h,
                g_ftx_data.supply_temp - g_ftx_data.outdoor_temp);
            if (ftx_update(&g_ftx_data) == 0) {
                web_server_update_ftx_data(&g_ftx_data);
            }
        } else if (sensor_data.num_sensors > 0) {
            populate_ftx_from_sensors(&sensor_data, &g_ftx_data);
            web_server_update_ftx_data(&g_ftx_data);
        }

        uint8_t fan1 = 0;
        uint8_t fan2 = 0;
        apply_fan_policy(temp_c, rh_percent, &fan1, &fan2);

        if (hardware_is_detected(HW_COMPONENT_FAN_1) && !fan_controller_is_failsafe()) {
            fan_controller_set_mode(FAN_1, FAN_MODE_MANUAL);
            fan_controller_set_speed(FAN_1, fan1);
        }
        if (hardware_is_detected(HW_COMPONENT_FAN_2) && !fan_controller_is_failsafe()) {
            fan_controller_set_mode(FAN_2, FAN_MODE_MANUAL);
            fan_controller_set_speed(FAN_2, fan2);
        }

        if (hardware_is_detected(HW_COMPONENT_OLED_DISPLAY)) {
            display_sensor_data_t display_data = {0};
            display_data.num_sensors = sensor_data.num_sensors;
            for (int i = 0; i < sensor_data.num_sensors && i < 4; i++) {
                display_data.temp[i] = sensor_data.temperature[i];
                display_data.humidity[i] = sensor_data.humidity[i];
                display_data.valid[i] = sensor_data.valid[i];
            }
            display_update_sensors(&display_data);
        }

        wifi_manager_run();
        try_start_mqtt();
        publish_mqtt_if_connected();

        if (!g_ota_health_ok &&
            (esp_log_timestamp() - g_boot_time_ms) >= OTA_HEALTH_DELAY_MS &&
            sensor_data.num_sensors > 0) {
            ota_status_t status;
            if (ota_manager_get_status(&status) == ESP_OK && status.can_rollback) {
                if (ota_manager_mark_valid() == ESP_OK) {
                    audit_log_event(AUDIT_EVENT_OTA_COMPLETE, AUDIT_SEVERITY_INFO,
                                    "Firmware marked valid after health check");
                }
            }
            g_ota_health_ok = true;
        }

        static uint32_t last_log_time = 0;
        uint32_t now = esp_log_timestamp();
        if (now - last_log_time >= MAIN_LOOP_INTERVAL_MS) {
            TF_LOG_INFO(TF_LOG_CAT_SENSOR, TAG,
                        "Temp=%.1fC RH=%.1f%% Fan=%u%% Mode=%d Cond=%s FTX=%.1f%%",
                        temp_c, rh_percent, fan1, (int)device_profile_get(),
                        anti_condensation_is_active() ? "YES" : "no",
                        g_ftx_data.efficiency_percent);
            last_log_time = now;
        }

        vTaskDelay(pdMS_TO_TICKS(CONTROL_LOOP_INTERVAL_MS));
    }
}

static void ota_monitor_task(void *pvParameters)
{
    (void)pvParameters;

    vTaskDelay(pdMS_TO_TICKS(60000));

    while (1) {
        if (wifi_manager_is_connected() && OTA_SERVER_URL[0] != '\0') {
            esp_err_t ret = ota_manager_check_for_update();
            if (ret == ESP_OK) {
                ota_manager_start_update();
            }
        }

        ota_status_t status;
        if (ota_manager_get_status(&status) == ESP_OK && status.state == OTA_STATE_ERROR) {
            ota_manager_reset();
        }

        vTaskDelay(pdMS_TO_TICKS(OTA_MONITOR_INTERVAL_MS));
    }
}

static esp_err_t init_network_services(void)
{
    esp_err_t ret = web_server_init();
    if (ret != ESP_OK) {
        return ret;
    }

    https_config_t https_cfg;
    web_server_get_default_https_config(&https_cfg);
    https_cfg.use_https = false;
    web_server_set_https_config(&https_cfg);

    ret = web_server_start();
    if (ret == ESP_OK) {
        audit_log_event(AUDIT_EVENT_NETWORK_CONNECT, AUDIT_SEVERITY_INFO, "HTTP web server started");
    }
    return ret;
}

void app_main(void)
{
    g_boot_time_ms = esp_log_timestamp();

    ESP_LOGI(TAG, "ThermoFlow %s (%s, %s) on ESP32-S3",
             THERMOFLOW_VERSION_STRING, THERMOFLOW_VERSION_FULL, THERMOFLOW_CHANNEL);

    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(device_profile_init());

    g_sensor_mutex = xSemaphoreCreateMutex();
    if (!g_sensor_mutex) {
        ESP_LOGE(TAG, "Failed to create sensor mutex");
        return;
    }

    security_manager_init();

    audit_log_config_t audit_cfg = {
        .storage_path = NULL,
        /* 0 = auto capacity (larger ring when optional PSRAM is available) */
        .max_entries = 0,
        .wrap_around = true,
    };
    audit_log_init(&audit_cfg);
    TF_LOG_INFO(TF_LOG_CAT_SYSTEM, TAG, "Boot ThermoFlow %s (%s)",
                THERMOFLOW_VERSION_STRING, THERMOFLOW_VERSION_FULL);

    rate_limiter_init();

    hardware_manager_init();
    ESP_LOGI(TAG, "Hardware: %s", hardware_get_summary());

    sensor_manager_init();
    g_ftx_initialized = (ftx_init(FTX_CORE_EFFICIENCY_DEFAULT) == 0);

    wifi_manager_init();

    init_network_services();
    mqtt_ftx_init();

    if (hardware_is_detected(HW_COMPONENT_FAN_1)) {
        fan_controller_init();
    }

    if (hardware_is_detected(HW_COMPONENT_OLED_DISPLAY)) {
        display_manager_init();
    }

    anti_condensation_init();
    init_ota();

    xTaskCreatePinnedToCore(control_task, "control", CONTROL_TASK_STACK_SIZE,
                            NULL, CONTROL_TASK_PRIORITY, NULL, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(ota_monitor_task, "ota_monitor", OTA_MONITOR_TASK_STACK_SIZE,
                            NULL, OTA_MONITOR_TASK_PRIORITY, NULL, tskNO_AFFINITY);

    ESP_LOGI(TAG, "System initialization complete");
}