/**
 * @file web_server.c
 * @brief Web Server Implementation - ESP-IDF
 */

#include "web_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"

static const char *TAG = "WEB_SERVER";
static httpd_handle_t server = NULL;

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
    
    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server");
        return err;
    }
    
    ESP_LOGI(TAG, "Web server started on port %d", config.server_port);
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