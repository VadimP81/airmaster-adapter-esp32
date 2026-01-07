#include "am7.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/usb_host.h"
#include <string.h>

static const char *TAG = "AM7";

// AM7 uses CP2102 USB-to-UART Bridge (Silicon Labs)
#define AM7_VID 0x10c4  // Silicon Labs
#define AM7_PID 0xea60  // CP2102
#define AM7_IN_EP 0x81  // Bulk IN endpoint (typical)
#define AM7_OUT_EP 0x01 // Bulk OUT endpoint (typical)
#define AM7_INTERFACE 0 // Interface number

// AM7 Serial Configuration
#define AM7_BAUD_RATE 19200
#define AM7_DATA_BITS 8
#define AM7_PARITY 0    // None
#define AM7_STOP_BITS 0 // 1 stop bit

// CDC Line Coding structure
typedef struct {
    uint32_t dwDTERate;   // Baud rate
    uint8_t bCharFormat;  // Stop bits: 0=1, 1=1.5, 2=2
    uint8_t bParityType;  // Parity: 0=None, 1=Odd, 2=Even, 3=Mark, 4=Space
    uint8_t bDataBits;    // Data bits: 5, 6, 7, 8, 16
} __attribute__((packed)) cdc_line_coding_t;

am7_data_t am7_data = {0};
bool am7_connected = false;
int last_rx_sec = 0;

static usb_host_client_handle_t client_hdl = NULL;
static usb_device_handle_t am7_dev = NULL;
static bool usb_host_ready = false;

// USB Host event callback
static void usb_host_lib_task(void *arg)
{
    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGW(TAG, "No more USB clients");
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "All USB devices freed");
        }
    }
}

// USB Host client event callback
static void client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    switch (event_msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            ESP_LOGI(TAG, "New USB device detected");
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            ESP_LOGW(TAG, "USB device disconnected");
            am7_connected = false;
            if (am7_dev) {
                usb_host_device_close(client_hdl, am7_dev);
                am7_dev = NULL;
            }
            break;
        default:
            break;
    }
}

// Initialize USB Host
static bool am7_usb_host_init(void)
{
    if (usb_host_ready) return true;

    // Install USB Host driver
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    
    esp_err_t err = usb_host_insUSB task...");
    
    // Initialize USB Host
    if (!am7_usb_host_init()) {
        ESP_LOGE(TAG, "USB Host initialization failed!");
        vTaskDelete(NULL);
        return;
    }

    // Wait a bit for devices to enumerate
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        // Try to open device if not connected
        if (!am7_dev) {
            if (am7_device_open()) {
                ESP_LOGI(TAG, "AM7 device opened successfully");
                last_rx_sec = 0;
            } else {
                // Retry every 5 seconds
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
        }

        // Read data from AM7
        if (am7_dev) {
            uint8_t buf[16] = {0};
            if (am7_usb_read(buf, sizeof(buf))) {
                if (am7_parse_data(buf, &am7_data)) {
                    last_rx_sec = 0;
                    ESP_LOGI(TAG, "AM7: T=%.1f H=%.1f CO2=%d PM2.5=%d PM10=%d TVOC=%.2f HCHO=%.3f",
                             am7_data.temp, am7_data.humidity,
                             am7_data.co2, am7_data.pm25, am7_data.pm10, am7_data.tvoc, am7_data.hcho);
                } else {
                    ESP_LOGW(TAG, "Failed to parse AM7 data");
                }
            } else {
                last_rx_sec++;
                if (last_rx_sec > 30) {
                    ESP_LOGW(TAG, "No data from AM7 for 30+ seconds");
                    am7_connected = false;
                }
    return true;
}

// Find and open AM7 device
static bool am7_device_open(void)
{
    if (am7_dev) return true;

    uint8_t dev_addr_list[10];
    int num_devs = 10;
    
    esp_err_t err = usb_host_device_addr_list_fill(sizeof(dev_addr_list), dev_addr_list, &num_devs);
    if (err != ESP_OK || num_devs == 0) {
        return false;
    }

    // Check each device
    for (int i = 0; i < num_devs; i++) {
        usb_device_handle_t dev;
        err = usb_host_device_open(client_hdl, dev_addr_list[i], &dev);
        if (err != ESP_OK) continue;

        const usb_device_desc_t *dev_desc;
        err = usb_host_get_device_descriptor(dev, &dev_desc);
        if (err != ESP_OK) {
            usb_host_device_close(client_hdl, dev);
            continue;
        }

        // Check if this is AM7
        if (dev_desc->idVendor == AM7_VID && dev_desc->idProduct == AM7_PID) {
            ESP_LOGI(TAG, "AM7 device found! VID:0x%04X PID:0x%04X", 
                     dev_desc->idVendor, dev_desc->idProduct);
            
            // Claim interface
            err = usb_host_interface_claim(client_hdl, dev, AM7_INTERFACE, 0);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to claim interface: %s", esp_err_to_name(err));
                usb_host_device_close(client_hdl, dev);
                return false;
            }

            am7_dev = dev;
            am7_connected = true;
            
            // Configure serial parameters
            if (!am7_configure_serial()) {
                ESP_LOGW(TAG, "Failed to configure serial parameters");
            }
            
            return true;
        }

        usb_host_device_close(client_hdl, dev);
    }

    return false;
}

// Read data from AM7 via USB bulk transfer
static bool am7_usb_read(uint8_t *buf, size_t len)
{
    if (!am7_dev) return false;

    usb_transfer_t *transfer;
    esp_err_t err = usb_host_transfer_alloc(len, 0, &transfer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Transfer alloc failed: %s", esp_err_to_name(err));
        return false;
    }

    transfer->device_handle = am7_dev;
    transfer->bEndpointAddress = AM7_IN_EP;
    transfer->num_bytes = len;
    transfer->timeout_ms = 1000;

    err = usb_host_transfer_submit_control(client_hdl, transfer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Transfer submit failed: %s", esp_err_to_name(err));
        usb_host_transfer_free(transfer);
        return false;
    }

    // Wait for transfer to complete
    // Note: In production, use async callbacks for better performance
    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED) {
        memcpy(buf, transfer->data_buffer, transfer->actual_num_bytes);
        usb_host_transfer_free(transfer);
        return true;
    }

    usb_host_transfer_free(transfer);
    return false;
}

// Function to parse AM7 raw data
static bool am7_parse_data(uint8_t *buf, am7_data_t *data) {
    if(!buf || !data) return false;

    // Example parsing (adjust according to AM7 protocol)
    data->temp = buf[0] + buf[1]/100.0f;
    data->humidity = buf[2] + buf[3]/100.0f;
    data->co2 = (buf[4]<<8)|buf[5];
    data->pm25 = (buf[6]<<8)|buf[7];
    data->pm10 = (buf[8]<<8)|buf[9];
    data->tvoc = buf[10]/100.0f;
    data->hcho = buf[11]/100.0f;
    return true;
}

// AM7 polling task
void am7_task(void *arg)
{
    ESP_LOGI(TAG, "Starting AM7 task...");
    if(!am7_usb_init()) {
        ESP_LOGW(TAG, "AM7 not detected, will retry...");
    }

    while(1)
    {
        if(am7_dev) {
            uint8_t buf[16] = {0};
            if(am7_usb_read(buf, sizeof(buf))) {
                if(am7_parse_data(buf, &am7_data)) {
                    am7_connected = true;
                    last_rx_sec = 0;
                    ESP_LOGI(TAG, "AM7: T=%.1f H=%.1f CO2=%d PM2.5=%d VOC=%.2f",
                             am7_data.temp, am7_data.humidity,
                             am7_data.co2, am7_data.pm25, am7_data.voc);
                } else {
                    ESP_LOGW(TAG, "Failed to parse AM7 data");
                }
            } else {
                am7_connected = false;
                last_rx_sec++;
            }
        } else {
            // Try to reconnect
            if(am7_usb_init()) {
                ESP_LOGI(TAG, "AM7 reconnected");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Poll every second
    }
}
