/**
 * @file log_manager.h
 * @brief Unified structured logging for ThermoFlow (2026 architecture)
 *
 * Multi-sink logging hub: serial (UART), web ring buffer, NVS persistence,
 * MQTT remote (optional), SD card (stub).
 *
 * IEC 62443 SR-005 audit events map onto this layer via audit_log wrappers.
 */

#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TF_LOG_MAGIC            0x54464C47u  /* "TFLG" */
#define TF_LOG_MSG_LEN          256
#define TF_LOG_COMP_LEN         16
#define TF_LOG_DEFAULT_CAPACITY 100
#define TF_LOG_NVS_CAPACITY     32
#define TF_LOG_MQTT_TOPIC       "thermoflow/logs"

typedef enum {
    TF_LOG_LEVEL_TRACE = 0,
    TF_LOG_LEVEL_DEBUG,
    TF_LOG_LEVEL_INFO,
    TF_LOG_LEVEL_WARN,
    TF_LOG_LEVEL_ERROR,
    TF_LOG_LEVEL_FATAL,
    TF_LOG_LEVEL_MAX
} tf_log_level_t;

typedef enum {
    TF_LOG_CAT_SYSTEM = 0,
    TF_LOG_CAT_SECURITY,
    TF_LOG_CAT_NETWORK,
    TF_LOG_CAT_SENSOR,
    TF_LOG_CAT_FAN,
    TF_LOG_CAT_MQTT,
    TF_LOG_CAT_WEB,
    TF_LOG_CAT_OTA,
    TF_LOG_CAT_AUDIT,
    TF_LOG_CAT_MAX
} tf_log_category_t;

typedef enum {
    TF_LOG_SINK_SERIAL = (1 << 0),
    TF_LOG_SINK_WEB    = (1 << 1),
    TF_LOG_SINK_NVS    = (1 << 2),
    TF_LOG_SINK_MQTT   = (1 << 3),
    TF_LOG_SINK_SD     = (1 << 4),
} tf_log_sink_mask_t;

/** No audit event (-1 stored as int16) */
#define TF_LOG_NO_AUDIT_EVENT   (-1)

typedef struct {
    uint32_t magic;
    uint32_t sequence;
    uint64_t timestamp_us;
    uint32_t boot_id;
    uint32_t correlation_id;
    tf_log_level_t level;
    tf_log_category_t category;
    int16_t audit_event;
    int8_t audit_severity;
    uint8_t reserved;
    char component[TF_LOG_COMP_LEN];
    char message[TF_LOG_MSG_LEN];
    uint32_t checksum;
} tf_log_entry_t;

typedef struct {
    tf_log_sink_mask_t sinks;
    tf_log_level_t min_level;
    tf_log_level_t min_serial_level;
    bool serial_json;
    bool nvs_persist;
    uint16_t capacity;
    char mqtt_topic[64];
} tf_log_config_t;

typedef struct {
    uint32_t total_entries;
    uint32_t entries_in_buffer;
    uint32_t wrap_count;
    uint32_t boot_id;
    uint32_t correlation_seq;
    uint64_t first_entry_time_us;
    uint64_t last_entry_time_us;
    tf_log_sink_mask_t active_sinks;
} tf_log_stats_t;

typedef esp_err_t (*tf_log_mqtt_publish_fn)(const char *topic,
                                            const char *payload,
                                            size_t len);

esp_err_t log_manager_init(const tf_log_config_t *config);
esp_err_t log_manager_deinit(void);

esp_err_t log_manager_write(tf_log_level_t level,
                            tf_log_category_t category,
                            const char *component,
                            const char *message_fmt, ...);

esp_err_t log_manager_write_audit(int16_t audit_event,
                                  int8_t audit_severity,
                                  tf_log_level_t level,
                                  tf_log_category_t category,
                                  const char *component,
                                  const char *message_fmt, ...);

esp_err_t log_manager_get_recent(tf_log_entry_t *entries,
                                 uint32_t max_entries,
                                 uint32_t *num_retrieved);

esp_err_t log_manager_get_entry(uint32_t index, tf_log_entry_t *entry);
esp_err_t log_manager_clear(void);
esp_err_t log_manager_get_stats(tf_log_stats_t *stats);

esp_err_t log_manager_get_config(tf_log_config_t *config);
esp_err_t log_manager_set_config(const tf_log_config_t *config);

esp_err_t log_manager_set_sink_mask(tf_log_sink_mask_t mask);
esp_err_t log_manager_set_min_level(tf_log_level_t level);
esp_err_t log_manager_set_mqtt_publish(tf_log_mqtt_publish_fn fn);

esp_err_t log_manager_export_json(char *buffer, size_t buffer_len, size_t *exported_len);
esp_err_t log_manager_export_ndjson(char *buffer, size_t buffer_len, size_t *exported_len);

esp_err_t log_manager_verify(uint32_t *corrupted_entries);
esp_err_t log_manager_set_audit_filter(int16_t audit_event, bool filter_out);

uint32_t log_manager_new_correlation_id(void);

const char *tf_log_level_str(tf_log_level_t level);
const char *tf_log_category_str(tf_log_category_t category);

#define TF_LOG_TRACE(cat, tag, fmt, ...) \
    log_manager_write(TF_LOG_LEVEL_TRACE, cat, tag, fmt, ##__VA_ARGS__)
#define TF_LOG_DEBUG(cat, tag, fmt, ...) \
    log_manager_write(TF_LOG_LEVEL_DEBUG, cat, tag, fmt, ##__VA_ARGS__)
#define TF_LOG_INFO(cat, tag, fmt, ...) \
    log_manager_write(TF_LOG_LEVEL_INFO, cat, tag, fmt, ##__VA_ARGS__)
#define TF_LOG_WARN(cat, tag, fmt, ...) \
    log_manager_write(TF_LOG_LEVEL_WARN, cat, tag, fmt, ##__VA_ARGS__)
#define TF_LOG_ERROR(cat, tag, fmt, ...) \
    log_manager_write(TF_LOG_LEVEL_ERROR, cat, tag, fmt, ##__VA_ARGS__)
#define TF_LOG_FATAL(cat, tag, fmt, ...) \
    log_manager_write(TF_LOG_LEVEL_FATAL, cat, tag, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* LOG_MANAGER_H */