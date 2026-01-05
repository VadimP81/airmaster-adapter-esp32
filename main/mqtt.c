#include "mqtt.h"
#include "settings.h"
#include "am7.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mqtt_client.h"

static const char *TAG = "MQTT";

bool mqtt_connected = false;
static esp_mqtt_client_handle_t client = NULL;

bool connect_to_mqtt(const char *broker, int port, const char *user, const char *pass)
{
    esp_mqtt_client_config_t cfg = {
        .uri = broker,
        .port = port,
        .username = user,
        .password = pass
    };
    client = esp_mqtt_client_init(&cfg);
    if(!client) return false;
    esp_mqtt_client_start(client);
    mqtt_connected = true;
    return true;
}

bool mqtt_publish(const char *topic, const char *payload)
{
    if(!client || !mqtt_connected) return false;
    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
    return (msg_id > 0);
}

void mqtt_task(void *arg)
{
    int backoff = 1;
    while(1)
    {
        if(!mqtt_connected) {
            ESP_LOGI(TAG, "Connecting MQTT...");
            if(connect_to_mqtt(settings_get_mqtt_broker(), settings_get_mqtt_port(),
                               settings_get_mqtt_user(), settings_get_mqtt_pass())) {
                backoff = 1;
                ESP_LOGI(TAG, "MQTT connected");
            } else {
                ESP_LOGW(TAG, "MQTT connect failed, retry in %d sec", backoff);
                vTaskDelay(pdMS_TO_TICKS(backoff*1000));
                backoff = (backoff*2>60)?60:backoff*2;
                continue;
            }
        }

        if(am7_connected) {
            char payload[256];
            snprintf(payload, sizeof(payload),
                     "{\"temp\":%.1f,\"humidity\":%.1f,\"co2\":%d,\"pm25\":%d,\"voc\":%.2f}",
                     am7_data.temp, am7_data.humidity,
                     am7_data.co2, am7_data.pm25, am7_data.voc);

            if(!mqtt_publish("airmaster/sensors", payload)) {
                ESP_LOGW(TAG, "MQTT publish failed");
                mqtt_connected = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(settings_get_interval()*1000));
