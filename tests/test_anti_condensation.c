/**
 * @file test_anti_condensation.c
 * @brief Unit tests for Anti-Condensation Protection
 */

#include <unity.h>
#include "anti_condensation.h"

// Test threshold detection
void test_anti_condensation_threshold_trigger(void)
{
    // Should trigger alert at exactly 90%
    esp_err_t err = anti_condensation_check(90.0f, 25.0f);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, err);  // Alert triggered
    TEST_ASSERT_TRUE(anti_condensation_is_active());
}

// Test hysteresis - should not clear at 89%
void test_anti_condensation_hysteresis_not_clear(void)
{
    // First trigger alert
    anti_condensation_check(95.0f, 25.0f);
    TEST_ASSERT_TRUE(anti_condensation_is_active());
    
    // At 89%, should still be active (threshold - hysteresis = 85%)
    anti_condensation_check(89.0f, 25.0f);
    TEST_ASSERT_TRUE(anti_condensation_is_active());
}

// Test hysteresis - should clear at 85%
void test_anti_condensation_hysteresis_clear(void)
{
    // First trigger alert
    anti_condensation_check(95.0f, 25.0f);
    TEST_ASSERT_TRUE(anti_condensation_is_active());
    
    // At 85%, should clear (threshold - hysteresis = 85%)
    esp_err_t err = anti_condensation_check(85.0f, 25.0f);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_FALSE(anti_condensation_is_active());
}

// Test below threshold - no alert
void test_anti_condensation_below_threshold(void)
{
    // Reset state first
    anti_condensation_clear_alert();
    
    esp_err_t err = anti_condensation_check(80.0f, 25.0f);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_FALSE(anti_condensation_is_active());
}

// Test manual clear
void test_anti_condensation_manual_clear(void)
{
    // Trigger alert
    anti_condensation_check(95.0f, 25.0f);
    TEST_ASSERT_TRUE(anti_condensation_is_active());
    
    // Manual clear
    anti_condensation_clear_alert();
    TEST_ASSERT_FALSE(anti_condensation_is_active());
}

// Test status retrieval
void test_anti_condensation_status(void)
{
    anti_condensation_check(92.0f, 25.0f);
    
    float current_rh, threshold;
    esp_err_t err = anti_condensation_get_status(&current_rh, &threshold);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 92.0f, current_rh);
}

// Run all anti-condensation tests
void test_anti_condensation(void)
{
    UNITY_TEST_SUITE();
    
    // Initialize before tests
    anti_condensation_init();
    
    RUN_TEST(test_anti_condensation_threshold_trigger);
    RUN_TEST(test_anti_condensation_hysteresis_not_clear);
    RUN_TEST(test_anti_condensation_hysteresis_clear);
    RUN_TEST(test_anti_condensation_below_threshold);
    RUN_TEST(test_anti_condensation_manual_clear);
    RUN_TEST(test_anti_condensation_status);
    
    UNITY_TEST_SUITE_END();
}