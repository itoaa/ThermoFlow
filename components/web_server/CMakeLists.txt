idf_component_register(
    SRCS "web_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server nvs_flash json heat_recovery hardware_manager esp_wifi
)

# Add global include directory for thermoflow_config.h
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_SOURCE_DIR}/include")
