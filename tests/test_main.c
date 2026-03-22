/**
 * @file test_main.c
 * @brief Unity Test Runner for ThermoFlow
 */

#include <unity.h>
#include "esp_log.h"

// External test suites
void test_sht4x_sensor(void);
void test_fan_controller(void);
void test_anti_condensation(void);

static const char *TAG = "TEST";

void setUp(void)
{
    // Set up before each test
}

void tearDown(void)
{
    // Clean up after each test
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ThermoFlow Unit Tests");
    
    UNITY_BEGIN();
    
    // Run test suites
    test_sht4x_sensor();
    test_fan_controller();
    test_anti_condensation();
    
    UNITY_END();
    
    // Exit with test results
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}