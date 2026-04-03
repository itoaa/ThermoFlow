/**
 * @file main.c
 * @brief ThermoFlow - ESP32-S3 Climate Monitoring and Control System
 * 
 * Main application entry point for the climate monitoring system.
 * Implements IEC 62443 SL-2 security level compliance.
 * 
 * @author Ola Andersson
 * @version 1.0.0
 * @date 2026-03-22
 * 
 * @copyright Copyright (c) 2026
 * 
 * @section changelog Change Log
 * - 1.0.0 (2026-03-22): Initial release with core functionality
 *   - Fan control with fail-safe
 *   - Anti-condensation protection
 *   - Simulated sensor readings (placeholder for hardware)
 */

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>      /* FreeRTOS core functions */
#include <freertos/task.h>             /* Task management */
#include <freertos/semphr.h>          /* Semaphore/mutex support */
#include <esp_log.h>                  /* ESP-IDF logging */
#include <esp_system.h>               /* System info, watchdog */
#include <esp_chip_info.h>            /* Chip info (cores, revision) */
#include <esp_random.h>               /* Hardware RNG */
#include <esp_task_wdt.h>             /* Task watchdog timer */
#include <nvs_flash.h>               /* Non-volatile storage */

#include "fan_controller.h"           /* PWM fan control interface */
#include "anti_condensation.h"          /* RH monitoring and alerts */
#include "wifi_manager.h"             /* WiFi configuration and AP mode */

/* Compile-time version string */
#define THERMOFLOW_VERSION    "1.0.0"
#define THERMOFLOW_VERSION_MAJOR  1
#define THERMOFLOW_VERSION_MINOR  0
#define THERMOFLOW_VERSION_PATCH  0

/* Logging tag - appears in all log messages from this file */
static const char *TAG = "THERMOFLOW";

/* Task configuration constants */
#define CONTROL_TASK_STACK_SIZE    4096u     /* bytes */
#define CONTROL_TASK_PRIORITY      5         /* 1-24, higher = more urgent */
#define CONTROL_TASK_CORE          tskNO_AFFINITY  /* Run on any core */
#define MAIN_LOOP_INTERVAL_MS      5000u     /* 5 seconds between status logs */
#define CONTROL_LOOP_INTERVAL_MS   1000u     /* 1 second control loop */

/* Sensor simulation constants (placeholder until hardware integration) */
#define SIM_MIN_TEMP_C      20.0f    /* Minimum simulated temperature */
#define SIM_MAX_TEMP_C      35.0f    /* Maximum simulated temperature */
#define SIM_MIN_RH_PERCENT  40.0f    /* Minimum simulated humidity */
#define SIM_MAX_RH_PERCENT  95.0f    /* Maximum simulated humidity */

/**
 * @brief Simulated sensor readings (placeholder for actual SHT40)
 * 
 * Generates realistic but synthetic temperature and humidity values
 * for testing the control logic before hardware integration.
 * 
 * @param[out] temp_c Temperature in Celsius
 * @param[out] rh_percent Relative humidity percentage
 */
static void simulate_sensor_readings(float *temp_c, float *rh_percent)
{
    /* Get random values from hardware RNG (secure, unpredictable) */
    uint32_t rand_val = esp_random();
    
    /* Scale to temperature range */
    float temp_norm = (float)(rand_val & 0xFFFF) / 65535.0f;
    *temp_c = SIM_MIN_TEMP_C + temp_norm * (SIM_MAX_TEMP_C - SIM_MIN_TEMP_C);
    
    /* Get another random value for humidity */
    rand_val = esp_random();
    float rh_norm = (float)(rand_val & 0xFFFF) / 65535.0f;
    *rh_percent = SIM_MIN_RH_PERCENT + rh_norm * (SIM_MAX_RH_PERCENT - SIM_MIN_RH_PERCENT);
}

/* Global mutex for thread-safe sensor data access */
static SemaphoreHandle_t g_sensor_mutex = NULL;

/**
 * @brief Initialize Non-Volatile Storage
 * 
 * Required for persisting configuration and calibration data.
 * Handles first-boot initialization automatically.
 * 
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition truncated or version mismatch - erase and retry */
        ESP_LOGW(TAG, "NVS partition needs erase, reformatting...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

/**
 * @brief Main control loop task
 * 
 * Runs continuously to:
 * - Read sensor values (simulated for now)
 * - Check condensation risk
 * - Adjust fan speeds based on conditions
 * - Log status periodically
 * 
 * Designed to be thread-safe and resilient to errors.
 * 
 * @param[in] pvParameters Task parameters (unused)
 */
static void control_task(void *pvParameters)
{
    (void)pvParameters;  /* Unused parameter */
    
    ESP_LOGI(TAG, "Control task started on core %d", xPortGetCoreID());
    
    /* Add task to watchdog - ensures task is responsive */
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    
    /* Current sensor readings */
    float temp_c = 25.0f;
    float rh_percent = 50.0f;
    
    /* Control loop */
    while (1) {
        /* Feed watchdog to prevent reset */
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        
        /* Simulate sensor readings (replace with actual SHT40 driver) */
        simulate_sensor_readings(&temp_c, &rh_percent);
        
        /* Thread-safe sensor data update */
        if (g_sensor_mutex != NULL) {
            if (xSemaphoreTake(g_sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                /* In a real implementation, update global sensor struct here */
                xSemaphoreGive(g_sensor_mutex);
            }
        }
        
        /* Check condensation risk (IEC 62443 SR-010) */
        esp_err_t err = anti_condensation_check(rh_percent, temp_c);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Anti-condensation check failed: %s", esp_err_to_name(err));
        }
        
        /* Adjust fan speed based on temperature and condensation risk */
        if (anti_condensation_is_active()) {
            /* High condensation risk - maximum ventilation */
            fan_controller_set_mode(FAN_1, FAN_MODE_MANUAL);
            fan_controller_set_speed(FAN_1, 100);
            ESP_LOGW(TAG, "CONDENSATION ALERT: Fans at 100%% (RH=%.1f%%)", rh_percent);
        } else {
            /* Normal operation - moderate fan speed based on temp */
            uint8_t fan_speed = (uint8_t)((temp_c - SIM_MIN_TEMP_C) / 
                                         (SIM_MAX_TEMP_C - SIM_MIN_TEMP_C) * 50);
            if (fan_speed < 10) fan_speed = 10;  /* Minimum 10% */
            if (fan_speed > 60) fan_speed = 60; /* Normal max 60% */
            fan_controller_set_mode(FAN_1, FAN_MODE_MANUAL);
            fan_controller_set_speed(FAN_1, fan_speed);
        }
        
        /* Run WiFi manager */
        wifi_manager_run();
        
        /* Log status periodically */
        static uint32_t last_log_time = 0;
        uint32_t now = xTaskGetTickCount();
        if ((now - last_log_time) * portTICK_PERIOD_MS >= MAIN_LOOP_INTERVAL_MS) {
            ESP_LOGI(TAG, "Status: Temp=%.1f°C, RH=%.1f%%, Fan=%u%%, CondAlert=%s",
                     temp_c, rh_percent,
                     fan_controller_get_speed(FAN_1),
                     anti_condensation_is_active() ? "YES" : "no");
            last_log_time = now;
        }
        
        /* Yield to other tasks - 1 second control loop */
        vTaskDelay(pdMS_TO_TICKS(CONTROL_LOOP_INTERVAL_MS));
    }
}

/**
 * @brief Application entry point
 * 
 * Initializes all subsystems and starts the control task.
 * This function never returns - FreeRTOS scheduler takes over.
 */
void app_main(void)
{
    /* Print startup banner */
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ThermoFlow Climate Control System");
    ESP_LOGI(TAG, "  Version: %s", THERMOFLOW_VERSION);
    ESP_LOGI(TAG, "  Target: ESP32-S3");
    ESP_LOGI(TAG, "  Security: IEC 62443 SL-2");
    ESP_LOGI(TAG, "========================================");
    
    /* Log system information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "CPU cores: %d, Revision: %d", 
             chip_info.cores, chip_info.revision);
    ESP_LOGI(TAG, "Free heap at start: %lu bytes", esp_get_free_heap_size());
    
    /* Step 1: Initialize NVS (required for configuration storage) */
    ESP_LOGI(TAG, "Initializing NVS...");
    ESP_ERROR_CHECK(init_nvs());
    ESP_LOGI(TAG, "NVS initialized");
    
    /* Step 2: Create mutex for thread-safe sensor data access */
    g_sensor_mutex = xSemaphoreCreateMutex();
    if (g_sensor_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor mutex - critical error");
        return;  /* Cannot continue without mutex */
    }
    
    /* Step 3: Initialize WiFi manager (AP mode or connect to configured WiFi) */
    ESP_LOGI(TAG, "Initializing WiFi manager...");
    esp_err_t ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi manager init failed: %s", esp_err_to_name(ret));
        /* Continue - system can work without WiFi */
    } else {
        ESP_LOGI(TAG, "WiFi manager initialized");
        ESP_LOGI(TAG, "AP Name: %s", wifi_manager_get_ap_name());
        ESP_LOGI(TAG, "WiFi Mode: %s", wifi_manager_is_ap_mode() ? "AP Mode (setup)" : 
                 wifi_manager_is_connected() ? "Connected to WiFi" : "Connecting...");
    }
    
    /* Step 4: Initialize fan controller (IEC 62443 SR-009 - fail-safe) */
    ESP_LOGI(TAG, "Initializing fan controller...");
    ret = fan_controller_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fan controller init failed: %s", esp_err_to_name(ret));
        /* Continue - fans not critical for basic operation */
    } else {
        ESP_LOGI(TAG, "Fan controller initialized");
    }
    
    /* Step 5: Initialize anti-condensation protection (IEC 62443 SR-010) */
    ESP_LOGI(TAG, "Initializing anti-condensation...");
    ret = anti_condensation_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Anti-condensation init failed: %s", esp_err_to_name(ret));
        /* Continue but log error */
    } else {
        ESP_LOGI(TAG, "Anti-condensation protection active");
    }
    
    /* Step 6: Create control task (main application logic) */
    ESP_LOGI(TAG, "Creating control task...");
    BaseType_t task_created = xTaskCreatePinnedToCore(
        control_task,              /* Task function */
        "control",                 /* Task name */
        CONTROL_TASK_STACK_SIZE,   /* Stack size */
        NULL,                      /* Parameters */
        CONTROL_TASK_PRIORITY,     /* Priority */
        NULL,                      /* Task handle (not needed) */
        CONTROL_TASK_CORE          /* CPU core */
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create control task - critical error");
        return;
    }
    
    ESP_LOGI(TAG, "Control task created successfully");
    ESP_LOGI(TAG, "System initialization complete - entering main loop");
    ESP_LOGI(TAG, "========================================");
    
    /* Main task can now exit - control_task handles everything */
    /* vTaskDelete(NULL) would delete this task, but we keep it for debug logging */
    
    while (1) {
        /* Optional: Monitor system health, handle events, etc. */
        vTaskDelay(pdMS_TO_TICKS(10000));  /* 10 second heartbeat */
        ESP_LOGI(TAG, "Main task heartbeat - heap: %lu bytes", esp_get_free_heap_size());
    }
}
