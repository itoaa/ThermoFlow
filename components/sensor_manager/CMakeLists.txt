idf_component_register(
    SRCS "sensor_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES driver sht4x_sensor hardware_manager esp_hw_support
)

target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_SOURCE_DIR}/include")

target_link_libraries(${COMPONENT_LIB} PRIVATE m)