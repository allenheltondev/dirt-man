# Dirt Man

IoT environmental monitoring system for tracking soil moisture, temperature, humidity, and atmospheric pressure. Built with ESP32 firmware, designed for agricultural and greenhouse applications.

## What's Inside

This repository contains the complete firmware for an ESP32-based environmental monitoring device that:

- Reads multiple environmental sensors (BME280, DS18B20, capacitive soil moisture)
- Displays real-time data and historical graphs on a TFT screen
- Transmits averaged sensor readings to a remote API via WiFi
- Buffers data offline and syncs when connectivity returns
- Provides serial console configuration (no recompilation needed)

## Project Structure

```
dirt-man/
├── firmware/          # ESP32 firmware (C++)
│   ├── src/          # Source code
│   ├── include/      # Headers and models
│   ├── test/         # Unit tests
│   ├── docs/         # Comprehensive documentation
│   └── scripts/      # Build and test scripts
├── backend/          # API server (planned)
└── ui/               # Web dashboard (planned)
```

## Quick Start

### Hardware Requirements

- ESP32 development board (ESP32-WROOM-32 or compatible)
- BME280 sensor (temperature, humidity, pressure)
- DS18B20 temperature probe
- Capacitive soil moisture sensor
- TFT display (SPI, compatible with TFT_eSPI)
- 4.7kΩ pull-up resistors for I2C and OneWire

### Software Setup

1. **Install PlatformIO**
   ```bash
   pip install platformio
   ```

2. **Build and upload firmware**
   ```bash
   cd firmware
   pio run -e esp32dev --target upload
   pio device monitor -e esp32dev
   ```

3. **Configure via serial console** (115200 baud)
   ```
   > wifi
   Enter WiFi SSID: YourNetwork
   Enter WiFi Password: YourPassword

   > api
   Enter API Endpoint URL: https://api.example.com/sensor-data
   Enter API Token: your-secret-token

   > deviceid
   Enter Device ID: greenhouse-01

   > save
   Configuration saved successfully
   ```

4. **Calibrate soil sensor**
   ```
   > calibrate
   [Follow prompts for dry and wet calibration]

   > save
   ```

## Features

### Firmware Capabilities

- **Multi-sensor support** - BME280, DS18B20, capacitive soil moisture
- **Real-time display** - TFT screen with multiple pages and historical graphs
- **WiFi connectivity** - Automatic reconnection with offline buffering
- **Data averaging** - Configurable sample averaging before transmission
- **Idempotent retries** - Batch ID system prevents duplicate data on server
- **Serial configura
67800000,
  "sample_count": 20,
  "time_synced": true,
  "sensors": {
    "bme280_temp_c": 22.5,
    "ds18b20_temp_c": 21.8,
    "humidity_pct": 45.2,
    "pressure_hpa": 1013.25,
    "soil_moisture_pct": 62.3
  },
  "sensor_status": {
    "bme280": "ok",
    "ds18b20": "ok",
    "soil_moisture": "ok"
  },
  "health": {
    "uptime_ms": 7800000,
    "free_heap_bytes": 245760,
    "wifi_rssi_dbm": -65,
    "error_counters": {
      "sensor_read_failures": 0,
      "network_failures": 0,
      "buffer_overflows": 0
    }
  }
}
```

## Documentation

### Firmware Documentation

Comprehensive documentation is available in the `firmware/docs/` directory:

- **[Hardware Wiring](firmware/docs/HARDWARE_WIRING.md)** - Complete pin assignments and wiring diagrams
- **[Serial Console](firmware/docs/SERIAL_CONSOLE.md)** - Configuration commands and usage
- **[Calibration](firmware/docs/CALIBRATION.md)** - Soil moisture sensor calibration procedures
- **[API Specification](firmware/docs/API_SPECIFICATION.md)** - API endpoint requirements and data format
- **[Deployment](firmware/docs/DEPLOYMENT.md)** - Complete deployment checklist
- **[Unit Test Guide](firmware/docs/UNIT_TEST_GUIDE.md)** - Testing framework and procedures
- **[Development Guide](firmware/DEVELOPMENT.md)** - Development workflow and best practices

### Getting Started

1. Start with [firmware/README.md](firmware/README.md) for firmware overview
2. Follow [firmware/docs/HARDWARE_WIRING.md](firmware/docs/HARDWARE_WIRING.md) for hardware setup
3. Use [firmware/docs/SERIAL_CONSOLE.md](firmware/docs/SERIAL_CONSOLE.md) for configuration
4. Implement your API using [firmware/docs/API_SPECIFICATION.md](firmware/docs/API_SPECIFICATION.md)
5. Deploy using [firmware/docs/DEPLOYMENT.md](firmware/docs/DEPLOYMENT.md)

## Development

### Build Commands

```bash
# Build firmware
pio run -e esp32dev

# Upload to device
pio run -e esp32dev --target upload

# Monitor serial output
pio device monitor -e esp32dev

# Run tests
cd firmware
scripts\test.bat test_config_manager
scripts\test.bat test_data_manager

# Format code
scripts\lint.bat fix

# Check formatting
scripts\lint.bat check
```

### Testing

The firmware includes comprehensive unit tests with property-based testing:

```bash
# Run specific test
scripts\test.bat test_config_manager

# Run with verbose output
scripts\test.bat test_config_manager -v
```

Available test suites:
- Configuration management
- Data processing and averaging
- Ring buffer operations
- Display buffer management
- Time management
- Network operations
- Sensor initialization

See [firmware/docs/UNIT_TEST_GUIDE.md](firmware/docs/UNIT_TEST_GUIDE.md) for details.

### Code Quality

The project uses clang-format and clang-tidy for code quality:

```bash
# Check code formatting
scripts\lint.bat check

# Auto-fix formatting issues
scripts\lint.bat fix

# Run static analysis
pio check -e esp32dev
```

## API Server (Planned)

The `backend/` directory will contain the API server implementation for receiving and storing sensor data. The server must implement the API specification defined in [firmware/docs/API_SPECIFICATION.md](firmware/docs/API_SPECIFICATION.md).

Key requirements:
- Accept POST requests with JSON sensor data
- Implement batch_id deduplication for idempotent retries
- Return acknowledged_batch_ids in response
- Support both single and batch transmissions
- Provide authentication via Bearer token

Example implementations (Python Flask, Node.js Express) are provided in the API specification.

## Web Dashboard (Planned)

The `ui/` directory will contain a web-based dashboard for:
- Viewing real-time sensor data
- Historical data visualization
- Device management and configuration
- Alert configuration and monitoring
- Multi-device support

## Use Cases

- **Greenhouse monitoring** - Track temperature, humidity, and soil moisture
- **Garden automation** - Trigger irrigation based on soil moisture
- **Weather stations** - Monitor atmospheric conditions
- **Indoor plant care** - Ensure optimal growing conditions
- **Agricultural research** - Collect environmental data for analysis
- **Hydroponics** - Monitor growing environment parameters

## Hardware Compatibility

### Tested Boards
- ESP32-WROOM-32 DevKit
- ESP32 DevKitC
- NodeMCU-32S

### Supported Sensors
- **BME280** - Temperature, humidity, pressure (I2C)
- **DS18B20** - Waterproof temperature probe (OneWire)
- **Capacitive soil moisture** - Analog sensor (ADC)

### Display Support
- TFT displays compatible with TFT_eSPI library
- SPI interface
- Tested with ILI9341, ST7735, ST7789

## Troubleshooting

### Common Issues

**Sensors not reading:**
- Check wiring connections
- Verify pull-up resistors (4.7kΩ on I2C and OneWire)
- Run `diag` command in serial console

**WiFi not connecting:**
- Verify SSID and password
- Check 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Check signal strength (RSSI > -75 dBm)

**Data not reaching server:**
- Verify API endpoint URL
- Check API token
- Test endpoint with curl
- Review server logs

See [firmware/README.md](firmware/README.md) for detailed troubleshooting.

## Contributing

Contributions welcome! Areas for improvement:

- Additional sensor support
- Enhanced power management
- Advanced calibration methods
- Additional display features
- Backend API implementation
- Web dashboard development
- Documentation improvements

## License

[Specify your license here]

## Acknowledgments

This project uses the following open-source libraries:

- [Adafruit BME280 Library](https://github.com/adafruit/Adafruit_BME280_Library)
- [DallasTemperature Library](https://github.com/milesburton/Arduino-Temperature-Control-Library)
- [OneWire Library](https://github.com/PaulStoffregen/OneWire)
- [TFT_eSPI Library](https://github.com/Bodmer/TFT_eSPI)
- [ArduinoJson Library](https://github.com/bblanchon/ArduinoJson)
- [Unity Testing Framework](https://github.com/ThrowTheSwitch/Unity)

## Support

For issues or questions:
1. Check the [firmware documentation](firmware/docs/README.md)
2. Review troubleshooting sections
3. Run diagnostics: `diag` command in serial console
4. Check serial output for error messages
5. Open an issue on GitHub

## Roadmap

- [x] ESP32 firmware with multi-sensor support
- [x] TFT display with real-time graphs
- [x] WiFi connectivity with offline buffering
- [x] Serial console configuration
- [x] Comprehensive unit tests
- [x] Complete documentation
- [ ] Backend API server implementation
- [ ] Web dashboard for data visualization
- [ ] Mobile app support
- [ ] Additional sensor types
- [ ] OTA firmware updates
- [ ] MQTT support
- [ ] Home Assistant integration

---

**Status:** Firmware complete and tested. Backend and UI in planning phase.
