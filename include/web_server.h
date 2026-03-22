/**
 * @file web_server.h
 * @brief Web Server Interface (Stub)
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"

// Forward declaration
typedef struct httpd_req httpd_req_t;

typedef struct {
    uint16_t port;
    bool use_https;
} web_server_config_t;

esp_err_t web_server_init(void);
esp_err_t web_server_start(void);
esp_err_t web_server_stop(void);
bool web_server_is_running(void);
esp_err_t web_server_deinit(void);

// Simplified registration
typedef esp_err_t (*web_handler_t)(httpd_req_t *req);
esp_err_t web_server_register_handler(const char *uri, web_handler_t handler);

#endif