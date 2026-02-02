#include "spiffs.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "config.h"

static const char *TAG = "SPIFFS";
static bool spiffs_mounted = false;

esp_err_t spiffs_init(void)
{
    if (spiffs_mounted) {
        return ESP_OK;
    }

    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_SPIFFS_BASE_PATH,
        .partition_label = NULL,
        .max_files = CONFIG_SPIFFS_MAX_FILES,
        .format_if_mount_failed = false
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    spiffs_mounted = true;
    ESP_LOGI(TAG, "SPIFFS mounted at %s", conf.base_path);
    return ESP_OK;
}

void spiffs_deinit(void)
{
    if (!spiffs_mounted) {
        return;
    }

    esp_vfs_spiffs_unregister(NULL);
    spiffs_mounted = false;
    ESP_LOGI(TAG, "SPIFFS unmounted");
}
