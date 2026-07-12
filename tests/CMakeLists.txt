idf_component_register(
    SRCS "test_main.c" "test_sht4x.c" "test_fan_controller.c" "test_anti_condensation.c"
           "test_heat_recovery.c" "test_secure_boot.c"
    INCLUDE_DIRS "."
                 "../components/sht4x_sensor/include"
                 "../components/fan_control/include"
                 "../components/anti_condensation/include"
                 "../components/heat_recovery/include"
    REQUIRES unity sht4x_sensor fan_control anti_condensation heat_recovery efuse esp_secure_boot
)