#!/bin/bash

set -e

DEVICE_IP="${1:-192.168.1.51}"

echo "Building SPIFFS image..."
. "$HOME/esp/esp-idf/export.sh" > /dev/null 2>&1
cmake --build build --target spiffs_image

echo "Deploying SPIFFS to $DEVICE_IP..."
curl -X POST "http://$DEVICE_IP/api/ota/spiffs" --data-binary @build/spiffs.bin

echo ""
echo "SPIFFS deployment complete!"
