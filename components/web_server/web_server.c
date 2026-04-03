/**
 * @file web_server.c
 * @brief Web Server Implementation with FTX API - ESP-IDF
 */

#include "web_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "string.h"

// Include FTX components
#include "heat_recovery.h"

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;

// FTX data cache (updated by main loop)
static heat_recovery_data_t s_ftx_data = {0};
static bool s_ftx_data_valid = false;

// Forward declarations
static esp_err_t ftx_api_handler(httpd_req_t *req);
static esp_err_t ftx_sensors_handler(httpd_req_t *req);
static esp_err_t ftx_efficiency_handler(httpd_req_t *req);
static esp_err_t ftx_control_handler(httpd_req_t *req);
static esp_err_t ftx_status_handler(httpd_req_t *req);

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
    config.max_uri_handlers = 16;
    
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
    
    ESP_LOGI(TAG, "Web server started on port %d", config.server_port);
    ESP_LOGI(TAG, "FTX API endpoints registered");
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
    return (server != NULL);
}

// Update FTX data from main loop
void web_server_update_ftx_data(const heat_recovery_data_t *data)
{
    if (data) {
        memcpy(&s_ftx_data, data, sizeof(heat_recovery_data_t));
        s_ftx_data_valid = true;
    }
}

// Helper: Send JSON response
static esp_err_t send_json_response(httpd_req_t *req, cJSON *json)
{
    char *response = cJSON_Print(json);
    if (!response) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, response, strlen(response));
    
    free(response);
    cJSON_Delete(json);
    return ESP_OK;
}

// GET /api/ftx - Complete FTX data
static esp_err_t ftx_api_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    // Timestamp
    cJSON_AddStringToObject(root, "timestamp", "2026-04-03T16:30:00Z");
    cJSON_AddBoolToObject(root, "valid", s_ftx_data_valid);
    
    if (s_ftx_data_valid) {
        // Sensors object
        cJSON *sensors = cJSON_CreateObject();
        cJSON_AddNumberToObject(sensors, "outdoor_temp", s_ftx_data.outdoor_temp);
        cJSON_AddNumberToObject(sensors, "outdoor_rh", s_ftx_data.outdoor_rh);
        cJSON_AddNumberToObject(sensors, "supply_temp", s_ftx_data.supply_temp);
        cJSON_AddNumberToObject(sensors, "supply_rh", s_ftx_data.supply_rh);
        cJSON_AddNumberToObject(sensors, "exhaust_temp", s_ftx_data.exhaust_temp);
        cJSON_AddNumberToObject(sensors, "exhaust_rh", s_ftx_data.exhaust_rh);
        cJSON_AddNumberToObject(sensors, "extract_temp", s_ftx_data.extract_temp);
        cJSON_AddNumberToObject(sensors, "extract_rh", s_ftx_data.extract_rh);
        cJSON_AddItemToObject(root, "sensors", sensors);
        
        // Efficiency object
        cJSON *eff = cJSON_CreateObject();
        cJSON_AddNumberToObject(eff, "percent", s_ftx_data.efficiency_percent);
        cJSON_AddNumberToObject(eff, "airflow", s_ftx_data.airflow_supply_m3h);
        cJSON_AddItemToObject(root, "efficiency", eff);
        
        // Fans
        cJSON *fans = cJSON_CreateObject();
        cJSON_AddNumberToObject(fans, "supply", s_ftx_data.airflow_supply_m3h); // Using airflow as proxy
        cJSON_AddNumberToObject(fans, "exhaust", s_ftx_data.airflow_exhaust_m3h);
        cJSON_AddItemToObject(root, "fans", fans);
        
        // Status
        cJSON *status = cJSON_CreateObject();
        cJSON_AddBoolToObject(status, "frost_risk", s_ftx_data.frost_protection_active);
        cJSON_AddBoolToObject(status, "bypass", s_ftx_data.bypass_active);
        cJSON_AddItemToObject(root, "status", status);
    }
    
    return send_json_response(req, root);
}

// GET /api/ftx/sensors - Sensor readings only
static esp_err_t ftx_sensors_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
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
    
    return send_json_response(req, root);
}

// GET /api/ftx/efficiency - Efficiency calculations
static esp_err_t ftx_efficiency_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    
    if (s_ftx_data_valid) {
        // Use stored efficiency
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
    
    if (s_ftx_data_valid) {
        cJSON_AddBoolToObject(root, "frost_risk", s_ftx_data.frost_protection_active);
        cJSON_AddBoolToObject(root, "bypass_active", s_ftx_data.bypass_active);
        cJSON_AddBoolToObject(root, "filter_warning", 
            (s_ftx_data.status == FTX_STATUS_FILTER_WARNING || 
             s_ftx_data.status == FTX_STATUS_FILTER_CRITICAL));
    }
    
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
    
    // Read POST data
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
    
    // Parse JSON
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
    
    // Process command (would trigger callbacks to main application)
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddStringToObject(response, "command", cmd_str);
    cJSON_AddNumberToObject(response, "value", val);
    
    cJSON_Delete(json);
    return send_json_response(req, response);
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
