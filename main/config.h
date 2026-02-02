#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define CONFIG_AP_SSID "AirMaster-Setup"
#define CONFIG_AP_PASSWORD ""  // Open network
#define CONFIG_AP_MAX_CONNECTIONS 4
#define CONFIG_WIFI_MAX_STA_RETRY 5  // Attempts before fallback to AP mode

// SPIFFS Configuration
#define CONFIG_SPIFFS_BASE_PATH "/spiffs"
#define CONFIG_SPIFFS_MAX_FILES 10

// Logging Configuration
#define CONFIG_MAX_LOG_LINES 100
#define CONFIG_MAX_LOG_MESSAGE_LENGTH 200

// Crash Logging Configuration
#define CONFIG_CRASHLOG_NAMESPACE "crashlog"
#define CONFIG_CRASHLOG_MAX_ENTRIES 10

// DNS/Captive Portal Configuration
#define CONFIG_DNS_PORT 53
#define CONFIG_DNS_MAX_LEN 512
#define CONFIG_AP_IP 0x0104A8C0  // 192.168.4.1 in network byte order

// USB AM7 Sensor Configuration
#define CONFIG_AM7_VID 0x10C4  // Silicon Labs
#define CONFIG_AM7_PID 0xEA60  // CP2102

// HTTP Server Configuration
#define CONFIG_HTTPD_MAX_URI_HANDLERS 24

#endif // CONFIG_H
