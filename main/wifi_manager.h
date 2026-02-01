#pragma once
#include "esp_err.h"
#include <stdbool.h>

void wifi_init(void);
bool wifi_is_connected(void);
esp_err_t wifi_connect(const char *ssid, const char *password);
esp_err_t wifi_start_ap(void);
bool wifi_is_ap_mode(void);
