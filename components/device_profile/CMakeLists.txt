idf_component_register(
    SRCS "device_profile.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash driver
)

target_include_directories(${COMPONENT_LIB} PUBLIC "${CMAKE_SOURCE_DIR}/include")