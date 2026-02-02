# Sensor Hardware Testing Guide

This guide explains how to test all sensors on your ESP32 to ensure they're properly connected and functional.

## Quick Test

Connect your ESP32 with all sensors wired, then run:

```bash
scripts\test.bat integration
```

This will upload test firmware to your ESP32 and verify all sensors are working.

## What Gets Tested

### Sensor Detection
- ✓ BME280 environmental sensor (I2C)
- ✓ DS18B20 temperature sensor (OneWire)
- ✓ Soil moisture sensor (ADC)

### Sensor Readings
- ✓ Temperature readings are within valid ranges
- ✓ Humidity readings are 0-100%
- ✓ Pressure readings are realistic
- ✓ Soil moisture readings are 0-100%
- ✓ Readings are stable over time

### Hardware Configuration
- ✓ I2C pins (GPIO21, GPIO22)
- ✓ OneWire pin (GPIO4)
- ✓ ADC pin (GPIO32 - ADC1 for WiFi compatibility)

## Expected Output

### All Tests Pass ✓
```
test_sensor_hardware.cpp:45:test_bme280_sensor_is_available [PASSED]
test_sensor_hardware.cpp:54:test_ds18b20_sensor_is_available [PASSED]
test_sensor_hardware.cpp:63:test_humidity_sensor_is_available [PASSED]
test_sensor_hardware.cpp:72:test_pressure_sensor_is_available [PASSED]
test_sensor_hardware.cpp:81:test_soil_moisture_sensor_is_available [PASSED]
test_sensor_hardware.cpp:120:test_bme280_temperature_reading_is_valid [PASSED]
test_sensor_hardware.cpp:132:test_ds18b20_temperature_reading_is_valid [PASSED]
test_sensor_hardware.cpp:144:test_humidity_reading_is_valid [PASSED]
tes
est_humidity_sensor_is_available` fails
- `test_pressure_sensor_is_available` fails

**Solutions:**
1. Check I2C wiring:
   - SDA → GPIO21
   - SCL → GPIO22
   - VCC → 3.3V
   - GND → GND

2. Verify I2C address:
   - Most BME280 modules use 0x76
   - Some use 0x77
   - Check your module's documentation

3. Add pull-up resistors:
   - 4.7kΩ resistor from SDA to 3.3V
   - 4.7kΩ resistor from SCL to 3.3V

4. Check power:
   - BME280 requires 3.3V (not 5V!)
   - Verify voltage with multimeter

### DS18B20 Not Detected

**Symptoms:**
- `test_ds18b20_sensor_is_available` fails

**Solutions:**
1. Check OneWire wiring:
   - Data → GPIO4
   - VCC → 3.3V
   - GND → GND

2. Add pull-up resistor:
   - 4.7kΩ resistor from Data to 3.3V
   - This is REQUIRED for DS18B20

3. Verify sensor polarity:
   - DS18B20 has specific pin order
   - Check datasheet for your module

4. Test with single sensor:
   - If using multiple DS18B20s, test one at a time
   - Each sensor needs unique address

### Soil Moisture Always 0% or 100%

**Symptoms:**
- `test_soil_moisture_reading_is_valid` passes but reading is always 0% or 100%

**Solutions:**
1. Check ADC wiring:
   - Signal → GPIO32 (must be ADC1 pin!)
   - VCC → 3.3V
   - GND → GND

2. Verify sensor type:
   - Use capacitive sensor (recommended)
   - Resistive sensors may give poor results

3. Calibrate sensor:
   - Run calibration with sensor in dry soil (0%)
   - Run calibration with sensor in wet soil (100%)
   - See calibration guide below

4. Check ADC pin:
   - Must use GPIO 32-39 (ADC1)
   - ADC2 pins conflict with WiFi

### Invalid Temperature Readings

**Symptoms:**
- `test_bme280_temperature_reading_is_valid` fails
- Temperature is -40°C or 85°C (sensor limits)

**Solutions:**
1. Check sensor initialization:
   - Sensor may not be initializing properly
   - Try power cycling the ESP32

2. Verify connections:
   - Loose wires can cause invalid readings
   - Check all connections are secure

3. Check for interference:
   - Keep sensor away from heat sources
   - Avoid direct sunlight during testing

### Unstable Readings

**Symptoms:**
- `test_multiple_consecutive_readings_are_stable` fails
- Readings vary significantly between tests

**Solutions:**
1. Improve power supply:
   - Use stable 3.3V power source
   - Add 0.1µF decoupling capacitors near sensors

2. Reduce electrical noise:
   - Keep sensor wires short
   - Use shielded cables for long runs
   - Ensure proper grounding

3. Wait for sensor stabilization:
   - Some sensors need warm-up time
   - Add delay before first reading

## Sensor Calibration

### Soil Moisture Calibration

1. **Dry Calibration:**
   ```cpp
   // Place sensor in completely dry soil
   // Note the raw ADC value (typically 2800-3200)
   uint16_t dryValue = 3000;
   ```

2. **Wet Calibration:**
   ```cpp
   // Place sensor in saturated soil
   // Note the raw ADC value (typically 1200-1600)
   uint16_t wetValue = 1500;
   ```

3. **Apply Calibration:**
   ```cpp
   sensorManager.calibrateSoilMoisture(dryValue, wetValue);
   ```

4. **Verify:**
   - Dry soil should read ~0%
   - Wet soil should read ~100%
   - Moist soil should read 40-60%

## Hardware Checklist

Before running tests, verify:

- [ ] ESP32 connected via USB
- [ ] All sensors powered (3.3V)
- [ ] Common ground between ESP32 and sensors
- [ ] BME280 on I2C (GPIO21, GPIO22)
- [ ] DS18B20 on OneWire (GPIO4) with pull-up
- [ ] Soil moisture on ADC1 (GPIO32)
- [ ] Pull-up resistors installed where needed
- [ ] No loose connections
- [ ] Correct sensor models (BME280, not BMP280)

## Running Individual Tests

Test specific functionality:

```bash
# Test only sensor detection
pio test -e esp32test -f integration/test_sensor_hardware

# Test only network connectivity
pio test -e esp32test -f integration/test_network_manager
```

## Continuous Monitoring

For ongoing sensor validation:

1. Run tests before deployment
2. Run tests after hardware changes
3. Run tests if sensors behave unexpectedly
4. Include in CI/CD pipeline with hardware-in-the-loop

## Next Steps

Once all tests pass:

1. ✓ Sensors are properly connected
2. ✓ Hardware configuration is correct
3. ✓ Ready for production firmware deployment

Deploy production firmware:
```bash
pio run -e esp32dev --target upload
```

## Related Documentation

- `test/integration/README.md` - Detailed integration test documentation
- `docs/HARDWARE_WIRING.md` - Complete wiring guide
- `docs/UNIT_TEST_GUIDE.md` - Unit testing guide
- `README.md` - Project overview

## Support

If tests continue to fail:
1. Double-check all wiring against `docs/HARDWARE_WIRING.md`
2. Verify sensor specifications match expected models
3. Test sensors individually to isolate issues
4. Check ESP32 board variant matches configuration
5. Consult sensor datasheets for troubleshooting

**Common Issues:**
- Wrong I2C address (0x76 vs 0x77)
- Missing pull-up resistors
- 5V sensors on 3.3V ESP32 (use level shifters)
- ADC2 pins used instead of ADC1 (WiFi conflict)
- Incorrect sensor models (BMP280 vs BME280)
