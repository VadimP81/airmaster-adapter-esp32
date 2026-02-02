#include "crashlog.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "config.h"
#include <stdio.h>
#include <inttypes.h>

static const char *TAG = "CRASHLOG";

typedef struct {
    uint32_t boot_count;
    uint32_t reset_reason;
    uint64_t timestamp_us;
} crashlog_entry_t;

static bool is_crash_reset(esp_reset_reason_t reason)
{
    switch (reason) {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
            return true;
        default:
            return false;
    }
}

void crashlog_init(void)
{
    esp_reset_reason_t reason = esp_reset_reason();
    ESP_LOGI(TAG, "Reset reason: %d", (int)reason);

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(CONFIG_CRASHLOG_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return;
    }

    uint32_t boot_count = 0;
    if (nvs_get_u32(nvs, "boot_count", &boot_count) != ESP_OK) {
        boot_count = 0;
    }
    boot_count++;
    nvs_set_u32(nvs, "boot_count", boot_count);

    if (is_crash_reset(reason)) {
        uint32_t crash_count = 0;
        if (nvs_get_u32(nvs, "crash_count", &crash_count) != ESP_OK) {
            crash_count = 0;
        }

        uint32_t index = crash_count % CONFIG_CRASHLOG_MAX_ENTRIES;
        crashlog_entry_t entry = {
            .boot_count = boot_count,
            .reset_reason = (uint32_t)reason,
            .timestamp_us = (uint64_t)esp_timer_get_time()
        };

        char key[16];
        snprintf(key, sizeof(key), "entry%" PRIu32, index);
        nvs_set_blob(nvs, key, &entry, sizeof(entry));

        crash_count++;
        nvs_set_u32(nvs, "crash_count", crash_count);

        ESP_LOGW(TAG, "Crash recorded to flash (entry %u)", index);
    }

    nvs_commit(nvs);
    nvs_close(nvs);
}
