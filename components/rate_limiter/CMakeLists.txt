idf_component_register(
    SRCS "rate_limiter.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_timer
)