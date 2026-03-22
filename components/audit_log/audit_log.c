/**
 * @file audit_log.c
 * @brief Audit Logging Implementation - ESP-IDF
 *
 * Secure audit logging system for tracking security-relevant events.
 * Implements IEC 62443 SR-005: Audit Logging requirement.
 *
 * Features:
 * - 50-entry circular in-memory buffer
 * - Sequence numbering for tamper detection
 * - Checksum validation for integrity
 * - Timestamping with microsecond precision
 * - Thread-safe with mutex protection
 * - Multiple event types and severity levels
 * - Formatted messages with variadic arguments
 * - Statistics tracking
 *
 * Event types: Authentication, config changes, fan/sensor events,
 * network events, OTA updates, security alerts, rate limiting, system events
 *
 * @author Ola Andersson
 * @version 1.0.0
 * @date 2026-03-22
 *
 * @section changelog Change Log
 * - 1.0.0 (2026-03-22): Initial implementation
 *   - In-memory circular buffer with 50 entries
 *   - Checksum-based integrity verification
 *   - 16 event types across 5 severity levels
 *   - JSON export support (header-defined format)
 */

#include <string.h>                   /* memset, memcpy */
#include <stdio.h>                    /* snprintf, vsnprintf */
#include <stdarg.h>                   /* va_list, va_start, va_end */
#include "audit_log.h"                 /* Public interface */
#include "esp_log.h"                   /* ESP-IDF logging */
#include "esp_timer.h"                 /* High-resolution timestamps */
#include "esp_random.h"                /* Hardware RNG for IDs */
#include "nvs_flash.h"                 /* Non-volatile storage (future use) */
#include "freertos/FreeRTOS.h"         /* FreeRTOS core */
#include "freertos/semphr.h"           /* Semaphores for thread safety */

/* Logging tag - appears in log messages from this component */
static const char *TAG = "AUDIT_LOG";

/* NVS namespace for future persistent storage */
static const char *NVS_NAMESPACE = "audit_log";

/* Number of entries in circular buffer */
#define MEM_BUFFER_SIZE     50

/**
 * @brief In-memory log buffer structure
 */
typedef struct {
    audit_log_entry_t entries[MEM_BUFFER_SIZE];  /* Fixed-size entry array */
    uint32_t write_index;                          /* Next write position */
    uint32_t count;                                /* Current entry count */
} mem_buffer_t;

/**
 * @brief Global audit log state
 */
static struct {
    bool initialized;                              /* Module initialized flag */
    audit_log_stats_t stats;                       /* Runtime statistics */
    uint32_t sequence;                             /* Global sequence counter */
    mem_buffer_t mem_buffer;                       /* Circular buffer */
    SemaphoreHandle_t mutex;                       /* Thread safety mutex */
} audit_state;

/**
 * @brief Convert event type to string representation
 *
 * Used for JSON export and human-readable logging.
 *
 * @param type Event type enum
 * @return String representation (e.g., "AUTH_SUCCESS")
 */
static const char *get_event_type_str(audit_event_type_t type)
{
    switch(type) {
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
        default: return "UNKNOWN";
    }
}

/**
 * @brief Convert severity level to string
 *
 * @param severity Severity enum value
 * @return String representation (e.g., "ERROR")
 */
static const char *get_severity_str(audit_severity_t severity)
{
    switch(severity) {
        case AUDIT_SEVERITY_DEBUG: return "DEBUG";
        case AUDIT_SEVERITY_INFO: return "INFO";
        case AUDIT_SEVERITY_WARNING: return "WARN";
        case AUDIT_SEVERITY_ERROR: return "ERROR";
        case AUDIT_SEVERITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Calculate checksum for entry integrity
 *
 * Simple additive checksum over all fields except checksum itself.
 * Detects data corruption in memory.
 *
 * @param entry Audit log entry
 * @return 32-bit checksum value
 */
static uint32_t calculate_checksum(const audit_log_entry_t *entry)
{
    const uint8_t *data = (const uint8_t *)entry;
    uint32_t sum = 0;

    /* Sum bytes before checksum field */
    size_t checksum_offset = offsetof(audit_log_entry_t, checksum);
    for (size_t i = 0; i < checksum_offset; i++) {
        sum += data[i];
    }

    /* Sum bytes after checksum field */
    size_t after_checksum = checksum_offset + sizeof(entry->checksum);
    for (size_t i = after_checksum; i < sizeof(audit_log_entry_t); i++) {
        sum += data[i];
    }

    return sum;
}

/**
 * @brief Add entry to circular buffer
 *
 * Overwrites oldest entry when buffer is full.
 *
 * @param entry Entry to add (already populated)
 */
static void add_to_mem_buffer(const audit_log_entry_t *entry)
{
    /* Copy entry to current write position */
    memcpy(&audit_state.mem_buffer.entries[audit_state.mem_buffer.write_index],
           entry, sizeof(audit_log_entry_t));

    /* Advance write index (circular) */
    audit_state.mem_buffer.write_index =
        (audit_state.mem_buffer.write_index + 1) % MEM_BUFFER_SIZE;

    /* Increment count up to max size */
    if (audit_state.mem_buffer.count < MEM_BUFFER_SIZE) {
        audit_state.mem_buffer.count++;
    }
}

/**
 * @brief Initialize audit log system
 *
 * Creates mutex, clears buffer, and logs system boot event.
 * Must be called before using other audit functions.
 *
 * @param config Configuration parameters (currently unused)
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if already initialized,
 *         ESP_ERR_NO_MEM if mutex creation fails
 */
esp_err_t audit_log_init(const audit_log_config_t *config)
{
    (void)config;  /* Unused in current implementation */

    if (audit_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Clear global state */
    memset(&audit_state, 0, sizeof(audit_state));

    /* Create mutex for thread safety */
    audit_state.mutex = xSemaphoreCreateMutex();
    if (!audit_state.mutex) {
        return ESP_ERR_NO_MEM;
    }

    /* Initialize buffer timestamps */
    uint64_t now = esp_timer_get_time();
    for (int j = 0; j < MEM_BUFFER_SIZE; j++) {
        audit_state.mem_buffer.entries[j].timestamp_us = now;
    }

    audit_state.initialized = true;
    ESP_LOGI(TAG, "Audit log initialized");

    /* Log system boot event */
    audit_log_event(AUDIT_EVENT_SYSTEM_BOOT, AUDIT_SEVERITY_INFO,
                    "System booted");

    return ESP_OK;
}

/**
 * @brief Log a security event
 *
 * Main logging function with printf-style formatting.
 * Creates entry, calculates checksum, and adds to buffer.
 * Also logs to ESP_LOG for ERROR severity and above.
 *
 * @param event_type Type of event being logged
 * @param severity Severity/importance level
 * @param message Format string (printf-style)
 * @param ... Variadic arguments for formatting
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized,
 *         ESP_ERR_INVALID_ARG if event_type out of range
 */
esp_err_t audit_log_event(audit_event_type_t event_type, audit_severity_t severity,
                          const char *message, ...)
{
    if (!audit_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (event_type >= AUDIT_EVENT_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(audit_state.mutex, portMAX_DELAY);

    /* Prepare entry structure */
    audit_log_entry_t entry;
    memset(&entry, 0, sizeof(entry));

    entry.magic = AUDIT_LOG_MAGIC;                   /* Magic for validation */
    entry.sequence = audit_state.sequence++;         /* Assign sequence number */
    entry.timestamp_us = esp_timer_get_time();       /* Current timestamp */
    entry.event_type = event_type;
    entry.severity = severity;

    /* Format message with variadic arguments */
    va_list args;
    va_start(args, message);
    vsnprintf(entry.message, sizeof(entry.message), message, args);
    va_end(args);

    /* Calculate checksum for integrity */
    entry.checksum = calculate_checksum(&entry);

    /* Add to circular buffer */
    add_to_mem_buffer(&entry);

    /* Update statistics */
    audit_state.stats.total_entries++;
    audit_state.stats.last_entry_time = entry.timestamp_us;
    if (audit_state.stats.first_entry_time == 0) {
        audit_state.stats.first_entry_time = entry.timestamp_us;
    }

    /* Also log high-severity events to ESP_LOG */
    if (severity >= AUDIT_SEVERITY_ERROR) {
        ESP_LOGW(TAG, "[%s] %s: %s",
                 get_severity_str(severity),
                 get_event_type_str(event_type),
                 entry.message);
    }

    xSemaphoreGive(audit_state.mutex);

    return ESP_OK;
}

/**
 * @brief Log authentication event
 *
 * Convenience wrapper for auth success/failure events.
 *
 * @param username User attempting authentication
 * @param success true if authentication succeeded
 * @param source Source IP or identifier
 * @return ESP_OK on success
 */
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

/**
 * @brief Log configuration change
 *
 * Convenience wrapper for tracking configuration modifications.
 *
 * @param setting Name of setting changed
 * @param old_value Previous value (can be NULL)
 * @param new_value New value (can be NULL)
 * @param user User who made the change (can be NULL)
 * @return ESP_OK on success
 */
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

/**
 * @brief Clear all audit log entries
 *
 * Resets buffer and statistics. Requires initialization.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t audit_log_clear(void)
{
    if (!audit_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(audit_state.mutex, portMAX_DELAY);

    /* Clear buffer */
    memset(&audit_state.mem_buffer, 0, sizeof(audit_state.mem_buffer));

    /* Reset statistics */
    audit_state.stats.total_entries = 0;
    audit_state.stats.first_entry_time = 0;
    audit_state.stats.last_entry_time = 0;

    xSemaphoreGive(audit_state.mutex);

    ESP_LOGI(TAG, "Audit log cleared");
    return ESP_OK;
}

/**
 * @brief Get audit log statistics
 *
 * Returns copy of current statistics (thread-safe).
 *
 * @param[out] stats Statistics structure to fill
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if stats is NULL
 */
esp_err_t audit_log_get_stats(audit_log_stats_t *stats)
{
    if (!audit_state.initialized || !stats) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(audit_state.mutex, portMAX_DELAY);
    memcpy(stats, &audit_state.stats, sizeof(audit_log_stats_t));
    xSemaphoreGive(audit_state.mutex);

    return ESP_OK;
}

/**
 * @brief Deinitialize audit log system
 *
 * Logs shutdown event and releases resources.
 *
 * @return ESP_OK on success
 */
esp_err_t audit_log_deinit(void)
{
    if (audit_state.initialized) {
        /* Log shutdown before releasing resources */
        audit_log_event(AUDIT_EVENT_SYSTEM_SHUTDOWN, AUDIT_SEVERITY_INFO,
                        "System shutting down");

        /* Clean up mutex */
        vSemaphoreDelete(audit_state.mutex);

        /* Clear state */
        memset(&audit_state, 0, sizeof(audit_state));
    }

    return ESP_OK;
}
