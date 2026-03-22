/**
 * @file main.c
 * @brief ThermoFlow - ESP32-S3 Climate Monitoring and Control System
 * 
 * Main entry point for the application.
 * 
 * @version 1.0.0
 * @date 2026-03-22
 * @author Ola Andersson
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "nvs_flash.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "driver/i2c.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

// ThermoFlow components
#include "thermoflow_config.h"
#include "sensor_manager.h"
#include "fan_controller.h"
#include "web_server.h"
#include "mqtt_client.h"
#include "ota_manager.h"
#include "display_manager.h"
#include "security_manager.h"
#include "anti_condensation.h"

// Logging tag
static const char *TAG = "THERMOFLOW";

// Task priorities
#define TASK_PRIORITY_SENSORS       (configMAX_PRIORITIES - 2)
#define TASK_PRIORITY_CONTROL       (configMAX_PRIORITIES - 3)
#define TASK_PRIORITY_WEB           (configMAX_PRIORITIES - 4)
#define TASK_PRIORITY_MQTT            (configMAX_PRIORITIES - 4)
#define TASK_PRIORITY_DISPLAY         (configMAX_PRIORITIES - 5)
#define TASK_PRIORITY_OTA             (configMAX_PRIORITIES - 6)

// Stack sizes
#define STACK_SIZE_SENSORS          (4096)
#define STACK_SIZE_CONTROL          (4096)
#define STACK_SIZE_WEB              (8192)
#define STACK_SIZE_MQTT             (8192)
#define STACK_SIZE_DISPLAY          (4096)

// Event group bits
#define EVENT_WIFI_CONNECTED        (1 << 0)
#define EVENT_WIFI_DISCONNECTED     (1 << 1)
#define EVENT_MQTT_CONNECTED        (1 << 2)
#define EVENT_MQTT_DISCONNECTED     (1 << 3)
#define EVENT_FAN_FAULT             (1 << 4)
#define EVENT_CONDENSATION_ALERT    (1 << 5)

// Global event group
static EventGroupHandle_t s_event_group = NULL;

// Global mutex for sensor data
static SemaphoreHandle_t s_sensor_mutex = NULL;

/**
 * @brief Initialize NVS (Non-Volatile Storage)
 */
static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

/**
 * @brief Initialize system components
 */
static esp_err_t init_system(void)
{
    ESP_LOGI(TAG, "Initializing ThermoFlow v%s", PROJECT_VERSION);
    
    // Create event group
    s_event_group = xEventGroupCreate();
    if (s_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }
    
    // Create sensor mutex
    s_sensor_mutex = xSemaphoreCreateMutex();
    if (s_sensor_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create sensor mutex");
        return ESP_FAIL;
    }
    
    // Initialize security (SR-002, SR-003)
    ESP_LOGI(TAG, "Initializing security...");
    ESP_ERROR_CHECK(security_manager_init());
    
    // Initialize I2C for sensors
    ESP_LOGI(TAG, "Initializing I2C...");
    ESP_ERROR_CHECK(sensor_manager_init_i2c());
    
    // Initialize fan controller (SR-009)
    ESP_LOGI(TAG, "Initializing fan controller...");
    ESP_ERROR_CHECK(fan_controller_init());
    
    // Initialize display
    ESP_LOGI(TAG, "Initializing display...");
    display_manager_init();
    
    // Initialize WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(wifi_manager_init());
    
    // Initialize MQTT (SR-003)
    ESP_LOGI(TAG, "Initializing MQTT...");
    ESP_ERROR_CHECK(mqtt_client_init());
    
    // Initialize web server
    ESP_LOGI(TAG, "Initializing web server...");
    ESP_ERROR_CHECK(web_server_init());
    
    // Initialize OTA (SR-011)
    ESP_LOGI(TAG, "Initializing OTA...");
    ESP_ERROR_CHECK(ota_manager_init());
    
    // Initialize anti-condensation (SR-010)
    ESP_LOGI(TAG, "Initializing anti-condensation...");
    ESP_ERROR_CHECK(anti_condensation_init());
    
    ESP_LOGI(TAG, "System initialization complete");
    return ESP_OK;
}

/**
 * @brief Sensor reading task
 * Reads temperature and humidity from all sensors
 */
static void sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Sensor task started");
    
    sensor_data_t data;
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(5000); // 5 seconds
    
    while (1) {
        // Read all sensors (SR-001: Input validation in sensor_manager)
        if (sensor_manager_read_all(&data) == ESP_OK) {
            // Take mutex for sensor data
            if (xSemaphoreTake(s_sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Update global sensor data
                sensor_manager_update_data(&data);
                xSemaphoreGive(s_sensor_mutex);
                
                // Check for condensation (SR-010)
                anti_condensation_check(&data);
                
                // Update display
                display_manager_update(&data);
                
                // Publish to MQTT
                mqtt_client_publish_sensors(&data);
            }
        } else {
            ESP_LOGW(TAG, "Failed to read sensors");
            // Fail-safe: log error, continue trying (SR-004)
        }
        
        // Wait for next cycle
        vTaskDelayUntil(&last_wake_time, frequency);
    }
}

/**
 * @brief Control task
 * Manages fan control based on sensor data and setpoints
 */
static void control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Control task started");
    
    sensor_data_t data;
    fan_speed_t fan1_speed, fan2_speed;
    
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(1000); // 1 second
    
    while (1) {
        // Get current sensor data
        if (xSemaphoreTake(s_sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sensor_manager_get_data(&data);
            xSemaphoreGive(s_sensor_mutex);
            
            // Calculate fan speeds based on:
            // - Temperature setpoints
            // - Humidity (anti-condensation protection)
            // - Manual overrides from web/MQTT
            
            // Check anti-condensation first (SR-010)
            if (anti_condensation_is_active()) {
                // Force fans ON to reduce humidity
                fan1_speed = FAN_SPEED_HIGH;
                fan2_speed = FAN_SPEED_HIGH;
                ESP_LOGW(TAG, "Anti-condensation mode active");
            } else {
                // Normal control logic
                fan_controller_calculate_speeds(&data, &fan1_speed, &fan2_speed);
            }
            
            // Apply fan speeds (SR-009: Fail-safe in fan_controller)
            fan_controller_set_speed(FAN_1, fan1_speed);
            fan_controller_set_speed(FAN_2, fan2_speed);
            
            // Log fan status (SR-005)
            ESP_LOGD(TAG, "Fan1: %d%%, Fan2: %d%%", 
                     fan_speed_to_percent(fan1_speed),
                     fan_speed_to_percent(fan2_speed));
        }
        
        vTaskDelayUntil(&last_wake_time, frequency);
    }
}

/**
 * @brief Main entry point
 */
void app_main(void)
{
    ESP_LOGI(TAG, "========================");
    ESP_LOGI(TAG, "ThermoFlow Starting...");
    ESP_LOGI(TAG, "Version: %s", PROJECT_VERSION);
    ESP_LOGI(TAG, "========================");
    
    // Initialize NVS
    ESP_ERROR_CHECK(init_nvs());
    
    // Initialize system
    ESP_ERROR_CHECK(init_system());
    
    // Create tasks
    ESP_LOGI(TAG, "Creating tasks...");
    
    // Sensor task
    BaseType_t ret = xTaskCreatePinnedToCore(
        sensor_task,
        "sensor_task",
        STACK_SIZE_SENSORS,
        NULL,
        TASK_PRIORITY_SENSORS,
        NULL,
        0  // Core 0
    );
    assert(ret == pdPASS);
    
    // Control task
    ret = xTaskCreatePinnedToCore(
        control_task,
        "control_task",
        STACK_SIZE_CONTROL,
        NULL,
        TASK_PRIORITY_CONTROL,
        NULL,
        1  // Core 1
    );
    assert(ret == pdPASS);
    
    // Main loop - monitor system health
    while (1) {
        // Check system status
        // - Watchdog refresh
        // - Memory usage
        // - Task health
        
        // Feed watchdog
        esp_task_wdt_reset();
        
        // Log system stats
        ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10 seconds
    }
}
