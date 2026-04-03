# ThermoFlow Main Application

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        freertos
        esp_event
        esp_wifi
        esp_netif
        nvs_flash
        driver
        sht4x_sensor
        fan_control
        mqtt_client
        web_server
        security_utils
        display_driver
        anti_condensation
        sensor_manager
        rate_limiter
        audit_log
        wifi_manager
)
