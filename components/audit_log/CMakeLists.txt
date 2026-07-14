idf_component_register(
    SRCS "audit_log.c"
    INCLUDE_DIRS "include"
    REQUIRES log_manager nvs_flash esp_timer freertos log esp_hw_support
)