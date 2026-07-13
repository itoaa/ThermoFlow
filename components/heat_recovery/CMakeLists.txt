idf_component_register(
    SRCS "src/heat_recovery.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer
)

target_link_libraries(${COMPONENT_LIB} PRIVATE m)
