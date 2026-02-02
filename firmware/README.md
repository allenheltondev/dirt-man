# ESP32 Sensor Firmware

Environmental monitoring system for ESP32 that collects sensor data, displays real-time information on a TFT screen, and transmits averaged readings to a remote API via WiFi.

## Features

- **Multi-sensor support**: BME280 (temperature, humidity, pressure), DS18B20 (temperature probe), capacitive soil moisture
- **Real-time display**: TFT screen with multiple pages showing current readings and historical graphs
- **WiFi connectivity**: Automatic connection with reconnection and offline buffering
- **Data averaging**: Configurable sample averaging before transmission
- **Idempotent retries**: Batch ID system prevents duplicate data on server
- **Serial configuration**: No recompilation needed for settings changes
- **Comprehensive diagnostics**: Error tracking and system health monitoring

## Quick Start

### 1. Hardware Setup

Wire your sensors and display according to the pin assignments in [docs/HARDWARE_WIRING.md](docs/HARDWARE_WIRING.md).

**Required components:**
- ESP32 development board
- BME280 sensor (I2C)
- DS18B20 temperature probe (OneWire)
- Capacitive soil moisture sensor (analog)
- TFT display (SPI, compatible with TFT_eSPI library)
- 4.7kΩ pull-up resistors

### 2. Build and Upload

```bash
# Install PlatformIO
pip install platformio

# Build and upload firmware
pio run -e esp32dev --target upload

# Monitor serial output
pio device monitor -e esp32dev
```

### 3. Configure via Serial Console

Connect to the serial console (115200 baud) and configure your device:

```
> wifi
Enter WiFi SSID: YourNetwork
Enter WiFi Password: YourPassword

> api
Enter API Endpoint URL: https://api.example.com/sensor-data
Enter API Token: your-secret-token

> deviceid
Enter Device ID: greenhouse-sensor-01

> save
Configuration saved successfully
```

### 4. Calibrate Soil Moisture Sensor

```
> calibrate
[Follow prompts for dry and wet calibration]

> save
```

See [docs/CALIBRATION.md](docs/CALIBRATION.md) for detailed calibration procedures.

## Documentation

### Essential Guides

- **[Hardware Wiring](docs/HARDWARE_WIRING.md)** - Complete pin assignments and wiring diagrams
- **[Serial Console](docs/SERIAL_CONSOLE.md)** - Configuration commands and usage
- **[Calibration](docs/CALIBRATION.md)** - Soil moisture sensor calibration procedures
- **[API Specification](docs/API_SPECIFICATION.md)** - API endpoint requirements and data format
- **[Deployment](docs/DEPLOYMENT.md)** - Complete deployment checklist

### Additional Documentation

- **[Unit Test Guide](docs/UNIT_TEST_GUIDE.md)** - Testing framework and procedures
- **[README](docs/README.md)** - Complete documentation index

## Project Structure

```
├── src/                    # Source code
│   ├── main.cpp           # Main application
│   ├── ConfigManager.cpp  # Configuration management
│   ├── SensorManager.cpp  # Sensor reading
│   ├── DataManager.cpp    # Data processing
│   ├── NetworkManager.cpp # WiFi and HTTP
│   ├── DisplayManager.cpp # TFT display
│   └── ...
├── include/               # Header files
│   ├── models/           # Data structures
│   └── ...
├── test/                  # Unit tests
├── docs/                  # Documentation
├── .kiro/specs/          # Design specifications
└── platformio.ini        # Build configuration
```

## Configuration

All configuration is done via serial console - no recompilation needed:

- **WiFi credentials**: SSID and password
- **API endpoint**: URL and authentication token
- **Device identifier**: Unique device name
- **Timing intervals**: Reading, publish, and display cycle intervals
- **Soil calibration**: Dry and wet ADC values

Configuration is stored in non-volatile storage (NVS) and persists across reboots.

## API Integration

The firmware transmits JSON payloads to your API endpoint:

```json
{
  "batch_id": "device-001_e_1704067200000_1704067800000",
  "device_id": "device-001",
  "sample_start_epoch_ms": 1704067200000,
  "sample_end_epoch_ms": 1704067800000,
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

See [docs/API_SPECIFICATION.md](docs/API_SPECIFICATION.md) for complete API details and example server implementations.

## Testing

The firmware includes comprehensive unit tests. Due to the current test structure, tests should be run individually:

```bash
# Run a specific test
scripts\test.bat test_config_manager
scripts\test.bat test_data_manager
scripts\test.bat test_time_manager

# Run with verbose output
scripts\test.bat test_config_manager -v
```

**Available tests:**
- `test_config_manager` - Configuration management tests
- `test_config_manager_properties` - Property-based config tests
- `test_data_manager` - Data processing tests
- `test_data_manager_properties` - Property-based data tests
- `test_data_manager_ring_buffer` - Ring buffer tests
- `test_data_manager_ring_buffer_properties` - Property-based ring buffer tests
- `test_data_manager_display_buffer` - Display buffer tests
- `test_data_manager_display_buffer_properties` - Property-based display buffer tests
- `test_time_manager` - Time management tests
- `test_time_manager_properties` - Property-based time tests
- `test_display_manager` - Display management tests (requires mocks)
- `test_display_manager_properties` - Property-based display tests (requires mocks)
- `test_sensitive_data_properties` - Sensitive data handling tests

Tests use mock sensors and run on the host machine without requiring ESP32 hardware.

See [docs/UNIT_TEST_GUIDE.md](docs/UNIT_TEST_GUIDE.md) for detailed testing documentation.

## Code Quality

### Linting and Formatting

The project uses clang-format and clang-tidy for code quality:

```bash
# Check code formatting
scripts\lint.bat check

# Auto-fix formatting issues
scripts\lint.bat fix

# Run static analysis (requires clang-tidy)
pio check -e esp32dev
```

**Requirements:**
- clang-format (install via LLVM or Visual Studio)
- clang-tidy (optional, for static analysis)

See [scripts/README.md](scripts/README.md) for detailed script documentation.

## Troubleshooting

### Sensors Not Reading
- Check wiring connections
- Verify pull-up resistors (4.7kΩ on I2C and OneWire)
- Run `diag` command in serial console
- See [docs/HARDWARE_WIRING.md](docs/HARDWARE_WIRING.md)

### WiFi Not Connecting
- Verify SSID and password
- Check 2.4GHz WiFi (ESP32 doesn't support 5GHz)
- Check signal strength (RSSI > -75 dBm)
- See [docs/SERIAL_CONSOLE.md](docs/SERIAL_CONSOLE.md)

### Display Not Working
- Check SPI wiring
- Verify TFT_eSPI configuration
- Check backlight connection
- See [docs/HARDWARE_WIRING.md](docs/HARDWARE_WIRING.md)

### Data Not Reaching Server
- Verify API endpoint URL
- Check API token
- Test endpoint with curl
- Review server logs
- See [docs/API_SPECIFICATION.md](docs/API_SPECIFICATION.md)

## System Requirements

### Hardware
- ESP32 development board (ESP32-WROOM-32 or compatible)
- 4MB flash minimum
- 520KB RAM
- 2.4GHz WiFi

### Software
- PlatformIO Core 6.1+
- Python 3.7+
- USB drivers for ESP32

### Libraries (automatically installed)
- Adafruit BME280 Library
- DallasTemperature Library
- OneWire Library
- TFT_eSPI Library
- ArduinoJson Library
- Unity (for testing)

## License

[Specify your license here]

## Contributing

Contributions welcome! Areas for improvement:
- Additional sensor support
- Enhanced power management
- Advanced calibration methods
- Additional display features
- Improved error handling

## Support

For issues or questions:
1. Check the [documentation](docs/README.md)
2. Review troubleshooting sections
3. Run diagnostics: `diag` command in serial console
4. Check serial output for error messages

## Acknowledgments

This firmware uses the following open-source libraries:
- [Adafruit BME280 Library](https://github.com/adafruit/Adafruit_BME280_Library)
- [DallasTemperature Library](https://github.com/milesburton/Arduino-Temperature-Control-Library)
- [OneWire Library](https://github.com/PaulStoffregen/OneWire)
- [TFT_eSPI Library](https://github.com/Bodmer/TFT_eSPI)
- [ArduinoJson Library](https://github.com/bblanchon/ArduinoJson)
