idf_component_register(
    SRCS "web_server.c" "web_static.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        freertos
        esp_netif
        esp_http_server
        esp-tls
        nvs_flash
        json
        heat_recovery
        hardware_manager
        sensor_manager
        esp_wifi
        security_utils
        rate_limiter
        audit_log
        wifi_manager
        ota_manager
    EMBED_FILES
        "web/index.html"
        "web/wifi_config.html"
        "web/wifi_reconnect.html"
        "web/style.css"
        "web/script.js"
        "web/ftx.html"
        "web/ftx-style.css"
        "web/ftx-script.js"
        "web/manifest.json"
        "web/sw.js"
)

# Add global include directory for thermoflow_config.h
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_SOURCE_DIR}/include")
