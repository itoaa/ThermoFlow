/**
 * @file web_server.h
 * @brief Web Server Interface with FTX API - ESP-IDF
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "heat_recovery.h"

// Core functions
esp_err_t web_server_init(void);
esp_err_t web_server_start(void);
esp_err_t web_server_stop(void);
bool web_server_is_running(void);
esp_err_t web_server_register_handler(const char *uri, httpd_method_t method, 
                                       esp_err_t (*handler)(httpd_req_t *req));
esp_err_t web_server_deinit(void);

// FTX-specific functions - uses heat_recovery_data_t from heat_recovery.h
void web_server_update_ftx_data(const heat_recovery_data_t *data);

#endif
