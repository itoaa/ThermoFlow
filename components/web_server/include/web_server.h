/**
 * @file web_server.h
 * @brief Web Server Interface with FTX API - ESP-IDF
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "heat_recovery.h"

// FTX data structure for web API
typedef struct {
    // Sensors
    float outdoor_temp;
    float outdoor_rh;
    float supply_temp;
    float supply_rh;
    float exhaust_temp;
    float exhaust_rh;
    float extract_temp;
    float extract_rh;
    
    // Efficiency
    float efficiency;
    float airflow;
    
    // Fans
    int fan_speed_supply;
    int fan_speed_exhaust;
    
    // Mode: 0=auto, 1=manual, 2=summer, 3=winter
    int mode;
    
    // Status flags
    bool frost_risk;
    bool frost_protection_active;
    bool bypass_active;
    bool filter_warning;
    bool high_humidity;
    int filter_hours_remaining;
} ftx_data_t;

// Core functions
esp_err_t web_server_init(void);
esp_err_t web_server_start(void);
esp_err_t web_server_stop(void);
bool web_server_is_running(void);
esp_err_t web_server_register_handler(const char *uri, httpd_method_t method, 
                                       esp_err_t (*handler)(httpd_req_t *req));
esp_err_t web_server_deinit(void);

// FTX-specific functions
void web_server_update_ftx_data(const ftx_data_t *data);

#endif
