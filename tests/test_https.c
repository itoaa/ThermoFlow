/**
 * @file test_https.c
 * @brief HTTPS Web Server Unit Tests
 * 
 * Test suite for SEC-026: ThermoFlow HTTPS Web Server Implementation
 * 
 * @security SEC-026
 */

#include "unity.h"
#include "web_server.h"
#include "cert_manager.h"
#include "esp_log.h"

static const char *TAG = "TEST_HTTPS";

/* ============================================
 * Certificate Manager Tests
 * ============================================ */

TEST_CASE("cert_manager_init", "[cert_manager]")
{
    ESP_LOGI(TAG, "Testing cert_manager_init...");
    
    cert_manager_config_t config = {
        .validity_days = 365,
        .renewal_days_before = 30,
        .key_size = 2048,
        .enable_auto_renewal = true
    };
    strncpy(config.device_id, "TestDevice", sizeof(config.device_id));
    strncpy(config.org, "TestOrg", sizeof(config.org));
    strncpy(config.country, "SE", sizeof(config.country));
    
    esp_err_t ret = cert_manager_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    cert_manager_config_t read_config;
    ret = cert_manager_get_config(&read_config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(365, read_config.validity_days);
    TEST_ASSERT_EQUAL(30, read_config.renewal_days_before);
    TEST_ASSERT_EQUAL(2048, read_config.key_size);
}

TEST_CASE("cert_manager_provision_self_signed", "[cert_manager]")
{
    ESP_LOGI(TAG, "Testing certificate provisioning...");
    
    // Initialize
    esp_err_t ret = cert_manager_init(NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Delete any existing certificate
    cert_manager_delete();
    
    // Provision self-signed certificate
    ret = cert_manager_provision(false);  // Self-signed
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Verify certificate exists
    TEST_ASSERT_TRUE(cert_manager_has_certificate());
    
    // Load certificate
    cert_manager_cert_t cert;
    ret = cert_manager_load(&cert);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(cert.cert_len > 0);
    TEST_ASSERT_TRUE(cert.key_len > 0);
    TEST_ASSERT_TRUE(cert.days_remaining > 0);
    
    ESP_LOGI(TAG, "Certificate valid for %d days", cert.days_remaining);
}

TEST_CASE("cert_manager_status", "[cert_manager]")
{
    ESP_LOGI(TAG, "Testing certificate status...");
    
    esp_err_t ret = cert_manager_init(NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Provision if needed
    if (!cert_manager_has_certificate()) {
        ret = cert_manager_provision(false);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
    
    // Get status
    cert_manager_status_t status;
    ret = cert_manager_get_status(&status);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(status.has_certificate);
    TEST_ASSERT_FALSE(status.expired);
    TEST_ASSERT_TRUE(status.days_remaining > 0);
    TEST_ASSERT_TRUE(strlen(status.fingerprint) > 0);
    
    ESP_LOGI(TAG, "Certificate fingerprint: %s", status.fingerprint);
    ESP_LOGI(TAG, "Days remaining: %d", status.days_remaining);
}

TEST_CASE("cert_manager_days_remaining", "[cert_manager]")
{
    ESP_LOGI(TAG, "Testing days remaining...");
    
    cert_manager_init(NULL);
    
    // Ensure certificate exists
    if (!cert_manager_has_certificate()) {
        cert_manager_provision(false);
    }
    
    int days = cert_manager_days_remaining();
    TEST_ASSERT_TRUE(days > 0);
    
    ESP_LOGI(TAG, "Days remaining: %d", days);
}

TEST_CASE("cert_manager_delete", "[cert_manager]")
{
    ESP_LOGI(TAG, "Testing certificate deletion...");
    
    cert_manager_init(NULL);
    
    // Ensure certificate exists
    if (!cert_manager_has_certificate()) {
        cert_manager_provision(false);
    }
    
    // Delete
    esp_err_t ret = cert_manager_delete();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Verify deleted
    TEST_ASSERT_FALSE(cert_manager_has_certificate());
}

/* ============================================
 * Web Server Configuration Tests
 * ============================================ */

TEST_CASE("web_server_https_config", "[web_server]")
{
    ESP_LOGI(TAG, "Testing HTTPS configuration...");
    
    https_config_t config;
    esp_err_t ret = web_server_get_default_https_config(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    TEST_ASSERT_TRUE(config.use_https);
    TEST_ASSERT_EQUAL(443, config.https_port);
    TEST_ASSERT_EQUAL(80, config.http_port);
    TEST_ASSERT_TRUE(config.enable_http_redirect);
    TEST_ASSERT_TRUE(config.enable_hsts);
    TEST_ASSERT_TRUE(config.hsts_max_age > 0);
    
    // Modify and set
    config.tls_1_3_only = true;
    ret = web_server_set_https_config(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Verify stored
    https_config_t read_config;
    ret = web_server_get_https_config(&read_config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(read_config.tls_1_3_only);
}

TEST_CASE("web_server_init", "[web_server]")
{
    ESP_LOGI(TAG, "Testing web server initialization...");
    
    esp_err_t ret = web_server_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    ret = web_server_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

TEST_CASE("web_server_cert_info", "[web_server]")
{
    ESP_LOGI(TAG, "Testing certificate info...");
    
    web_server_init();
    
    // Provision certificate if needed
    if (!web_server_has_certificates()) {
        cert_manager_init(NULL);
        cert_manager_provision(false);
    }
    
    web_server_cert_info_t info;
    esp_err_t ret = web_server_get_cert_info(&info);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    ESP_LOGI(TAG, "Has server cert: %s", info.has_server_cert ? "yes" : "no");
    ESP_LOGI(TAG, "Has server key: %s", info.has_server_key ? "yes" : "no");
    ESP_LOGI(TAG, "Days until expiry: %d", info.days_until_expiry);
}

/* ============================================
 * CSR Generation Test
 * ============================================ */

TEST_CASE("cert_manager_generate_csr", "[cert_manager]")
{
    ESP_LOGI(TAG, "Testing CSR generation...");
    
    esp_err_t ret = cert_manager_init(NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    char csr[4096];
    ret = cert_manager_generate_csr(csr, sizeof(csr));
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(strlen(csr) > 0);
    
    // Verify CSR format
    TEST_ASSERT_NOT_NULL(strstr(csr, "BEGIN CERTIFICATE REQUEST"));
    TEST_ASSERT_NOT_NULL(strstr(csr, "END CERTIFICATE REQUEST"));
    
    ESP_LOGI(TAG, "CSR generated successfully (%zu bytes)", strlen(csr));
}

/* ============================================
 * Integration Test
 * ============================================ */

TEST_CASE("https_full_lifecycle", "[https][integration]")
{
    ESP_LOGI(TAG, "=== HTTPS Full Lifecycle Test ===");
    
    // 1. Initialize components
    esp_err_t ret = cert_manager_init(NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    ret = web_server_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // 2. Delete any existing certificate
    cert_manager_delete();
    TEST_ASSERT_FALSE(cert_manager_has_certificate());
    
    // 3. Provision new certificate
    ret = cert_manager_provision(false);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(cert_manager_has_certificate());
    
    // 4. Load certificate
    cert_manager_cert_t cert;
    ret = cert_manager_load(&cert);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    ESP_LOGI(TAG, "Certificate loaded: %d days remaining", cert.days_remaining);
    
    // 5. Check status
    cert_manager_status_t status;
    ret = cert_manager_get_status(&status);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(status.has_certificate);
    TEST_ASSERT_FALSE(status.expired);
    ESP_LOGI(TAG, "Certificate fingerprint: %s", status.fingerprint);
    
    // 6. Check renewal (should not need it)
    ret = cert_manager_check_and_renew();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    ESP_LOGI(TAG, "=== HTTPS Full Lifecycle Test PASSED ===");
}
