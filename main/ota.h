#pragma once
#include "esp_err.h"
#include "esp_http_server.h"

// OTA update state
typedef enum {
    OTA_STATE_IDLE,
    OTA_STATE_IN_PROGRESS,
    OTA_STATE_SUCCESS,
    OTA_STATE_ERROR
} ota_state_t;

void ota_init(void);
esp_err_t ota_begin(void);
esp_err_t ota_write(const uint8_t *data, size_t len);
esp_err_t ota_end(void);
void ota_abort(void);
ota_state_t ota_get_state(void);
const char* ota_get_error_message(void);
int ota_get_progress_percent(void);

// HTTP handler for OTA upload
esp_err_t ota_upload_handler(httpd_req_t *req);
