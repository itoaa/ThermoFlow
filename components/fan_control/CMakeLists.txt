idf_component_register(
    SRCS "fan_controller.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer esp_driver_ledc
)

target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_SOURCE_DIR}/include")
