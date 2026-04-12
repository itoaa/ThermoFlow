/* ============================================
 * HTTP Redirect Server
 * ============================================ */

static esp_err_t redirect_handler(httpd_req_t *req)
{
    https_config_t config;
    web_server_get_https_config(&config);
    
    // Build HTTPS URL
    char redirect_url[256];
    
    // Get host from request header, fallback to IP
    char host_buf[64] = {0};
    const char *host = NULL;
    if (httpd_req_get_hdr_value_str(req, "Host", host_buf, sizeof(host_buf)) == ESP_OK) {
        host = host_buf;
    }
    
    if (!host || strlen(host) == 0) {
        // Get IP address
        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            char ip_str[16];
            esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
            host = ip_str;
        } else {
            host = "localhost";
        }
    }
    
    snprintf(redirect_url, sizeof(redirect_url), 
             "https://%s:%d%s", host, config.https_port, req->uri);
    
    ESP_LOGI(TAG, "Redirecting HTTP request to: %s", redirect_url);
    
    // Send 301 Moved Permanently
    httpd_resp_set_status(req, "301 Moved Permanently");
    httpd_resp_set_hdr(req, "Location", redirect_url);
    
    // Add HSTS header even on redirect
    if (config.enable_hsts) {
        char hsts_header[128];
        snprintf(hsts_header, sizeof(hsts_header), 
                 "max-age=%lu; includeSubDomains",
                 (unsigned long)config.hsts_max_age);
        httpd_resp_set_hdr(req, "Strict-Transport-Security", hsts_header);
    }
    
    httpd_resp_send(req, NULL, 0);
    
    return ESP_OK;
}
