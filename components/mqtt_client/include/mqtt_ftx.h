/**
 * @file mqtt_ftx.h
 * @brief MQTT Client Extension for Mini-FTX
 */

#ifndef MQTT_FTX_H
#define MQTT_FTX_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* FTX MQTT Topic Prefix */
#define FTX_TOPIC_PREFIX        "thermoflow/ftx"
#define FTX_TOPIC_STATUS        FTX_TOPIC_PREFIX "/status"
#define FTX_TOPIC_SENSORS       FTX_TOPIC_PREFIX "/sensors"
#define FTX_TOPIC_EFFICIENCY    FTX_TOPIC_PREFIX "/efficiency"
#define FTX_TOPIC_ENERGY        FTX_TOPIC_PREFIX "/energy"
#define FTX_TOPIC_CONTROL       FTX_TOPIC_PREFIX "/control"
#define FTX_TOPIC_STATS         FTX_TOPIC_PREFIX "/stats/daily"
#define FTX_TOPIC_ALERTS        FTX_TOPIC_PREFIX "/alerts"
#define FTX_TOPIC_CONFIG        FTX_TOPIC_PREFIX "/config"

/* Home Assistant Discovery Topics */
#define FTX_HA_DISCOVERY_PREFIX "homeassistant/sensor/thermoflow_ftx"

/**
 * @brief FTX sensor data structure for MQTT
 */
typedef struct {
    float outdoor_temp;
    float outdoor_rh;
    float supply_temp;
    float supply_rh;
    float exhaust_temp;
    float exhaust_rh;
    float extract_temp;
    float extract_rh;
} ftx_sensor_data_t;

/**
 * @brief FTX efficiency data structure
 */
typedef struct {
    float efficiency_percent;
    float power_recovered_w;
    float airflow_m3h;
    float temp_diff_in_out;
    float temp_diff_exhaust_supply;
} ftx_efficiency_data_t;

/**
 * @brief FTX energy statistics
 */
typedef struct {
    float energy_kwh_day;
    float cost_saving_sek;
    float avg_efficiency;
    uint32_t runtime_hours;
    uint32_t filter_hours_remaining;
} ftx_energy_stats_t;

/**
 * @brief FTX status flags
 */
typedef struct {
    bool frost_risk;
    bool frost_protection_active;
    bool bypass_active;
    bool filter_warning;
    bool filter_critical;
    bool high_humidity_alert;
    bool low_efficiency_alert;
} ftx_status_flags_t;

/**
 * @brief FTX control commands (from Home Assistant)
 */
typedef enum {
    FTX_CMD_NONE = 0,
    FTX_CMD_FAN_SPEED_SET,      // Set fan speed (0-100%)
    FTX_CMD_MODE_AUTO,          // Auto mode
    FTX_CMD_MODE_MANUAL,        // Manual mode
    FTX_CMD_MODE_SUMMER,        // Summer mode (bypass)
    FTX_CMD_MODE_WINTER,        // Winter mode
    FTX_CMD_RESET_FILTER,       // Reset filter timer
    FTX_CMD_EMERGENCY_STOP      // Emergency stop
} ftx_command_t;

/* Initialize FTX MQTT module */
esp_err_t mqtt_ftx_init(void);

/* Publish FTX sensor data */
esp_err_t mqtt_ftx_publish_sensors(const ftx_sensor_data_t *data);

/* Publish FTX efficiency and calculations */
esp_err_t mqtt_ftx_publish_efficiency(const ftx_efficiency_data_t *data);

/* Publish energy statistics */
esp_err_t mqtt_ftx_publish_energy_stats(const ftx_energy_stats_t *stats);

/* Publish status flags */
esp_err_t mqtt_ftx_publish_status(const ftx_status_flags_t *status);

/* Publish complete FTX state (all in one JSON) */
esp_err_t mqtt_ftx_publish_full_state(
    const ftx_sensor_data_t *sensors,
    const ftx_efficiency_data_t *efficiency,
    const ftx_status_flags_t *status
);

/* Send alert message */
esp_err_t mqtt_ftx_send_alert(const char *alert_type, const char *message);

/* Home Assistant Auto Discovery */
esp_err_t mqtt_ftx_ha_discovery(void);

/* Process incoming commands */
esp_err_t mqtt_ftx_process_command(const char *topic, const char *payload, ftx_command_t *cmd, int *value);

/* Deinitialize FTX MQTT module */
esp_err_t mqtt_ftx_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
