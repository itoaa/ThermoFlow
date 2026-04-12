idf_component_register(
    SRCS "security_manager.c" "ed25519_impl.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        mbedtls 
        nvs_flash
        esp_event
    PRIV_REQUIRES
        esp_timer
)

# Set compile options for security features
target_compile_options(${COMPONENT_LIB} PRIVATE
    -DMBEDTLS_CONFIG_FILE="mbedtls/esp_config.h"
    -DCONFIG_MBEDTLS_TLS_VERSION_TLS1_3
)

# Security hardening flags
if(CONFIG_COMPILER_OPTIMIZATION_DEFAULT OR CONFIG_COMPILER_OPTIMIZATION_SIZE)
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -fstack-protector-strong
        -Wformat-security
    )
endif()
