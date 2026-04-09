/**
 * @file hardware_manager.c
 * @brief Hardware detection and simulation mode manager implementation
 * 
 * Detects I2C sensors (SHT40) and PWM fan controllers at boot.
 * Automatically falls back to simulation mode if no hardware found.
 * 
 * @version 1.1.0
 * @date 2026-04-09
 */

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "hardware_manager.h"
#include "thermoflow_config.h"

static const char *TAG = "HW_MANAGER";

/* Internal state */
static hw_status_t g_hw_status = {0};
static SemaphoreHandle_t g_hw_mutex = NULL;

/* SHT40 detection constants */
#define SHT40_CMD_SOFT_RESET    0x94
#define SHT40_CMD_READ_ID       0x89
#define SHT40_I2C_TIMEOUT_MS    100

/* I2C probe helper - returns true if device responds */
static bool i2c_probe_device(uint8_t addr)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) return false;
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 
                                           pdMS_TO_TICKS(SHT40_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    return (ret == ESP_OK);
}

/* Detect SHT40 sensors on I2C bus */
static uint8_t detect_sht40_sensors(void)
{
    uint8_t count = 0;
    uint8_t addresses[] = {SHT40_ADDR_A, SHT40_ADDR_B, SHT40_ADDR_C, SHT40_ADDR_D};
    
    ESP_LOGI(TAG, "Probing I2C bus for SHT40 sensors...");
    ESP_LOGI(TAG, "I2C SDA: GPIO%d, SCL: GPIO%d", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    
    for (int i = 0; i < 4; i++) {
        if (i2c_probe_device(addresses[i])) {
            ESP_LOGI(TAG, "SHT40 detected at address 0x%02X", addresses[i]);
            count++;
            /* Mark component as detected */
            switch (i) {
                case 0: g_hw_status.detected[HW_COMPONENT_SHT40_SENSOR_1] = true; break;
                case 1: g_hw_status.detected[HW_COMPONENT_SHT40_SENSOR_2] = true; break;
                case 2: g_hw_status.detected[HW_COMPONENT_SHT40_SENSOR_3] = true; break;
                case 3: g_hw_status.detected[HW_COMPONENT_SHT40_SENSOR_4] = true; break;
            }
        } else {
            ESP_LOGD(TAG, "No device at address 0x%02X", addresses[i]);
        }
    }
    
    return count;
}

/* Detect OLED display */
static bool detect_oled_display(void)
{
    /* Common OLED addresses: 0x3C, 0x3D */
    uint8_t oled_addrs[] = {0x3C, 0x3D};
    
    for (int i = 0; i < 2; i++) {
        if (i2c_probe_device(oled_addrs[i])) {
            ESP_LOGI(TAG, "OLED display detected at address 0x%02X", oled_addrs[i]);
            return true;
        }
    }
    
    ESP_LOGW(TAG, "No OLED display detected");
    return false;
}

/* Detect PWM fan controllers (GPIO check) */
static uint8_t detect_fans(void)
{
    uint8_t count = 0;
    
    /* For fan detection, we check if GPIO pins are available/configured */
    /* In a real implementation, we might try to read tachometer feedback */
    
    ESP_LOGI(TAG, "Checking fan GPIO configuration...");
    ESP_LOGI(TAG, "Fan 1 GPIO: %d, Fan 2 GPIO: %d", FAN_1_GPIO, FAN_2_GPIO);
    
    /* For now, assume fans are present if we can configure the GPIO */
    /* A more robust check would try to read RPM feedback */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << FAN_1_GPIO) | (1ULL << FAN_2_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret == ESP_OK) {
        /* GPIO configured - mark as "detected" for now */
        g_hw_status.detected[HW_COMPONENT_FAN_1] = true;
        g_hw_status.detected[HW_COMPONENT_FAN_2] = true;
        count = 2;
        ESP_LOGI(TAG, "Fan GPIOs configured successfully");
    } else {
        ESP_LOGW(TAG, "Fan GPIO config failed: %s", esp_err_to_name(ret));
    }
    
    return count;
}

/* Initialize I2C master */
static esp_err_t init_i2c_master(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 
                             I2C_MASTER_RX_BUF_DISABLE, 
                             I2C_MASTER_TX_BUF_DISABLE, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "I2C master initialized");
    return ESP_OK;
}

/* Generate simulation data */
static void generate_simulation_summary(void)
{
    snprintf(g_hw_status.detected_components_str, sizeof(g_hw_status.detected_components_str),
             "[SIMULATION MODE] No hardware detected - using simulated sensors");
}

/* Generate detection summary */
static void generate_detection_summary(void)
{
    char *p = g_hw_status.detected_components_str;
    size_t remaining = sizeof(g_hw_status.detected_components_str);
    int written = 0;
    
    if (g_hw_status.simulation_mode) {
        generate_simulation_summary();
        return;
    }
    
    written = snprintf(p, remaining, "Detected: ");
    p += written;
    remaining -= written;
    
    bool first = true;
    
    /* Sensors */
    if (g_hw_status.detected[HW_COMPONENT_SHT40_SENSOR_1]) {
        written = snprintf(p, remaining, "%sSHT40-1", first ? "" : ", ");
        p += written;
        remaining -= written;
        first = false;
    }
    if (g_hw_status.detected[HW_COMPONENT_SHT40_SENSOR_2]) {
        written = snprintf(p, remaining, "%sSHT40-2", first ? "" : ", ");
        p += written;
        remaining -= written;
        first = false;
    }
    if (g_hw_status.detected[HW_COMPONENT_SHT40_SENSOR_3]) {
        written = snprintf(p, remaining, "%sSHT40-3", first ? "" : ", ");
        p += written;
        remaining -= written;
        first = false;
    }
    if (g_hw_status.detected[HW_COMPONENT_SHT40_SENSOR_4]) {
        written = snprintf(p, remaining, "%sSHT40-4", first ? "" : ", ");
        p += written;
        remaining -= written;
        first = false;
    }
    
    /* Display */
    if (g_hw_status.detected[HW_COMPONENT_OLED_DISPLAY]) {
        written = snprintf(p, remaining, "%sOLED", first ? "" : ", ");
        p += written;
        remaining -= written;
        first = false;
    }
    
    /* Fans */
    if (g_hw_status.detected[HW_COMPONENT_FAN_1]) {
        written = snprintf(p, remaining, "%sFan1", first ? "" : ", ");
        p += written;
        remaining -= written;
        first = false;
    }
    if (g_hw_status.detected[HW_COMPONENT_FAN_2]) {
        written = snprintf(p, remaining, "%sFan2", first ? "" : ", ");
        p += written;
        remaining -= written;
        first = false;
    }
    
    if (first) {
        snprintf(p, remaining, "No hardware detected");
    }
}

/* Public API Implementation */

esp_err_t hardware_manager_init(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Hardware Detection Starting");
    ESP_LOGI(TAG, "========================================");
    
    /* Create mutex for thread safety */
    g_hw_mutex = xSemaphoreCreateMutex();
    if (g_hw_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create hardware mutex");
        return ESP_ERR_NO_MEM;
    }
    
    /* Clear status */
    memset(&g_hw_status, 0, sizeof(g_hw_status));
    
    /* Initialize I2C for detection */
    esp_err_t ret = init_i2c_master();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "I2C init failed - proceeding in simulation mode");
        g_hw_status.simulation_mode = true;
        goto detection_complete;
    }
    
    /* Detect sensors */
    uint8_t sensor_count = detect_sht40_sensors();
    
    /* Detect display */
    g_hw_status.detected[HW_COMPONENT_OLED_DISPLAY] = detect_oled_display();
    
    /* Detect fans */
    uint8_t fan_count = detect_fans();
    
    /* Determine if simulation mode is needed */
    if (sensor_count == 0) {
        ESP_LOGW(TAG, "============================================");
        ESP_LOGW(TAG, "  NO SENSORS DETECTED!");
        ESP_LOGW(TAG, "  Entering SIMULATION MODE");
        ESP_LOGW(TAG, "  Connect SHT40 sensors to GPIO %d/%d",
                 I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
        ESP_LOGW(TAG, "  and reboot to use real hardware.");
        ESP_LOGW(TAG, "============================================");
        g_hw_status.simulation_mode = true;
    } else {
        ESP_LOGI(TAG, "Detected %d SHT40 sensor(s)", sensor_count);
        g_hw_status.simulation_mode = false;
    }
    
detection_complete:
    g_hw_status.detection_time_ms = esp_timer_get_time() / 1000;
    generate_detection_summary();
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  %s", g_hw_status.detected_components_str);
    ESP_LOGI(TAG, "  Simulation mode: %s", 
             g_hw_status.simulation_mode ? "YES" : "NO");
    ESP_LOGI(TAG, "========================================");
    
    return ESP_OK;
}

bool hardware_is_detected(hw_component_t component)
{
    if (component <= HW_COMPONENT_NONE || component >= HW_COMPONENT_COUNT) {
        return false;
    }
    
    bool detected = false;
    if (xSemaphoreTake(g_hw_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        detected = g_hw_status.detected[component];
        xSemaphoreGive(g_hw_mutex);
    }
    return detected;
}

bool hardware_is_simulation_mode(void)
{
    bool mode = false;
    if (xSemaphoreTake(g_hw_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        mode = g_hw_status.simulation_mode;
        xSemaphoreGive(g_hw_mutex);
    }
    return mode;
}

esp_err_t hardware_set_simulation_mode(bool enable)
{
    if (xSemaphoreTake(g_hw_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    g_hw_status.simulation_mode = enable;
    
    if (enable) {
        /* Clear all hardware flags when entering simulation */
        memset(g_hw_status.detected, 0, sizeof(g_hw_status.detected));
    }
    
    generate_detection_summary();
    
    xSemaphoreGive(g_hw_mutex);
    
    ESP_LOGI(TAG, "Simulation mode %s", enable ? "ENABLED" : "DISABLED");
    return ESP_OK;
}

esp_err_t hardware_get_status(hw_status_t *status)
{
    if (status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_hw_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    memcpy(status, &g_hw_status, sizeof(hw_status_t));
    
    xSemaphoreGive(g_hw_mutex);
    return ESP_OK;
}

const char* hardware_get_summary(void)
{
    return g_hw_status.detected_components_str;
}

esp_err_t hardware_redetect(void)
{
    ESP_LOGI(TAG, "Re-detecting hardware...");
    
    if (xSemaphoreTake(g_hw_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    /* Clear current detection */
    memset(&g_hw_status.detected, 0, sizeof(g_hw_status.detected));
    
    /* Re-detect */
    uint8_t sensor_count = detect_sht40_sensors();
    g_hw_status.detected[HW_COMPONENT_OLED_DISPLAY] = detect_oled_display();
    detect_fans();
    
    /* Update simulation mode */
    g_hw_status.simulation_mode = (sensor_count == 0);
    
    generate_detection_summary();
    g_hw_status.detection_time_ms = esp_timer_get_time() / 1000;
    
    xSemaphoreGive(g_hw_mutex);
    
    ESP_LOGI(TAG, "Re-detection complete: %s", g_hw_status.detected_components_str);
    return ESP_OK;
}

uint8_t hardware_get_sensor_count(void)
{
    uint8_t count = 0;
    
    if (xSemaphoreTake(g_hw_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        for (int i = HW_COMPONENT_SHT40_SENSOR_1; i <= HW_COMPONENT_SHT40_SENSOR_4; i++) {
            if (g_hw_status.detected[i]) count++;
        }
        xSemaphoreGive(g_hw_mutex);
    }
    
    return count;
}

uint8_t hardware_get_fan_count(void)
{
    uint8_t count = 0;
    
    if (xSemaphoreTake(g_hw_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (g_hw_status.detected[HW_COMPONENT_FAN_1]) count++;
        if (g_hw_status.detected[HW_COMPONENT_FAN_2]) count++;
        xSemaphoreGive(g_hw_mutex);
    }
    
    return count;
}
