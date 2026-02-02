#!/bin/bash

# Script to build and upload SPIFFS partition via OTA
# Usage: ./upload_spiffs.sh [--force] [IP]

set -e

FORCE_BUILD=false
IP="192.168.1.51"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --force)
            FORCE_BUILD=true
            shift
            ;;
        *)
            IP=$1
            shift
            ;;
    esac
done

cd "$(dirname "$0")"

# Check if rebuild is needed
if [ "$FORCE_BUILD" = true ] || [ ! -f build/spiffs.bin ]; then
    echo "Building SPIFFS image..."
    cd "$(dirname "$0")"
    ./build.sh > /dev/null 2>&1
    cmake --build build --target spiffs_image > /dev/null 2>&1
    echo "✓ Build complete"
fi

if [ ! -f build/spiffs.bin ]; then
    echo "ERROR: spiffs.bin not found"
    exit 1
fi

echo "Uploading SPIFFS to $IP..."

# Upload SPIFFS image
curl -sS -X POST http://$IP/api/ota/spiffs \
    -H "Content-Type: application/octet-stream" \
    --data-binary @build/spiffs.bin | jq .

echo "✓ SPIFFS uploaded - device rebooting..."
