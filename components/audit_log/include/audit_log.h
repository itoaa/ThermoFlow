/**
 * @file audit_log.h
 * @brief Audit Logging Interface
 * 
 * Persistent security event logging with integrity verification
 * Implements IEC 62443 SR-005: Audit Logging
 * 
 * @version 1.0.0
 * @date 2026-03-22
 */

#ifndef AUDIT_LOG_H
#define AUDIT_LOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIT_LOG_MAX_MESSAGE_LEN   256
#define AUDIT_LOG_MAX_ENTRIES       1000
#define AUDIT_LOG_MAGIC             0x41554454  // "AUDT"

/**
 * @brief Audit event types
 */
typedef enum {
    AUDIT_EVENT_AUTH_SUCCESS,       /**< Authentication successful */
    AUDIT_EVENT_AUTH_FAILURE,       /**< Authentication failed */
    AUDIT_EVENT_CONFIG_CHANGE,      /**< Configuration modified */
    AUDIT_EVENT_FAN_STATE_CHANGE,   /**< Fan control change */
    AUDIT_EVENT_SENSOR_FAILURE,     /**< Sensor error */
    AUDIT_EVENT_NETWORK_CONNECT,    /**< Network connection */
    AUDIT_EVENT_NETWORK_DISCONNECT, /**< Network disconnection */
    AUDIT_EVENT_MQTT_CONNECT,       /**< MQTT connected */
    AUDIT_EVENT_MQTT_DISCONNECT,    /**< MQTT disconnected */
    AUDIT_EVENT_OTA_START,          /**< OTA update started */
    AUDIT_EVENT_OTA_COMPLETE,       /**< OTA completed */
    AUDIT_EVENT_OTA_FAILURE,        /**< OTA failed */
    AUDIT_EVENT_SECURITY_ALERT,     /**< Security violation detected */
    AUDIT_EVENT_RATE_LIMIT_HIT,     /**< Rate limit exceeded */
    AUDIT_EVENT_SYSTEM_BOOT,        /**< System boot */
    AUDIT_EVENT_SYSTEM_SHUTDOWN,    /**< System shutdown */
    AUDIT_EVENT_MAX
} audit_event_type_t;

/**
 * @brief Severity levels
 */
typedef enum {
    AUDIT_SEVERITY_DEBUG,           /**< Debug information */
    AUDIT_SEVERITY_INFO,            /**< Informational */
    AUDIT_SEVERITY_WARNING,         /**< Warning */
    AUDIT_SEVERITY_ERROR,           /**< Error */
    AUDIT_SEVERITY_CRITICAL         /**< Critical security event */
} audit_severity_t;

/**
 * @brief Audit log entry
 */
typedef struct {
    uint32_t magic;                 /**< Magic number for validation */
    uint32_t sequence;              /**< Sequence number */
    uint64_t timestamp_us;          /**< Timestamp in microseconds */
    audit_event_type_t event_type;   /**< Event type */
    audit_severity_t severity;       /**< Severity level */
    uint32_t checksum;              /**< Entry checksum */
    char message[AUDIT_LOG_MAX_MESSAGE_LEN]; /**< Log message */
} audit_log_entry_t;

/**
 * @brief Audit log configuration
 */
typedef struct {
    const char *storage_path;         /**< Storage path (NULL for NVS) */
    uint32_t max_entries;           /**< Maximum entries to keep */
    bool wrap_around;               /**< Overwrite oldest when full */
} audit_log_config_t;

/**
 * @brief Audit log statistics
 */
typedef struct {
    uint32_t total_entries;         /**< Total entries logged */
    uint32_t entries_in_storage;    /**< Current entries in storage */
    uint32_t wrap_count;            /**< Number of times log wrapped */
    uint64_t first_entry_time;      /**< Timestamp of first entry */
    uint64_t last_entry_time;       /**< Timestamp of last entry */
} audit_log_stats_t;

/**
 * @brief Initialize audit log
 * 
 * @param config Configuration (NULL for defaults)
 * @return ESP_OK on success
 */
esp_err_t audit_log_init(const audit_log_config_t *config);

/**
 * @brief Log a security event
 * 
 * @param event_type Type of event
 * @param severity Severity level
 * @param message Format string (like printf)
 * @param ... Variable arguments
 * @return ESP_OK on success
 */
esp_err_t audit_log_event(audit_event_type_t event_type, audit_severity_t severity,
                            const char *message, ...);

/**
 * @brief Log authentication event
 * 
 * Convenience wrapper for auth events
 * 
 * @param username Username
 * @param success true if successful
 * @param source Source IP or session
 * @return ESP_OK on success
 */
esp_err_t audit_log_auth(const char *username, bool success, const char *source);

/**
 * @brief Log configuration change
 * 
 * @param setting Setting name
 * @param old_value Old value
 * @param new_value New value
 * @param user User making change
 * @return ESP_OK on success
 */
esp_err_t audit_log_config_change(const char *setting, const char *old_value,
                                   const char *new_value, const char *user);

/**
 * @brief Get log entry by index
 * 
 * @param index Entry index (0 = oldest)
 * @param[out] entry Entry buffer
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if invalid index
 */
esp_err_t audit_log_get_entry(uint32_t index, audit_log_entry_t *entry);

/**
 * @brief Get most recent entries
 * 
 * @param[out] entries Buffer for entries
 * @param max_entries Maximum entries to retrieve
 * @param[out] num_retrieved Number actually retrieved
 * @return ESP_OK on success
 */
esp_err_t audit_log_get_recent(audit_log_entry_t *entries, uint32_t max_entries,
                                uint32_t *num_retrieved);

/**
 * @brief Clear all log entries
 * 
 * @return ESP_OK on success
 */
esp_err_t audit_log_clear(void);

/**
 * @brief Get log statistics
 * 
 * @param[out] stats Statistics structure
 * @return ESP_OK on success
 */
esp_err_t audit_log_get_stats(audit_log_stats_t *stats);

/**
 * @brief Export log to JSON string
 * 
 * @param[out] buffer Output buffer
 * @param buffer_len Buffer length
 * @param[out] exported_len Length exported
 * @return ESP_OK on success
 */
esp_err_t audit_log_export_json(char *buffer, size_t buffer_len, size_t *exported_len);

/**
 * @brief Verify log integrity
 * 
 * Checks checksums and sequence numbers
 * 
 * @param[out] corrupted_entries Number of corrupted entries found
 * @return ESP_OK if valid, ESP_ERR_INVALID_CRC if corruption detected
 */
esp_err_t audit_log_verify(uint32_t *corrupted_entries);

/**
 * @brief Set event type filter
 * 
 * Events of filtered types are not logged
 * 
 * @param event_type Event type to filter
 * @param filter true to filter out
 * @return ESP_OK on success
 */
esp_err_t audit_log_set_filter(audit_event_type_t event_type, bool filter);

/**
 * @brief Deinitialize audit log
 * 
 * @return ESP_OK on success
 */
esp_err_t audit_log_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIT_LOG_H */