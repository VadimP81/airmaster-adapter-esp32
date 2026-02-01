#!/bin/bash
# Flash firmware and SPIFFS (web interface)

DEVICE="/dev/tty.usbmodem1101"
SPIFFS_SIZE=983040  # 960KB in bytes
SPIFFS_OFFSET=0x290000  # From partitions.csv

echo "Building SPIFFS image..."
. ~/esp/esp-idf/export.sh && \
python ~/esp/esp-idf/components/spiffs/spiffsgen.py $SPIFFS_SIZE spiffs build/spiffs.bin && \
echo "" && \
echo "Flashing firmware and SPIFFS..." && \
idf.py -p $DEVICE flash && \
python -m esptool --chip esp32s3 --port $DEVICE write-flash $SPIFFS_OFFSET build/spiffs.bin && \
echo "" && \
echo "✅ Flash complete! Web UI is now available."
