idf_component_register(
    SRCS "sht4x_sensor.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer
)