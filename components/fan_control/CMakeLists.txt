idf_component_register(
    SRCS "fan_controller.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer
)
