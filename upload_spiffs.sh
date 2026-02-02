#!/bin/bash

# Script to build and upload SPIFFS partition via OTA

set -e

IP="${1:-192.168.1.51}"

echo "Building SPIFFS image..."
cd "$(dirname "$0")"

# Build SPIFFS using CMake target
cmake --build build --target spiffs_spiffs_bin

echo "✓ SPIFFS image created: build/spiffs.bin"
echo "Uploading to $IP..."

# Upload SPIFFS image
curl -sS -X POST http://$IP/api/ota/spiffs \
    -H "Content-Type: application/octet-stream" \
    --data-binary @build/spiffs.bin | jq .

echo "✓ SPIFFS upload complete - device will reboot"
