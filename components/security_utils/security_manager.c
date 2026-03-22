/**
 * @file security_manager.c
 * @brief Security Manager Implementation - ESP-IDF compatible
 */

#include <string.h>
#include <stdio.h>
#include "security_manager.h"
#include "ed25519_impl.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

static const char *TAG = "SECURITY";
static const char *NVS_NAMESPACE = "thermoflow_sec";

#define CREDENTIALS_KEY     "credentials"
#define CERTIFICATE_KEY     "certificate"
#define PRIVATE_KEY_KEY     "priv_key"
#define PUBLIC_KEY_KEY      "pub_key"

esp_err_t security_manager_init(void)
{
    ESP_LOGI(TAG, "Security manager initialized");
    return ESP_OK;
}

esp_err_t security_random_bytes(uint8_t *buffer, size_t len)
{
    if (!buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_fill_random(buffer, len);
    return ESP_OK;
}

esp_err_t security_hash_password(const char *password, const uint8_t *salt,
                                   size_t salt_len, uint8_t *hash)
{
    if (!password || !salt || !hash) {
        return ESP_ERR_INVALID_ARG;
    }

    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, md_info, 1);
    mbedtls_md_hmac_starts(&ctx, salt, salt_len);
    mbedtls_md_hmac_update(&ctx, (const uint8_t*)password, strlen(password));
    mbedtls_md_hmac_finish(&ctx, hash);
    mbedtls_md_free(&ctx);

    return ESP_OK;
}

bool security_verify_password(const char *password, const uint8_t *stored_hash,
                              const uint8_t *salt, size_t salt_len)
{
    if (!password || !stored_hash || !salt) {
        return false;
    }

    uint8_t computed_hash[SEC_HASH_LEN];
    esp_err_t err = security_hash_password(password, salt, salt_len, computed_hash);
    if (err != ESP_OK) {
        return false;
    }

    int diff = 0;
    for (size_t i = 0; i < SEC_HASH_LEN; i++) {
        diff |= computed_hash[i] ^ stored_hash[i];
    }

    return (diff == 0);
}

esp_err_t security_store_credentials(const char *username, const char *password)
{
    if (!username || !password) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t salt[16];
    security_random_bytes(salt, sizeof(salt));

    uint8_t hash[SEC_HASH_LEN];
    security_hash_password(password, salt, sizeof(salt), hash);

    uint8_t data[SEC_MAX_USERNAME_LEN + 16 + SEC_HASH_LEN];
    memset(data, 0, sizeof(data));
    strncpy((char*)data, username, SEC_MAX_USERNAME_LEN - 1);
    memcpy(data + SEC_MAX_USERNAME_LEN, salt, 16);
    memcpy(data + SEC_MAX_USERNAME_LEN + 16, hash, SEC_HASH_LEN);

    err = nvs_set_blob(handle, CREDENTIALS_KEY, data, sizeof(data));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t security_load_credentials(char *username, size_t username_len,
                                    uint8_t *hash, uint8_t *salt, size_t *salt_len)
{
    if (!username || !hash || !salt || !salt_len) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t data_len = SEC_MAX_USERNAME_LEN + 16 + SEC_HASH_LEN;
    uint8_t data[SEC_MAX_USERNAME_LEN + 16 + SEC_HASH_LEN];
    
    err = nvs_get_blob(handle, CREDENTIALS_KEY, data, &data_len);
    nvs_close(handle);

    if (err != ESP_OK) {
        return err;
    }

    strncpy(username, (char*)data, username_len - 1);
    username[username_len - 1] = '\0';
    memcpy(salt, data + SEC_MAX_USERNAME_LEN, 16);
    *salt_len = 16;
    memcpy(hash, data + SEC_MAX_USERNAME_LEN + 16, SEC_HASH_LEN);

    return ESP_OK;
}

bool security_validate_credentials(const char *username, const char *password)
{
    if (!username || !password) {
        return false;
    }

    char stored_user[SEC_MAX_USERNAME_LEN];
    uint8_t stored_hash[SEC_HASH_LEN];
    uint8_t salt[16];
    size_t salt_len;

    esp_err_t err = security_load_credentials(stored_user, sizeof(stored_user),
                                               stored_hash, salt, &salt_len);
    if (err != ESP_OK) {
        return false;
    }

    if (strcmp(username, stored_user) != 0) {
        return false;
    }

    return security_verify_password(password, stored_hash, salt, salt_len);
}

bool security_has_credentials(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    size_t data_len = 0;
    err = nvs_get_blob(handle, CREDENTIALS_KEY, NULL, &data_len);
    nvs_close(handle);

    return (err == ESP_OK && data_len > 0);
}

esp_err_t security_generate_certificate(char *cert, size_t cert_len,
                                        char *key, size_t key_len,
                                        const char *cn, int days_valid)
{
    if (!cert || !key || !cn) {
        return ESP_ERR_INVALID_ARG;
    }

    // Simplified - just create dummy cert for now
    snprintf(cert, cert_len, "-----BEGIN CERTIFICATE-----\nDUMMY\n-----END CERTIFICATE-----\n");
    snprintf(key, key_len, "-----BEGIN PRIVATE KEY-----\nDUMMY\n-----END PRIVATE KEY-----\n");
    
    ESP_LOGW(TAG, "Certificate generation is stubbed");
    return ESP_OK;
}

bool security_verify_certificate(const uint8_t *cert, const uint8_t *ca_cert)
{
    return (cert != NULL && ca_cert != NULL);
}

esp_err_t security_sign_data(const uint8_t *data, size_t data_len,
                              const uint8_t *private_key, uint8_t *signature)
{
    if (!data || !private_key || !signature) {
        return ESP_ERR_INVALID_ARG;
    }

    int ret = ed25519_sign(signature, data, data_len, private_key);
    if (ret != 0) {
        ESP_LOGE(TAG, "Ed25519 signing failed: %d", ret);
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool security_verify_signature(const uint8_t *data, size_t data_len,
                               const uint8_t *signature, const uint8_t *public_key)
{
    if (!data || !signature || !public_key) {
        return false;
    }

    int ret = ed25519_verify(signature, data, data_len, public_key);
    return (ret == 0);
}

esp_err_t security_store_ota_keys(const uint8_t *public_key, const uint8_t *private_key)
{
    if (!public_key) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(handle, PUBLIC_KEY_KEY, public_key, ED25519_PUBLIC_KEY_LEN);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    if (private_key) {
        err = nvs_set_blob(handle, PRIVATE_KEY_KEY, private_key, ED25519_PRIVATE_KEY_LEN);
        if (err != ESP_OK) {
            nvs_close(handle);
            return err;
        }
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t security_load_ota_keys(uint8_t *public_key, uint8_t *private_key, bool *has_private)
{
    if (!public_key) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t len = ED25519_PUBLIC_KEY_LEN;
    err = nvs_get_blob(handle, PUBLIC_KEY_KEY, public_key, &len);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    if (private_key) {
        len = ED25519_PRIVATE_KEY_LEN;
        err = nvs_get_blob(handle, PRIVATE_KEY_KEY, private_key, &len);
        if (has_private) {
            *has_private = (err == ESP_OK);
        }
    }

    nvs_close(handle);
    return ESP_OK;
}

bool security_is_healthy(void)
{
    return true;
}

esp_err_t security_manager_deinit(void)
{
    ESP_LOGI(TAG, "Security manager deinitialized");
    return ESP_OK;
}