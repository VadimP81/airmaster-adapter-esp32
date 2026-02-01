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
    
    // Check if WiFi credentials are configured
    const char* ssid = settings_get_wifi_ssid();
    if (ssid && strlen(ssid) > 0) {
        ESP_LOGI(TAG, "WiFi credentials found, connecting to network...");
        wifi_connect(ssid, settings_get_wifi_password());
    } else {
        ESP_LOGW(TAG, "No WiFi credentials configured.");
        ESP_LOGI(TAG, "Starting Access Point mode for setup...");
        ESP_LOGI(TAG, "Connect to WiFi: 'AirMaster-Setup' (no password)");
        ESP_LOGI(TAG, "Then open http://192.168.4.1/ in your browser");
        wifi_start_ap();
    }

    // Start tasks
    xTaskCreate(am7_task, "am7_task", 4096, NULL, 5, NULL);
    xTaskCreate(mqtt_task, "mqtt_task", 4096, NULL, 5, NULL);

    // Start web server
    web_server_start();

    ESP_LOGI(TAG, "System initialized successfully");
}
