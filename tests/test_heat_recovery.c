/**
 * @file test_heat_recovery.c
 * @brief Unity tests for heat recovery calculations
 */

#include <math.h>
#include <unity.h>
#include "heat_recovery.h"

void test_ftx_validate_sensor_rejects_nan(void)
{
    TEST_ASSERT_FALSE(ftx_validate_sensor(NAN, 50.0f));
    TEST_ASSERT_FALSE(ftx_validate_sensor(20.0f, NAN));
}

void test_ftx_validate_sensor_accepts_valid(void)
{
    TEST_ASSERT_TRUE(ftx_validate_sensor(20.0f, 50.0f));
}

void test_ftx_calc_efficiency(void)
{
    float eff = ftx_calc_efficiency(0.0f, 18.0f, 22.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 81.8f, eff);
}

void test_ftx_frost_hysteresis(void)
{
    frost_protection_state_t state = FROST_STATE_IDLE;
    state = ftx_check_frost_hysteresis(-1.0f, 80.0f, state);
    TEST_ASSERT_EQUAL(FROST_STATE_WARNING, state);
}

void test_heat_recovery_suite(void)
{
    RUN_TEST(test_ftx_validate_sensor_rejects_nan);
    RUN_TEST(test_ftx_validate_sensor_accepts_valid);
    RUN_TEST(test_ftx_calc_efficiency);
    RUN_TEST(test_ftx_frost_hysteresis);
}