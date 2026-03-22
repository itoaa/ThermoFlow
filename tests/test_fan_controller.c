/**
 * @file test_fan_controller.c
 * @brief Unit tests for Fan Controller
 */

#include <unity.h>
#include "fan_controller.h"

// Test fail-safe mode on init
void test_fan_fail_safe_on_init(void)
{
    // After init, fail-safe should be active
    bool failsafe = fan_controller_is_failsafe();
    TEST_ASSERT_TRUE(failsafe);
}

// Test speed calculation - below min temp
void test_fan_calc_speed_below_min(void)
{
    uint8_t speed = fan_controller_calc_speed_from_temp(18.0f, 20.0f, 20.0f, 30.0f);
    TEST_ASSERT_EQUAL_UINT8(0, speed);
}

// Test speed calculation - at min temp
void test_fan_calc_speed_at_min(void)
{
    uint8_t speed = fan_controller_calc_speed_from_temp(20.0f, 20.0f, 20.0f, 30.0f);
    TEST_ASSERT_EQUAL_UINT8(0, speed);
}

// Test speed calculation - at max temp
void test_fan_calc_speed_at_max(void)
{
    uint8_t speed = fan_controller_calc_speed_from_temp(30.0f, 20.0f, 20.0f, 30.0f);
    TEST_ASSERT_EQUAL_UINT8(100, speed);
}

// Test speed calculation - above max temp
void test_fan_calc_speed_above_max(void)
{
    uint8_t speed = fan_controller_calc_speed_from_temp(35.0f, 20.0f, 20.0f, 30.0f);
    TEST_ASSERT_EQUAL_UINT8(100, speed);
}

// Test speed calculation - mid range
void test_fan_calc_speed_mid_range(void)
{
    // 25°C is halfway between 20°C and 30°C
    uint8_t speed = fan_controller_calc_speed_from_temp(25.0f, 20.0f, 20.0f, 30.0f);
    TEST_ASSERT_EQUAL_UINT8(50, speed);
}

// Test speed calculation - 25% point
void test_fan_calc_speed_quarter(void)
{
    // 22.5°C is 25% between 20°C and 30°C
    uint8_t speed = fan_controller_calc_speed_from_temp(22.5f, 20.0f, 20.0f, 30.0f);
    TEST_ASSERT_EQUAL_UINT8(25, speed);
}

// Test speed calculation - 75% point
void test_fan_calc_speed_three_quarters(void)
{
    // 27.5°C is 75% between 20°C and 30°C
    uint8_t speed = fan_controller_calc_speed_from_temp(27.5f, 20.0f, 20.0f, 30.0f);
    TEST_ASSERT_EQUAL_UINT8(75, speed);
}

// Test fan speed enum to percent
void test_fan_speed_enum_values(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, fan_speed_to_percent(FAN_SPEED_OFF));
    TEST_ASSERT_EQUAL_UINT8(30, fan_speed_to_percent(FAN_SPEED_LOW));
    TEST_ASSERT_EQUAL_UINT8(60, fan_speed_to_percent(FAN_SPEED_MEDIUM));
    TEST_ASSERT_EQUAL_UINT8(100, fan_speed_to_percent(FAN_SPEED_HIGH));
    TEST_ASSERT_EQUAL_UINT8(100, fan_speed_to_percent(FAN_SPEED_MAX));
}

// Test fail-safe exit and re-enter
void test_fan_failsafe_toggle(void)
{
    // Enter fail-safe
    fan_controller_enter_failsafe("test");
    TEST_ASSERT_TRUE(fan_controller_is_failsafe());
    
    // Exit fail-safe
    fan_controller_exit_failsafe();
    TEST_ASSERT_FALSE(fan_controller_is_failsafe());
    
    // Re-enter
    fan_controller_enter_failsafe("test again");
    TEST_ASSERT_TRUE(fan_controller_is_failsafe());
}

// Run all fan controller tests
void test_fan_controller(void)
{
    UNITY_TEST_SUITE();
    
    // Note: These tests assume fan_controller is initialized
    // In real tests, would need proper setup
    
    RUN_TEST(test_fan_fail_safe_on_init);
    RUN_TEST(test_fan_calc_speed_below_min);
    RUN_TEST(test_fan_calc_speed_at_min);
    RUN_TEST(test_fan_calc_speed_at_max);
    RUN_TEST(test_fan_calc_speed_above_max);
    RUN_TEST(test_fan_calc_speed_mid_range);
    RUN_TEST(test_fan_calc_speed_quarter);
    RUN_TEST(test_fan_calc_speed_three_quarters);
    RUN_TEST(test_fan_speed_enum_values);
    RUN_TEST(test_fan_failsafe_toggle);
    
    UNITY_TEST_SUITE_END();
}