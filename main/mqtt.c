#include "mqtt.h"
#include "settings.h"
#include "am7.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mqtt_client.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "MQTT";

bool mqtt_connected = false;
static esp_mqtt_client_handle_t client = NULL;
static bool ha_discovery_sent = false;

// MQTT event handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            mqtt_connected = true;
            ha_discovery_sent = false; // Reset flag on reconnect
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            mqtt_connected = false;
            ha_discovery_sent = false;
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            mqtt_connected = false;
            break;
        default:
            break;
    }
}

bool connect_to_mqtt(const char *broker, int port, const char *user, const char *pass)
{
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = NULL;
    }

    // Build URI string
    char uri[128];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", broker, port);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = uri,
    };
    
    if (user && strlen(user) > 0) {
        cfg.credentials.username = user;
    }
    if (pass && strlen(pass) > 0) {
        cfg.credentials.authentication.password = pass;
    }

    client = esp_mqtt_client_init(&cfg);
    if(!client) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return false;
    }
    
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_err_t ret = esp_mqtt_client_start(client);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client");
        return false;
    }
    
    return true;
}

bool mqtt_publish(const char *topic, const char *payload)
{
    if(!client || !mqtt_connected) return false;
    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
    return (msg_id >= 0);
}

// Publish Home Assistant MQTT Discovery messages
bool mqtt_publish_ha_discovery(void)
{
    if (!mqtt_connected || !settings_get_ha_discovery_enabled()) {
        return false;
    }

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char device_id[32];
    snprintf(device_id, sizeof(device_id), "airmaster_%02x%02x%02x", mac[3], mac[4], mac[5]);
    
    const char *device_name = settings_get_device_name();
    const char *state_topic = settings_get_mqtt_topic();

    // Array of sensors to create
    struct {
        const char *name;
        const char *sensor_type;
        const char *value_template;
        const char *unit;
        const char *device_class;
        const char *state_class;
    } sensors[] = {
        {"Temperature", "temperature", "{{ value_json.temp }}", "°C", "temperature", "measurement"},
        {"Humidity", "humidity", "{{ value_json.humidity }}", "%", "humidity", "measurement"},
        {"CO2", "co2", "{{ value_json.co2 }}", "ppm", "carbon_dioxide", "measurement"},
        {"PM2.5", "pm25", "{{ value_json.pm25 }}", "µg/m³", "pm25", "measurement"},
        {"PM10", "pm10", "{{ value_json.pm10 }}", "µg/m³", "pm10", "measurement"},
        {"TVOC", "tvoc", "{{ value_json.tvoc }}", "ppb", "volatile_organic_compounds", "measurement"},
        {"HCHO", "hcho", "{{ value_json.hcho }}", "mg/m³", "volatile_organic_compounds", "measurement"},
    };

    for (int i = 0; i < 7; i++) {
        char config_topic[128];
        snprintf(config_topic, sizeof(config_topic), 
                 "homeassistant/sensor/%s_%s/config", device_id, sensors[i].sensor_type);

        cJSON *config = cJSON_CreateObject();
        cJSON_AddStringToObject(config, "name", sensors[i].name);
        
        char unique_id[64];
        snprintf(unique_id, sizeof(unique_id), "%s_%s", device_id, sensors[i].sensor_type);
        cJSON_AddStringToObject(config, "unique_id", unique_id);
        
        char object_id[64];
        snprintf(object_id, sizeof(object_id), "%s_%s", device_id, sensors[i].sensor_type);
        cJSON_AddStringToObject(config, "object_id", object_id);
        
        cJSON_AddStringToObject(config, "state_topic", state_topic);
        cJSON_AddStringToObject(config, "value_template", sensors[i].value_template);
        cJSON_AddStringToObject(config, "unit_of_measurement", sensors[i].unit);
        cJSON_AddStringToObject(config, "device_class", sensors[i].device_class);
        cJSON_AddStringToObject(config, "state_class", sensors[i].state_class);

        // Device information
        cJSON *device = cJSON_CreateObject();
        cJSON_AddStringToObject(device, "identifiers", device_id);
        cJSON_AddStringToObject(device, "name", device_name);
        cJSON_AddStringToObject(device, "model", "AM7 Gateway");
        cJSON_AddStringToObject(device, "manufacturer", "Custom");
        cJSON_AddStringToObject(device, "sw_version", "1.0.0");
        cJSON_AddItemToObject(config, "device", device);

        char *config_str = cJSON_PrintUnformatted(config);
        bool success = mqtt_publish(config_topic, config_str);
        
        cJSON_free(config_str);
        cJSON_Delete(config);

        if (!success) {
            ESP_LOGE(TAG, "Failed to publish discovery for %s", sensors[i].name);
            return false;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Small delay between messages
    }

    ESP_LOGI(TAG, "Home Assistant discovery messages published");
    return true;
}

void mqtt_task(void *arg)
{
    int backoff = 1;
    ESP_LOGI(TAG, "MQTT task started");
    
    // Wait for network connection
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    while(1)
    {
        if(!mqtt_connected) {
            ESP_LOGI(TAG, "Connecting to MQTT broker %s:%d...", 
                     settings_get_mqtt_broker(), settings_get_mqtt_port());
            if(connect_to_mqtt(settings_get_mqtt_broker(), settings_get_mqtt_port(),
                               settings_get_mqtt_user(), settings_get_mqtt_pass())) {
                backoff = 1;
                // Wait a bit for connection to establish
                vTaskDelay(pdMS_TO_TICKS(2000));
            } else {
                ESP_LOGW(TAG, "MQTT connect failed, retry in %d sec", backoff);
                vTaskDelay(pdMS_TO_TICKS(backoff*1000));
                backoff = (backoff*2>60)?60:backoff*2;
                continue;
            }
        }

        if(mqtt_connected && am7_connected) {
            char payload[256];
            snprintf(payload, sizeof(payload),
                     "{\"temp\":%.1f,\"humidity\":%.1f,\"co2\":%d,\"pm25\":%d,\"voc\":%.2f}",
            // Send Home Assistant discovery on first connection
            if (!ha_discovery_sent && settings_get_ha_discovery_enabled()) {
                mqtt_publish_ha_discovery();
                ha_discovery_sent = true;
            }

            char payload[256];
            snprintf(payload, sizeof(payload),
                     "{\"temp\":%.1f,\"humidity\":%.1f,\"co2\":%d,\"pm25\":%d,\"pm10\":%d,\"tvoc\":%.2f,\"hcho\":%.3f}",
                     am7_data.temp, am7_data.humidity,
                     am7_data.co2, am7_data.pm25, am7_data.pm10, am7_data.tvoc, am7_data.hcho);

            if(!mqtt_publish(settings_get_mqtt_topic(), payload)) {
                ESP_LOGW(TAG, "MQTT publish failed");
            } else {
                ESP_LOGD(TAG, "Published: %s", payload);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(settings_get_interval()*1000));
    }
}