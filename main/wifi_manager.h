#pragma once
#include "esp_err.h"

void wifi_init(void);
bool wifi_is_connected(void);
esp_err_t wifi_connect(const char *ssid, const char *password);
