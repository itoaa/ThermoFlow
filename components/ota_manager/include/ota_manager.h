/**
 * @file ota_manager.h
 * @brief OTA Update Manager
 * 
 * Implements secure OTA updates with HTTPS, Ed25519 signatures,
 * anti-rollback protection, and automatic rollback on failure.
 * 
 * @version 1.0.0
 * @date 2026-04-13
 * @security DEV-001: Secure OTA Implementation
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OTA_URL_MAX_LEN         256
#define OTA_VERSION_MAX_LEN     32
#define OTA_SIGNATURE_LEN       64
#define OTA_PARTITION_LABEL_LEN 32

/**
 * @brief OTA state machine states
 */
typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_CHECKING,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_READY,
    OTA_STATE_APPLYING,
    OTA_STATE_ROLLBACK,
    OTA_STATE_ERROR
} ota_state_t;

/**
 * @brief OTA error codes
 */
typedef enum {
    OTA_ERROR_NONE = 0,
    OTA_ERROR_NETWORK,
    OTA_ERROR_DOWNLOAD,
    OTA_ERROR_VERIFICATION,
    OTA_ERROR_SIGNATURE,
    OTA_ERROR_ROLLBACK,
    OTA_ERROR_PARTITION,
    OTA_ERROR_FLASH,
    OTA_ERROR_UNKNOWN
} ota_error_t;

/**
 * @brief OTA progress callback
 */
typedef void (*ota_progress_cb_t)(uint32_t progress, uint32_t total, void *user_data);

/**
 * @brief OTA event callback
 */
typedef void (*ota_event_cb_t)(ota_state_t state, ota_error_t error, void *user_data);

/**
 * @brief OTA configuration
 */
typedef struct {
    bool use_https;
    bool verify_signature;
    bool verify_hash;
    bool enable_anti_rollback;
    uint8_t min_security_version;
    char update_url[OTA_URL_MAX_LEN];
    uint32_t check_interval_ms;
    uint8_t security_version;
    bool auto_rollback;
    ota_progress_cb_t progress_cb;
    ota_event_cb_t event_cb;
    void *user_data;
    const uint8_t *ca_cert;
    size_t ca_cert_len;
    bool use_certificate_pinning;
} ota_config_t;

/**
 * @brief OTA status information
 */
typedef struct {
    ota_state_t state;
    bool update_available;
    bool download_in_progress;
    bool update_ready;
    bool can_rollback;
    char current_version[OTA_VERSION_MAX_LEN];
    char pending_version[OTA_VERSION_MAX_LEN];
    char partition_label[OTA_PARTITION_LABEL_LEN];
    uint32_t download_progress;
    uint32_t download_total;
    int last_error;
} ota_status_t;

/**
 * @brief Initialize OTA manager
 * @param config OTA configuration
 * @return ESP_OK on success
 */
esp_err_t ota_manager_init(const ota_config_t *config);

/**
 * @brief Set Ed25519 public key for OTA signature verification
 */
esp_err_t ota_manager_set_public_key(const uint8_t *public_key, size_t len);

/**
 * @brief Deinitialize OTA manager
 * @return ESP_OK on success
 */
esp_err_t ota_manager_deinit(void);

/**
 * @brief Get current OTA status
 * @param status Pointer to status structure
 * @return ESP_OK on success
 */
esp_err_t ota_manager_get_status(ota_status_t *status);

/**
 * @brief Check for available OTA update
 * @return ESP_OK if update available, ESP_ERR_NOT_FOUND if no update
 */
esp_err_t ota_manager_check_for_update(void);

/**
 * @brief Start OTA download and update
 * @return ESP_OK on success
 */
esp_err_t ota_manager_start_update(void);

/**
 * @brief Apply downloaded update
 * @return ESP_OK on success
 */
esp_err_t ota_manager_apply_update(void);

/**
 * @brief Rollback to previous firmware
 * @return ESP_OK on success
 */
esp_err_t ota_manager_rollback(void);

/**
 * @brief Mark current firmware as valid
 * @return ESP_OK on success
 */
esp_err_t ota_manager_mark_valid(void);

/**
 * @brief Reset OTA state
 * @return ESP_OK on success
 */
esp_err_t ota_manager_reset(void);

/**
 * @brief Get error string for error code
 * @param error Error code
 * @return Error string
 */
const char* ota_manager_error_string(int error);

/**
 * @brief Is OTA in progress
 * @return true if OTA operation is in progress
 */
bool ota_manager_is_in_progress(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_MANAGER_H */
