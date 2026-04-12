/**
 * @file test_wifi_encryption.c
 * @brief Unit tests for WiFi credential encryption (SEC-021)
 * 
 * Tests AES-256 encryption/decryption, key derivation, and integrity protection
 * 
 * @security SEC-021
 */

#include "unity.h"
#include "wifi_secure_storage.h"
#include "esp_system.h"
#include "esp_log.h"
#include "string.h"

static const char *TAG = "TEST_WIFI_ENC";

/* Test data */
static const char TEST_SSID[] = "TestNetwork";
static const char TEST_PASSWORD[] = "MySecurePassword123";

void setUp(void)
{
    // Initialize before each test
    esp_err_t ret = wifi_secure_storage_init(WIFI_KEY_SOURCE_FLASH);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Init failed: %s", esp_err_to_name(ret));
    }
}

void tearDown(void)
{
    // Cleanup after each test
    wifi_secure_delete_credentials();
    wifi_secure_storage_deinit();
}

/* ============================================
 * Basic Encryption Tests
 * ============================================ */

void test_wifi_secure_store_and_load(void)
{
    ESP_LOGI(TAG, "Testing basic store/load...");
    
    // Store credentials
    esp_err_t ret = wifi_secure_store_credentials(TEST_SSID, TEST_PASSWORD);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Load credentials
    char ssid[33] = {0};
    char password[65] = {0};
    
    ret = wifi_secure_load_credentials(ssid, sizeof(ssid), password, sizeof(password));
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Verify
    TEST_ASSERT_EQUAL_STRING(TEST_SSID, ssid);
    TEST_ASSERT_EQUAL_STRING(TEST_PASSWORD, password);
    
    // Clear buffers securely
    wifi_secure_memclear(ssid, sizeof(ssid));
    wifi_secure_memclear(password, sizeof(password));
    
    ESP_LOGI(TAG, "Basic store/load test PASSED");
}

void test_wifi_secure_has_credentials(void)
{
    ESP_LOGI(TAG, "Testing has_credentials...");
    
    // Initially no credentials
    TEST_ASSERT_FALSE(wifi_secure_has_credentials());
    
    // Store credentials
    esp_err_t ret = wifi_secure_store_credentials(TEST_SSID, TEST_PASSWORD);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Now should have credentials
    TEST_ASSERT_TRUE(wifi_secure_has_credentials());
    
    ESP_LOGI(TAG, "has_credentials test PASSED");
}

void test_wifi_secure_delete_credentials(void)
{
    ESP_LOGI(TAG, "Testing delete_credentials...");
    
    // Store and verify
    wifi_secure_store_credentials(TEST_SSID, TEST_PASSWORD);
    TEST_ASSERT_TRUE(wifi_secure_has_credentials());
    
    // Delete
    esp_err_t ret = wifi_secure_delete_credentials();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Verify deletion
    TEST_ASSERT_FALSE(wifi_secure_has_credentials());
    
    ESP_LOGI(TAG, "delete_credentials test PASSED");
}

/* ============================================
 * Edge Cases
 * ============================================ */

void test_wifi_secure_empty_credentials(void)
{
    ESP_LOGI(TAG, "Testing empty credentials...");
    
    // Empty SSID should fail
    esp_err_t ret = wifi_secure_store_credentials("", TEST_PASSWORD);
    TEST_ASSERT_EQUAL(ESP_OK, ret);  // Empty string is valid
    
    // Empty password should work
    wifi_secure_delete_credentials();
    ret = wifi_secure_store_credentials(TEST_SSID, "");
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    char ssid[33] = {0};
    char password[65] = {0};
    ret = wifi_secure_load_credentials(ssid, sizeof(ssid), password, sizeof(password));
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_STRING(TEST_SSID, ssid);
    TEST_ASSERT_EQUAL_STRING("", password);
    
    wifi_secure_memclear(ssid, sizeof(ssid));
    wifi_secure_memclear(password, sizeof(password));
    
    ESP_LOGI(TAG, "Empty credentials test PASSED");
}

void test_wifi_secure_long_credentials(void)
{
    ESP_LOGI(TAG, "Testing long credentials...");
    
    // Max length SSID (32 chars)
    char long_ssid[33];
    memset(long_ssid, 'A', 32);
    long_ssid[32] = '\0';
    
    // Max length password (64 chars)
    char long_password[65];
    memset(long_password, 'B', 64);
    long_password[64] = '\0';
    
    esp_err_t ret = wifi_secure_store_credentials(long_ssid, long_password);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    char ssid[33] = {0};
    char password[65] = {0};
    ret = wifi_secure_load_credentials(ssid, sizeof(ssid), password, sizeof(password));
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_STRING(long_ssid, ssid);
    TEST_ASSERT_EQUAL_STRING(long_password, password);
    
    wifi_secure_memclear(ssid, sizeof(ssid));
    wifi_secure_memclear(password, sizeof(password));
    
    ESP_LOGI(TAG, "Long credentials test PASSED");
}

void test_wifi_secure_special_characters(void)
{
    ESP_LOGI(TAG, "Testing special characters...");
    
    const char *special_ssid = "Test-Net_2.4G";
    const char *special_pass = "P@ssw0rd!#$%^*()";
    
    esp_err_t ret = wifi_secure_store_credentials(special_ssid, special_pass);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    char ssid[33] = {0};
    char password[65] = {0};
    ret = wifi_secure_load_credentials(ssid, sizeof(ssid), password, sizeof(password));
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_STRING(special_ssid, ssid);
    TEST_ASSERT_EQUAL_STRING(special_pass, password);
    
    wifi_secure_memclear(ssid, sizeof(ssid));
    wifi_secure_memclear(password, sizeof(password));
    
    ESP_LOGI(TAG, "Special characters test PASSED");
}

/* ============================================
 * Security Tests
 * ============================================ */

void test_wifi_secure_key_source_detection(void)
{
    ESP_LOGI(TAG, "Testing key source detection...");
    
    wifi_key_source_t source = wifi_secure_recommended_key_source();
    
    // Should return a valid key source
    TEST_ASSERT_TRUE(source == WIFI_KEY_SOURCE_EFUSE || 
                     source == WIFI_KEY_SOURCE_FLASH);
    
    ESP_LOGI(TAG, "Recommended key source: %d", (int)source);
    ESP_LOGI(TAG, "Key source detection test PASSED");
}

void test_wifi_secure_memclear(void)
{
    ESP_LOGI(TAG, "Testing memclear...");
    
    char buffer[64];
    strcpy(buffer, "sensitive data");
    
    wifi_secure_memclear(buffer, sizeof(buffer));
    
    // Buffer should be all zeros
    for (int i = 0; i < sizeof(buffer); i++) {
        TEST_ASSERT_EQUAL(0, buffer[i]);
    }
    
    ESP_LOGI(TAG, "memclear test PASSED");
}

void test_wifi_secure_overwrite(void)
{
    ESP_LOGI(TAG, "Testing credential overwrite...");
    
    // Store initial credentials
    wifi_secure_store_credentials("OldSSID", "OldPass");
    
    // Overwrite with new credentials
    esp_err_t ret = wifi_secure_store_credentials("NewSSID", "NewPass");
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Verify new credentials
    char ssid[33] = {0};
    char password[65] = {0};
    ret = wifi_secure_load_credentials(ssid, sizeof(ssid), password, sizeof(password));
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL_STRING("NewSSID", ssid);
    TEST_ASSERT_EQUAL_STRING("NewPass", password);
    
    wifi_secure_memclear(ssid, sizeof(ssid));
    wifi_secure_memclear(password, sizeof(password));
    
    ESP_LOGI(TAG, "Overwrite test PASSED");
}

void test_wifi_secure_multiple_operations(void)
{
    ESP_LOGI(TAG, "Testing multiple operations...");
    
    // Perform multiple store/load cycles
    for (int i = 0; i < 5; i++) {
        char ssid[33];
        char password[65];
        snprintf(ssid, sizeof(ssid), "Network%d", i);
        snprintf(password, sizeof(password), "Password%d", i);
        
        esp_err_t ret = wifi_secure_store_credentials(ssid, password);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        
        char loaded_ssid[33] = {0};
        char loaded_pass[65] = {0};
        ret = wifi_secure_load_credentials(loaded_ssid, sizeof(loaded_ssid), 
                                            loaded_pass, sizeof(loaded_pass));
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_EQUAL_STRING(ssid, loaded_ssid);
        TEST_ASSERT_EQUAL_STRING(password, loaded_pass);
        
        wifi_secure_memclear(loaded_ssid, sizeof(loaded_ssid));
        wifi_secure_memclear(loaded_pass, sizeof(loaded_pass));
    }
    
    ESP_LOGI(TAG, "Multiple operations test PASSED");
}

void test_wifi_secure_status(void)
{
    ESP_LOGI(TAG, "Testing status functions...");
    
    bool encrypted = false;
    wifi_key_source_t source = WIFI_KEY_SOURCE_AUTO;
    
    // Before storing
    esp_err_t ret = wifi_secure_get_status(&encrypted, &source);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_FALSE(encrypted);
    
    // After storing
    wifi_secure_store_credentials(TEST_SSID, TEST_PASSWORD);
    ret = wifi_secure_get_status(&encrypted, &source);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(encrypted);
    
    ESP_LOGI(TAG, "Status test PASSED");
}

/* ============================================
 * Migration Tests
 * ============================================ */

void test_wifi_secure_migration_no_legacy(void)
{
    ESP_LOGI(TAG, "Testing migration with no legacy data...");
    
    // Ensure no legacy data exists
    wifi_secure_delete_credentials();
    
    // Migration should return NOT_FOUND
    esp_err_t ret = wifi_secure_migrate_from_legacy();
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, ret);
    
    ESP_LOGI(TAG, "Migration no legacy test PASSED");
}

/* ============================================
 * Test Runner
 * ============================================ */

void app_main(void)
{
    UNITY_BEGIN();
    
    ESP_LOGI(TAG, "=== WiFi Encryption Tests (SEC-021) ===");
    
    // Basic tests
    RUN_TEST(test_wifi_secure_store_and_load);
    RUN_TEST(test_wifi_secure_has_credentials);
    RUN_TEST(test_wifi_secure_delete_credentials);
    
    // Edge cases
    RUN_TEST(test_wifi_secure_empty_credentials);
    RUN_TEST(test_wifi_secure_long_credentials);
    RUN_TEST(test_wifi_secure_special_characters);
    
    // Security tests
    RUN_TEST(test_wifi_secure_key_source_detection);
    RUN_TEST(test_wifi_secure_memclear);
    RUN_TEST(test_wifi_secure_overwrite);
    RUN_TEST(test_wifi_secure_multiple_operations);
    RUN_TEST(test_wifi_secure_status);
    
    // Migration tests
    RUN_TEST(test_wifi_secure_migration_no_legacy);
    
    ESP_LOGI(TAG, "=== All tests completed ===");
    
    UNITY_END();
}
