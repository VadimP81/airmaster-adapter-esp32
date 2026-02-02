#include "ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include <string.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "OTA";

static esp_ota_handle_t ota_handle = 0;
static const esp_partition_t *update_partition = NULL;
static ota_state_t ota_state = OTA_STATE_IDLE;
static char error_message[128] = {0};
static size_t bytes_written = 0;
static size_t total_size = 0;

void ota_init(void)
{
    ESP_LOGI(TAG, "OTA module initialized");
}

esp_err_t ota_begin(void)
{
    if (ota_state == OTA_STATE_IN_PROGRESS) {
        snprintf(error_message, sizeof(error_message), "OTA already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    // Get next OTA partition
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        snprintf(error_message, sizeof(error_message), "No OTA partition found");
        ESP_LOGE(TAG, "%s", error_message);
        ota_state = OTA_STATE_ERROR;
        return ESP_FAIL;
    }

    // Begin OTA update
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        snprintf(error_message, sizeof(error_message), "OTA begin failed: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", error_message);
        ota_state = OTA_STATE_ERROR;
        return err;
    }

    bytes_written = 0;
    total_size = 0;
    ota_state = OTA_STATE_IN_PROGRESS;
    ESP_LOGI(TAG, "OTA update started");
    return ESP_OK;
}

esp_err_t ota_write(const uint8_t *data, size_t len)
{
    if (ota_state != OTA_STATE_IN_PROGRESS) {
        snprintf(error_message, sizeof(error_message), "OTA not in progress");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0) {
        return ESP_OK;
    }

    esp_err_t err = esp_ota_write(ota_handle, data, len);
    if (err != ESP_OK) {
        snprintf(error_message, sizeof(error_message), "OTA write failed: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", error_message);
        ota_state = OTA_STATE_ERROR;
        return err;
    }

    bytes_written += len;
    
    if (bytes_written % 102400 == 0) { // Log every 100KB
        ESP_LOGI(TAG, "Written %zu bytes", bytes_written);
    }

    return ESP_OK;
}

esp_err_t ota_end(void)
{
    if (ota_state != OTA_STATE_IN_PROGRESS) {
        snprintf(error_message, sizeof(error_message), "OTA not in progress");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        snprintf(error_message, sizeof(error_message), "OTA end failed: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", error_message);
        ota_state = OTA_STATE_ERROR;
        return err;
    }

    // Set boot partition
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        snprintf(error_message, sizeof(error_message), "Set boot partition failed: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "%s", error_message);
        ota_state = OTA_STATE_ERROR;
        return err;
    }

    ota_state = OTA_STATE_SUCCESS;
    ESP_LOGI(TAG, "OTA update successful! Total: %zu bytes. Reboot to apply.", bytes_written);
    return ESP_OK;
}

void ota_abort(void)
{
    if (ota_state == OTA_STATE_IN_PROGRESS && ota_handle) {
        esp_ota_abort(ota_handle);
        ESP_LOGW(TAG, "OTA aborted");
    }
    ota_state = OTA_STATE_IDLE;
    bytes_written = 0;
    total_size = 0;
}

ota_state_t ota_get_state(void)
{
    return ota_state;
}

const char* ota_get_error_message(void)
{
    return error_message;
}

int ota_get_progress_percent(void)
{
    if (total_size == 0) return 0;
    return (int)((bytes_written * 100) / total_size);
}

// HTTP handler for OTA upload
esp_err_t ota_upload_handler(httpd_req_t *req)
{
    char buf[1024];
    int received;
    int remaining = req->content_len;

    ESP_LOGI(TAG, "OTA upload started, size: %d bytes", remaining);

    // Begin OTA
    esp_err_t err = ota_begin();
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, ota_get_error_message());
        return ESP_FAIL;
    }

    total_size = remaining;

    // Receive and write firmware data
    while (remaining > 0) {
        received = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ota_abort();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload failed");
            return ESP_FAIL;
        }

        err = ota_write((const uint8_t *)buf, received);
        if (err != ESP_OK) {
            ota_abort();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, ota_get_error_message());
            return ESP_FAIL;
        }

        remaining -= received;
    }

    // Finalize OTA
    err = ota_end();
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, ota_get_error_message());
        return ESP_FAIL;
    }

    // Send success response
    const char *resp = "{\"ok\":true,\"message\":\"Update successful. Rebooting...\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp);

    ESP_LOGI(TAG, "OTA update complete, restarting in 2 seconds...");
    
    // Reboot after short delay
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}
