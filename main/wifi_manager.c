#include "wifi_manager.h"
#include "settings.h"
#include "captive_portal.h"
#include "config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char *TAG = "WIFI";
static EventGroupHandle_t wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static bool connected = false;
static bool ap_mode = false;
static int sta_retry_count = 0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        connected = false;
        sta_retry_count++;
        
        if (sta_retry_count < CONFIG_WIFI_MAX_STA_RETRY) {
            ESP_LOGI(TAG, "Disconnected from AP, retry %d/%d...", sta_retry_count, CONFIG_WIFI_MAX_STA_RETRY);
            esp_wifi_connect();
        } else {
            ESP_LOGW(TAG, "Failed to connect after %d attempts, starting AP mode...", CONFIG_WIFI_MAX_STA_RETRY);
            wifi_start_ap();
        }
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        connected = true;
        sta_retry_count = 0;  // Reset retry counter on successful connection
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station " MACSTR " joined, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station " MACSTR " left, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    
    // Set DHCP hostname from settings
    if (sta_netif) {
        const char *hostname = settings_get_hostname();
        if (hostname && strlen(hostname) > 0) {
            esp_netif_set_hostname(sta_netif, hostname);
            ESP_LOGI(TAG, "DHCP hostname set to: %s", hostname);
        }
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // Don't start WiFi here - let wifi_connect() or wifi_start_ap() do it

    ESP_LOGI(TAG, "WiFi initialization complete");
}

bool wifi_is_connected(void)
{
    return connected;
}

int wifi_get_rssi(void)
{
    if (!connected) {
        return 0;
    }
    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err == ESP_OK) {
        return ap_info.rssi;
    }
    return 0;
}

esp_err_t wifi_connect(const char *ssid, const char *password)
{
    if (!ssid || strlen(ssid) == 0) {
        ESP_LOGE(TAG, "SSID cannot be empty");
        return ESP_ERR_INVALID_ARG;
    }

    // Reset retry counter when attempting new connection
    sta_retry_count = 0;
    ap_mode = false;

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    
    if (password && strlen(password) > 0) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    wifi_config.sta.threshold.authmode = (password && strlen(password) > 0) ? 
                                         WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", ssid);
    return esp_wifi_connect();
}

esp_err_t wifi_start_ap(void)
{
    ESP_LOGI(TAG, "Starting Access Point mode...");
    
    // Stop any existing WiFi
    esp_wifi_stop();
    
        // Stop captive portal if running
        captive_portal_stop();
    
    // Create AP network interface
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif) {
        esp_netif_set_hostname(ap_netif, "airmaster-setup");
    }
    
    // Configure AP
    wifi_config_t ap_config = {0};
    memcpy(ap_config.ap.ssid, CONFIG_AP_SSID, strlen(CONFIG_AP_SSID));
    memcpy(ap_config.ap.password, CONFIG_AP_PASSWORD, strlen(CONFIG_AP_PASSWORD));
    ap_config.ap.ssid_len = strlen(CONFIG_AP_SSID);
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = CONFIG_AP_MAX_CONNECTIONS;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ap_config.ap.pmf_cfg.required = false;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    
        // Start captive portal DNS server
        captive_portal_start();
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ap_mode = true;
    ESP_LOGI(TAG, "WiFi AP started. SSID: %s, IP: 192.168.4.1", CONFIG_AP_SSID);
    
    return ESP_OK;
}

bool wifi_is_ap_mode(void)
{
    return ap_mode;
}
