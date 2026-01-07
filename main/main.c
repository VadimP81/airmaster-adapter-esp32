#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "am7.h"
#include "mqtt.h"
#include "settings.h"
#include "webserver.h"
#include "wifi_manager.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting AirMaster Gateway...");

    // Initialize settings (NVS)
    settings_init();

    // Initialize WiFi
    wifi_init();
    // TODO: Get WiFi credentials from settings
    // wifi_connect(settings_get_wifi_ssid(), settings_get_wifi_password());

    // Start tasks
    xTaskCreate(am7_task, "am7_task", 4096, NULL, 5, NULL);
    xTaskCreate(mqtt_task, "mqtt_task", 4096, NULL, 5, NULL);

    // Start web server
    web_server_start();

    ESP_LOGI(TAG, "System initialized successfully");
}
