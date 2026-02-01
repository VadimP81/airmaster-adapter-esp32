#!/bin/bash
# Monitor serial output from ESP32-S3 Super Mini

DEVICE="/dev/tty.usbmodem1101"

. ~/esp/esp-idf/export.sh && idf.py -p $DEVICE -b 74880 monitor
