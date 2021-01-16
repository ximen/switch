idf_component_register(
    SRCS "app_board.c" "app_common.c" "app_ble.c" "app_mqtt.c" "main.c"
    INCLUDE_DIRS "include"
    REQUIRES esp32-config mqtt
)
