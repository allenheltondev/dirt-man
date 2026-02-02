# Integration Tests for ESP32 Sensor Firmware

This directory contains integration tests that run on actual ESP32 hardware to verify sensor functionality and hardware connectivity.

## Overview

Integration tests validate:
- **Sensor Hardware**: All sensors are properly connected and responding
- **Network Connectivity**: WiFi connection and network operations
- **Real-world Behavior**: Actual sensor readings are within expected ranges

## Test Files

### test_sensor_hardware.cpp
Comprehensive tests for all sensor hardware:
- **Initialization Tests**: Verify all sensors initialize successfully
- **Availability Tests**: Check each sensor is detected and responding
- **Reading Tests**: Validate sensor readings are within physical ranges
- **Stability Tests**: Ensure consecutive readings are stable
- **Calibration Tests**: Verify calibration functions work correctly
- **Pin Configuration Tests**: Confirm GPIO pins are correctly configured

### test_network_manager.cpp
Tests for network connectivity and WiFi operations.
ive moisture sensor
   - Connection: Analog (GPIO32 - ADC1_CH4)
   - Note: Must use ADC1 pins (GPIO 32-39) to avoid WiFi conflicts

### Wiring Diagram
See `docs/HARDWARE_WIRING.md` for detailed wiring instructions.

## Running Integration Tests

### Prerequisites
1. ESP32 board connected via USB
2. All sensors properly wired and powered
3. PlatformIO installed

### Run All Integration Tests
```bash
# Windows
scripts\test.bat integration

# Direct PlatformIO command
pio test -e esp32test
```

### Run Specific Test
```bash
# Run only sensor hardware tests
pio test -e esp32test -f integration/test_sensor_hardware

# Run only network tests
pio test -e esp32test -f integration/test_network_manager
```

### Verbose Output
```bash
# Get detailed test output
scripts\test.bat integration -v
```

## Test Execution Flow

1. **Upload**: PlatformIO compiles and uploads test firmware to ESP32
2. **Execute**: ESP32 runs tests and sends results via serial
3. **Report**: Test results are displayed in terminal

## Understanding Test Results

### Success
```
test_sensor_hardware.cpp:45:test_bme280_sensor_is_available [PASSED]
test_sensor_hardware.cpp:54:test_ds18b20_sensor_is_available [PASSED]
...
-----------------------
5 Tests 0 Failures 0 Ignored
OK
```

### Failure Examples

#### Sensor Not Detected
```
test_sensor_hardware.cpp:45:test_bme280_sensor_is_available [FAILED]
BME280 sensor not detected. Check I2C wiring (SDA=GPIO21, SCL=GPIO22) and I2C address (0x76 or 0x77)
```

**Solution**:
- Verify I2C wiring
- Check sensor power (3.3V)
- Verify I2C address with `i2cdetect` tool
- Ensure pull-up resistors are present

#### Invalid Reading
```
test_sensor_hardware.cpp:120:test_bme280_temperature_reading_is_valid [FAILED]
BME280 temperature reading out of valid range (-40 to 85°C)
```

**Solution**:
- Sensor may be faulty
- Check for loose connections
- Verify sensor is not overheating

## Troubleshooting

### All Sensors Fail
- **Check Power**: Verify 3.3V power supply to all sensors
- **Check Ground**: Ensure common ground between ESP32 and sensors
- **Check USB Cable**: Use a data cable, not charge-only

### BME280 Not Detected
- **I2C Address**: Try both 0x76 and 0x77 addresses
- **Pull-up Resistors**: Add 4.7kΩ resistors on SDA and SCL if missing
- **Wire Length**: Keep I2C wires short (<30cm)

### DS18B20 Not Detected
- **Pull-up Resistor**: Ensure 4.7kΩ resistor between data line and 3.3V
- **Polarity**: Verify correct pin connections (VCC, GND, Data)
- **Multiple Sensors**: Each DS18B20 needs unique address

### Soil Moisture Always 0% or 100%
- **Calibration**: Run calibration with sensor in dry and wet soil
- **ADC Pin**: Verify using ADC1 pin (GPIO 32-39)
- **Sensor Type**: Ensure using capacitive sensor, not resistive

### Unstable Readings
- **Power Supply**: Use stable 3.3V power source
- **Decoupling Capacitors**: Add 0.1µF capacitors near sensors
- **Wire Shielding**: Use shielded cables for long wire runs
- **Grounding**: Ensure proper grounding

## Test Development

### Adding New Tests

1. Create test function:
```cpp
void test_new_sensor_feature(void) {
    sensorManager->initialize();

    // Your test code here

    TEST_ASSERT_TRUE_MESSAGE(condition, "Error message");
}
```

2. Register in `setup()`:
```cpp
void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_new_sensor_feature);
    UNITY_END();
}
```

### Test Assertions

Common Unity assertions:
- `TEST_ASSERT_TRUE(condition)` - Assert condition is true
- `TEST_ASSERT_EQUAL(expected, actual)` - Assert values are equal
- `TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)` - Assert floats are close
- `TEST_ASSERT_TRUE_MESSAGE(condition, msg)` - Assert with custom message

## Continuous Integration

These tests can be integrated into CI/CD pipelines with hardware-in-the-loop testing:

1. Connect ESP32 to CI server
2. Run tests automatically on code changes
3. Report results to development team

## Best Practices

1. **Test Isolation**: Each test should be independent
2. **Clear Messages**: Provide helpful error messages for failures
3. **Realistic Ranges**: Use expected ranges for your environment
4. **Stability Checks**: Verify readings are stable over time
5. **Hardware Validation**: Test actual hardware, not mocks

## Related Documentation

- `docs/HARDWARE_WIRING.md` - Detailed wiring instructions
- `docs/UNIT_TEST_GUIDE.md` - Unit testing guide
- `test/unit/README.md` - Unit tests (run on host machine)
- `README.md` - Project overview

## Support

If tests fail consistently:
1. Check hardware connections
2. Verify sensor specifications
3. Review error messages carefully
4. Consult sensor datasheets
5. Check ESP32 pin assignments

For persistent issues, verify:
- Sensor compatibility with 3.3V logic
- Correct sensor models (BME280, not BMP280)
- ESP32 board variant matches configuration
