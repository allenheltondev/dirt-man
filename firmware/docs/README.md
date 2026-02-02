# ESP32 Sensor Firmware - Documentation Index

## Overview

This directory contains comprehensive documentation for the ESP32 sensor firmware project. The firmware collects environmental data from multiple sensors, displays real-time information on a TFT screen, and transmits averaged readings to a remote API via WiFi.

## Quick Start Guide

For first-time users, follow these documents in order:

1. **[HARDWARE_WIRING.md](HARDWARE_WIRING.md)** - Wire your sensors and display to the ESP32
2. **[SERIAL_CONSOLE.md](SERIAL_CONSOLE.md)** - Configure WiFi, API endpoint, and device settings
3. **[CALIBRATION.md](CALIBRATION.md)** - Calibrate the soil moisture sensor
4. **[API_SPECIFICATION.md](API_SPECIFICATION.md)** - Set up your API endpoint to receive data
5. **[DEPLOYMENT.md](DEPLOYMENT.md)** - Complete deployment checklist and go live

## Documentation Files

### Hardware Setup

**[HARDWARE_WIRING.md](HARDWARE_WIRING.md)**
- Complete pin assignments and wiring diagrams
- Component specifications and requirements
- Power supply considerations
- Troubleshooting hardware issues
- Safety notes and best practices

**Key Topics:**
- BME280 sensor (I2C) wiring
- DS18B20 sensor (OneWire) wiring with pull-up resistor
- Soil moisture sensor (ADC) wiring
- TFT display (SPI) wiring
- Pin conflict avoidance
- Power consumption estimates

### Configuration

**[SERIAL_CONSOLE.md](SERIAL_CONSOLE.md)**
- Serial console connection instructions
- Complete command reference
- Configuration workflows
- Diagnostic commands
- Monitoring and troubleshooting

**Key Topics:**
- WiFi credential configuration
- API endpoint setup
- Timing interval adjustment
- Device identifier assignment
- Configuration validation and saving
- System diagnostics

### Calibration

**[CALIBRATION.md](CALIBRATION.md)**
- Soil moisture sensor calibration procedures
- Multiple calibration methods (quick, soil-based, in-situ)
- Step-by-step calibration workflow
- Validation and testing
- Troubleshooting calibration issues

**Key Topics:**
- Two-point calibration theory
- Dry point calibration
- Wet point calibration
- Calibration verification
- Temperature compensation
- Calibration maintenance

### API Integration

**[API_SPECIFICATION.md](API_SPECIFICATION.md)**
- API endpoint requirements
- JSON payload format specification
- Request/response examples
- Idempotent retry mechanism
- Example server implementations

**Key Topics:**
- HTTPS/HTTP protocol requirements
- Authentication with bearer tokens
- Dual timestamp system (epoch and uptime)
- Batch ID format and uniqueness
- Sensor status and health metrics
- Server-side deduplication

### Deployment

**[DEPLOYMENT.md](DEPLOYMENT.md)**
- Comprehensive deployment checklist
- Pre-deployment preparation
- Initial configuration steps
- Network and sensor testing
- 24-hour burn-in procedures
- Ongoing maintenance schedule

**Key Topics:**
- Hardware verification
- Firmware upload and testing
- Network connectivity testing
- Sensor calibration verification
- Physical installation guidelines
- Production sign-off criteria

## Additional Documentation

### Implementation Documentation

Located in the main `docs/` directory:

- **ConfigManager_Implementation.md** - Configuration manager implementation details
- **UNIT_TEST_GUIDE.md** - Unit testing framework and procedures
- **Task_*_Complete.md** - Implementation completion records for various tasks

### Hardware Testing

- **HARDWARE_TASKS.md** - Hardware testing task list
- **HARDWARE_TASKS_TRACKER.md** - Hardware testing progress tracker
- **HARDWARE_TESTING_CHECKLIST.md** - Hardware validation checklist

## System Architecture

### Components

The firmware consists of several manager components:

1. **ConfigManager** - Configuration storage and validation (NVS)
2. **SensorManager** - Sensor initialization and reading
3. **DataManager** - Data averaging, buffering, and storage
4. **NetworkManager** - WiFi connectivity and HTTP transmission
5. **DisplayManager** - TFT display rendering and page management
6. **TimeManager** - NTP synchronization and timestamp management
7. **SystemStatusManager** - System health and diagnostics
8. **PowerManager** - Power management and sleep modes (optional)

### Data Flow

```
Sensors → SensorManager → DataManager → NetworkManager → API Server
                              ↓
                        DisplayManager → TFT Display
```

### Key Features

- **Multi-sensor support:** BME280, DS18B20, soil moisture
- **Data averaging:** Configurable sample averaging before transmission
- **Offline buffering:** Ring buffer for data when network unavailable
- **Idempotent retries:** Batch ID system prevents duplicate data
- **Real-time display:** TFT screen with multiple pages and graphs
- **Dual timestamps:** Epoch and uptime timestamps for reliability
- **Comprehensive diagnostics:** Error tracking and system health monitoring
- **Serial configuration:** No recompilation needed for settings changes

## Common Workflows

### Initial Setup Workflow

1. Wire hardware according to HARDWARE_WIRING.md
2. Upload firmware to ESP32
3. Connect to serial console (115200 baud)
4. Configure WiFi credentials
5. Configure API endpoint and token
6. Set device identifier
7. Save configuration
8. Reboot and verify connection

### Calibration Workflow

1. Connect to serial console
2. Run `calibrate` command
3. Place sensor in dry condition
4. Record dry ADC value
5. Place sensor in wet condition
6. Record wet ADC value
7. Save configuration
8. Verify calibration accuracy

### Troubleshooting Workflow

1. Run `diag` command to check system status
2. Review error counters and last error
3. Check sensor status (OK/ERROR)
4. Verify WiFi connection and RSSI
5. Monitor serial output for error messages
6. Consult troubleshooting sections in relevant docs

### Deployment Workflow

1. Complete pre-deployment preparation
2. Perform initial configuration
3. Calibrate sensors
4. Test network connectivity
5. Verify display functionality
6. Run 24-hour burn-in test
7. Install in final location
8. Complete deployment sign-off

## Firmware Versions

### Version 1.0.0 (Current)

**Features:**
- Multi-sensor data collection
- WiFi connectivity with auto-reconnection
- HTTPS transmission with certificate validation
- TFT display with multiple pages
- Serial console configuration
- Soil moisture calibration
- Offline data buffering
- Idempotent retry mechanism
- System diagnostics

**Requirements:**
- ESP32 development board
- PlatformIO or Arduino IDE
- Required libraries: BME280, DS18B20, OneWire, TFT_eSPI, ArduinoJson

## Support and Resources

### Getting Help

1. **Check documentation** - Most questions answered in these docs
2. **Review troubleshooting sections** - Common issues and solutions
3. **Run diagnostics** - Use `diag` command to check system status
4. **Check serial output** - Error messages provide clues
5. **Community forums** - ESP32, Arduino, PlatformIO communities

### Reporting Issues

When reporting issues, include:
- Firmware version
- Hardware configuration
- Serial console output
- Diagnostic command output
- Steps to reproduce
- Expected vs. actual behavior

### Contributing

Contributions welcome! Areas for improvement:
- Additional sensor support
- Enhanced power management
- Advanced calibration methods
- Additional display features
- Improved error handling
- Documentation improvements

## License

[Specify your license here]

## Acknowledgments

This firmware uses the following open-source libraries:
- Adafruit BME280 Library
- DallasTemperature Library
- OneWire Library
- TFT_eSPI Library
- ArduinoJson Library

## Contact

[Your contact information here]

---

**Last Updated:** 2024-01-15

**Documentation Version:** 1.0.0

**Firmware Version:** 1.0.0
