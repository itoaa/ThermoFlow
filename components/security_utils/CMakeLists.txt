idf_component_register(
    SRCS "security_manager.c" "ed25519_impl.c" "monocypher.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        mbedtls 
        nvs_flash
        esp_event
        esp_hw_support
    PRIV_REQUIRES
        esp_timer
)

# Set compile options for security features
target_compile_options(${COMPONENT_LIB} PRIVATE
    -DMBEDTLS_CONFIG_FILE="mbedtls/esp_config.h"
)

# Security hardening flags
if(CONFIG_COMPILER_OPTIMIZATION_DEFAULT OR CONFIG_COMPILER_OPTIMIZATION_SIZE)
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -fstack-protector-strong
        -Wformat-security
    )
endif()
