#include "webserver.h"
#include "version.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "am7.h"
#include "mqtt.h"
#include "settings.h"
#include "wifi_manager.h"
#include "ota.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <time.h>

static const char *TAG = "WEB";
static httpd_handle_t server = NULL;

// Log buffer for storing recent logs
#define MAX_LOG_LINES 100
#define MAX_LOG_MESSAGE_LENGTH 200

typedef struct {
    char timestamp[32];  // ISO8601 format: 2026-02-02T12:34:56Z
    char level;          // I/W/E/D
    char message[MAX_LOG_MESSAGE_LENGTH];
} log_entry_t;

static log_entry_t log_buffer[MAX_LOG_LINES];
static int log_write_index = 0;
static int log_count = 0;
static SemaphoreHandle_t log_mutex = NULL;

// Custom log hook to capture logs with timestamps
static int custom_log_vprintf(const char *fmt, va_list args)
{
    // Call default vprintf first
    int ret = vprintf(fmt, args);
    
    // Capture to buffer if mutex is initialized
    if (log_mutex && xSemaphoreTake(log_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        log_entry_t *entry = &log_buffer[log_write_index];
        
        // Get uptime in milliseconds and format as ISO8601 duration
        uint64_t uptime_ms = esp_timer_get_time() / 1000;
        uint32_t total_seconds = uptime_ms / 1000;
        uint32_t milliseconds = uptime_ms % 1000;
        uint32_t days = total_seconds / 86400;
        uint32_t hours = (total_seconds % 86400) / 3600;
        uint32_t minutes = (total_seconds % 3600) / 60;
        uint32_t seconds = total_seconds % 60;
        
        // Format as ISO 8601 duration: P[n]DT[n]H[n]M[n].[n]S
        if (days > 0) {
            snprintf(entry->timestamp, sizeof(entry->timestamp),
                     "P%luDT%luH%luM%lu.%03luS",
                     (unsigned long)days, (unsigned long)hours, 
                     (unsigned long)minutes, (unsigned long)seconds,
                     (unsigned long)milliseconds);
        } else {
            snprintf(entry->timestamp, sizeof(entry->timestamp),
                     "PT%luH%luM%lu.%03luS",
                     (unsigned long)hours, (unsigned long)minutes, 
                     (unsigned long)seconds, (unsigned long)milliseconds);
        }
        
        // Create temporary buffer for full log
        char temp_buffer[MAX_LOG_MESSAGE_LENGTH + 50];
        vsnprintf(temp_buffer, sizeof(temp_buffer), fmt, args);
        
        // Parse log format: "I (1772) TAG: message"
        // Extract log level
        entry->level = temp_buffer[0];
        
        // Skip to message after ')'
        char *msg = strchr(temp_buffer, ')');
        if (msg) {
            msg++;
            while (*msg == ' ') msg++;  // Skip spaces
        } else {
            msg = temp_buffer;
        }
        
        // Copy message and remove trailing newline
        strncpy(entry->message, msg, MAX_LOG_MESSAGE_LENGTH - 1);
        entry->message[MAX_LOG_MESSAGE_LENGTH - 1] = '\0';
        size_t len = strlen(entry->message);
        if (len > 0 && entry->message[len - 1] == '\n') {
            entry->message[len - 1] = '\0';
        }
        
        log_write_index = (log_write_index + 1) % MAX_LOG_LINES;
        if (log_count < MAX_LOG_LINES) {
            log_count++;
        }
        
        xSemaphoreGive(log_mutex);
    }
    
    return ret;
}

// File serving handler
static esp_err_t serve_file(httpd_req_t *req, const char *filepath, const char *content_type)
{
    FILE *f = fopen(filepath, "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, content_type);
    
    // Add cache control headers to prevent browser caching
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    
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
    // If in AP mode, redirect to setup page for captive portal
    if (wifi_is_ap_mode()) {
        return serve_file(req, "/spiffs/setup.html", "text/html");
    }
    return serve_file(req, "/spiffs/index.html", "text/html");
}

// Setup page handler (for AP mode)
static esp_err_t setup_page_handler(httpd_req_t *req)
{
    return serve_file(req, "/spiffs/setup.html", "text/html");
}

// Captive portal redirect handler - catches all unknown routes in AP mode
static esp_err_t captive_portal_handler(httpd_req_t *req)
{
    // Redirect any unknown URL to setup page
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/setup");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
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

// Logs page handler
static esp_err_t logs_page_handler(httpd_req_t *req)
{
    return serve_file(req, "/spiffs/logs.html", "text/html");
}

static esp_err_t sensor_page_handler(httpd_req_t *req)
{
    return serve_file(req, "/spiffs/sensor.html", "text/html");
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

static esp_err_t sensor_js_handler(httpd_req_t *req)
{
    return serve_file(req, "/spiffs/sensor.js", "application/javascript");
}

// API: Get logs
static esp_err_t api_logs_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *logs_array = cJSON_CreateArray();
    
    if (log_mutex && xSemaphoreTake(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        int start_index = (log_count < MAX_LOG_LINES) ? 0 : log_write_index;
        int total_logs = (log_count < MAX_LOG_LINES) ? log_count : MAX_LOG_LINES;
        
        for (int i = 0; i < total_logs; i++) {
            int index = (start_index + i) % MAX_LOG_LINES;
            log_entry_t *entry = &log_buffer[index];
            if (entry->message[0] != '\0') {
                cJSON *log_obj = cJSON_CreateObject();
                cJSON_AddStringToObject(log_obj, "timestamp", entry->timestamp);
                cJSON_AddStringToObject(log_obj, "level", (char[]){entry->level, '\0'});
                cJSON_AddStringToObject(log_obj, "message", entry->message);
                cJSON_AddItemToArray(logs_array, log_obj);
            }
        }
        
        xSemaphoreGive(log_mutex);
    }
    
    cJSON_AddItemToObject(root, "logs", logs_array);
    cJSON_AddNumberToObject(root, "count", log_count);
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// API: Clear logs
static esp_err_t api_logs_clear_handler(httpd_req_t *req)
{
    if (log_mutex && xSemaphoreTake(log_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memset(log_buffer, 0, sizeof(log_buffer));
        log_write_index = 0;
        log_count = 0;
        xSemaphoreGive(log_mutex);
        ESP_LOGI(TAG, "Logs cleared");
    }
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "ok", true);
    char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    cJSON_free(json_str);
    cJSON_Delete(response);
    return ESP_OK;
}

// API: Get status
static esp_err_t api_status_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", VERSION_STRING);
    cJSON_AddNumberToObject(root, "build", VERSION_BUILD);
    cJSON_AddStringToObject(root, "build_date", BUILD_DATE);
    cJSON_AddStringToObject(root, "git_hash", GIT_HASH);
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

// API: Sensor values
static esp_err_t api_sensor_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", VERSION_STRING);
    cJSON_AddBoolToObject(root, "connected", am7_connected);
    cJSON_AddNumberToObject(root, "last_rx_sec", last_rx_sec);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "temp", am7_data.temp);
    cJSON_AddNumberToObject(data, "humidity", am7_data.humidity);
    cJSON_AddNumberToObject(data, "co2", am7_data.co2);
    cJSON_AddNumberToObject(data, "pm25", am7_data.pm25);
    cJSON_AddNumberToObject(data, "pm10", am7_data.pm10);
    cJSON_AddNumberToObject(data, "tvoc", am7_data.tvoc);
    cJSON_AddNumberToObject(data, "hcho", am7_data.hcho);
    cJSON_AddItemToObject(root, "data", data);

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
    cJSON_AddStringToObject(root, "version", VERSION_STRING);
    
    cJSON *wifi = cJSON_CreateObject();
    cJSON_AddStringToObject(wifi, "ssid", settings_get_wifi_ssid());
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
    cJSON_AddStringToObject(root, "hostname", settings_get_hostname());

    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    cJSON_free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

// API: Save settings
static esp_err_t api_post_settings_handler(httpd_req_t *req) {
    // Add CORS headers
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    
    char buffer[512];
    int ret = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
    if (ret <= 0) {
        const char *err_resp = "{\"ok\":false,\"error\":\"Failed to receive data\"}";
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
    
    // Parse WiFi settings
    cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
    if (wifi) {
        cJSON *ssid = cJSON_GetObjectItem(wifi, "ssid");
        cJSON *pass = cJSON_GetObjectItem(wifi, "pass");
        
        if (ssid && cJSON_IsString(ssid) && strlen(ssid->valuestring) > 0) {
            settings_set_wifi_ssid(ssid->valuestring);
            settings_updated = true;
        }
        if (pass && cJSON_IsString(pass) && strlen(pass->valuestring) > 0) {
            settings_set_wifi_password(pass->valuestring);
            settings_updated = true;
        }
    }
    
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (mqtt) {
        cJSON *broker = cJSON_GetObjectItem(mqtt, "broker");
        cJSON *port = cJSON_GetObjectItem(mqtt, "port");
        cJSON *user = cJSON_GetObjectItem(mqtt, "user");
        cJSON *pass = cJSON_GetObjectItem(mqtt, "pass");
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
        settings_set_ha_discovery_enabled(cJSON_IsTrue(ha_discovery));
        settings_updated = true;
    }
    
    cJSON *hostname = cJSON_GetObjectItem(root, "hostname");
    if (hostname && cJSON_IsString(hostname)) {
        settings_set_hostname(hostname->valuestring);
        settings_updated = true;
    }
    
    // Save to NVS
    esp_err_t save_result = ESP_OK;
    if (settings_updated) {
        save_result = settings_save();
    }
    
    cJSON_Delete(root);
    
    // Create response
    cJSON *response = cJSON_CreateObject();
    if (save_result == ESP_OK) {
        cJSON_AddBoolToObject(response, "ok", true);
        cJSON_AddStringToObject(response, "message", "Settings saved successfully");
    } else {
        cJSON_AddBoolToObject(response, "ok", false);
        cJSON_AddStringToObject(response, "error", "Failed to save settings to flash");
    }
    
    char *json_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);
    
    cJSON_free(json_str);
    cJSON_Delete(response);
    return ESP_OK;
}

// CORS preflight handler
static esp_err_t api_options_handler(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
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
    
    // Initialize log mutex
    if (!log_mutex) {
        log_mutex = xSemaphoreCreateMutex();
        memset(log_buffer, 0, sizeof(log_buffer));
        
        // Install custom log handler to capture logs
        esp_log_set_vprintf(custom_log_vprintf);
        ESP_LOGI(TAG, "Log capture enabled");
    }
    
    // Mount SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }

    // Start HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 24;  // Increase from default 8 to handle all endpoints
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return;
    }

    // Register URI handlers
    httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_handler};
    httpd_register_uri_handler(server, &index_uri);

    httpd_uri_t settings_uri = {.uri = "/settings", .method = HTTP_GET, .handler = settings_page_handler};
    httpd_register_uri_handler(server, &settings_uri);

    httpd_uri_t sensor_page_uri = {.uri = "/sensor", .method = HTTP_GET, .handler = sensor_page_handler};
    httpd_register_uri_handler(server, &sensor_page_uri);

    httpd_uri_t ota_page_uri = {.uri = "/ota", .method = HTTP_GET, .handler = ota_page_handler};
    httpd_register_uri_handler(server, &ota_page_uri);

    httpd_uri_t logs_page_uri = {.uri = "/logs", .method = HTTP_GET, .handler = logs_page_handler};
    httpd_register_uri_handler(server, &logs_page_uri);

    httpd_uri_t setup_page_uri = {.uri = "/setup", .method = HTTP_GET, .handler = setup_page_handler};
    httpd_register_uri_handler(server, &setup_page_uri);

    httpd_uri_t css_uri = {.uri = "/style.css", .method = HTTP_GET, .handler = css_handler};
    httpd_register_uri_handler(server, &css_uri);

    httpd_uri_t app_js_uri = {.uri = "/app.js", .method = HTTP_GET, .handler = app_js_handler};
    httpd_register_uri_handler(server, &app_js_uri);

    httpd_uri_t settings_js_uri = {.uri = "/settings.js", .method = HTTP_GET, .handler = settings_js_handler};
    httpd_register_uri_handler(server, &settings_js_uri);

    httpd_uri_t sensor_js_uri = {.uri = "/sensor.js", .method = HTTP_GET, .handler = sensor_js_handler};
    httpd_register_uri_handler(server, &sensor_js_uri);

    httpd_uri_t api_status_uri = {.uri = "/api/status", .method = HTTP_GET, .handler = api_status_handler};
    ret = httpd_register_uri_handler(server, &api_status_uri);
    ESP_LOGI(TAG, "Registered /api/status: %s", ret == ESP_OK ? "OK" : esp_err_to_name(ret));

    httpd_uri_t api_sensor_uri = {.uri = "/api/sensor", .method = HTTP_GET, .handler = api_sensor_handler};
    ret = httpd_register_uri_handler(server, &api_sensor_uri);
    ESP_LOGI(TAG, "Registered /api/sensor: %s", ret == ESP_OK ? "OK" : esp_err_to_name(ret));

    httpd_uri_t api_logs_uri = {.uri = "/api/logs", .method = HTTP_GET, .handler = api_logs_handler};
    ret = httpd_register_uri_handler(server, &api_logs_uri);
    ESP_LOGI(TAG, "Registered /api/logs: %s", ret == ESP_OK ? "OK" : esp_err_to_name(ret));

    httpd_uri_t api_logs_clear_uri = {.uri = "/api/logs/clear", .method = HTTP_POST, .handler = api_logs_clear_handler};
    ret = httpd_register_uri_handler(server, &api_logs_clear_uri);
    ESP_LOGI(TAG, "Registered /api/logs/clear: %s", ret == ESP_OK ? "OK" : esp_err_to_name(ret));

    httpd_uri_t api_get_settings_uri = {.uri = "/api/settings", .method = HTTP_GET, .handler = api_get_settings_handler};
    ret = httpd_register_uri_handler(server, &api_get_settings_uri);
    ESP_LOGI(TAG, "Registered GET /api/settings: %s", ret == ESP_OK ? "OK" : esp_err_to_name(ret));

    httpd_uri_t api_options_settings_uri = {.uri = "/api/settings", .method = HTTP_OPTIONS, .handler = api_options_handler};
    ret = httpd_register_uri_handler(server, &api_options_settings_uri);
    ESP_LOGI(TAG, "Registered OPTIONS /api/settings: %s", ret == ESP_OK ? "OK" : esp_err_to_name(ret));

    httpd_uri_t api_post_settings_uri = {.uri = "/api/settings", .method = HTTP_POST, .handler = api_post_settings_handler};
    ret = httpd_register_uri_handler(server, &api_post_settings_uri);
    ESP_LOGI(TAG, "Registered POST /api/settings: %s", ret == ESP_OK ? "OK" : esp_err_to_name(ret));

    httpd_uri_t api_options_reboot_uri = {.uri = "/api/reboot", .method = HTTP_OPTIONS, .handler = api_options_handler};
    ret = httpd_register_uri_handler(server, &api_options_reboot_uri);
    ESP_LOGI(TAG, "Registered OPTIONS /api/reboot: %s", ret == ESP_OK ? "OK" : esp_err_to_name(ret));

    httpd_uri_t api_reboot_uri = {.uri = "/api/reboot", .method = HTTP_POST, .handler = api_reboot_handler};
    ret = httpd_register_uri_handler(server, &api_reboot_uri);
    ESP_LOGI(TAG, "Registered POST /api/reboot: %s", ret == ESP_OK ? "OK" : esp_err_to_name(ret));

    httpd_uri_t api_ota_uri = {.uri = "/api/ota", .method = HTTP_POST, .handler = ota_upload_handler};
    ret = httpd_register_uri_handler(server, &api_ota_uri);
    ESP_LOGI(TAG, "Registered /api/ota: %s", ret == ESP_OK ? "OK" : esp_err_to_name(ret));

    httpd_uri_t api_ota_spiffs_uri = {.uri = "/api/ota/spiffs", .method = HTTP_POST, .handler = ota_spiffs_upload_handler};
    ret = httpd_register_uri_handler(server, &api_ota_spiffs_uri);
    ESP_LOGI(TAG, "Registered /api/ota/spiffs: %s", ret == ESP_OK ? "OK" : esp_err_to_name(ret));

    // Wildcard handler for captive portal (must be last)
    if (wifi_is_ap_mode()) {
        httpd_uri_t wildcard_uri = {.uri = "/*", .method = HTTP_GET, .handler = captive_portal_handler};
        httpd_register_uri_handler(server, &wildcard_uri);
        ESP_LOGI(TAG, "Captive portal active - wildcard redirect enabled");
    }

    ESP_LOGI(TAG, "Web server started");
}
