#include "am7.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_host.h"
#include "string.h"
#include "time.h"

static const char *TAG = "AM7";

am7_data_t am7_data = {0};
bool am7_connected = false;
int last_rx_sec = 0;

// Placeholder USB device handle
static usb_device_handle_t am7_dev = NULL;

// Function to initialize USB host and find AM7
static bool am7_usb_init() {
    // Initialize USB Host stack
    usb_host_config_t host_cfg = {0};
    host_cfg.skip_phy_setup = false;
    usb_host_install(&host_cfg);
    
    // Scan for AM7 device: placeholder VID/PID
    usb_device_desc_t desc;
    if (usb_host_device_open(0x16D0, 0x0ABC, &am7_dev)) { // replace with actual VID/PID
        ESP_LOGI(TAG, "AM7 USB device connected");
        return true;
    }
    return false;
}

// Function to read raw data from AM7 over USB
static bool am7_usb_read(uint8_t *buf, size_t len) {
    if(!am7_dev) return false;
    int read_len = 0;
    esp_err_t r = usb_host_bulk_transfer(am7_dev, 0x81, buf, len, &read_len, 1000);
    return (r==ESP_OK && read_len==len);
}

// Function to parse AM7 raw data
static bool am7_parse_data(uint8_t *buf, am7_data_t *data) {
    if(!buf || !data) return false;

    // Example parsing (adjust according to AM7 protocol)
    data->temp = buf[0] + buf[1]/100.0f;
    data->humidity = buf[2] + buf[3]/100.0f;
    data->co2 = (buf[4]<<8)|buf[5];
    data->pm25 = (buf[6]<<8)|buf[7];
    data->voc = buf[8]/100.0f;
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
                             am7_data.te_
