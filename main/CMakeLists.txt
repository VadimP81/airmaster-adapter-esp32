# Component CMakeLists.txt for main component

idf_component_register(
    SRCS 
        "main.c"
        "am7.c"
        "mqtt.c"
        "settings.c"
        "webserver.c"
        "wifi_manager.c"
        "ota.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        nvs_flash
        esp_http_server
        esp_wifi
        esp_netif
        mqtt
        json
        spiffs
        usb
        app_update
)
