/**
 * @file mqtt_ftx.c
 * @brief MQTT Client Extension for Mini-FTX - Implementation
 */

#include "mqtt_ftx.h"
#include "mqtt_client.h"
#include <string.h>
#include <stdio.h>
#include <cJSON.h>

#include "esp_log.h"
static const char *TAG = "MQTT_FTX";

esp_err_t mqtt_ftx_init(void) {
    ESP_LOGI(TAG, "FTX MQTT module initialized");
    return ESP_OK;
}

esp_err_t mqtt_ftx_publish_sensors(const ftx_sensor_data_t *data) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    // Outdoor (inlet before HX)
    cJSON_AddNumberToObject(root, "outdoor_temp", data->outdoor_temp);
    cJSON_AddNumberToObject(root, "outdoor_rh", data->outdoor_rh);
    
    // Supply (outlet after HX - heated fresh air)
    cJSON_AddNumberToObject(root, "supply_temp", data->supply_temp);
    cJSON_AddNumberToObject(root, "supply_rh", data->supply_rh);
    
    // Exhaust (from room - warm stale air)
    cJSON_AddNumberToObject(root, "exhaust_temp", data->exhaust_temp);
    cJSON_AddNumberToObject(root, "exhaust_rh", data->exhaust_rh);
    
    // Extract (before HX)
    cJSON_AddNumberToObject(root, "extract_temp", data->extract_temp);
    cJSON_AddNumberToObject(root, "extract_rh", data->extract_rh);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!payload) return ESP_ERR_NO_MEM;

    esp_err_t ret = mqtt_client_publish(FTX_TOPIC_SENSORS, 
                                        (uint8_t*)payload, strlen(payload), 
                                        MQTT_QOS_1, false);
    free(payload);
    
    return ret;
}

esp_err_t mqtt_ftx_publish_efficiency(const ftx_efficiency_data_t *data) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    cJSON_AddNumberToObject(root, "efficiency_percent", data->efficiency_percent);
    cJSON_AddNumberToObject(root, "power_recovered_w", data->power_recovered_w);
    cJSON_AddNumberToObject(root, "airflow_m3h", data->airflow_m3h);
    cJSON_AddNumberToObject(root, "temp_diff_in_out", data->temp_diff_in_out);
    cJSON_AddNumberToObject(root, "temp_diff_exhaust_supply", data->temp_diff_exhaust_supply);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!payload) return ESP_ERR_NO_MEM;

    esp_err_t ret = mqtt_client_publish(FTX_TOPIC_EFFICIENCY, 
                                        (uint8_t*)payload, strlen(payload), 
                                        MQTT_QOS_1, false);
    free(payload);
    
    return ret;
}

esp_err_t mqtt_ftx_publish_energy_stats(const ftx_energy_stats_t *stats) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    cJSON_AddNumberToObject(root, "energy_kwh_day", stats->energy_kwh_day);
    cJSON_AddNumberToObject(root, "cost_saving_sek", stats->cost_saving_sek);
    cJSON_AddNumberToObject(root, "avg_efficiency", stats->avg_efficiency);
    cJSON_AddNumberToObject(root, "runtime_hours", stats->runtime_hours);
    cJSON_AddNumberToObject(root, "filter_hours_remaining", stats->filter_hours_remaining);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!payload) return ESP_ERR_NO_MEM;

    esp_err_t ret = mqtt_client_publish(FTX_TOPIC_STATS, 
                                        (uint8_t*)payload, strlen(payload), 
                                        MQTT_QOS_1, true); // Retain last stats
    free(payload);
    
    return ret;
}

esp_err_t mqtt_ftx_publish_status(const ftx_status_flags_t *status) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    cJSON_AddBoolToObject(root, "frost_risk", status->frost_risk);
    cJSON_AddBoolToObject(root, "frost_protection_active", status->frost_protection_active);
    cJSON_AddBoolToObject(root, "bypass_active", status->bypass_active);
    cJSON_AddBoolToObject(root, "filter_warning", status->filter_warning);
    cJSON_AddBoolToObject(root, "filter_critical", status->filter_critical);
    cJSON_AddBoolToObject(root, "high_humidity_alert", status->high_humidity_alert);
    cJSON_AddBoolToObject(root, "low_efficiency_alert", status->low_efficiency_alert);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!payload) return ESP_ERR_NO_MEM;

    esp_err_t ret = mqtt_client_publish(FTX_TOPIC_STATUS, 
                                        (uint8_t*)payload, strlen(payload), 
                                        MQTT_QOS_1, false);
    free(payload);
    
    return ret;
}

esp_err_t mqtt_ftx_publish_full_state(
    const ftx_sensor_data_t *sensors,
    const ftx_efficiency_data_t *efficiency,
    const ftx_status_flags_t *status
) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    // Add timestamp
    cJSON_AddStringToObject(root, "timestamp", "2026-04-03T16:00:00Z");

    // Sensors object
    cJSON *sensors_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(sensors_obj, "outdoor_temp", sensors->outdoor_temp);
    cJSON_AddNumberToObject(sensors_obj, "outdoor_rh", sensors->outdoor_rh);
    cJSON_AddNumberToObject(sensors_obj, "supply_temp", sensors->supply_temp);
    cJSON_AddNumberToObject(sensors_obj, "supply_rh", sensors->supply_rh);
    cJSON_AddNumberToObject(sensors_obj, "exhaust_temp", sensors->exhaust_temp);
    cJSON_AddNumberToObject(sensors_obj, "exhaust_rh", sensors->exhaust_rh);
    cJSON_AddItemToObject(root, "sensors", sensors_obj);

    // Efficiency object
    cJSON *eff_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(eff_obj, "percent", efficiency->efficiency_percent);
    cJSON_AddNumberToObject(eff_obj, "power_w", efficiency->power_recovered_w);
    cJSON_AddNumberToObject(eff_obj, "airflow_m3h", efficiency->airflow_m3h);
    cJSON_AddItemToObject(root, "efficiency", eff_obj);

    // Status object
    cJSON *status_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(status_obj, "frost_risk", status->frost_risk);
    cJSON_AddBoolToObject(status_obj, "bypass", status->bypass_active);
    cJSON_AddBoolToObject(status_obj, "filter_warning", status->filter_warning);
    cJSON_AddItemToObject(root, "status", status_obj);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!payload) return ESP_ERR_NO_MEM;

    esp_err_t ret = mqtt_client_publish(FTX_TOPIC_PREFIX "/state", 
                                        (uint8_t*)payload, strlen(payload), 
                                        MQTT_QOS_1, false);
    free(payload);
    
    return ret;
}

esp_err_t mqtt_ftx_send_alert(const char *alert_type, const char *message) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;

    cJSON_AddStringToObject(root, "type", alert_type);
    cJSON_AddStringToObject(root, "message", message);
    cJSON_AddNumberToObject(root, "timestamp", 1712150400); // Unix timestamp

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!payload) return ESP_ERR_NO_MEM;

    esp_err_t ret = mqtt_client_publish(FTX_TOPIC_ALERTS, 
                                        (uint8_t*)payload, strlen(payload), 
                                        MQTT_QOS_2, false); // QoS 2 for alerts
    free(payload);
    
    return ret;
}

esp_err_t mqtt_ftx_ha_discovery(void) {
    // Home Assistant MQTT Discovery for sensors
    const char *discovery_template = "{"
        "\"name\":\"FTX Supply Temp\","
        "\"uniq_id\":\"thermoflow_ftx_supply_temp\","
        "\"state_topic\":\"thermoflow/ftx/sensors\","
        "\"unit_of_meas\":\"°C\","
        "\"value_template\":\"{{ value_json.supply_temp }}\","
        "\"dev_cla\":\"temperature\","
        "\"stat_cla\":\"measurement\"""};

    mqtt_client_publish("homeassistant/sensor/thermoflow_ftx_supply_temp/config",
                         (uint8_t*)discovery_template, strlen(discovery_template),
                         MQTT_QOS_1, true); // Retain

    // More sensors can be added here...
    
    ESP_LOGI(TAG, "Home Assistant discovery published");
    return ESP_OK;
}

esp_err_t mqtt_ftx_process_command(const char *topic, const char *payload, ftx_command_t *cmd, int *value) {
    *cmd = FTX_CMD_NONE;
    *value = 0;

    if (strstr(topic, "/control/fan_speed")) {
        *cmd = FTX_CMD_FAN_SPEED_SET;
        *value = atoi(payload);
    } else if (strstr(topic, "/control/mode")) {
        if (strcmp(payload, "auto") == 0) *cmd = FTX_CMD_MODE_AUTO;
        else if (strcmp(payload, "manual") == 0) *cmd = FTX_CMD_MODE_MANUAL;
        else if (strcmp(payload, "summer") == 0) *cmd = FTX_CMD_MODE_SUMMER;
        else if (strcmp(payload, "winter") == 0) *cmd = FTX_CMD_MODE_WINTER;
    } else if (strstr(topic, "/control/reset_filter")) {
        *cmd = FTX_CMD_RESET_FILTER;
    } else if (strstr(topic, "/control/emergency_stop")) {
        *cmd = FTX_CMD_EMERGENCY_STOP;
    }

    return ESP_OK;
}

esp_err_t mqtt_ftx_deinit(void) {
    ESP_LOGI(TAG, "FTX MQTT module deinitialized");
    return ESP_OK;
}
