idf_component_register(
    SRCS "wifi_manager.c" "wifi_secure_storage.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event esp_netif nvs_flash mbedtls
)
