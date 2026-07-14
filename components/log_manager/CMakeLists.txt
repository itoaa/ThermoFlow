idf_component_register(
    SRCS "log_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_timer freertos log esp_hw_support
)