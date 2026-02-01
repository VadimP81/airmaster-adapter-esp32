#!/bin/bash
# Build, flash, and monitor ESP32-S3 Super Mini

DEVICE="/dev/tty.usbmodem1101"

. ~/esp/esp-idf/export.sh && idf.py -p $DEVICE flash monitor
