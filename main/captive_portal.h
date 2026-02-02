#pragma once

#include "esp_err.h"

/**
 * @brief Start the captive portal DNS server
 * 
 * This redirects all DNS queries to the AP IP (192.168.4.1)
 * so any domain name resolves to the config portal
 */
esp_err_t captive_portal_start(void);

/**
 * @brief Stop the captive portal DNS server
 */
void captive_portal_stop(void);
