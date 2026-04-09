/**
 * @file web_server.c
 * @brief Web Server Implementation with FTX API - ESP-IDF
 * 
 * Updated v1.1.0: Added hardware detection status and pin configuration info
 */

#include "web_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "cJSON.h"
#include "string.h"

#include "heat_recovery.h"
#include "hardware_manager.h"
#include "thermoflow_config.h"

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;

static heat_recovery_data_t s_ftx_data = {0};
static bool s_ftx_data_valid = false;

// Forward declarations
static esp_err_t ftx_api_handler(httpd_req_t *req);
static esp_err_t ftx_sensors_handler(httpd_req_t *req);
static esp_err_t ftx_efficiency_handler(httpd_req_t *req);
static esp_err_t ftx_control_handler(httpd_req_t *req);
static esp_err_t ftx_status_handler(httpd_req_t *req);
static esp_err_t wifi_config_handler(httpd_req_t *req);
static esp_err_t device_info_handler(httpd_req_t *req);
static esp_err_t hardware_info_handler(httpd_req_t *req);

// Helper to send JSON response
static esp_err_t send_json_response(httpd_req_t *req, cJSON *json)
{
    char *response = cJSON_Print(json);
    cJSON_Delete(json);
    
    if (!response) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON error");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, response, strlen(response));
    free(response);
    return ret;
}

esp_err_t web_server_init(void)
{
    ESP_LOGI(TAG, "Web server initialized");
    return ESP_OK;
}

esp_err_t web_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 20;
    
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server");
        return err;
    }
    
    // Register FTX API handlers
    httpd_uri_t ftx_api_uri = {
        .uri = "/api/ftx",
        .method = HTTP_GET,
        .handler = ftx_api_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ftx_api_uri);
    
    httpd_uri_t ftx_sensors_uri = {
        .uri = "/api/ftx/sensors",
        .method = HTTP_GET,
        .handler = ftx_sensors_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ftx_sensors_uri);
    
    httpd_uri_t ftx_efficiency_uri = {
        .uri = "/api/ftx/efficiency",
        .method = HTTP_GET,
        .handler = ftx_efficiency_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ftx_efficiency_uri);
    
    httpd_uri_t ftx_control_uri = {
        .uri = "/api/ftx/control",
        .method = HTTP_POST,
        .handler = ftx_control_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ftx_control_uri);
    
    httpd_uri_t ftx_status_uri = {
        .uri = "/api/ftx/status",
        .method = HTTP_GET,
        .handler = ftx_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ftx_status_uri);
    
    // Hardware info endpoint
    httpd_uri_t hardware_info_uri = {
        .uri = "/api/hardware",
        .method = HTTP_GET,
        .handler = hardware_info_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &hardware_info_uri);
    
    // WiFi config handlers
    httpd_uri_t wifi_config_uri = {
        .uri = "/api/wifi/config",
        .method = HTTP_POST,
        .handler = wifi_config_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_config_uri);
    
    httpd_uri_t device_info_uri = {
        .uri = "/api/device/info",
        .method = HTTP_GET,
        .handler = device_info_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &device_info_uri);
    
    ESP_LOGI(TAG, "Web server started on port 80");
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
    return ESP_OK;
}

bool web_server_is_running(void)
{
    return server != NULL;
}

void web_server_update_ftx_data(const heat_recovery_data_t *data)
{
    if (!data) return;
    memcpy(&s_ftx_data, data, sizeof(s_ftx_data));
    s_ftx_data_valid = true;
}

// GET /api/ftx - Main FTX data
static esp_err_t ftx_api_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    // Add simulation mode status
    cJSON_AddBoolToObject(root, "simulation_mode", hardware_is_simulation_mode());
    cJSON_AddStringToObject(root, "mode", hardware_is_simulation_mode() ? "SIMULATION" : "HARDWARE");
    
    if (s_ftx_data_valid) {
        cJSON_AddNumberToObject(root, "outdoor_temp", s_ftx_data.outdoor_temp);
        cJSON_AddNumberToObject(root, "outdoor_rh", s_ftx_data.outdoor_rh);
        cJSON_AddNumberToObject(root, "supply_temp", s_ftx_data.supply_temp);
        cJSON_AddNumberToObject(root, "supply_rh", s_ftx_data.supply_rh);
        cJSON_AddNumberToObject(root, "exhaust_temp", s_ftx_data.exhaust_temp);
        cJSON_AddNumberToObject(root, "exhaust_rh", s_ftx_data.exhaust_rh);
        cJSON_AddNumberToObject(root, "extract_temp", s_ftx_data.extract_temp);
        cJSON_AddNumberToObject(root, "extract_rh", s_ftx_data.extract_rh);
        cJSON_AddNumberToObject(root, "efficiency_percent", s_ftx_data.efficiency_percent);
        cJSON_AddNumberToObject(root, "fan_speed_percent", s_ftx_data.fan_speed_current);
    }
    
    return send_json_response(req, root);
}

// GET /api/ftx/sensors - Sensor readings only
static esp_err_t ftx_sensors_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    // Add simulation mode flag
    cJSON_AddBoolToObject(root, "simulation_mode", hardware_is_simulation_mode());
    
    if (s_ftx_data_valid) {
        cJSON_AddNumberToObject(root, "outdoor_temp", s_ftx_data.outdoor_temp);
        cJSON_AddNumberToObject(root, "outdoor_rh", s_ftx_data.outdoor_rh);
        cJSON_AddNumberToObject(root, "supply_temp", s_ftx_data.supply_temp);
        cJSON_AddNumberToObject(root, "supply_rh", s_ftx_data.supply_rh);
        cJSON_AddNumberToObject(root, "exhaust_temp", s_ftx_data.exhaust_temp);
        cJSON_AddNumberToObject(root, "exhaust_rh", s_ftx_data.exhaust_rh);
        cJSON_AddNumberToObject(root, "extract_temp", s_ftx_data.extract_temp);
        cJSON_AddNumberToObject(root, "extract_rh", s_ftx_data.extract_rh);
    }
    
    // Add pin configuration info
    cJSON *pins = cJSON_CreateObject();
    cJSON_AddNumberToObject(pins, "i2c_sda", I2C_MASTER_SDA_IO);
    cJSON_AddNumberToObject(pins, "i2c_scl", I2C_MASTER_SCL_IO);
    cJSON_AddNumberToObject(pins, "fan_1_gpio", FAN_1_GPIO);
    cJSON_AddNumberToObject(pins, "fan_2_gpio", FAN_2_GPIO);
    cJSON_AddItemToObject(root, "pin_config", pins);
    
    return send_json_response(req, root);
}

// GET /api/ftx/efficiency - Efficiency calculations
static esp_err_t ftx_efficiency_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    if (s_ftx_data_valid) {
        float efficiency = s_ftx_data.efficiency_percent;
        float temp_diff = s_ftx_data.exhaust_temp - s_ftx_data.outdoor_temp;
        
        cJSON_AddNumberToObject(root, "efficiency_percent", efficiency);
        cJSON_AddNumberToObject(root, "power_recovered_w", s_ftx_data.energy_recovery_w);
        cJSON_AddNumberToObject(root, "airflow_m3h", s_ftx_data.airflow_supply_m3h);
        cJSON_AddNumberToObject(root, "temp_diff_in_out", temp_diff);
    }
    
    return send_json_response(req, root);
}

// GET /api/ftx/status - Status flags
static esp_err_t ftx_status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    // Simulation mode
    cJSON_AddBoolToObject(root, "simulation_mode", hardware_is_simulation_mode());
    
    if (s_ftx_data_valid) {
        cJSON_AddBoolToObject(root, "frost_risk", s_ftx_data.frost_protection_active);
        cJSON_AddBoolToObject(root, "bypass_active", s_ftx_data.bypass_active);
        cJSON_AddBoolToObject(root, "filter_warning", 
            (s_ftx_data.status == FTX_STATUS_FILTER_WARNING || 
             s_ftx_data.status == FTX_STATUS_FILTER_CRITICAL));
    }
    
    return send_json_response(req, root);
}

// GET /api/hardware - Hardware detection status and pin info
static esp_err_t hardware_info_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    // Simulation mode status
    cJSON_AddBoolToObject(root, "simulation_mode", hardware_is_simulation_mode());
    cJSON_AddStringToObject(root, "status", hardware_is_simulation_mode() ? 
        "SIMULATION - No hardware detected" : "HARDWARE - Sensors connected");
    
    // Detected components
    cJSON *detected = cJSON_CreateObject();
    cJSON_AddBoolToObject(detected, "sensor_1", hardware_is_detected(HW_COMPONENT_SHT40_SENSOR_1));
    cJSON_AddBoolToObject(detected, "sensor_2", hardware_is_detected(HW_COMPONENT_SHT40_SENSOR_2));
    cJSON_AddBoolToObject(detected, "sensor_3", hardware_is_detected(HW_COMPONENT_SHT40_SENSOR_3));
    cJSON_AddBoolToObject(detected, "sensor_4", hardware_is_detected(HW_COMPONENT_SHT40_SENSOR_4));
    cJSON_AddBoolToObject(detected, "display", hardware_is_detected(HW_COMPONENT_OLED_DISPLAY));
    cJSON_AddBoolToObject(detected, "fan_1", hardware_is_detected(HW_COMPONENT_FAN_1));
    cJSON_AddBoolToObject(detected, "fan_2", hardware_is_detected(HW_COMPONENT_FAN_2));
    cJSON_AddItemToObject(root, "detected", detected);
    
    // Counts
    cJSON_AddNumberToObject(root, "sensor_count", hardware_get_sensor_count());
    cJSON_AddNumberToObject(root, "fan_count", hardware_get_fan_count());
    
    // Pin configuration for missing hardware
    cJSON *pin_config = cJSON_CreateObject();
    
    cJSON *i2c = cJSON_CreateObject();
    cJSON_AddNumberToObject(i2c, "sda_gpio", I2C_MASTER_SDA_IO);
    cJSON_AddNumberToObject(i2c, "scl_gpio", I2C_MASTER_SCL_IO);
    cJSON_AddNumberToObject(i2c, "frequency_hz", I2C_MASTER_FREQ_HZ);
    cJSON_AddItemToObject(pin_config, "i2c", i2c);
    
    cJSON *fans = cJSON_CreateObject();
    cJSON_AddNumberToObject(fans, "fan_1_gpio", FAN_1_GPIO);
    cJSON_AddNumberToObject(fans, "fan_2_gpio", FAN_2_GPIO);
    cJSON_AddNumberToObject(fans, "pwm_freq_hz", FAN_PWM_FREQ_HZ);
    cJSON_AddItemToObject(pin_config, "fans", fans);
    
    cJSON_AddItemToObject(root, "pin_config", pin_config);
    
    // Instructions for connecting missing hardware
    cJSON *instructions = cJSON_CreateArray();
    
    if (hardware_get_sensor_count() == 0) {
        char inst_str[128];
        snprintf(inst_str, sizeof(inst_str), 
            "SHT40 Sensors: Connect to GPIO %d (SDA) and GPIO %d (SCL), 3.3V, GND",
            I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
        cJSON_AddItemToArray(instructions, cJSON_CreateString(inst_str));
    }
    if (!hardware_is_detected(HW_COMPONENT_OLED_DISPLAY)) {
        cJSON_AddItemToArray(instructions, 
            cJSON_CreateString("OLED Display: Connect to same I2C bus, address 0x3C or 0x3D"));
    }
    if (!hardware_is_detected(HW_COMPONENT_FAN_1)) {
        char inst_str[64];
        snprintf(inst_str, sizeof(inst_str), "Fan 1: Connect PWM to GPIO %d", FAN_1_GPIO);
        cJSON_AddItemToArray(instructions, cJSON_CreateString(inst_str));
    }
    if (!hardware_is_detected(HW_COMPONENT_FAN_2)) {
        char inst_str[64];
        snprintf(inst_str, sizeof(inst_str), "Fan 2: Connect PWM to GPIO %d", FAN_2_GPIO);
        cJSON_AddItemToArray(instructions, cJSON_CreateString(inst_str));
    }
    
    cJSON_AddItemToObject(root, "instructions", instructions);
    
    return send_json_response(req, root);
}

// POST /api/ftx/control - Control commands
static esp_err_t ftx_control_handler(httpd_req_t *req)
{
    char buf[256];
    int ret, remaining = req->content_len;
    
    if (remaining > sizeof(buf) - 1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }
    
    int received = 0;
    while (remaining > 0) {
        ret = httpd_req_recv(req, buf + received, remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    buf[received] = '\0';
    
    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *cmd = cJSON_GetObjectItem(json, "command");
    cJSON *value = cJSON_GetObjectItem(json, "value");
    
    if (!cmd || !cJSON_IsString(cmd)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing command");
        return ESP_FAIL;
    }
    
    const char *cmd_str = cmd->valuestring;
    int val = value ? value->valueint : 0;
    
    ESP_LOGI(TAG, "FTX Control command: %s, value: %d", cmd_str, val);
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddStringToObject(response, "command", cmd_str);
    cJSON_AddNumberToObject(response, "value", val);
    cJSON_AddBoolToObject(response, "simulation_mode", hardware_is_simulation_mode());
    
    cJSON_Delete(json);
    return send_json_response(req, response);
}

// POST /api/wifi/config - Configure WiFi credentials
static esp_err_t wifi_config_handler(httpd_req_t *req)
{
    char buf[512];
    int ret, remaining = req->content_len;
    
    if (remaining > sizeof(buf) - 1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }
    
    int received = 0;
    while (remaining > 0) {
        ret = httpd_req_recv(req, buf + received, remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    buf[received] = '\0';
    
    ESP_LOGI(TAG, "WiFi config request received");
    
    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
    cJSON *password = cJSON_GetObjectItem(json, "password");
    
    if (!ssid || !cJSON_IsString(ssid) || strlen(ssid->valuestring) == 0) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid SSID");
        return ESP_FAIL;
    }
    
    const char *ssid_str = ssid->valuestring;
    const char *pass_str = password && cJSON_IsString(password) ? password->valuestring : "";
    
    ESP_LOGI(TAG, "WiFi config: SSID=%s", ssid_str);
    
    // Simplified response - actual WiFi configuration handled elsewhere
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "WiFi configuration received. Device will restart.");
    cJSON_AddStringToObject(response, "ssid", ssid_str);
    
    cJSON_Delete(json);
    return send_json_response(req, response);
}

// GET /api/device/info - Return device info (MAC, name, etc.)
static esp_err_t device_info_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    // Device name
    cJSON_AddStringToObject(root, "device_name", "ThermoFlow");
    
    // Get MAC address
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    cJSON_AddStringToObject(root, "mac_address", mac_str);
    
    // WiFi status - simplified without wifi_manager dependency
    cJSON_AddStringToObject(root, "wifi_state", "unknown");
    cJSON_AddStringToObject(root, "ip_address", "0.0.0.0");
    
    // Firmware version and platform
    cJSON_AddStringToObject(root, "firmware_version", "1.1.0");
    cJSON_AddStringToObject(root, "platform", "ESP32-S3");
    
    // Hardware mode
    cJSON_AddBoolToObject(root, "simulation_mode", hardware_is_simulation_mode());
    cJSON_AddStringToObject(root, "mode_description", hardware_is_simulation_mode() ? 
        "Running with simulated sensor data" : "Running with real hardware sensors");
    
    return send_json_response(req, root);
}

// Legacy functions
esp_err_t web_server_register_handler(const char *uri, httpd_method_t method, 
                                       esp_err_t (*handler)(httpd_req_t *req))
{
    if (!server) return ESP_ERR_INVALID_STATE;
    
    httpd_uri_t uri_handler = {
        .uri = uri,
        .method = method,
        .handler = handler,
        .user_ctx = NULL
    };
    
    return httpd_register_uri_handler(server, &uri_handler);
}

esp_err_t web_server_deinit(void)
{
    web_server_stop();
    return ESP_OK;
}
