#include "webserver.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "am7.h"
#include "mqtt.h"
#include "settings.h"
#include "wifi_manager.h"
#include "ota.h"
#include "esp_system.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "WEB";
static httpd_handle_t server = NULL;

// File serving handler
static esp_err_t serve_file(httpd_req_t *req, const char *filepath, const char *content_type)
{
    FILE *f = fopen(filepath, "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, content_type);
    char buffer[512];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }
    httpd_resp_send_chunk(req, NULL, 0); // End chunk
    fclose(f);
    return ESP_OK;
}

// Root handler
static esp_err_t index_handler(httpd_req_t *req)
{
    return serve_file(req, "/spiffs/index.html", "text/html");
}

// Settings page handler
static esp_err_t settings_page_handler(httpd_req_t *req)
{
    return serve_file(req, "/spiffs/settings.html", "text/html");
}

// OTA page handler
static esp_err_t ota_page_handler(httpd_req_t *req)
{
    return serve_file(req, "/spiffs/ota.html", "text/html");
}

// CSS handler
static esp_err_t css_handler(httpd_req_t *req)
{
    return serve_file(req, "/spiffs/style.css", "text/css");
}

// JS handlers
static esp_err_t app_js_handler(httpd_req_t *req)
{
    return serve_file(req, "/spiffs/app.js", "application/javascript");
}

static esp_err_t settings_js_handler(httpd_req_t *req)
{
    return serve_file(req, "/spiffs/settings.js", "application/javascript");
}

// API: Get status
static esp_err_t api_status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", "1.0.0");
    cJSON_AddNumberToObject(root, "uptime", esp_timer_get_time() / 1000000);
    cJSON_AddNumberToObject(root, "interval", settings_get_interval());
    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());

    cJSON *am7 = cJSON_CreateObject();
    cJSON_AddBoolToObject(am7, "connected", am7_connected);
    cJSON_AddNumberToObject(am7, "last_rx_sec", last_rx_sec);
    cJSON_AddItemToObject(root, "am7", am7);

    cJSON *mqtt = cJSON_CreateObject();
    cJSON_AddBoolToObject(mqtt, "connected", mqtt_connected);
    cJSON_AddItemToObject(root, "mqtt", mqtt);

    cJSON *wifi = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi, "connected", wifi_is_connected());
    cJSON_AddItemToObject(root, "wifi", wifi);

    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// API: Get settings
static esp_err_t api_get_settings_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", "1.0.0");
    
    cJSON *wifi = cJSON_CreateObject();
    cJSON_AddStringToObject(wifi, "ssid", ""); // Would need to store this
    cJSON_AddItemToObject(root, "wifi", wifi);

    cJSON *mqtt = cJSON_CreateObject();
    cJSON_AddStringToObject(mqtt, "broker", settings_get_mqtt_broker());
    cJSON_AddNumberToObject(mqtt, "port", settings_get_mqtt_port());
    cJSON_AddStringToObject(mqtt, "user", settings_get_mqtt_user());
    cJSON_AddStringToObject(mqtt, "topic", settings_get_mqtt_topic());
    cJSON_AddItemToObject(root, "mqtt", mqtt);

    cJSON_AddNumberToObject(root, "interval", settings_get_interval());
    cJSON_AddStringToObject(root, "device_name", settings_get_device_name());
    cJSON_AddBoolToObject(root, "ha_discovery", settings_get_ha_discovery_enabled());

    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// API: Save settings
static econst char *err_resp = "{\"ok\":false,\"error\":\"Failed to receive data\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, err_resp, strlen(err_resp));
        return ESP_FAIL;
    }
    buffer[ret] = '\0';

    cJSON *root = cJSON_Parse(buffer);
    if (!root) {
        const char *err_resp = "{\"ok\":false,\"error\":\"Invalid JSON format\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, err_resp, strlen(err_resp));
        return ESP_FAIL;
    }

    // Parse and update settings
    bool settings_updated = false;
    
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (mqtt) {
        cJSON *broker = cJSON_GetObjectItem(mqtt, "broker");
        cJSON *topic = cJSON_GetObjectItem(mqtt, "topic");
        
        if (broker && cJSON_IsString(broker)) {
            settings_set_mqtt_broker(broker->valuestring);
            settings_updated = true;
        }
        if (port && cJSON_IsNumber(port)) {
            settings_set_mqtt_port(port->valueint);
            settings_updated = true;
        }
        if (user && cJSON_IsString(user)) {
            settings_set_mqtt_user(user->valuestring);
            settings_updated = true;
        }
        if (pass && cJSON_IsString(pass) && strlen(pass->valuestring) > 0) {
            settings_set_mqtt_pass(pass->valuestring);
            settings_updated = true;
        }
        if (topic && cJSON_IsString(topic)) {
            settings_set_mqtt_topic(topic->valuestring);
            settings_updated = true;
        }
    }
    
    cJSON *interval = cJSON_GetObjectItem(root, "interval");
    if (interval && cJSON_IsNumber(interval)) {
        settings_set_interval(interval->valueint);
        settings_updated = true;
    }
    
    cJSON *device_name = cJSON_GetObjectItem(root, "device_name");
    if (device_name && cJSON_IsString(device_name)) {
        settings_set_device_name(device_name->valuestring);
        settings_updated = true;
    }
    
    cJSON *ha_discovery = cJSON_GetObjectItem(root, "ha_discovery");
    if (ha_discovery && cJSON_IsBool(ha_discovery)) {
        settings_set_ha_discovery_enabled(cJSON_IsTrue(ha_discovery)
    cJSON *interval = cJSON_GetObjectItem(root, "interval");
    if (interval && cJSON_IsNumber(interval)) {
        settings_set_interval(interval->valueint);
        settings_updated = true;
    }
Initialize OTA
    ota_init();

    // 
    // Save to NVS
    esp_err_t save_result = ESP_OK;
    if (settings_updated) {
        save_result = settings_save();
    }

    cJSON *response = cJSON_CreateObject();
    if (save_result == ESP_OK) {
        cJSON_AddBoolToObject(response, "ok", true);
        cJSON_AddStringToObject(response, "message", "Settings saved successfully");
    } else {
        cJSON_AddBoolToObject(response, "ok", false);
        cJSON_AddStringToObject(response, "error", "Failed to save settings to flash");
    }
    
    // TODO: Parse and save settings to NVS
    ESP_LOGI(TAG, "Settings received: %s", buffer);

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "ok", true);
    char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    cJSON_free(json_str);
    cJSON_Delete(response);
    cJSON_Delete(root);
    return ESP_OK;
}

// API: Reboot
static esp_err_t api_reboot_handler(httpd_req_t *req)
{
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "ok", true);
    char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    cJSON_free(json_str);
    cJSON_Delete(response);

    // Reboot after 1 second
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

void web_server_start(void)
{
    ESP_LOGI(TAG, "Starting web server...");
httpd_uri_t api_ota_uri = {.uri = "/api/ota", .method = HTTP_POST, .handler = ota_upload_handler};
    httpd_register_uri_handler(server, &api_ota_uri);

    
    // Mount SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }

    // Start HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    // Register URI handlers
    httpd_uri_t ota_page_uri = {.uri = "/ota", .method = HTTP_GET, .handler = ota_page_handler};
    httpd_register_uri_handler(server, &ota_page_uri);

    httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_handler};
    httpd_register_uri_handler(server, &index_uri);

    httpd_uri_t settings_uri = {.uri = "/settings", .method = HTTP_GET, .handler = settings_page_handler};
    httpd_register_uri_handler(server, &settings_uri);

    httpd_uri_t css_uri = {.uri = "/style.css", .method = HTTP_GET, .handler = css_handler};
    httpd_register_uri_handler(server, &css_uri);

    httpd_uri_t app_js_uri = {.uri = "/app.js", .method = HTTP_GET, .handler = app_js_handler};
    httpd_register_uri_handler(server, &app_js_uri);

    httpd_uri_t settings_js_uri = {.uri = "/settings.js", .method = HTTP_GET, .handler = settings_js_handler};
    httpd_register_uri_handler(server, &settings_js_uri);

    httpd_uri_t api_status_uri = {.uri = "/api/status", .method = HTTP_GET, .handler = api_status_handler};
    httpd_register_uri_handler(server, &api_status_uri);

    httpd_uri_t api_get_settings_uri = {.uri = "/api/settings", .method = HTTP_GET, .handler = api_get_settings_handler};
    httpd_register_uri_handler(server, &api_get_settings_uri);

    httpd_uri_t api_post_settings_uri = {.uri = "/api/settings", .method = HTTP_POST, .handler = api_post_settings_handler};
    httpd_register_uri_handler(server, &api_post_settings_uri);

    httpd_uri_t api_reboot_uri = {.uri = "/api/reboot", .method = HTTP_POST, .handler = api_reboot_handler};
    httpd_register_uri_handler(server, &api_reboot_uri);

    ESP_LOGI(TAG, "Web server started");
}
