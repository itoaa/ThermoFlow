idf_component_register(
    SRCS "mqtt_client.c" "mqtt_ftx.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        mqtt 
        nvs_flash 
        esp-tls
        mbedtls
        json
    PRIV_REQUIRES
        freertos
)

# Include path for esp_crt_bundle.h
target_include_directories(${COMPONENT_LIB} PRIVATE
    "$ENV{IDF_PATH}/components/mbedtls/esp_crt_bundle/include"
)

# Set compile options for security features
target_compile_options(${COMPONENT_LIB} PRIVATE
    -DMBEDTLS_CONFIG_FILE="mbedtls/esp_config.h"
)

# If TLS is enabled, ensure mbedtls has required features
if(CONFIG_MQTT_TLS_ENABLED)
    message(STATUS "MQTT-TLS enabled (SEC-016)")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        -DMQTT_TLS_ENABLED=1
    )
    
    if(CONFIG_MQTT_TLS_CERT_PINNING)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE
            -DMQTT_TLS_PINNING_ENABLED=1
        )
    endif()
    
    if(CONFIG_MQTT_TLS_MTLS)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE
            -DMQTT_TLS_MTLS_ENABLED=1
        )
    endif()
endif()

# If CA certificate is embedded, convert PEM to C header
if(CONFIG_MQTT_TLS_CA_CERT_EMBED AND CONFIG_MQTT_TLS_CA_CERT_PATH)
    set(CA_CERT_FILE "${CONFIG_MQTT_TLS_CA_CERT_PATH}")
    set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY ${GENERATED_DIR})
    
    # Generate header file from PEM
    set(CA_CERT_HEADER "${GENERATED_DIR}/embedded_ca_cert.h")
    
    add_custom_command(
        OUTPUT ${CA_CERT_HEADER}
        COMMAND ${CMAKE_COMMAND} -E echo "/* Auto-generated CA certificate */" > ${CA_CERT_HEADER}
        COMMAND ${CMAKE_COMMAND} -E echo "#ifndef EMBEDDED_CA_CERT_H" >> ${CA_CERT_HEADER}
        COMMAND ${CMAKE_COMMAND} -E echo "#define EMBEDDED_CA_CERT_H" >> ${CA_CERT_HEADER}
        COMMAND ${CMAKE_COMMAND} -E echo "static const char embedded_ca_cert[] = R\\\"(\" >> ${CA_CERT_HEADER}
        COMMAND ${CMAKE_COMMAND} -E cat "${CA_CERT_FILE}" >> ${CA_CERT_HEADER}
        COMMAND ${CMAKE_COMMAND} -E echo \")\\\";" >> ${CA_CERT_HEADER}
        COMMAND ${CMAKE_COMMAND} -E echo "#endif" >> ${CA_CERT_HEADER}
        DEPENDS "${CA_CERT_FILE}"
        COMMENT "Generating embedded CA certificate header"
    )
    
    add_custom_target(generate_ca_cert DEPENDS ${CA_CERT_HEADER})
    add_dependencies(${COMPONENT_LIB} generate_ca_cert)
    
    target_include_directories(${COMPONENT_LIB} PRIVATE ${GENERATED_DIR})
endif()
