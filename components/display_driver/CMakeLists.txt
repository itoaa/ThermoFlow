idf_component_register(
    SRCS "display_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES driver

target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_SOURCE_DIR}/include") esp_timer
)