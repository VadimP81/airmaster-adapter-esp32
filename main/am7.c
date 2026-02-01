#include "am7.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include <string.h>

static const char *TAG = "AM7";

// AM7 CP2102 USB identifiers
#define AM7_VID 0x10C4  // Silicon Labs
#define AM7_PID 0xEA60  // CP2102
#define AM7_BAUD_RATE 19200

// AM7 protocol expects 28 hex characters: 7 values * 4 chars each
#define AM7_RESPONSE_LEN 28

// AM7 data request command: 55CD4700000000000001690D0A
static const uint8_t am7_request_cmd[] = {
    0x55, 0xCD, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x69, 0x0D, 0x0A
};
#define AM7_REQUEST_CMD_LEN sizeof(am7_request_cmd)

// Global sensor data
am7_data_t am7_data = {0};
bool am7_connected = false;
int last_rx_sec = 0;

static bool usb_initialized = false;
static char rx_buffer[64] = {0};
static size_t rx_buffer_pos = 0;

// Forward declarations
static bool am7_parse_data(const char *hex_data, size_t len, am7_data_t *out);

// USB CDC callback for received data
static void cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    if (event->type == CDC_EVENT_RX) {
        uint8_t buf[64];
        size_t rx_size = 0;
        
        // Read available data
        esp_err_t ret = tinyusb_cdcacm_read(itf, buf, sizeof(buf), &rx_size);
        if (ret == ESP_OK && rx_size > 0) {
            // Accumulate data in buffer
            for (size_t i = 0; i < rx_size && rx_buffer_pos < sizeof(rx_buffer) - 1; i++) {
                char c = (char)buf[i];
                
                // Look for valid hex characters or newline
                if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
                    rx_buffer[rx_buffer_pos++] = c;
                } else if (c == '\n' || c == '\r') {
                    // End of response, try to parse
                    if (rx_buffer_pos >= AM7_RESPONSE_LEN) {
                        rx_buffer[rx_buffer_pos] = '\0';
                        ESP_LOGI(TAG, "AM7 response: %s", rx_buffer);
                        
                        if (am7_parse_data(rx_buffer, rx_buffer_pos, &am7_data)) {
                            last_rx_sec = 0;
                            am7_connected = true;
                        }
                    }
                    // Reset buffer for next response
                    rx_buffer_pos = 0;
                    memset(rx_buffer, 0, sizeof(rx_buffer));
                }
            }
        }
    }
}

// Initialize USB Host for CDC
static bool am7_usb_init(void)
{
    if (usb_initialized) {
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing USB Host for AM7 sensor...");
    
    // Configure TinyUSB for host mode
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,  // Host mode
        .string_descriptor = NULL,
        .external_phy = false,
        .configuration_descriptor = NULL,
    };
    
    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TinyUSB driver: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Configure CDC-ACM for host mode
    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 256,
        .callback_rx = &cdc_rx_callback,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL
    };
    
    ret = tusb_cdc_acm_init(&acm_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize CDC-ACM: %s", esp_err_to_name(ret));
        return false;
    }
    
    usb_initialized = true;
    ESP_LOGI(TAG, "USB Host CDC initialized successfully");
    return true;
}

// Send data request command to AM7 sensor
static bool am7_send_request(void)
{
    if (!usb_initialized) {
        return false;
    }
    
    esp_err_t ret = tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, 
                                                 am7_request_cmd, 
                                                 AM7_REQUEST_CMD_LEN);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send request command: %s", esp_err_to_name(ret));
        return false;
    }
    
    ret = tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 100);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to flush request command: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGD(TAG, "Sent data request to AM7");
    return true;
}

// Parse AM7 sensor data from hex string
// Protocol: 28 hex chars = 7 values (pm25, pm10, hcho, tvoc, co2, temp, hum)
// Each value is 4 hex characters (16-bit integer)
static bool am7_parse_data(const char *hex_data, size_t len, am7_data_t *out)
{
    if (!hex_data || !out || len < AM7_RESPONSE_LEN) {
        ESP_LOGW(TAG, "Invalid data for parsing: len=%d (need %d)", len, AM7_RESPONSE_LEN);
        return false;
    }
    
    // Parse 7 hex values (4 chars each)
    char hex_str[5] = {0};  // 4 chars + null terminator
    unsigned int values[7];
    
    for (int i = 0; i < 7; i++) {
        // Extract 4-character hex string
        memcpy(hex_str, &hex_data[i * 4], 4);
        hex_str[4] = '\0';
        
        // Parse as hex integer
        if (sscanf(hex_str, "%x", &values[i]) != 1) {
            ESP_LOGW(TAG, "Failed to parse hex value at position %d: %s", i, hex_str);
            return false;
        }
    }
    
    // Assign parsed values in order: pm25, pm10, hcho, tvoc, co2, temp, hum
    out->pm25 = values[0];
    out->pm10 = values[1];
    out->hcho = (float)values[2] / 1000.0f;  // Convert to mg/m³
    out->tvoc = (float)values[3] / 1000.0f;  // Convert to mg/m³
    out->co2 = values[4];
    out->temp = (float)values[5] / 10.0f;     // Convert to °C
    out->humidity = (float)values[6] / 10.0f; // Convert to %
    
    ESP_LOGI(TAG, "Parsed: PM2.5=%d PM10=%d HCHO=%.3f TVOC=%.3f CO2=%d Temp=%.1f Hum=%.1f",
             out->pm25, out->pm10, out->hcho, out->tvoc, out->co2, out->temp, out->humidity);
    
    return true;
}

// AM7 polling task
void am7_task(void *arg)
{
    ESP_LOGI(TAG, "AM7 task started");
    ESP_LOGW(TAG, "Note: USB Host CDC for CP2102 requires hardware testing");
    ESP_LOGW(TAG, "Using simulated data until hardware is connected and tested");
    
    // Try to initialize USB Host
    if (!am7_usb_init()) {
        ESP_LOGW(TAG, "USB Host initialization failed, using simulated data");
    }
    
    // Simulate sensor data for testing (will be replaced by real data when USB works)
    am7_data.temp = 23.5;
    am7_data.humidity = 45.0;
    am7_data.co2 = 420;
    am7_data.pm25 = 12;
    am7_data.pm10 = 15;
    am7_data.tvoc = 0.15;
    am7_data.hcho = 0.03;
    
    uint32_t request_counter = 0;
    
    while (1) {
        // Check if we have real USB connection
        if (usb_initialized) {
            // Send data request every 5 seconds
            if (request_counter % 5 == 0) {
                am7_send_request();
            }
            request_counter++;
            
            // Monitor connection status
            last_rx_sec++;
            
            if (last_rx_sec > 30) {
                am7_connected = false;
                if (last_rx_sec == 31) {
                    ESP_LOGW(TAG, "No data from AM7 for 30+ seconds");
                }
            }
        } else {
            // No USB, use simulated data
            am7_connected = true;
            last_rx_sec = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
