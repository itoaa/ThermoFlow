/**
 * @file security_manager.c
 * @brief Security Manager Implementation - ESP-IDF compatible
 * 
 * Implements certificate management, authentication, and secure operations
 * Includes MQTT-TLS certificate handling (SEC-016)
 * 
 * @version 2.0.0
 * @date 2026-04-12
 * @security SEC-016: MQTT-TLS Certificate Management
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "security_manager.h"
#include "ed25519_impl.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"
#include "mbedtls/pem.h"

static const char *TAG = "SECURITY";
static const char *NVS_NAMESPACE = "thermoflow_sec";
static const char *NVS_CERT_NAMESPACE = "thermoflow_certs";

#define CREDENTIALS_KEY     "credentials"
#define CERTIFICATE_KEY     "certificate"
#define PRIVATE_KEY_KEY     "priv_key"
#define PUBLIC_KEY_KEY      "pub_key"
#define PINNING_HASH_KEY    "pin_hash"

#define CSR_BUFFER_SIZE     2048
#define KEY_BUFFER_SIZE     2048

/* ============================================
 * Certificate Type Helpers
 * ============================================ */

static const char* get_cert_nvs_key(sec_cert_type_t type)
{
    switch (type) {
        case SEC_CERT_TYPE_CA:
            return "ca_cert";
        case SEC_CERT_TYPE_CLIENT:
            return "client_cert";
        case SEC_CERT_TYPE_CLIENT_KEY:
            return "client_key";
        case SEC_CERT_TYPE_SERVER:
            return "server_cert";
        default:
            return "unknown";
    }
}

static const char* get_cert_type_name(sec_cert_type_t type)
{
    switch (type) {
        case SEC_CERT_TYPE_CA:
            return "CA";
        case SEC_CERT_TYPE_CLIENT:
            return "Client";
        case SEC_CERT_TYPE_CLIENT_KEY:
            return "Client Key";
        case SEC_CERT_TYPE_SERVER:
            return "Server";
        default:
            return "Unknown";
    }
}

/* ============================================
 * Core Security Functions
 * ============================================ */

esp_err_t security_manager_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS flash init failed: %d", ret);
        return ret;
    }
    
    ESP_LOGI(TAG, "Security manager initialized");
    
    // Check what certificates are configured
    if (security_has_certificate(SEC_CERT_TYPE_CA)) {
        ESP_LOGI(TAG, "CA certificate found in NVS");
    }
    if (security_has_certificate(SEC_CERT_TYPE_CLIENT)) {
        ESP_LOGI(TAG, "Client certificate found in NVS");
    }
    
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
    
    ESP_LOGI(TAG, "Credentials stored for user: %s", username);
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

/* ============================================
 * Certificate Management (SEC-016)
 * ============================================ */

esp_err_t security_store_certificate(sec_cert_type_t type, const char *cert, size_t cert_len)
{
    if (!cert || cert_len == 0 || cert_len > SEC_MAX_CERT_LEN) {
        ESP_LOGE(TAG, "Invalid certificate parameters");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_CERT_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %d", err);
        return err;
    }

    const char *key = get_cert_nvs_key(type);
    
    // Store certificate
    err = nvs_set_str(handle, key, cert);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store certificate: %d", err);
        nvs_close(handle);
        return err;
    }
    
    // Store metadata (length)
    char meta_key[32];
    snprintf(meta_key, sizeof(meta_key), "%s_len", key);
    err = nvs_set_u32(handle, meta_key, cert_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to store certificate metadata: %d", err);
        // Continue anyway
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Stored %s certificate (%zu bytes)", get_cert_type_name(type), cert_len);
    }
    
    return err;
}

esp_err_t security_load_certificate(sec_cert_type_t type, char **cert, size_t *cert_len)
{
    if (!cert || !cert_len) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_CERT_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    const char *key = get_cert_nvs_key(type);
    
    // Get required size
    size_t size = 0;
    err = nvs_get_str(handle, key, NULL, &size);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }
    
    // Allocate and load
    *cert = malloc(size);
    if (!*cert) {
        nvs_close(handle);
        return ESP_ERR_NO_MEM;
    }
    
    err = nvs_get_str(handle, key, *cert, &size);
    nvs_close(handle);
    
    if (err != ESP_OK) {
        free(*cert);
        *cert = NULL;
        return err;
    }
    
    *cert_len = size - 1; // Exclude null terminator
    
    ESP_LOGD(TAG, "Loaded %s certificate (%zu bytes)", get_cert_type_name(type), *cert_len);
    
    return ESP_OK;
}

esp_err_t security_delete_certificate(sec_cert_type_t type)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_CERT_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    const char *key = get_cert_nvs_key(type);
    err = nvs_erase_key(handle, key);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return err;
    }
    
    // Also delete metadata
    char meta_key[32];
    snprintf(meta_key, sizeof(meta_key), "%s_len", key);
    nvs_erase_key(handle, meta_key);
    
    nvs_commit(handle);
    nvs_close(handle);
    
    ESP_LOGI(TAG, "Deleted %s certificate", get_cert_type_name(type));
    
    return ESP_OK;
}

bool security_has_certificate(sec_cert_type_t type)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_CERT_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }

    const char *key = get_cert_nvs_key(type);
    size_t size = 0;
    err = nvs_get_str(handle, key, NULL, &size);
    nvs_close(handle);

    return (err == ESP_OK && size > 0);
}

esp_err_t security_calc_cert_fingerprint(const char *cert, uint8_t *fingerprint)
{
    if (!cert || !fingerprint) {
        return ESP_ERR_INVALID_ARG;
    }

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const unsigned char*)cert, strlen(cert));
    mbedtls_sha256_finish(&ctx, fingerprint);
    mbedtls_sha256_free(&ctx);
    
    return ESP_OK;
}

bool security_validate_cert_format(const char *cert, int expected_type)
{
    if (!cert) return false;
    
    // Check PEM markers
    if (strstr(cert, "-----BEGIN CERTIFICATE-----") == NULL) {
        ESP_LOGE(TAG, "Missing BEGIN CERTIFICATE marker");
        return false;
    }
    if (strstr(cert, "-----END CERTIFICATE-----") == NULL) {
        ESP_LOGE(TAG, "Missing END CERTIFICATE marker");
        return false;
    }
    
    // Try to parse with mbedtls
    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);
    
    int ret = mbedtls_x509_crt_parse(&crt, (const unsigned char*)cert, strlen(cert) + 1);
    mbedtls_x509_crt_free(&crt);
    
    if (ret != 0) {
        ESP_LOGE(TAG, "Certificate parse failed: -0x%04x", -ret);
        return false;
    }
    
    return true;
}

bool security_is_cert_expired(const char *cert, int *days_remaining)
{
    if (!cert) return true; // Treat as expired if NULL
    
    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);
    
    int ret = mbedtls_x509_crt_parse(&crt, (const unsigned char*)cert, strlen(cert) + 1);
    if (ret != 0) {
        mbedtls_x509_crt_free(&crt);
        ESP_LOGE(TAG, "Failed to parse certificate: -0x%04x", -ret);
        return true; // Treat as expired on parse error
    }
    
    // Get current time
    time_t now = time(NULL);
    
    // Check validity
    if (crt.valid_from.year != 0) {
        struct tm valid_from = {
            .tm_year = crt.valid_from.year - 1900,
            .tm_mon = crt.valid_from.mon - 1,
            .tm_mday = crt.valid_from.day,
            .tm_hour = crt.valid_from.hour,
            .tm_min = crt.valid_from.min,
            .tm_sec = crt.valid_from.sec
        };
        time_t valid_from_time = mktime(&valid_from);
        
        if (now < valid_from_time) {
            ESP_LOGW(TAG, "Certificate not yet valid");
            mbedtls_x509_crt_free(&crt);
            return true;
        }
    }
    
    if (crt.valid_to.year != 0) {
        struct tm valid_to = {
            .tm_year = crt.valid_to.year - 1900,
            .tm_mon = crt.valid_to.mon - 1,
            .tm_mday = crt.valid_to.day,
            .tm_hour = crt.valid_to.hour,
            .tm_min = crt.valid_to.min,
            .tm_sec = crt.valid_to.sec
        };
        time_t valid_to_time = mktime(&valid_to);
        
        int days = (int)((valid_to_time - now) / 86400);
        
        if (days_remaining) {
            *days_remaining = days;
        }
        
        if (now > valid_to_time) {
            ESP_LOGW(TAG, "Certificate has expired");
            mbedtls_x509_crt_free(&crt);
            return true;
        }
        
        if (days < 30) {
            ESP_LOGW(TAG, "Certificate expires in %d days", days);
        }
    }
    
    mbedtls_x509_crt_free(&crt);
    return false; // Not expired
}

esp_err_t security_generate_csr(char *csr, size_t csr_len, 
                                const char *cn, const char *org, const char *country)
{
    if (!csr || csr_len == 0 || !cn) {
        return ESP_ERR_INVALID_ARG;
    }

    mbedtls_pk_context key;
    mbedtls_x509write_csr req;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "ecp_genkey";
    
    mbedtls_pk_init(&key);
    mbedtls_x509write_csr_init(&req);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    
    // Initialize RNG
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    (const unsigned char*)pers, strlen(pers));
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    // Generate EC key
    ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_pk_setup failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key),
                              mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_ecp_gen_key failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    // Set up CSR
    mbedtls_x509write_csr_set_key(&req, &key);
    
    // Build subject name
    char subject[256];
    if (org && country) {
        snprintf(subject, sizeof(subject), "C=%s,O=%s,CN=%s", country, org, cn);
    } else {
        snprintf(subject, sizeof(subject), "CN=%s", cn);
    }
    
    ret = mbedtls_x509write_csr_set_subject_name(&req, subject);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_x509write_csr_set_subject_name failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    mbedtls_x509write_csr_set_md_alg(&req, MBEDTLS_MD_SHA256);
    
    // Generate CSR PEM
    ret = mbedtls_x509write_csr_pem(&req, (unsigned char*)csr, csr_len,
                                    mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret < 0) {
        ESP_LOGE(TAG, "mbedtls_x509write_csr_pem failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    ESP_LOGI(TAG, "CSR generated for CN=%s", cn);
    ret = 0;
    
cleanup:
    mbedtls_pk_free(&key);
    mbedtls_x509write_csr_free(&req);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    
    return (ret == 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t security_store_pinning_hash(const uint8_t *hash)
{
    if (!hash) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_CERT_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(handle, PINNING_HASH_KEY, hash, 32);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Certificate pinning hash stored");
    }
    
    return err;
}

esp_err_t security_load_pinning_hash(uint8_t *hash)
{
    if (!hash) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_CERT_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t len = 32;
    err = nvs_get_blob(handle, PINNING_HASH_KEY, hash, &len);
    nvs_close(handle);

    return err;
}

/* ============================================
 * Legacy Certificate Functions
 * ============================================ */

esp_err_t security_generate_certificate(char *cert, size_t cert_len,
                                        char *key, size_t key_len,
                                        const char *cn, int days_valid)
{
    if (!cert || !key || !cn) {
        return ESP_ERR_INVALID_ARG;
    }

    mbedtls_x509write_cert crt;
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_mpi serial;
    
    const char *pers = "gen_self_signed";
    
    mbedtls_x509write_crt_init(&crt);
    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_mpi_init(&serial);
    
    // Initialize RNG
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    (const unsigned char*)pers, strlen(pers));
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    // Generate key
    ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_pk_setup failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(pk),
                              mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_ecp_gen_key failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    // Write key to PEM
    ret = mbedtls_pk_write_key_pem(&pk, (unsigned char*)key, key_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_pk_write_key_pem failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    // Set certificate fields
    mbedtls_x509write_crt_set_subject_key(&crt, &pk);
    mbedtls_x509write_crt_set_issuer_key(&crt, &pk);
    mbedtls_x509write_crt_set_subject_name(&crt, cn);
    mbedtls_x509write_crt_set_issuer_name(&crt, cn);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    
    // Set serial number (raw bytes for ESP-IDF v5.1.2 compatibility)
    uint8_t serial_buf[16] = {0};
    esp_fill_random(serial_buf, sizeof(serial_buf));
    ret = mbedtls_x509write_crt_set_serial_raw(&crt, serial_buf, sizeof(serial_buf));
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_x509write_crt_set_serial_raw failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    // Validity period
    char not_before[32], not_after[32];
    snprintf(not_before, sizeof(not_before), "%s", "20260101000000");
    
    time_t now = time(NULL);
    time_t expiry = now + (days_valid * 86400);
    struct tm *expiry_tm = gmtime(&expiry);
    snprintf(not_after, sizeof(not_after), "%04d%02d%02d%02d%02d%02d",
             expiry_tm->tm_year + 1900, expiry_tm->tm_mon + 1, expiry_tm->tm_mday,
             expiry_tm->tm_hour, expiry_tm->tm_min, expiry_tm->tm_sec);
    
    ret = mbedtls_x509write_crt_set_validity(&crt, not_before, not_after);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_x509write_crt_set_validity failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    // Generate certificate
    ret = mbedtls_x509write_crt_pem(&crt, (unsigned char*)cert, cert_len,
                                    mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret < 0) {
        ESP_LOGE(TAG, "mbedtls_x509write_crt_pem failed: -0x%04x", -ret);
        goto cleanup;
    }
    
    ESP_LOGI(TAG, "Self-signed certificate generated for CN=%s", cn);
    ret = 0;
    
cleanup:
    mbedtls_mpi_free(&serial);
    mbedtls_pk_free(&pk);
    mbedtls_x509write_crt_free(&crt);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    
    return (ret == 0) ? ESP_OK : ESP_FAIL;
}

bool security_verify_certificate(const uint8_t *cert, const uint8_t *ca_cert)
{
    if (!cert || !ca_cert) {
        return false;
    }
    
    // Simplified verification - check if both are valid PEM
    return (strstr((const char*)cert, "-----BEGIN CERTIFICATE-----") != NULL &&
            strstr((const char*)ca_cert, "-----BEGIN CERTIFICATE-----") != NULL);
}

/* ============================================
 * Cryptographic Functions
 * ============================================ */

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

/* ============================================
 * Status Functions
 * ============================================ */

bool security_is_healthy(void)
{
    // Check if NVS is accessible
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return false;
    }
    nvs_close(handle);
    
    return true;
}

esp_err_t security_get_status(uint8_t *certs_configured, bool *tls_ready)
{
    if (certs_configured) {
        *certs_configured = 0;
        if (security_has_certificate(SEC_CERT_TYPE_CA)) (*certs_configured)++;
        if (security_has_certificate(SEC_CERT_TYPE_CLIENT)) (*certs_configured)++;
        if (security_has_certificate(SEC_CERT_TYPE_CLIENT_KEY)) (*certs_configured)++;
        if (security_has_certificate(SEC_CERT_TYPE_SERVER)) (*certs_configured)++;
    }
    
    if (tls_ready) {
        // TLS is ready if we have at least CA cert
        *tls_ready = security_has_certificate(SEC_CERT_TYPE_CA);
    }
    
    return ESP_OK;
}

esp_err_t security_manager_deinit(void)
{
    ESP_LOGI(TAG, "Security manager deinitialized");
    return ESP_OK;
}
