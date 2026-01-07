# Quick Start Guide

## Prerequisites

1. **ESP-IDF**: Install ESP-IDF v5.0 or later
   ```bash
   # Follow: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/
   ```

2. **Hardware**:
   - ESP32-S3 Super Mini board (4MB flash, native USB OTG)
   - AirMaster AM7 sensor
   - USB-C cable for flashing and power
   - Wi-Fi network

## Build and Flash

1. **Set up ESP-IDF environment**:
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```

2. **Navigate to project**:
   ```bash
   cd /path/to/airmaster-adapter-esp32
   ```

3. **Configure project** (optional):
   ```bash
   idf.py menuconfig
   ```

4. **Build**:
   ```bash
   idf.py build
   ```

5. **Flash to device**:
   ```bash
   idf.py -p /dev/ttyUSB0 flash
   ```
   *Note: Replace `/dev/ttyUSB0` with your serial port*

6. **Monitor logs**:
   ```bash
   idf.py -p /dev/ttyUSB0 monitor
   ```

## First-Time Setup

1. **Find device IP**:
   - Check serial monitor output for IP address
   - Or scan your network for ESP32 device

2. **Configure via web interface**:
   - Open browser: `http://<device-ip>/`
   - Go to Settings (⚙ button)
   - Configure:
     - Wi-Fi credentials
     - MQTT broker details
     - Publishing interval
   - Save and reboot

3. **Verify operation**:
   - Dashboard should show sensor status
   - Check MQTT broker for messages on topic: `airmaster/sensors`

## Troubleshooting

### Device won't connect to Wi-Fi
- Check SSID and password are correct
- Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
- Check signal strength

### Can't access web interface
- Verify device has IP address (check serial monitor)
- Ping device IP
- Check firewall settings

### MQTT not working
- Verify broker IP and port
- Check broker allows connections from device IP
- Test with MQTT client (mosquitto_sub)

### AM7 sensor not detected
- Check USB connections
- Verify VID/PID in code matches your sensor
- USB host feature may need additional hardware

## Development

### Add dependencies
Edit [main/CMakeLists.txt](main/CMakeLists.txt) and add to `REQUIRES` section.

### Clean build
```bash
idf.py fullclean
idf.py build
```

### Change log level
```bash
idf.py menuconfig
# Component config → Log output → Default log verbosity
```

## Project Structure

```
.
├── main/                   # Main application
│   ├── main.c             # Entry point
│   ├── am7.c/h            # Sensor driver
│   ├── mqtt.c/h           # MQTT client
│   ├── settings.c/h       # NVS settings
│   ├── webserver.c/h      # HTTP server
│   └── wifi_manager.c/h   # WiFi management
├── spiffs/                # Web interface files
│   ├── index.html
│   ├── settings.html
│   ├── app.js
│   ├── settings.js
│   └── style.css
├── CMakeLists.txt         # Build configuration
├── partitions.csv         # Flash partition layout
└── README.md
```

## Next Steps

- See [IMPROVEMENTS.md](IMPROVEMENTS.md) for enhancement ideas
- Configure OTA updates for remote firmware updates
- Set up SSL/TLS for secure communication
- Implement data buffering for offline operation
