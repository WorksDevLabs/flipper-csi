idf_component_register(
    SRCS "main.c" "csi_motion.c" "../../flipper_app/csi_protocol.c"
    INCLUDE_DIRS "." "../../flipper_app"
    REQUIRES esp_wifi esp_event esp_netif esp_timer nvs_flash esp_driver_uart lwip
)
