/**
 * @file log_manager.c
 * @brief Unified multi-sink structured logging implementation
 */

#include "log_manager.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "LOG_MGR";
static const char *NVS_NAMESPACE = "tf_log";
static const char *NVS_KEY_META = "meta";
static const char *NVS_KEY_ENTRY_PREFIX = "e";

typedef struct {
    uint32_t boot_id;
    uint32_t write_index;
    uint32_t count;
} tf_log_nvs_meta_t;

typedef struct {
    bool initialized;
    tf_log_config_t config;
    tf_log_stats_t stats;
    tf_log_entry_t *entries;
    uint32_t write_index;
    uint32_t count;
    uint32_t sequence;
    uint32_t correlation_seq;
    uint32_t audit_filter_mask;
    tf_log_mqtt_publish_fn mqtt_publish;
    SemaphoreHandle_t mutex;
} log_state_t;

static log_state_t s_log = {0};

static uint32_t checksum_entry(const tf_log_entry_t *entry)
{
    const uint8_t *data = (const uint8_t *)entry;
    uint32_t sum = 0;
    size_t offset = offsetof(tf_log_entry_t, checksum);

    for (size_t i = 0; i < offset; i++) {
        sum += data[i];
    }
    for (size_t i = offset + sizeof(entry->checksum); i < sizeof(tf_log_entry_t); i++) {
        sum += data[i];
    }
    return sum;
}

static tf_log_level_t audit_severity_to_level(int8_t severity)
{
    switch (severity) {
        case 0: return TF_LOG_LEVEL_DEBUG;
        case 1: return TF_LOG_LEVEL_INFO;
        case 2: return TF_LOG_LEVEL_WARN;
        case 3: return TF_LOG_LEVEL_ERROR;
        case 4: return TF_LOG_LEVEL_FATAL;
        default: return TF_LOG_LEVEL_INFO;
    }
}

const char *tf_log_level_str(tf_log_level_t level)
{
    switch (level) {
        case TF_LOG_LEVEL_TRACE: return "TRACE";
        case TF_LOG_LEVEL_DEBUG: return "DEBUG";
        case TF_LOG_LEVEL_INFO:  return "INFO";
        case TF_LOG_LEVEL_WARN:  return "WARN";
        case TF_LOG_LEVEL_ERROR: return "ERROR";
        case TF_LOG_LEVEL_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

const char *tf_log_category_str(tf_log_category_t category)
{
    switch (category) {
        case TF_LOG_CAT_SYSTEM:   return "system";
        case TF_LOG_CAT_SECURITY: return "security";
        case TF_LOG_CAT_NETWORK:  return "network";
        case TF_LOG_CAT_SENSOR:   return "sensor";
        case TF_LOG_CAT_FAN:      return "fan";
        case TF_LOG_CAT_MQTT:     return "mqtt";
        case TF_LOG_CAT_WEB:      return "web";
        case TF_LOG_CAT_OTA:      return "ota";
        case TF_LOG_CAT_AUDIT:    return "audit";
        default: return "unknown";
    }
}

static void sink_serial(const tf_log_entry_t *entry)
{
    if (!(s_log.config.sinks & TF_LOG_SINK_SERIAL)) {
        return;
    }
    if (entry->level < s_log.config.min_serial_level) {
        return;
    }

    if (s_log.config.serial_json) {
        ESP_LOGI("TF_LOG",
                 "{\"boot\":%lu,\"seq\":%lu,\"cid\":%lu,\"ts\":%llu,"
                 "\"lvl\":\"%s\",\"cat\":\"%s\",\"cmp\":\"%s\",\"msg\":\"%s\"}",
                 (unsigned long)entry->boot_id,
                 (unsigned long)entry->sequence,
                 (unsigned long)entry->correlation_id,
                 (unsigned long long)entry->timestamp_us,
                 tf_log_level_str(entry->level),
                 tf_log_category_str(entry->category),
                 entry->component,
                 entry->message);
        return;
    }

    esp_log_level_t esp_level = ESP_LOG_INFO;
    if (entry->level >= TF_LOG_LEVEL_ERROR) {
        esp_level = ESP_LOG_ERROR;
    } else if (entry->level >= TF_LOG_LEVEL_WARN) {
        esp_level = ESP_LOG_WARN;
    } else if (entry->level <= TF_LOG_LEVEL_DEBUG) {
        esp_level = ESP_LOG_DEBUG;
    }

    esp_log_write(esp_level, entry->component[0] ? entry->component : "TF_LOG",
                  "[%s][%s] %s",
                  tf_log_category_str(entry->category),
                  tf_log_level_str(entry->level),
                  entry->message);
}

static void format_entry_ndjson(const tf_log_entry_t *entry, char *buf, size_t len)
{
    snprintf(buf, len,
             "{\"boot\":%lu,\"seq\":%lu,\"cid\":%lu,\"ts\":%llu,"
             "\"lvl\":\"%s\",\"cat\":\"%s\",\"cmp\":\"%s\",\"msg\":\"%s\"}\n",
             (unsigned long)entry->boot_id,
             (unsigned long)entry->sequence,
             (unsigned long)entry->correlation_id,
             (unsigned long long)entry->timestamp_us,
             tf_log_level_str(entry->level),
             tf_log_category_str(entry->category),
             entry->component,
             entry->message);
}

static void sink_mqtt(const tf_log_entry_t *entry)
{
    if (!(s_log.config.sinks & TF_LOG_SINK_MQTT) || !s_log.mqtt_publish) {
        return;
    }
    if (entry->level < TF_LOG_LEVEL_INFO) {
        return;
    }

    char payload[512];
    format_entry_ndjson(entry, payload, sizeof(payload));
    s_log.mqtt_publish(s_log.config.mqtt_topic, payload, strlen(payload));
}

static esp_err_t nvs_save_entry(uint32_t slot, const tf_log_entry_t *entry)
{
    char key[8];
    snprintf(key, sizeof(key), "%s%lu", NVS_KEY_ENTRY_PREFIX, (unsigned long)slot);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(handle, key, entry, sizeof(tf_log_entry_t));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t nvs_save_meta(void)
{
    tf_log_nvs_meta_t meta = {
        .boot_id = s_log.stats.boot_id,
        .write_index = s_log.write_index,
        .count = s_log.count,
    };

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(handle, NVS_KEY_META, &meta, sizeof(meta));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static void sink_nvs(const tf_log_entry_t *entry)
{
    if (!(s_log.config.sinks & TF_LOG_SINK_NVS) || !s_log.config.nvs_persist) {
        return;
    }

    uint32_t slot = entry->sequence % TF_LOG_NVS_CAPACITY;
    if (nvs_save_entry(slot, entry) != ESP_OK) {
        ESP_LOGW(TAG, "NVS log persist failed for seq %lu", (unsigned long)entry->sequence);
    } else {
        nvs_save_meta();
    }
}

static void sink_sd_stub(const tf_log_entry_t *entry)
{
    (void)entry;
    /* SD sink reserved for future LOG_FILE_PATH integration */
}

static void append_entry(const tf_log_entry_t *entry)
{
    if (!s_log.entries || s_log.config.capacity == 0) {
        return;
    }

    memcpy(&s_log.entries[s_log.write_index], entry, sizeof(tf_log_entry_t));
    s_log.write_index = (s_log.write_index + 1) % s_log.config.capacity;

    if (s_log.count < s_log.config.capacity) {
        s_log.count++;
    } else {
        s_log.stats.wrap_count++;
    }

    s_log.stats.entries_in_buffer = s_log.count;
}

static esp_err_t commit_entry(tf_log_entry_t *entry)
{
    entry->checksum = checksum_entry(entry);
    append_entry(entry);

    s_log.stats.total_entries++;
    s_log.stats.last_entry_time_us = entry->timestamp_us;
    if (s_log.stats.first_entry_time_us == 0) {
        s_log.stats.first_entry_time_us = entry->timestamp_us;
    }

    if (s_log.config.sinks & TF_LOG_SINK_WEB) {
        /* web buffer is the in-memory ring */
    }
    sink_serial(entry);
    sink_nvs(entry);
    sink_mqtt(entry);
    sink_sd_stub(entry);

    return ESP_OK;
}

static bool audit_filtered(int16_t audit_event)
{
    if (audit_event < 0 || audit_event >= 32) {
        return false;
    }
    return (s_log.audit_filter_mask & (1u << audit_event)) != 0;
}

static esp_err_t write_entry_va(tf_log_level_t level,
                                tf_log_category_t category,
                                const char *component,
                                int16_t audit_event,
                                int8_t audit_severity,
                                const char *fmt,
                                va_list args)
{
    if (!s_log.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (level < s_log.config.min_level) {
        return ESP_OK;
    }
    if (audit_event >= 0 && audit_filtered(audit_event)) {
        return ESP_OK;
    }

    tf_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.magic = TF_LOG_MAGIC;
    entry.sequence = s_log.sequence++;
    entry.timestamp_us = esp_timer_get_time();
    entry.boot_id = s_log.stats.boot_id;
    entry.correlation_id = 0;
    entry.level = level;
    entry.category = category;
    entry.audit_event = audit_event;
    entry.audit_severity = audit_severity;

    if (component) {
        strncpy(entry.component, component, sizeof(entry.component) - 1);
    }

    vsnprintf(entry.message, sizeof(entry.message), fmt, args);

    xSemaphoreTake(s_log.mutex, portMAX_DELAY);
    esp_err_t ret = commit_entry(&entry);
    xSemaphoreGive(s_log.mutex);
    return ret;
}

static esp_err_t nvs_load_recent(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    tf_log_nvs_meta_t meta = {0};
    size_t meta_len = sizeof(meta);
    err = nvs_get_blob(handle, NVS_KEY_META, &meta, &meta_len);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    uint32_t loaded = 0;
    uint32_t to_load = meta.count < TF_LOG_NVS_CAPACITY ? meta.count : TF_LOG_NVS_CAPACITY;

    for (uint32_t i = 0; i < to_load; i++) {
        uint32_t slot = (meta.write_index + TF_LOG_NVS_CAPACITY - 1 - i) % TF_LOG_NVS_CAPACITY;
        char key[8];
        snprintf(key, sizeof(key), "%s%lu", NVS_KEY_ENTRY_PREFIX, (unsigned long)slot);

        tf_log_entry_t entry;
        size_t len = sizeof(entry);
        if (nvs_get_blob(handle, key, &entry, &len) != ESP_OK) {
            continue;
        }
        if (entry.magic != TF_LOG_MAGIC || entry.checksum != checksum_entry(&entry)) {
            continue;
        }
        append_entry(&entry);
        loaded++;
    }

    nvs_close(handle);
    if (loaded > 0) {
        ESP_LOGI(TAG, "Restored %lu log entries from NVS", (unsigned long)loaded);
    }
    return ESP_OK;
}

esp_err_t log_manager_init(const tf_log_config_t *config)
{
    if (s_log.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_log, 0, sizeof(s_log));

    if (config) {
        s_log.config = *config;
    } else {
        s_log.config.sinks = TF_LOG_SINK_SERIAL | TF_LOG_SINK_WEB | TF_LOG_SINK_NVS;
        s_log.config.min_level = TF_LOG_LEVEL_INFO;
        s_log.config.min_serial_level = TF_LOG_LEVEL_INFO;
        s_log.config.serial_json = false;
        s_log.config.nvs_persist = true;
        s_log.config.capacity = TF_LOG_DEFAULT_CAPACITY;
        strncpy(s_log.config.mqtt_topic, TF_LOG_MQTT_TOPIC, sizeof(s_log.config.mqtt_topic) - 1);
    }

    if (s_log.config.capacity == 0) {
        s_log.config.capacity = TF_LOG_DEFAULT_CAPACITY;
    }

    s_log.entries = calloc(s_log.config.capacity, sizeof(tf_log_entry_t));
    if (!s_log.entries) {
        return ESP_ERR_NO_MEM;
    }

    s_log.mutex = xSemaphoreCreateMutex();
    if (!s_log.mutex) {
        free(s_log.entries);
        s_log.entries = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_log.stats.boot_id = esp_random();
    s_log.stats.active_sinks = s_log.config.sinks;
    s_log.initialized = true;

    nvs_load_recent();

    ESP_LOGI(TAG, "Log manager ready (boot_id=%lu, capacity=%u, sinks=0x%x)",
             (unsigned long)s_log.stats.boot_id,
             s_log.config.capacity,
             s_log.config.sinks);

    log_manager_write(TF_LOG_LEVEL_INFO, TF_LOG_CAT_SYSTEM, TAG,
                      "Log manager initialized (boot %lu)",
                      (unsigned long)s_log.stats.boot_id);
    return ESP_OK;
}

esp_err_t log_manager_deinit(void)
{
    if (!s_log.initialized) {
        return ESP_OK;
    }

    log_manager_write(TF_LOG_LEVEL_INFO, TF_LOG_CAT_SYSTEM, TAG, "Log manager shutting down");

    xSemaphoreTake(s_log.mutex, portMAX_DELAY);
    vSemaphoreDelete(s_log.mutex);
    free(s_log.entries);
    memset(&s_log, 0, sizeof(s_log));
    return ESP_OK;
}

esp_err_t log_manager_write(tf_log_level_t level,
                            tf_log_category_t category,
                            const char *component,
                            const char *message_fmt, ...)
{
    va_list args;
    va_start(args, message_fmt);
    esp_err_t ret = write_entry_va(level, category, component, TF_LOG_NO_AUDIT_EVENT, -1,
                                   message_fmt, args);
    va_end(args);
    return ret;
}

esp_err_t log_manager_write_audit(int16_t audit_event,
                                  int8_t audit_severity,
                                  tf_log_level_t level,
                                  tf_log_category_t category,
                                  const char *component,
                                  const char *message_fmt, ...)
{
    if (level < audit_severity_to_level(audit_severity)) {
        level = audit_severity_to_level(audit_severity);
    }

    va_list args;
    va_start(args, message_fmt);
    esp_err_t ret = write_entry_va(level, category, component, audit_event, audit_severity,
                                   message_fmt, args);
    va_end(args);
    return ret;
}

esp_err_t log_manager_get_recent(tf_log_entry_t *entries,
                                 uint32_t max_entries,
                                 uint32_t *num_retrieved)
{
    if (!s_log.initialized || !entries || !num_retrieved || max_entries == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_log.mutex, portMAX_DELAY);

    uint32_t to_copy = s_log.count < max_entries ? s_log.count : max_entries;
    uint32_t newest = (s_log.write_index + s_log.config.capacity - 1) % s_log.config.capacity;

    for (uint32_t i = 0; i < to_copy; i++) {
        uint32_t slot = (newest + s_log.config.capacity - i) % s_log.config.capacity;
        memcpy(&entries[i], &s_log.entries[slot], sizeof(tf_log_entry_t));
    }

    *num_retrieved = to_copy;
    xSemaphoreGive(s_log.mutex);
    return ESP_OK;
}

esp_err_t log_manager_get_entry(uint32_t index, tf_log_entry_t *entry)
{
    if (!s_log.initialized || !entry) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_log.mutex, portMAX_DELAY);

    if (index >= s_log.count) {
        xSemaphoreGive(s_log.mutex);
        return ESP_ERR_NOT_FOUND;
    }

    uint32_t slot;
    if (s_log.count < s_log.config.capacity) {
        slot = index;
    } else {
        slot = (s_log.write_index + index) % s_log.config.capacity;
    }
    memcpy(entry, &s_log.entries[slot], sizeof(tf_log_entry_t));

    xSemaphoreGive(s_log.mutex);
    return ESP_OK;
}

esp_err_t log_manager_clear(void)
{
    if (!s_log.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_log.mutex, portMAX_DELAY);
    memset(s_log.entries, 0, s_log.config.capacity * sizeof(tf_log_entry_t));
    s_log.write_index = 0;
    s_log.count = 0;
    s_log.stats.entries_in_buffer = 0;
    s_log.stats.first_entry_time_us = 0;
    s_log.stats.last_entry_time_us = 0;
    xSemaphoreGive(s_log.mutex);

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }

    ESP_LOGI(TAG, "Log buffer cleared");
    return ESP_OK;
}

esp_err_t log_manager_get_stats(tf_log_stats_t *stats)
{
    if (!s_log.initialized || !stats) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_log.mutex, portMAX_DELAY);
    *stats = s_log.stats;
    stats->entries_in_buffer = s_log.count;
    stats->active_sinks = s_log.config.sinks;
    xSemaphoreGive(s_log.mutex);
    return ESP_OK;
}

esp_err_t log_manager_get_config(tf_log_config_t *config)
{
    if (!s_log.initialized || !config) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_log.mutex, portMAX_DELAY);
    *config = s_log.config;
    xSemaphoreGive(s_log.mutex);
    return ESP_OK;
}

esp_err_t log_manager_set_config(const tf_log_config_t *config)
{
    if (!s_log.initialized || !config) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_log.mutex, portMAX_DELAY);
    s_log.config = *config;
    s_log.stats.active_sinks = s_log.config.sinks;
    xSemaphoreGive(s_log.mutex);
    return ESP_OK;
}

esp_err_t log_manager_set_sink_mask(tf_log_sink_mask_t mask)
{
    if (!s_log.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_log.mutex, portMAX_DELAY);
    s_log.config.sinks = mask;
    s_log.stats.active_sinks = mask;
    xSemaphoreGive(s_log.mutex);
    return ESP_OK;
}

esp_err_t log_manager_set_min_level(tf_log_level_t level)
{
    if (!s_log.initialized || level >= TF_LOG_LEVEL_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_log.mutex, portMAX_DELAY);
    s_log.config.min_level = level;
    xSemaphoreGive(s_log.mutex);
    return ESP_OK;
}

esp_err_t log_manager_set_mqtt_publish(tf_log_mqtt_publish_fn fn)
{
    if (!s_log.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_log.mutex, portMAX_DELAY);
    s_log.mqtt_publish = fn;
    if (fn) {
        s_log.config.sinks |= TF_LOG_SINK_MQTT;
        s_log.stats.active_sinks = s_log.config.sinks;
    }
    xSemaphoreGive(s_log.mutex);
    return ESP_OK;
}

static esp_err_t export_entries(bool ndjson, char *buffer, size_t buffer_len, size_t *exported_len)
{
    if (!buffer || !exported_len) {
        return ESP_ERR_INVALID_ARG;
    }

    tf_log_entry_t entries[TF_LOG_DEFAULT_CAPACITY];
    uint32_t count = 0;
    esp_err_t ret = log_manager_get_recent(entries, TF_LOG_DEFAULT_CAPACITY, &count);
    if (ret != ESP_OK) {
        return ret;
    }

    size_t offset = 0;
    if (!ndjson) {
        int n = snprintf(buffer + offset, buffer_len - offset, "{\"logs\":[");
        if (n < 0 || (size_t)n >= buffer_len - offset) {
            return ESP_ERR_NO_MEM;
        }
        offset += (size_t)n;
    }

    for (uint32_t i = 0; i < count; i++) {
        char line[512];
        format_entry_ndjson(&entries[i], line, sizeof(line));
        if (!ndjson) {
            if (i > 0) {
                if (offset + 1 >= buffer_len) {
                    return ESP_ERR_NO_MEM;
                }
                buffer[offset++] = ',';
            }
            size_t line_len = strlen(line);
            if (line_len > 0 && line[line_len - 1] == '\n') {
                line[line_len - 1] = '\0';
                line_len--;
            }
            if (offset + line_len >= buffer_len) {
                return ESP_ERR_NO_MEM;
            }
            memcpy(buffer + offset, line, line_len);
            offset += line_len;
        } else {
            size_t line_len = strlen(line);
            if (offset + line_len >= buffer_len) {
                return ESP_ERR_NO_MEM;
            }
            memcpy(buffer + offset, line, line_len);
            offset += line_len;
        }
    }

    if (!ndjson) {
        int n = snprintf(buffer + offset, buffer_len - offset, "],\"count\":%lu}", (unsigned long)count);
        if (n < 0 || (size_t)n >= buffer_len - offset) {
            return ESP_ERR_NO_MEM;
        }
        offset += (size_t)n;
    }

    *exported_len = offset;
    return ESP_OK;
}

esp_err_t log_manager_export_json(char *buffer, size_t buffer_len, size_t *exported_len)
{
    return export_entries(false, buffer, buffer_len, exported_len);
}

esp_err_t log_manager_export_ndjson(char *buffer, size_t buffer_len, size_t *exported_len)
{
    return export_entries(true, buffer, buffer_len, exported_len);
}

esp_err_t log_manager_verify(uint32_t *corrupted_entries)
{
    if (!s_log.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t bad = 0;
    xSemaphoreTake(s_log.mutex, portMAX_DELAY);
    for (uint32_t i = 0; i < s_log.count; i++) {
        uint32_t slot = (s_log.count < s_log.config.capacity)
                            ? i
                            : (s_log.write_index + i) % s_log.config.capacity;
        const tf_log_entry_t *entry = &s_log.entries[slot];
        if (entry->magic != TF_LOG_MAGIC || entry->checksum != checksum_entry(entry)) {
            bad++;
        }
    }
    xSemaphoreGive(s_log.mutex);

    if (corrupted_entries) {
        *corrupted_entries = bad;
    }
    return bad == 0 ? ESP_OK : ESP_ERR_INVALID_CRC;
}

esp_err_t log_manager_set_audit_filter(int16_t audit_event, bool filter_out)
{
    if (audit_event < 0 || audit_event >= 32) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_log.mutex, portMAX_DELAY);
    if (filter_out) {
        s_log.audit_filter_mask |= (1u << audit_event);
    } else {
        s_log.audit_filter_mask &= ~(1u << audit_event);
    }
    xSemaphoreGive(s_log.mutex);
    return ESP_OK;
}

uint32_t log_manager_new_correlation_id(void)
{
    if (!s_log.initialized) {
        return 0;
    }

    xSemaphoreTake(s_log.mutex, portMAX_DELAY);
    uint32_t id = ++s_log.correlation_seq;
    xSemaphoreGive(s_log.mutex);
    return id;
}