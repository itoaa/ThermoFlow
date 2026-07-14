/**
 * @file audit_log.c
 * @brief Audit logging facade over unified log_manager (IEC 62443 SR-005)
 */

#include "audit_log.h"
#include "log_manager.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "AUDIT_LOG";

static tf_log_level_t severity_to_level(audit_severity_t severity)
{
    switch (severity) {
        case AUDIT_SEVERITY_DEBUG:    return TF_LOG_LEVEL_DEBUG;
        case AUDIT_SEVERITY_INFO:     return TF_LOG_LEVEL_INFO;
        case AUDIT_SEVERITY_WARNING:  return TF_LOG_LEVEL_WARN;
        case AUDIT_SEVERITY_ERROR:    return TF_LOG_LEVEL_ERROR;
        case AUDIT_SEVERITY_CRITICAL: return TF_LOG_LEVEL_FATAL;
        default:                      return TF_LOG_LEVEL_INFO;
    }
}

static void tf_to_audit_entry(const tf_log_entry_t *src, audit_log_entry_t *dst)
{
    memset(dst, 0, sizeof(*dst));
    dst->magic = AUDIT_LOG_MAGIC;
    dst->sequence = src->sequence;
    dst->timestamp_us = src->timestamp_us;
    dst->boot_id = src->boot_id;
    dst->correlation_id = src->correlation_id;
    dst->category = (uint8_t)src->category;
    strncpy(dst->component, src->component, sizeof(dst->component) - 1);

    if (src->audit_event >= 0 && src->audit_event < AUDIT_EVENT_MAX) {
        dst->event_type = (audit_event_type_t)src->audit_event;
        dst->severity = (audit_severity_t)src->audit_severity;
    } else {
        dst->event_type = AUDIT_EVENT_GENERAL;
        switch (src->level) {
            case TF_LOG_LEVEL_TRACE:
            case TF_LOG_LEVEL_DEBUG:
                dst->severity = AUDIT_SEVERITY_DEBUG;
                break;
            case TF_LOG_LEVEL_WARN:
                dst->severity = AUDIT_SEVERITY_WARNING;
                break;
            case TF_LOG_LEVEL_ERROR:
                dst->severity = AUDIT_SEVERITY_ERROR;
                break;
            case TF_LOG_LEVEL_FATAL:
                dst->severity = AUDIT_SEVERITY_CRITICAL;
                break;
            default:
                dst->severity = AUDIT_SEVERITY_INFO;
                break;
        }
    }

    strncpy(dst->message, src->message, sizeof(dst->message) - 1);
    dst->checksum = 0;
}

esp_err_t audit_log_init(const audit_log_config_t *config)
{
    tf_log_config_t lm_cfg = {
        .sinks = TF_LOG_SINK_SERIAL | TF_LOG_SINK_WEB | TF_LOG_SINK_NVS,
        .min_level = TF_LOG_LEVEL_INFO,
        .min_serial_level = TF_LOG_LEVEL_INFO,
        .serial_json = false,
        .nvs_persist = true,
        .capacity = TF_LOG_DEFAULT_CAPACITY,
    };

    if (config && config->max_entries > 0) {
        if (config->max_entries > TF_LOG_DEFAULT_CAPACITY) {
            lm_cfg.capacity = (uint16_t)config->max_entries;
        } else {
            lm_cfg.capacity = (uint16_t)config->max_entries;
        }
    }

    strncpy(lm_cfg.mqtt_topic, TF_LOG_MQTT_TOPIC, sizeof(lm_cfg.mqtt_topic) - 1);

    esp_err_t ret = log_manager_init(&lm_cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "Audit log facade ready (backed by log_manager)");
    return audit_log_event(AUDIT_EVENT_SYSTEM_BOOT, AUDIT_SEVERITY_INFO, "System booted");
}

esp_err_t audit_log_event(audit_event_type_t event_type, audit_severity_t severity,
                          const char *message, ...)
{
    if (event_type >= AUDIT_EVENT_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    char buf[AUDIT_LOG_MAX_MESSAGE_LEN];
    va_list args;
    va_start(args, message);
    vsnprintf(buf, sizeof(buf), message, args);
    va_end(args);

    return log_manager_write_audit((int16_t)event_type,
                                   (int8_t)severity,
                                   severity_to_level(severity),
                                   TF_LOG_CAT_AUDIT,
                                   TAG,
                                   "%s",
                                   buf);
}

esp_err_t audit_log_auth(const char *username, bool success, const char *source)
{
    audit_event_type_t type = success ? AUDIT_EVENT_AUTH_SUCCESS : AUDIT_EVENT_AUTH_FAILURE;
    audit_severity_t severity = success ? AUDIT_SEVERITY_INFO : AUDIT_SEVERITY_WARNING;

    return audit_log_event(type, severity,
                           "User '%s' %s from %s",
                           username ? username : "unknown",
                           success ? "authenticated" : "failed authentication",
                           source ? source : "unknown");
}

esp_err_t audit_log_config_change(const char *setting, const char *old_value,
                                  const char *new_value, const char *user)
{
    return audit_log_event(AUDIT_EVENT_CONFIG_CHANGE, AUDIT_SEVERITY_INFO,
                           "Setting '%s' changed from '%s' to '%s' by '%s'",
                           setting ? setting : "?",
                           old_value ? old_value : "?",
                           new_value ? new_value : "?",
                           user ? user : "unknown");
}

esp_err_t audit_log_clear(void)
{
    return log_manager_clear();
}

esp_err_t audit_log_get_stats(audit_log_stats_t *stats)
{
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }

    tf_log_stats_t lm_stats;
    esp_err_t ret = log_manager_get_stats(&lm_stats);
    if (ret != ESP_OK) {
        return ret;
    }

    stats->total_entries = lm_stats.total_entries;
    stats->entries_in_storage = lm_stats.entries_in_buffer;
    stats->wrap_count = lm_stats.wrap_count;
    stats->first_entry_time = lm_stats.first_entry_time_us;
    stats->last_entry_time = lm_stats.last_entry_time_us;
    return ESP_OK;
}

esp_err_t audit_log_get_entry(uint32_t index, audit_log_entry_t *entry)
{
    tf_log_entry_t src;
    esp_err_t ret = log_manager_get_entry(index, &src);
    if (ret != ESP_OK) {
        return ret;
    }
    tf_to_audit_entry(&src, entry);
    return ESP_OK;
}

esp_err_t audit_log_get_recent(audit_log_entry_t *entries, uint32_t max_entries,
                               uint32_t *num_retrieved)
{
    if (!entries || !num_retrieved || max_entries == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    tf_log_entry_t *buf = calloc(max_entries, sizeof(tf_log_entry_t));
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }

    uint32_t count = 0;
    esp_err_t ret = log_manager_get_recent(buf, max_entries, &count);
    if (ret == ESP_OK) {
        for (uint32_t i = 0; i < count; i++) {
            tf_to_audit_entry(&buf[i], &entries[i]);
        }
        *num_retrieved = count;
    }

    free(buf);
    return ret;
}

esp_err_t audit_log_export_json(char *buffer, size_t buffer_len, size_t *exported_len)
{
    return log_manager_export_json(buffer, buffer_len, exported_len);
}

esp_err_t audit_log_verify(uint32_t *corrupted_entries)
{
    return log_manager_verify(corrupted_entries);
}

esp_err_t audit_log_set_filter(audit_event_type_t event_type, bool filter)
{
    return log_manager_set_audit_filter((int16_t)event_type, filter);
}

esp_err_t audit_log_deinit(void)
{
    audit_log_event(AUDIT_EVENT_SYSTEM_SHUTDOWN, AUDIT_SEVERITY_INFO, "System shutting down");
    return log_manager_deinit();
}

const char *audit_log_event_type_str(audit_event_type_t type)
{
    switch (type) {
        case AUDIT_EVENT_AUTH_SUCCESS: return "AUTH_SUCCESS";
        case AUDIT_EVENT_AUTH_FAILURE: return "AUTH_FAILURE";
        case AUDIT_EVENT_CONFIG_CHANGE: return "CONFIG_CHANGE";
        case AUDIT_EVENT_FAN_STATE_CHANGE: return "FAN_CHANGE";
        case AUDIT_EVENT_SENSOR_FAILURE: return "SENSOR_FAIL";
        case AUDIT_EVENT_NETWORK_CONNECT: return "NET_CONNECT";
        case AUDIT_EVENT_NETWORK_DISCONNECT: return "NET_DISCONNECT";
        case AUDIT_EVENT_MQTT_CONNECT: return "MQTT_CONNECT";
        case AUDIT_EVENT_MQTT_DISCONNECT: return "MQTT_DISCONNECT";
        case AUDIT_EVENT_OTA_START: return "OTA_START";
        case AUDIT_EVENT_OTA_COMPLETE: return "OTA_COMPLETE";
        case AUDIT_EVENT_OTA_FAILURE: return "OTA_FAIL";
        case AUDIT_EVENT_SECURITY_ALERT: return "SECURITY_ALERT";
        case AUDIT_EVENT_RATE_LIMIT_HIT: return "RATE_LIMIT";
        case AUDIT_EVENT_SYSTEM_BOOT: return "BOOT";
        case AUDIT_EVENT_SYSTEM_SHUTDOWN: return "SHUTDOWN";
        case AUDIT_EVENT_GENERAL: return "LOG";
        default: return "UNKNOWN";
    }
}

const char *audit_log_severity_str(audit_severity_t severity)
{
    switch (severity) {
        case AUDIT_SEVERITY_DEBUG: return "DEBUG";
        case AUDIT_SEVERITY_INFO: return "INFO";
        case AUDIT_SEVERITY_WARNING: return "WARN";
        case AUDIT_SEVERITY_ERROR: return "ERROR";
        case AUDIT_SEVERITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}