idf_component_register(
    SRCS "hardware_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer
)

# Add global include directory for thermoflow_config.h
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_SOURCE_DIR}/include")