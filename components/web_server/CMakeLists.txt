idf_component_register(
    SRCS "web_server.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        esp_http_server
        esp_https_server
        esp-tls
        nvs_flash
        json
        heat_recovery
        hardware_manager
        esp_wifi
        security_utils
        cert_manager
        rate_limiter
        audit_log
)

# Include paths for HTTPS server
target_include_directories(${COMPONENT_LIB} PRIVATE
    "$ENV{IDF_PATH}/components/esp_https_server/include"
)

# Add global include directory for thermoflow_config.h
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_SOURCE_DIR}/include")
