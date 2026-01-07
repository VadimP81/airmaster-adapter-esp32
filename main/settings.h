#pragma once
#include "esp_err.h"

void settings_init(void);
esp_err_t settings_save(void);

// Getters
int settings_get_interval(void);
const char* settings_get_mqtt_broker(void);
int settings_get_mqtt_port(void);
const char* settings_get_mqtt_user(void);
const char* settings_get_mqtt_pass(void);
const char* settings_get_mqtt_topic(void);
const char* settings_get_device_name(void);
bool settings_get_ha_discovery_enabled(void);

// Setters
void settings_set_interval(int value);
void settings_set_mqtt_broker(const char *value);
void settings_set_mqtt_port(int value);
void settings_set_mqtt_user(const char *value);
void settings_set_mqtt_pass(const char *value);
void settings_set_mqtt_topic(const char *value);
void settings_set_device_name(const char *value);
void settings_set_ha_discovery_enabled(bool enabled);
