#pragma once
void settings_init(void);
int settings_get_interval(void);
const char* settings_get_mqtt_broker(void);
int settings_get_mqtt_port(void);
const char* settings_get_mqtt_user(void);
const char* settings_get_mqtt_pass(void);
