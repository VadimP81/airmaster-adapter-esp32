#!/bin/bash
set -euo pipefail

DEVICE_IP=${DEVICE_IP:-192.168.1.51}
BIN_PATH=${BIN_PATH:-build/airmaster-adapter-esp32.bin}

if [[ ! -f "$BIN_PATH" ]]; then
	echo "Firmware not found at $BIN_PATH" >&2
	exit 1
fi

curl -X POST "http://$DEVICE_IP/api/ota" --data-binary "@$BIN_PATH"