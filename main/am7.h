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
    int battery_status;   // 0 = battery, 1 = charging
    int battery_level;    // 1-4 bars
    int runtime_hours;    // cumulative runtime
    int pc03;             // >0.3 µm particle count
    int pc05;             // >0.5 µm particle count
    int pc10;             // >1.0 µm particle count
    int pc25;             // >2.5 µm particle count
    int pc50;             // >5.0 µm particle count
    int pc100;            // >10 µm particle count
} am7_data_t;

extern am7_data_t am7_data;
extern bool am7_connected;
extern int last_rx_sec;
extern bool am7_debug_mode;  // Runtime debug toggle (not persisted)

void am7_task(void *arg); // FreeRTOS task
