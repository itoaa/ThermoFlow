idf_component_register(
    SRCS "ota_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        app_update
        esp_https_ota
        esp_http_client
        esp_partition
        nvs_flash
        esp-tls
        mbedtls
        security_utils
    PRIV_REQUIRES
        freertos
)

# Include path for mbedtls
include_directories(${COMPONENT_LIB} PRIVATE
    "$ENV{IDF_PATH}/components/mbedtls/mbedtls/include"
)

# Set compile options for OTA security features
target_compile_options(${COMPONENT_LIB} PRIVATE
    -DOTA_SECURE_HTTPS_REQUIRED=1
)

if(CONFIG_SECURE_BOOT_V2_RSA)
    message(STATUS "OTA Manager: Secure Boot V2 RSA enabled")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        -DOTA_SECURE_BOOT_V2_ENABLED=1
    )
endif()

if(CONFIG_SECURE_FLASH_ENCRYPTION_AES256_XTS)
    message(STATUS "OTA Manager: Flash Encryption enabled")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        -DOTA_FLASH_ENCRYPTION_ENABLED=1
    )
endif()
