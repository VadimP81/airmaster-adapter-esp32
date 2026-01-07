#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float temp;
    float humidity;
    int co2;
    int pm25;
    int pm10;
    float tvoc;
    float hcho;
} am7_data_t;

extern am7_data_t am7_data;
extern bool am7_connected;
extern int last_rx_sec;

void am7_task(void *arg); // FreeRTOS task
