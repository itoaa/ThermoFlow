idf_component_register(
    SRCS "sensor_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES driver sht4x_sensor hardware_manager
)

target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_SOURCE_DIR}/include")