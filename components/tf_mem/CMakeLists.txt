idf_component_register(
    SRCS "tf_mem.c"
    INCLUDE_DIRS "include"
    REQUIRES heap esp_common log
    PRIV_REQUIRES esp_psram
)
