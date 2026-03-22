idf_component_register(
    SRCS "test_main.c" "test_sht4x.c" "test_fan_controller.c" "test_anti_condensation.c"
    INCLUDE_DIRS "." "../components/sht4x_sensor/include" "../components/fan_control/include" "../components/anti_condensation/include"
    REQUIRES unity sht4x_sensor fan_control anti_condensation
)