#include "settings.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SETTINGS";
static const char *NVS_NAMESPACE = "settings";

static int interval = 10;
static char mqtt_broker[64] = "192.168.1.94";
static int mqtt_port = 1883;
static char mqtt_user[32] = "homeassistant";
static char mqtt_pass[32] = "homeassistant";
static char mqtt_topic[64] = "airmaster/sensors";
static char device_name[32] = "AirMaster Adapter";
static bool ha_discovery_enabled = true;
static char wifi_ssid[32] = "ejeg";
static char wifi_password[64] = "ejmegapass";
static char hostname[32] = "sh-airmaster-adapter-esp";

void settings_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Load settings from NVS
    nvs_handle_t handle;
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret == ESP_OK) {
        size_t len;
        int32_t temp_interval = interval;
        int32_t temp_port = mqtt_port;
        
        nvs_get_i32(handle, "interval", &temp_interval);
        interval = (int)temp_interval;
        
        len = sizeof(mqtt_broker);
        nvs_get_str(handle, "mqtt_broker", mqtt_broker, &len);
        
        nvs_get_i32(handle, "mqtt_port", &temp_port);
        mqtt_port = (int)temp_port;
        
        len = sizeof(mqtt_user);
        nvs_get_str(handle, "mqtt_user", mqtt_user, &len);
        
        len = sizeof(mqtt_pass);
        nvs_get_str(handle, "mqtt_pass", mqtt_pass, &len);
        
        len = sizeof(mqtt_topic);
        nvs_get_str(handle, "mqtt_topic", mqtt_topic, &len);
        
        len = sizeof(device_name);
        nvs_get_str(handle, "device_name", device_name, &len);
        
        uint8_t ha_disc = 1;
        nvs_get_u8(handle, "ha_discovery", &ha_disc);
        ha_discovery_enabled = (ha_disc != 0);
        
        len = sizeof(wifi_ssid);
        nvs_get_str(handle, "wifi_ssid", wifi_ssid, &len);
        
        len = sizeof(wifi_password);
        nvs_get_str(handle, "wifi_password", wifi_password, &len);
        
        len = sizeof(hostname);
        nvs_get_str(handle, "hostname", hostname, &len);
        
        nvs_close(handle);
        ESP_LOGI(TAG, "Settings loaded from NVS");
    } else {
        ESP_LOGW(TAG, "No saved settings found, using defaults");
    }

    ESP_LOGI(TAG, "Interval: %d sec, MQTT: %s:%d, Topic: %s", 
             interval, mqtt_broker, mqtt_port, mqtt_topic);
}

esp_err_t settings_save(void) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle");
        return ret;
    }

    nvs_set_i32(handle, "interval", (int32_t)interval);
    nvs_set_str(handle, "mqtt_broker", mqtt_broker);
    nvs_set_i32(handle, "mqtt_port", (int32_t)mqtt_port);
    nvs_set_str(handle, "mqtt_user", mqtt_user);
    nvs_set_str(handle, "mqtt_pass", mqtt_pass);
    nvs_set_str(handle, "mqtt_topic", mqtt_topic);
    nvs_set_str(handle, "device_name", device_name);
    nvs_set_u8(handle, "ha_discovery", ha_discovery_enabled ? 1 : 0);
    nvs_set_str(handle, "wifi_ssid", wifi_ssid);
    nvs_set_str(handle, "wifi_password", wifi_password);
    nvs_set_str(handle, "hostname", hostname);

    ret = nvs_commit(handle);
    nvs_close(handle);
    
    ESP_LOGI(TAG, "Settings saved to NVS");
    return ret;
}

void settings_set_interval(int value) { interval = value; }
void settings_set_mqtt_broker(const char *value) { strncpy(mqtt_broker, value, sizeof(mqtt_broker) - 1); }
void settings_set_mqtt_port(int value) { mqtt_port = value; }
void settings_set_mqtt_user(const char *value) { strncpy(mqtt_user, value, sizeof(mqtt_user) - 1); }
void settings_set_mqtt_pass(const char *value) { strncpy(mqtt_pass, value, sizeof(mqtt_pass) - 1); }
void settings_set_mqtt_topic(const char *value) { strncpy(mqtt_topic, value, sizeof(mqtt_topic) - 1); }
void settings_set_device_name(const char *value) { strncpy(device_name, value, sizeof(device_name) - 1); }
void settings_set_ha_discovery_enabled(bool enabled) { ha_discovery_enabled = enabled; }
void settings_set_wifi_ssid(const char *value) { strncpy(wifi_ssid, value, sizeof(wifi_ssid) - 1); }
void settings_set_wifi_password(const char *value) { strncpy(wifi_password, value, sizeof(wifi_password) - 1); }
void settings_set_hostname(const char *value) { strncpy(hostname, value, sizeof(hostname) - 1); }

int settings_get_interval(void) { return interval; }
const char* settings_get_mqtt_broker(void) { return mqtt_broker; }
int settings_get_mqtt_port(void) { return mqtt_port; }
const char* settings_get_mqtt_user(void) { return mqtt_user; }
const char* settings_get_mqtt_pass(void) { return mqtt_pass; }
const char* settings_get_mqtt_topic(void) { return mqtt_topic; }
const char* settings_get_device_name(void) { return device_name; }
bool settings_get_ha_discovery_enabled(void) { return ha_discovery_enabled; }
const char* settings_get_wifi_ssid(void) { return wifi_ssid; }
const char* settings_get_wifi_password(void) { return wifi_password; }
const char* settings_get_hostname(void) { return hostname; }
