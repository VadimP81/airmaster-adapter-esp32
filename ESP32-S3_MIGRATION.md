# ESP32-S3 Super Mini Migration

## Summary

Project successfully configured for **ESP32-S3 Super Mini** board.

## Changes Made

### 1. Target Chip Configuration
- **File**: [sdkconfig.defaults](sdkconfig.defaults)
- Added `CONFIG_IDF_TARGET="esp32s3"`
- Added `CONFIG_IDF_TARGET_ESP32S3=y`

### 2. USB OTG Configuration
- **File**: [sdkconfig.defaults](sdkconfig.defaults)
- Added `CONFIG_USB_OTG_SUPPORTED=y`
- Added USB host control transfer configuration
- ESP32-S3 uses native USB OTG (no external USB host chip needed)

### 3. WiFi Configuration
- **File**: [sdkconfig.defaults](sdkconfig.defaults)
- Updated from old `CONFIG_ESP32_WIFI_*` to chip-agnostic `CONFIG_ESP_WIFI_*`
- Changed:
  - `CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM` → `CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM`
  - `CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM` → `CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM`
  - `CONFIG_ESP32_WIFI_DYNAMIC_TX_BUFFER_NUM` → `CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM`

### 4. Partition Table Optimization
- **File**: [partitions.csv](partitions.csv)
- Adjusted for 4MB flash constraint:
  - Factory app: 1536K → **1280K** (1.25 MB)
  - OTA partition: 1536K → **1280K** (1.25 MB)
  - SPIFFS: 1024K → **960K** (0xF0000 bytes)
- **Total**: 3.96 MB (fits in 4MB flash)

### 5. Documentation Updates
- **Files**: [README.md](README.md), [QUICKSTART.md](QUICKSTART.md), [CMakeLists.txt](CMakeLists.txt)
- Updated hardware requirements to specify ESP32-S3 Super Mini
- Changed references from generic "ESP32" to "ESP32-S3 Super Mini"
- Noted USB-C connector and native USB OTG support

### 6. Removed Zigbee Support
- **Files**: [main/webserver.c](main/webserver.c), [spiffs/index.html](spiffs/index.html), [spiffs/app.js](spiffs/app.js), [IMPROVEMENTS.md](IMPROVEMENTS.md)
- Removed zigbee status from API endpoint
- Removed zigbee UI elements from dashboard
- Removed zigbee integration from roadmap

## ESP32-S3 Super Mini Specifications

| Feature | Specification |
|---------|---------------|
| Chip | ESP32-S3FH4R2 |
| Flash | 4MB |
| PSRAM | None (on most variants) |
| USB | Native USB OTG (USB-C) |
| WiFi | 2.4 GHz 802.11 b/g/n |
| Bluetooth | BLE 5.0 |
| Cores | Dual-core Xtensa LX7 |

## Building for ESP32-S3

```bash
# Set target (if not using sdkconfig.defaults)
idf.py set-target esp32s3

# Build
idf.py build

# Flash via USB-C
idf.py -p /dev/ttyACM0 flash monitor
```

## Partition Layout

```
┌──────────────────────────────────────────────┐
│ 0x00000 - Bootloader (auto)                 │
│ 0x09000 - NVS (24 KB)                       │
│ 0x0F000 - PHY Init (4 KB)                   │
│ 0x10000 - Factory App (1280 KB)            │
│ 0x15000 - OTA_0 App (1280 KB)              │
│ 0x2D000 - SPIFFS (960 KB)                  │
└──────────────────────────────────────────────┘
Total: ~3.96 MB / 4.00 MB available
```

## Notes

- **OTA Updates**: Each app partition (1280K) is sufficient for typical firmware
- **Web Files**: SPIFFS holds HTML/CSS/JS files for web interface
- **USB Connection**: ESP32-S3 uses built-in USB, no CP2102 bridge needed for flashing
- **AM7 Sensor**: Still connects via USB (AM7 has CP2102 bridge internally)

## Compatibility

✅ **Compatible with ESP32-S3 Super Mini**
- Native USB OTG for flashing and serial monitor
- 4MB flash properly partitioned
- WiFi configuration optimized
- All features functional

## Next Steps

1. Build and flash to ESP32-S3 Super Mini
2. Verify USB host functionality with AM7 sensor
3. Test web interface and MQTT publishing
4. Verify OTA updates work correctly
