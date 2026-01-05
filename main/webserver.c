#include "esp_log.h"
#include "esp_http_server.h"

static const char *TAG = "WEB";

void web_server_start(void)
{
    ESP_LOGI(TAG, "Starting web server...");

    // TODO: Implement SPIFFS mounting
    // Serve index.html, settings.html, style.css, app.js, settings.js
    // Implement API endpoints:
    // GET /api/status
    // GET /api/settings
    // POST /api/settings
    // POST /api/reboot
}
