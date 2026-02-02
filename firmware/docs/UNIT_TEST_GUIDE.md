# UNIT_TEST Build Flag Guide

## Overview

The `UNIT_TEST` build flag enables mock sensor mode for unit testing without physical hardware. This allows testing sensor logic, data processing, and business logic on the host machine using PlatformIO's native environment.

## Configuration

The `UNIT_TEST` flag is configured in `platformio.ini`:

```ini
[env:native]
platform = native
build_flags =
    -D UNIT_TEST
    -std=c++17
lib_deps =
    bblanchon/ArduinoJson@^7.0.4
```

## Usage in Code

### Conditional Compilation

Use `#ifdef UNIT_TEST` to provide mock implementations for hardware-dependent code:

```cpp
#ifdef UNIT_TEST
// Mock implementation for unit tests
float readBME280Temperature() {
    return 22.5;  // Mock temperature value
}
#else
// Real hardware implementation
float readBME280Temperature() {
    return bme280.readTemperature();
}
#endif
```

### Mock Sensor Manager Example

```cpp
class SensorManager {
public:
    SensorReadings readSensors() {
        SensorReadings readings = {};

        #ifdef UNIT_TEST
        // Mock sensor data for testing
        readings.bme280Temp = 22.5;
        readings.ds18b20Temp = 21.8;
        readings.humidity = 45.2;
        readings.pressure = 1013.25;
        readings.soilMoisture = 62.3;
        readings.soilMoistureRaw = 2048;
        readings.sensorStatus = 0xFF;  // All sensors available
        readings.monotonicMs = millis();
        #else
        // Real sensor reading implementation
        readings = readRealSensors();
        #endif

        return readings;
    }
};
```

### Testing Strategy

**Unit Tests (Native Environment):**
- Test data processing logic without hardware
- Test averaging calculations
- Test JSON formatting
- Test configuration validation
- Test buffer management
- Test batch ID generation

**Hardware Tests (ESP32 Environment):**
- Test actual sensor communication
- Test I2C/OneWire/ADC functionality
- Test display rendering
- Test WiFi connectivity
- Test HTTPS transmission

## Running Tests

### Native Unit Tests

```bash
# Run all native tests
pio test -e native

# Run specific test
pio test -e native -f test_data_manager

# Run with verbose output
pio test -e native -v
```

### ESP32 Hardware Tests

```bash
# Upload and run tests on ESP32
pio test -e esp32dev

# Run specific hardware test
pio test -e esp32dev -f test_sensor_hardware
```

## Mock Data Guidelines

### Realistic Mock Values

Use physically realistic values for mock data:

**BME280 Temperature:**
- Range: -40°C to 85°C
- Typical: 15°C to 30°C
- Mock: 22.5°C

**DS18B20 Temperature:**
- Range: -55°C to 125°C
- Typical: 10°C to 40°C
- Mock: 21.8°C

**Humidity:**
- Range: 0% to 100%
- Typical: 30% to 70%
- Mock: 45.2%

**Pressure:**
- Range: 300 hPa to 1100 hPa
- Typical: 950 hPa to 1050 hPa
- Mock: 1013.25 hPa

**Soil Moisture:**
- ADC Range: 0 to 4095
- Percentage: 0% to 100%
- Mock ADC: 2048 (mid-range)
- Mock Percentage: 62.3%

### Deterministic vs Random

**Deterministic mocks** (for unit tests):
```cpp
#ifdef UNIT_TEST
float getMockTemperature() {
    return 22.5;  // Always returns same value
}
#endif
```

**Random mocks** (for property-based tests):
```cpp
#ifdef UNIT_TEST
float getMockTemperature() {
    return random(15.0, 30.0);  // Random within range
}
#endif
```

## Example Test Structure

```cpp
#include <unity.h>

#ifdef UNIT_TEST

void test_averaging_calculation() {
    DataManager dataManager;

    // Add 20 mock readings
    for (int i = 0; i < 20; i++) {
        SensorReadings reading;
        reading.bme280Temp = 22.5;
        reading.humidity = 45.0;
        dataManager.addReading(reading);
    }

    // Verify averaging
    TEST_ASSERT_TRUE(dataManager.shouldPublish());
    AveragedData avg = dataManager.getAveragedData();
    TEST_ASSERT_EQUAL_FLOAT(22.5, avg.avgBme280Temp);
    TEST_ASSERT_EQUAL_FLOAT(45.0, avg.avgHumidity);
}

void test_batch_id_generation() {
    AveragedData data;
    data.sampleStartEpochMs = 1704067200000;
    data.sampleEndEpochMs = 1704067800000;

    String batchId = generateBatchId(data);

    TEST_ASSERT_TRUE(batchId.startsWith("device123_"));
    TEST_ASSERT_TRUE(batchId.indexOf("1704067200000") > 0);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_averaging_calculation);
    RUN_TEST(test_batch_id_generation);
    return UNITY_END();
}

#endif
```

## Best Practices

### 1. Isolate Hardware Dependencies

Keep hardware-specific code separate from business logic:

```cpp
// Good: Business logic separated
class DataProcessor {
    AveragedData calculateAverage(const std::vector<SensorReadings>& readings) {
        // Pure calculation - no hardware dependency
        // Can be tested in native environment
    }
};

// Hardware interface
class SensorHardware {
    #ifdef UNIT_TEST
    SensorReadings getMockReadings();
    #else
    SensorReadings getRealReadings();
    #endif
};
```

### 2. Use Dependency Injection

```cpp
class NetworkManager {
    NetworkManager(IHttpClient* client) : httpClient(client) {}

    // Can inject mock HTTP client for testing
};
```

### 3. Test Business Logic Thoroughly

Focus native tests on:
- Data transformations
- Calculations (averaging, calibration)
- Buffer management
- JSON formatting
- Configuration validation
- Error handling logic

### 4. Keep Mocks Simple

Don't over-engineer mocks. Simple return values are sufficient:

```cpp
#ifdef UNIT_TEST
// Simple mock - good
float readSensor() { return 22.5; }

// Complex mock - unnecessary
class MockSensorWithStateAndHistory {
    // Too complex for simple unit tests
};
#endif
```

## Limitations

### What Cannot Be Tested in Native Environment

- I2C/SPI/OneWire communication
- ADC readings
- GPIO operations
- WiFi connectivity
- TFT display rendering
- Hardware timers
- Interrupts
- Deep sleep modes

### What Can Be Tested in Native Environment

- Data structures
- Algorithms (averaging, calibration formulas)
- JSON serialization
- Configuration validation
- Buffer management (ring buffers, queues)
- Batch ID generation
- Error counter logic
- Timestamp calculations
- String formatting

## Troubleshooting

### Compilation Errors in Native Environment

**Problem:** Arduino-specific types not available

```cpp
// Error: 'uint8_t' was not declared
```

**Solution:** Include standard types or provide typedefs

```cpp
#ifdef UNIT_TEST
#include <cstdint>
typedef unsigned long millis_t;
#else
#include <Arduino.h>
typedef unsigned long millis_t;
#endif
```

### Missing Arduino Functions

**Problem:** `millis()`, `delay()`, etc. not available

**Solution:** Provide mock implementations

```cpp
#ifdef UNIT_TEST
unsigned long millis() {
    return 1000;  // Mock timestamp
}
void delay(unsigned long ms) {
    // No-op in tests
}
#endif
```

## Summary

The `UNIT_TEST` flag enables:
- ✅ Fast unit tests on host machine
- ✅ Testing without physical hardware
- ✅ Continuous integration compatibility
- ✅ Rapid development iteration
- ✅ Property-based testing support

Use it to test business logic, then validate hardware integration on actual ESP32 devices.
