#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "am7.h"
#include "mqtt.h"
#include "settings.h"
#include "web_server.h"

void app_main(void)
{
    ESP_LOGI("MAIN", "Starting AirMaster Gateway...");

    settings_init();

    xTaskCreate(am7_task, "am7_task", 4096, NULL, 5, NULL);
    xTaskCreate(mqtt_task, "mqtt_task", 4096, NULL, 5, NULL);

    web_server_start();

    ESP_LOGI("MAIN", "System initialized");
}
