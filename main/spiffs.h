#ifndef SPIFFS_MANAGER_H
#define SPIFFS_MANAGER_H

#include "esp_err.h"

esp_err_t spiffs_init(void);
void spiffs_deinit(void);

#endif // SPIFFS_MANAGER_H
