idf_component_register(
    SRCS "security_manager.c" "ed25519_impl.c"
    INCLUDE_DIRS "include"
    REQUIRES mbedtls nvs_flash
)