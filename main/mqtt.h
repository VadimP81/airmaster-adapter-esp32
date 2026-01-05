#pragma once
#include <stdbool.h>
#include "am7.h"

extern bool mqtt_connected;

void mqtt_task(void *arg);
bool mqtt_publish(const char *topic, const char *payload);
bool connect_to_mqtt(const char *broker, int port, const char *user, const char *pass);
