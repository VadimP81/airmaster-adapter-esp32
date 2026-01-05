#include "settings.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static int interval = 10;
static char mqtt_broker[64] = "192.168.1.50";
static int mqtt_port = 1883;
static char mqtt_user[32] = "";
static char mqtt_pass[32] = "";

void settings_init(void) {
    nvs_flash_init();
    // Load from NVS here, fallback to defaults
}

int settings_get_interval(void) { return interval; }
const char* settings_get_mqtt_broker(void) { return mqtt_broker; }
int settings_get_mqtt_port(void) { return mqtt_port; }
const char* settings_get_mqtt_user(void) { return mqtt_user; }
const char* settings_get_mqtt_pass(void) { return mqtt_pass; }
