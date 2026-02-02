#include "am7.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "AM7";

// AM7 CP2102 USB identifiers
#define AM7_VID 0x10C4  // Silicon Labs
#define AM7_PID 0xEA60  // CP2102
static uint32_t am7_baud_rate = 19200;

// CP210x vendor-specific requests (from Silicon Labs AN571)
#define CP210X_IFC_ENABLE      0x00
#define CP210X_SET_LINE_CTL    0x03
#define CP210X_SET_MHS         0x07
#define CP210X_SET_BAUDRATE    0x1E

#define CP210X_UART_ENABLE     0x0001

#define CP210X_BITS_DATA_8     0x0800
#define CP210X_BITS_PARITY_NONE 0x0000
#define CP210X_BITS_STOP_1     0x0000

#define CP210X_MCR_DTR         0x0001
#define CP210X_MCR_RTS         0x0002
#define CP210X_CONTROL_WRITE_DTR 0x0100
#define CP210X_CONTROL_WRITE_RTS 0x0200

#define CP210X_REQ_TYPE_OUT    0x41

// AM7 protocol expects 28 hex characters: 7 values * 4 chars each
#define AM7_RESPONSE_LEN 28

// Global sensor data
am7_data_t am7_data = {0};
bool am7_connected = false;
int last_rx_sec = 0;

static bool usb_initialized = false;
static cdc_acm_dev_hdl_t cdc_dev = NULL;
static uint8_t rx_buffer[256] = {0};
static size_t rx_buffer_pos = 0;
static bool frame_complete = false;

// Forward declaration
static bool am7_parse_data(const uint8_t *data, size_t len, am7_data_t *out);

static esp_err_t cp210x_configure(uint16_t iface_index)
{
    uint8_t baud_le[4] = {
        (uint8_t)(am7_baud_rate & 0xFF),
        (uint8_t)((am7_baud_rate >> 8) & 0xFF),
        (uint8_t)((am7_baud_rate >> 16) & 0xFF),
        (uint8_t)((am7_baud_rate >> 24) & 0xFF)
    };

    esp_err_t ret = cdc_acm_host_send_custom_request(
        cdc_dev, CP210X_REQ_TYPE_OUT, CP210X_IFC_ENABLE,
        CP210X_UART_ENABLE, iface_index, 0, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

    uint16_t line_ctl = CP210X_BITS_DATA_8 | CP210X_BITS_PARITY_NONE | CP210X_BITS_STOP_1;
    ret = cdc_acm_host_send_custom_request(
        cdc_dev, CP210X_REQ_TYPE_OUT, CP210X_SET_LINE_CTL,
        line_ctl, iface_index, 0, NULL);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = cdc_acm_host_send_custom_request(
        cdc_dev, CP210X_REQ_TYPE_OUT, CP210X_SET_BAUDRATE,
        0, iface_index, sizeof(baud_le), baud_le);
    if (ret != ESP_OK) {
        return ret;
    }

    // Toggle DTR/RTS low -> high for sensor reset
    uint16_t mhs_low = (uint16_t)(CP210X_CONTROL_WRITE_DTR | CP210X_CONTROL_WRITE_RTS);
    ret = cdc_acm_host_send_custom_request(
        cdc_dev, CP210X_REQ_TYPE_OUT, CP210X_SET_MHS,
        mhs_low, iface_index, 0, NULL);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(20));

    uint16_t mhs_high = (uint16_t)(CP210X_CONTROL_WRITE_DTR | CP210X_CONTROL_WRITE_RTS | CP210X_MCR_DTR | CP210X_MCR_RTS);
    ret = cdc_acm_host_send_custom_request(
        cdc_dev, CP210X_REQ_TYPE_OUT, CP210X_SET_MHS,
        mhs_high, iface_index, 0, NULL);
    return ret;
}

// USB CDC-ACM data callback
// Look for frames: 'aa' hex prefix + data, terminated by \r or \n
// Frame can be 15 bytes (0xaa + 14 binary) or longer (hex format)
static bool cdc_rx_callback(const uint8_t *data, size_t len, void *arg)
{
    // Accumulate raw bytes in buffer
    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];

        // Check buffer state: if empty, only accept 'a' (start of 'aa' ASCII) or 0xaa (binary)
        if (rx_buffer_pos == 0) {
            if (b != 'a' && b != 'A' && b != 0xaa) {
                // Skip junk bytes
                continue;
            }
        }

        // Check if we're about to overflow
        if (rx_buffer_pos >= sizeof(rx_buffer) - 1) {
            ESP_LOGW(TAG, "RX buffer overflow, discarding frame");
            rx_buffer_pos = 0;
            memset(rx_buffer, 0, sizeof(rx_buffer));
            continue;
        }

        // Accumulate byte
        rx_buffer[rx_buffer_pos++] = b;

        // Check for frame completion:
        bool should_parse = false;
        
        // Binary frame: 0xaa prefix + data ending with \r\n
        // Full response is much longer than initially thought (~54 bytes)
        if ((b == '\r' || b == '\n') && rx_buffer_pos >= 3) {
            if (rx_buffer[0] == 0xaa) {
                should_parse = true;
            } else {
                // Not a valid frame, discard
                rx_buffer_pos = 0;
                memset(rx_buffer, 0, sizeof(rx_buffer));
                continue;
            }
        }

        if (should_parse) {
            // Try to parse this frame
            if (am7_parse_data(rx_buffer, rx_buffer_pos, &am7_data)) {
                last_rx_sec = 0;
                am7_connected = true;
                ESP_LOGI(TAG, "Parsed: PM2.5=%d PM10=%d HCHO=%.3f TVOC=%.2f CO2=%d Temp=%.1f Hum=%.1f",
                         am7_data.pm25, am7_data.pm10, am7_data.hcho, am7_data.tvoc, am7_data.co2, am7_data.temp, am7_data.humidity);
            }
            
            // Reset for next frame
            rx_buffer_pos = 0;
            memset(rx_buffer, 0, sizeof(rx_buffer));
        }
    }
    return true;
}

// USB Host library event handler task
static void usb_lib_task(void *arg)
{
    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGD(TAG, "No more USB clients");
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGD(TAG, "All USB devices freed");
        }
    }
}

// Initialize USB Host for AM7
static bool am7_usb_init(void)
{
    if (usb_initialized) {
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing USB Host for AM7 sensor...");
    ESP_LOGI(TAG, "Connect AM7 via USB OTG cable");
    
    // Install USB Host driver
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    
    esp_err_t ret = usb_host_install(&host_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB Host: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Create USB Host library task
    xTaskCreate(usb_lib_task, "usb_host", 4096, NULL, 5, NULL);
    
    // Install CDC-ACM driver
    ret = cdc_acm_host_install(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install CDC-ACM driver: %s", esp_err_to_name(ret));
        return false;
    }
    
    ESP_LOGI(TAG, "USB Host initialized successfully");
    usb_initialized = true;
    return true;
}

// Try to open AM7 device
static bool am7_open_device(void)
{
    if (!usb_initialized || cdc_dev != NULL) {
        return (cdc_dev != NULL);
    }
    
    // Try to open AM7 device
    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 64,
        .in_buffer_size = 256,
        .event_cb = NULL,
        .data_cb = cdc_rx_callback,
        .user_arg = NULL,
    };
    
    esp_err_t ret = cdc_acm_host_open(AM7_VID, AM7_PID, 0, &dev_config, &cdc_dev);
    if (ret != ESP_OK) {
        return false;
    }
    
    ESP_LOGI(TAG, "AM7 device opened (VID:0x%04X PID:0x%04X)", AM7_VID, AM7_PID);
    cdc_acm_host_desc_print(cdc_dev);
    
    // Configure line coding (baud rate, etc.)
    cdc_acm_line_coding_t line_coding = {
        .dwDTERate = am7_baud_rate,
        .bCharFormat = 0,  // 1 stop bit
        .bParityType = 0,  // No parity
        .bDataBits = 8
    };
    
    ret = cdc_acm_host_line_coding_set(cdc_dev, &line_coding);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "CDC line coding not supported: %s", esp_err_to_name(ret));
    }

    // Set control line state (DTR=1, RTS=1) to activate serial communication
    ret = cdc_acm_host_set_control_line_state(cdc_dev, true, true);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "CDC control not supported: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Control lines set (DTR=1 RTS=1)");
    }

    // CP210x fallback if CDC requests are not supported
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGD(TAG, "Using CP210x vendor requests for configuration...");
        esp_err_t cp_ret = cp210x_configure(0);
        if (cp_ret != ESP_OK) {
            ESP_LOGD(TAG, "CP210x config on iface 0: %s, trying iface 1", esp_err_to_name(cp_ret));
            cp_ret = cp210x_configure(1);
        }
        if (cp_ret == ESP_OK) {
            ESP_LOGI(TAG, "CP210x configured (baud: %lu)", (unsigned long)am7_baud_rate);
        } else {
            ESP_LOGW(TAG, "CP210x config failed: %s", esp_err_to_name(cp_ret));
        }
    }
    
    ESP_LOGI(TAG, "AM7 configured (baud: %lu)", (unsigned long)am7_baud_rate);
    return true;
}

// Send data request command to AM7 sensor
// Sensor responds better with dual requests (binary + ASCII)
static bool am7_send_request(void)
{
    if (!am7_open_device()) {
        return false;
    }
    
    // Send binary request
    static const uint8_t binary_cmd[] = {
        0x55, 0xCD, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x69, 0x0D, 0x0A
    };
    esp_err_t ret = cdc_acm_host_data_tx_blocking(cdc_dev, binary_cmd, sizeof(binary_cmd), 100);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send binary request: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "Sent request to AM7");
    return true;
}

// Parse AM7 sensor data from binary response
// Response format: 0xaa (1 byte) + 7 uint16_t values (14 bytes) + extra padding = ~54 bytes total
// Values: PM2.5, PM10, HCHO, TVOC, CO2, Temp, Humidity (all big-endian uint16_t)
static bool am7_parse_data(const uint8_t *data, size_t len, am7_data_t *out)
{
    if (!data || !out || len < 15) {
        return false;
    }

    // Must start with 0xaa marker
    if (data[0] != 0xaa) {
        return false;
    }

    // Extract 7 uint16_t values starting at byte 1 (big-endian)
    uint16_t values[7];
    for (int i = 0; i < 7; i++) {
        values[i] = ((uint16_t)data[1 + i*2] << 8) | (uint16_t)data[1 + i*2 + 1];
    }
    
    // Validate: reject frames with all zeros or clearly invalid data
    // (all zeros = sensor not initialized or error state)
    if (values[0] == 0 && values[1] == 0 && values[4] == 0 && values[5] == 0 && values[6] == 0) {
        ESP_LOGD(TAG, "Rejecting all-zero frame (sensor error)");
        return false;
    }
    
    // Convert to sensor readings with appropriate scaling
    out->pm25 = values[0];           // PM2.5 in µg/m³
    out->pm10 = values[1];           // PM10 in µg/m³
    out->hcho = values[2] / 1000.0;  // HCHO in mg/m³
    out->tvoc = values[3] / 1000.0;  // TVOC in mg/m³
    out->co2 = values[4];            // CO2 in ppm
    out->temp = values[5] / 100.0;   // Temperature in °C
    out->humidity = values[6] / 100.0; // Humidity in %

    return true;
}

// AM7 polling task
void am7_task(void *arg)
{
    ESP_LOGI(TAG, "AM7 task started");
    
    // Initialize USB Host
    if (!am7_usb_init()) {
        ESP_LOGE(TAG, "Failed to initialize USB Host, using simulated data");
        goto simulated_mode;
    }
    
    // Wait for device connection (up to 60 seconds)
    ESP_LOGI(TAG, "Waiting up to 60 seconds for AM7 device...");
    int wait_seconds = 0;
    const int max_wait_seconds = 60;
    
    while (wait_seconds < max_wait_seconds) {
        // Try to open device
        if (am7_open_device()) {
            ESP_LOGI(TAG, "AM7 device connected!");
            break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
        wait_seconds += 2;
        
        if (wait_seconds % 10 == 0) {
            ESP_LOGI(TAG, "Still waiting for AM7... (%d/%d seconds)", wait_seconds, max_wait_seconds);
        }
    }
    
    if (cdc_dev == NULL) {
        ESP_LOGW(TAG, "AM7 device not found after %d seconds", max_wait_seconds);
        goto simulated_mode;
    }
    
    // Device connected, start polling loop
    ESP_LOGI(TAG, "Starting AM7 polling loop");
    uint32_t poll_counter = 0;
    
    while (1) {
        // Send request every 3 seconds
        if (poll_counter % 3 == 0) {
            if (am7_send_request()) {
                ESP_LOGD(TAG, "Request sent");
            }
        }
        
        // Monitor connection
        last_rx_sec++;
        if (last_rx_sec > 30) {
            if (last_rx_sec == 31) {
                ESP_LOGW(TAG, "No data from AM7 for 30+ seconds");
            }
            am7_connected = false;
        }
        
        poll_counter++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

simulated_mode:
    ESP_LOGW(TAG, "Using simulated data (all zeros)");
    
    // Initialize simulated data
    am7_data.temp = 0.0;
    am7_data.humidity = 0.0;
    am7_data.co2 = 0;
    am7_data.pm25 = 0;
    am7_data.pm10 = 0;
    am7_data.tvoc = 0.0;
    am7_data.hcho = 0.0;
    am7_connected = true;
    last_rx_sec = 0;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
