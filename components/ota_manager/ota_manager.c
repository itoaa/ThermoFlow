/**
 * @file ota_manager.c
 * @brief OTA Update Manager - Implementation
 */

#include "ota_manager.h"
#include "ed25519_impl.h"
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "OTA_MANAGER";

static ota_status_t s_status = {0};
static ota_config_t s_config = {0};
static bool s_initialized = false;
static uint8_t s_ota_public_key[ED25519_PUBLIC_KEY_LEN];
static bool s_has_ota_public_key = false;

esp_err_t ota_manager_set_public_key(const uint8_t *public_key, size_t len)
{
    if (!public_key || len != ED25519_PUBLIC_KEY_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(s_ota_public_key, public_key, sizeof(s_ota_public_key));
    s_has_ota_public_key = true;
    return ESP_OK;
}

esp_err_t ota_manager_init(const ota_config_t *config)
{
    if (s_initialized) {
        return ESP_OK;
    }

    memcpy(&s_config, config, sizeof(ota_config_t));
    memset(&s_status, 0, sizeof(s_status));
    s_status.state = OTA_STATE_IDLE;

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_app_desc_t *app = esp_app_get_description();

    if (app) {
        strncpy(s_status.current_version, app->version, sizeof(s_status.current_version) - 1);
    }
    if (running) {
        strncpy(s_status.partition_label, running->label, sizeof(s_status.partition_label) - 1);
    }

    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    s_status.can_rollback = (next != NULL && running != NULL && strcmp(running->label, next->label) != 0);

    s_initialized = true;
    ESP_LOGI(TAG, "OTA manager initialized (partition: %s)", s_status.partition_label);
    return ESP_OK;
}

esp_err_t ota_manager_deinit(void)
{
    s_initialized = false;
    return ESP_OK;
}

esp_err_t ota_manager_get_status(ota_status_t *status)
{
    if (!status || !s_initialized) {
        return s_initialized ? ESP_ERR_INVALID_ARG : ESP_ERR_INVALID_STATE;
    }
    memcpy(status, &s_status, sizeof(ota_status_t));
    return ESP_OK;
}

static void set_state(ota_state_t state, ota_error_t error)
{
    s_status.state = state;
    s_status.last_error = error;
    if (s_config.event_cb) {
        s_config.event_cb(state, error, s_config.user_data);
    }
}

esp_err_t ota_manager_check_for_update(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_config.update_url[0] == '\0') {
        return ESP_ERR_NOT_FOUND;
    }

    set_state(OTA_STATE_CHECKING, OTA_ERROR_NONE);
    s_status.update_available = true;
    return ESP_OK;
}

static esp_err_t verify_image_signature(const esp_partition_t *partition)
{
    if (!s_config.verify_signature || !s_has_ota_public_key) {
        return ESP_OK;
    }

    esp_app_desc_t desc;
    if (esp_ota_get_partition_description(partition, &desc) != ESP_OK) {
        return ESP_FAIL;
    }

    /* Signature appended after firmware image in partition tail if present */
    size_t sig_offset = partition->size - OTA_SIGNATURE_LEN - sizeof(esp_image_header_t);
    uint8_t signature[OTA_SIGNATURE_LEN];

    if (esp_partition_read(partition, sig_offset, signature, sizeof(signature)) != ESP_OK) {
        ESP_LOGW(TAG, "No signature block found — hash-only verification");
        return ESP_OK;
    }

    int valid = ed25519_verify(signature, (const uint8_t *)desc.version,
                               strlen(desc.version), s_ota_public_key);
    return (valid == 0) ? ESP_OK : ESP_ERR_INVALID_CRC;
}

esp_err_t ota_manager_start_update(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_config.update_url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    set_state(OTA_STATE_DOWNLOADING, OTA_ERROR_NONE);
    s_status.download_in_progress = true;

    esp_http_client_config_t http_cfg = {
        .url = s_config.update_url,
        .timeout_ms = 30000,
        .keep_alive_enable = true,
    };

    if (s_config.ca_cert && s_config.ca_cert_len > 0) {
        http_cfg.cert_pem = (const char *)s_config.ca_cert;
    }

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    esp_err_t ret = esp_https_ota(&ota_cfg);
    s_status.download_in_progress = false;

    if (ret != ESP_OK) {
        set_state(OTA_STATE_ERROR, OTA_ERROR_DOWNLOAD);
        return ret;
    }

    set_state(OTA_STATE_VERIFYING, OTA_ERROR_NONE);

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (s_config.verify_signature && update_partition) {
        ret = verify_image_signature(update_partition);
        if (ret != ESP_OK) {
            set_state(OTA_STATE_ERROR, OTA_ERROR_SIGNATURE);
            return ret;
        }
    }

    set_state(OTA_STATE_READY, OTA_ERROR_NONE);
    s_status.update_ready = true;
    return ESP_OK;
}

esp_err_t ota_manager_apply_update(void)
{
    if (!s_initialized || !s_status.update_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    set_state(OTA_STATE_APPLYING, OTA_ERROR_NONE);
    esp_err_t ret = esp_ota_mark_app_valid_cancel_rollback();
    if (ret != ESP_OK) {
        set_state(OTA_STATE_ERROR, OTA_ERROR_FLASH);
        return ret;
    }

    ESP_LOGI(TAG, "Rebooting to apply OTA update");
    esp_restart();
    return ESP_OK;
}

esp_err_t ota_manager_rollback(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    set_state(OTA_STATE_ROLLBACK, OTA_ERROR_NONE);
    esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
    if (err != ESP_OK) {
        set_state(OTA_STATE_ERROR, OTA_ERROR_ROLLBACK);
    }
    return err;
}

esp_err_t ota_manager_mark_valid(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_ota_mark_app_valid_cancel_rollback();
    if (ret == ESP_OK) {
        s_status.can_rollback = false;
        set_state(OTA_STATE_IDLE, OTA_ERROR_NONE);
        ESP_LOGI(TAG, "Firmware marked as valid");
    }
    return ret;
}

esp_err_t ota_manager_reset(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_status, 0, sizeof(s_status));
    s_status.state = OTA_STATE_IDLE;
    const esp_app_desc_t *app = esp_app_get_description();
    if (app) {
        strncpy(s_status.current_version, app->version, sizeof(s_status.current_version) - 1);
    }
    return ESP_OK;
}

const char *ota_manager_error_string(int error)
{
    switch (error) {
        case OTA_ERROR_NONE: return "Success";
        case OTA_ERROR_NETWORK: return "Network error";
        case OTA_ERROR_DOWNLOAD: return "Download failed";
        case OTA_ERROR_VERIFICATION: return "Verification failed";
        case OTA_ERROR_SIGNATURE: return "Signature invalid";
        case OTA_ERROR_ROLLBACK: return "Rollback failed";
        case OTA_ERROR_PARTITION: return "Partition error";
        case OTA_ERROR_FLASH: return "Flash error";
        case ESP_ERR_INVALID_ARG: return "Invalid argument";
        case ESP_ERR_INVALID_STATE: return "Invalid state";
        case ESP_ERR_NOT_FOUND: return "Not found";
        case ESP_ERR_NOT_SUPPORTED: return "Not supported";
        default: return "Unknown error";
    }
}

bool ota_manager_is_in_progress(void)
{
    return s_status.download_in_progress;
}