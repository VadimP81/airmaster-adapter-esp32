#!/bin/bash
# Build the ESP32-S3 firmware

# Increment version before building
./increment_version.sh

. ~/esp/esp-idf/export.sh && idf.py build
