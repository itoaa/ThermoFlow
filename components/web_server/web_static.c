#include "web_static.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "wifi_manager.h"
#include <string.h>

static const char *TAG = "WEB_STATIC";

extern const uint8_t web_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t web_index_html_end[] asm("_binary_index_html_end");
extern const uint8_t web_wifi_config_html_start[] asm("_binary_wifi_config_html_start");
extern const uint8_t web_wifi_config_html_end[] asm("_binary_wifi_config_html_end");
extern const uint8_t web_style_css_start[] asm("_binary_style_css_start");
extern const uint8_t web_style_css_end[] asm("_binary_style_css_end");
extern const uint8_t web_script_js_start[] asm("_binary_script_js_start");
extern const uint8_t web_script_js_end[] asm("_binary_script_js_end");
extern const uint8_t web_ftx_html_start[] asm("_binary_ftx_html_start");
extern const uint8_t web_ftx_html_end[] asm("_binary_ftx_html_end");
extern const uint8_t web_ftx_style_css_start[] asm("_binary_ftx_style_css_start");
extern const uint8_t web_ftx_style_css_end[] asm("_binary_ftx_style_css_end");
extern const uint8_t web_ftx_script_js_start[] asm("_binary_ftx_script_js_start");
extern const uint8_t web_ftx_script_js_end[] asm("_binary_ftx_script_js_end");
extern const uint8_t web_manifest_json_start[] asm("_binary_manifest_json_start");
extern const uint8_t web_manifest_json_end[] asm("_binary_manifest_json_end");
extern const uint8_t web_sw_js_start[] asm("_binary_sw_js_start");
extern const uint8_t web_sw_js_end[] asm("_binary_sw_js_end");

typedef struct {
    const char *uri;
    const char *content_type;
    const uint8_t *start;
    const uint8_t *end;
} web_static_asset_t;

static const web_static_asset_t s_assets[] = {
    { "/index.html", "text/html", web_index_html_start, web_index_html_end },
    { "/wifi_config.html", "text/html", web_wifi_config_html_start, web_wifi_config_html_end },
    { "/style.css", "text/css", web_style_css_start, web_style_css_end },
    { "/script.js", "application/javascript", web_script_js_start, web_script_js_end },
    { "/ftx.html", "text/html", web_ftx_html_start, web_ftx_html_end },
    { "/ftx-style.css", "text/css", web_ftx_style_css_start, web_ftx_style_css_end },
    { "/ftx-script.js", "application/javascript", web_ftx_script_js_start, web_ftx_script_js_end },
    { "/manifest.json", "application/json", web_manifest_json_start, web_manifest_json_end },
    { "/sw.js", "application/javascript", web_sw_js_start, web_sw_js_end },
};

static esp_err_t send_asset(httpd_req_t *req, const web_static_asset_t *asset)
{
    if (!asset || asset->end <= asset->start) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Empty asset");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, asset->content_type);
    return httpd_resp_send(req, (const char *)asset->start, asset->end - asset->start);
}

static const web_static_asset_t *find_asset(const char *uri)
{
    for (size_t i = 0; i < sizeof(s_assets) / sizeof(s_assets[0]); i++) {
        if (strcmp(uri, s_assets[i].uri) == 0) {
            return &s_assets[i];
        }
    }
    return NULL;
}

static esp_err_t root_handler(httpd_req_t *req)
{
    const web_static_asset_t *asset = wifi_manager_is_ap_mode()
        ? find_asset("/wifi_config.html")
        : find_asset("/index.html");
    return send_asset(req, asset);
}

static esp_err_t static_file_handler(httpd_req_t *req)
{
    const web_static_asset_t *asset = find_asset(req->uri);
    if (!asset) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_FAIL;
    }
    return send_asset(req, asset);
}

esp_err_t web_static_register_handlers(httpd_handle_t server)
{
    if (!server) {
        return ESP_ERR_INVALID_ARG;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL,
    };

    esp_err_t ret = httpd_register_uri_handler(server, &root_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register / handler: %s", esp_err_to_name(ret));
        return ret;
    }

    for (size_t i = 0; i < sizeof(s_assets) / sizeof(s_assets[0]); i++) {
        httpd_uri_t uri = {
            .uri = s_assets[i].uri,
            .method = HTTP_GET,
            .handler = static_file_handler,
            .user_ctx = NULL,
        };

        ret = httpd_register_uri_handler(server, &uri);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register %s: %s", s_assets[i].uri, esp_err_to_name(ret));
            return ret;
        }
    }

    ESP_LOGI(TAG, "Registered %u static web assets", (unsigned)(sizeof(s_assets) / sizeof(s_assets[0]) + 1));
    return ESP_OK;
}