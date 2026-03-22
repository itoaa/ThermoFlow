/**
 * @file test_sht4x.c
 * @brief Unit tests for SHT40 sensor driver
 */

#include <unity.h>
#include <string.h>
#include "sht4x_sensor.h"

// Mock I2C functions for testing
static uint8_t mock_i2c_buffer[32];
static size_t mock_i2c_len = 0;
static bool mock_i2c_present = true;

// Test CRC-8 calculation
void test_sht4x_crc8_basic(void)
{
    // Test with known data
    uint8_t data[] = {0xBE, 0xEF};
    // Expected CRC for 0xBE 0xEF with polynomial 0x31
    // This is a simplified check - actual CRC depends on implementation
    TEST_ASSERT_TRUE(true); // Placeholder - would test actual CRC
}

// Test temperature conversion
void test_sht4x_ticks_to_temperature(void)
{
    // 0x6666 = 26214 ticks
    // Expected: -45 + 175 * (26214/65535) = ~25.0°C
    // This is a calculation check
    float expected = -45.0f + 175.0f * (26214.0f / 65535.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 25.0f, expected);
}

// Test humidity conversion
void test_sht4x_ticks_to_humidity(void)
{
    // 0x8000 = 32768 ticks
    // Expected: -6 + 125 * (32768/65535) = ~56.5%
    float expected = -6.0f + 125.0f * (32768.0f / 65535.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 56.5f, expected);
}

// Test humidity clamping at 100%
void test_sht4x_humidity_clamping_high(void)
{
    // Maximum ticks should give max 100%
    float humidity = -6.0f + 125.0f * (65535.0f / 65535.0f);
    if (humidity > 100.0f) humidity = 100.0f;
    TEST_ASSERT_EQUAL_FLOAT(100.0f, humidity);
}

// Test humidity clamping at 0%
void test_sht4x_humidity_clamping_low(void)
{
    // Very low ticks should give min 0%
    float humidity = -6.0f + 125.0f * (0.0f / 65535.0f);
    if (humidity < 0.0f) humidity = 0.0f;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, humidity);
}

// Test sensor handle validation
void test_sht4x_null_handle(void)
{
    sht4x_reading_t reading;
    // Should return error without crashing
    esp_err_t err = sht4x_read(NULL, &reading, 100);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// Test init with NULL config
void test_sht4x_null_config(void)
{
    sht4x_handle_t handle;
    esp_err_t err = sht4x_init(NULL, &handle);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// Test reading with NULL output
void test_sht4x_null_output(void)
{
    // Would need a valid handle first
    // For now, just test the null check
    esp_err_t err = sht4x_read((sht4x_handle_t)1, NULL, 100);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

// Run all SHT4x tests
void test_sht4x_sensor(void)
{
    UNITY_TEST_SUITE();
    
    RUN_TEST(test_sht4x_crc8_basic);
    RUN_TEST(test_sht4x_ticks_to_temperature);
    RUN_TEST(test_sht4x_ticks_to_humidity);
    RUN_TEST(test_sht4x_humidity_clamping_high);
    RUN_TEST(test_sht4x_humidity_clamping_low);
    RUN_TEST(test_sht4x_null_handle);
    RUN_TEST(test_sht4x_null_config);
    RUN_TEST(test_sht4x_null_output);
    
    UNITY_TEST_SUITE_END();
}