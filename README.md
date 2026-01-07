# AirMaster Adapter ESP32

ESP32-based gateway for AirMaster AM7 air quality sensors with MQTT publishing and web interface.

## Features

- **AM7 Sensor Interface**: Reads air quality data via USB (temperature, humidity, CO2, PM2.5, VOC)
- **MQTT Publishing**: Publishes sensor data to MQTT broker in JSON format
- **Web Interface**: Configuration and monitoring via embedded web server
- **Persistent Settings**: Configuration stored in NVS flash memory
- **Auto-reconnect**: Automatic reconnection for both MQTT and sensor failures

## Hardware Requirements

- ESP32-S3 Super Mini board (with native USB OTG support)
- AirMaster AM7 air quality sensor
- Wi-Fi network access

## Software Architecture

### Components

- **main.c**: Application entry point and task initialization
- **am7.c/h**: AM7 sensor communication via USB
- **mqtt.c/h**: MQTT client with auto-reconnect
- **settings.c/h**: NVS-based persistent configuration
- **webserver.c/h**: HTTP server with REST API and static file serving
- **spiffs/**: Web interface files (HTML, CSS, JavaScript)

### Data Flow

```
AM7 Sensor (USB) → am7_task → MQTT Task → MQTT Broker
                              ↓
                         Web Server (Status API)
```

## API Endpoints

- `GET /` - Main dashboard
- `GET /settings` - Settings page
- `GET /api/status` - Get system status (JSON)
- `GET /api/settings` - Get current settings (JSON)
- `POST /api/settings` - Save settings (JSON)
- `POST /api/reboot` - Reboot device

## Building & Flashing

```bash
# Configure project
idf.py menuconfig

# Build
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

## Configuration

Default settings can be changed via web interface at `http://<device-ip>/settings`:

- **Wi-Fi**: SSID and password
- **MQTT**: Broker IP, port, username, password
- **Publish Interval**: Data publishing frequency (seconds)

## MQTT Message Format

Topic: `airmaster/sensors`

```json
{
  "temp": 23.5,
  "humidity": 45.2,
  "co2": 420,
  "pm25": 12,
  "voc": 0.15
}
```

## Known Issues & TODs

1. **USB Host Implementation**: AM7 USB communication is not fully implemented
   - Need actual VID/PID values for AM7 device
   - Device enumeration and configuration needs completion
   - Bulk transfer implementation required

2. **Wi-Fi Management**: Wi-Fi configuration not yet implemented
   - Settings UI exists but backend needs WiFi provisioning
   - Should add WiFi connection status to web dashboard

3. **Settings Persistence**: Settings POST handler needs full NVS write implementation

4. **Error Recovery**: Add watchdog timer for crash recovery

## License

[Add license information]

## Version

1.0.0
