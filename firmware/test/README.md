# Tests

This directory contains tests for the ESP32 sensor firmware.

## Test Types

### Unit Tests (test/unit/)
Fast tests that run on your development machine without ESP32 hardware. Use mocks to test business logic, algorithms, and data structures.

**Location:** `test/unit/`
**Environment:** Native (host machine)
**Speed:** Fast (milliseconds)
**Hardware:** None required

### Integration Tests (test/integration/)
Tests that run on actual ESP32 hardware to verify sensor connectivity, hardware functionality, and real-world behavior.

**Location:** `test/integration/`
**Environment:** ESP32 hardware
**Speed:** Slower (seconds to minutes)
**Hardware:** ESP32 + all sensors required

## Quick Start

### Run All Tests
```bash
# Windows
scripts\test.bat all

# Unit tests only (fast, no hardware)
scripts\test.bat unit

# Integration tests only (requires ESP32 + sensors)
scripts\test.bat integration
```

### Direct PlatformIO Commands
```bash
# Unit tests on host machine
pio test -e native

# Integration tests on ESP32 hardware
pio test -e esp32test
```

## Test Environments

### Native Environment (Unit Tests)

Tests run on your development machine without requiring ESP32 hardware. Uses the `UNIT_TEST` flag to enable mock sensors.

**Run all unit tests:**
```bash
scripts\test.bat unit
# or
pio test -e native
```

**Run specific test:**
```bash
pio test -e native -f unit/test_data_structures
```

**Run with verbose output:**
```bash
scripts\test.bat unit -v
```

### ESP32 Test Environment (Integration Tests)

Tests run on actual ESP32 hardware with real sensors connected. Validates hardware connectivity and real-world sensor behavior.

**Requirements:**
- ESP32 connected via USB
- All sensors wired and powered (BME280, DS18B20, soil moisture)
- See `test/integration/README.md` for wiring details

**Run all integration tests:**
```bash
scripts\test.bat integration
# or
pio test -e esp32test
```

**Run specific test:**
```bash
pio test -e esp32test -f integration/test_sensor_hardware
```

## Available Tests

### Unit Tests (test/unit/)

**test_data_structures.cpp** - Tests for data structures and models
**test_mock_sensor_example.cpp** - Demonstrates mock sensor usage

### Integration Tests (test/integration/)

**test_sensor_hardware.cpp** - Comprehensive sensor hardware validation:
- Verifies all sensors are detected and responding
- Validates sensor readings are within physical ranges
- Tests reading stability and calibration
- Confirms correct GPIO pin configuration

**test_network_manager.cpp** - Network connectivity tests
**test_network_manager_properties.cpp** - Property-based network tests

See `test/integration/README.md` for detailed integration test documentation.

### Legacy Tests (test/ root)

Tests in the root `test/` directory depend on source files and may need to be run individually or moved to appropriate subdirectories.

## Test Structure

### Example Test File

```cpp
#ifdef UNIT_TEST

#include <unity.h>
#include "MockSensor.h"

void setUp(void) {
    // Setup before each test
}

void tearDown(void) {
    // Cleanup after each test
}

void test_example() {
    TEST_ASSERT_EQUAL(1, 1);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_example);
    return UNITY_END();
}

#endif
```

## Available Tests

### test_mock_sensor_example.cpp

Demonstrates mock sensor usage and tests:
- Mock BME280 sensor values
- Mock DS18B20 sensor values
- Mock soil moisture ADC readings
- Soil moisture calibration formula
- Sensor failure modes
- Arduino mock functions (millis, delay)

**Run this test:**
```bash
pio test -e native -f test_mock_sensor_example
```

## Writing New Tests

### 1. Create Test File

Create a new file in the `test/` directory:
```
test/test_my_feature.cpp
```

### 2. Include Required Headers

```cpp
#ifdef UNIT_TEST

#include <unity.h>
#include "MockSensor.h"
// Include your code to test
#include "MyFeature.h"
```

### 3. Write Test Functions

```cpp
void test_my_feature_works() {
    // Arrange
    MyFeature feature;

    // Act
    int result = feature.doSomething();

    // Assert
    TEST_ASSERT_EQUAL(42, result);
}
```

### 4. Add Test Runner

```cpp
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_my_feature_works);
    return UNITY_END();
}

#endif
```

### 5. Run Your Test

```bash
pio test -e native -f test_my_feature
```

## Unity Assertions

Common assertions available:

```cpp
// Equality
TEST_ASSERT_EQUAL(expected, actual)
TEST_ASSERT_EQUAL_INT(expected, actual)
TEST_ASSERT_EQUAL_FLOAT(expected, actual)
TEST_ASSERT_EQUAL_STRING(expected, actual)

// Comparison
TEST_ASSERT_GREATER_THAN(threshold, actual)
TEST_ASSERT_LESS_THAN(threshold, actual)
TEST_ASSERT_GREATER_OR_EQUAL(threshold, actual)
TEST_ASSERT_LESS_OR_EQUAL(threshold, actual)

// Within Range
TEST_ASSERT_INT_WITHIN(delta, expected, actual)
TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)

// Boolean
TEST_ASSERT_TRUE(condition)
TEST_ASSERT_FALSE(condition)

// Null
TEST_ASSERT_NULL(pointer)
TEST_ASSERT_NOT_NULL(pointer)
```

## Mock Sensors

### Using Mock Sensors

```cpp
#ifdef UNIT_TEST
#include "MockSensor.h"

void test_with_mock_sensor() {
    MockBME280 sensor;
    sensor.begin();

    float temp = sensor.readTemperature();
    TEST_ASSERT_EQUAL_FLOAT(22.5f, temp);
}
#endif
```

### Mock Sensor Values

Default mock values:
- BME280 Temperature: 22.5°C
- BME280 Humidity: 45.2%
- BME280 Pressure: 1013.25 hPa
- DS18B20 Temperature: 21.8°C
- Soil Moisture Raw: 2048 (ADC)
- Soil Moisture Percentage: 62.3%

### Mock Sensor with Noise

For testing with variation:

```cpp
void test_with_noisy_sensor() {
    float temp = MockSensorWithNoise::getBME280Temp(0.5f);
    // Returns 22.5 ± 0.5°C

    TEST_ASSERT_FLOAT_WITHIN(0.5f, 22.5f, temp);
}
```

### Mock Sensor Failures

Test error handling:

```cpp
void test_sensor_failure() {
    MockSensorFailure::setFailureMode(
        MockSensorFailure::FailureMode::BME280_INIT_FAIL
    );

    TEST_ASSERT_TRUE(MockSensorFailure::shouldBME280InitFail());
}
```

## Best Practices

### 1. Test One Thing Per Test

```cpp
// Good
void test_calibration_at_dry_point() {
    float result = calibrate(3200, 3200, 1200);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, result);
}

// Bad - tests multiple things
void test_calibration() {
    TEST_ASSERT_EQUAL_FLOAT(0.0f, calibrate(3200, 3200, 1200));
    TEST_ASSERT_EQUAL_FLOAT(100.0f, calibrate(1200, 3200, 1200));
    TEST_ASSERT_EQUAL_FLOAT(50.0f, calibrate(2200, 3200, 1200));
}
```

### 2. Use Descriptive Test Names

```cpp
// Good
void test_soil_moisture_calibration_clamps_below_zero()

// Bad
void test_calibration_1()
```

### 3. Arrange-Act-Assert Pattern

```cpp
void test_example() {
    // Arrange - set up test data
    int input = 42;

    // Act - perform the operation
    int result = myFunction(input);

    // Assert - verify the result
    TEST_ASSERT_EQUAL(84, result);
}
```

### 4. Clean Up in tearDown

```cpp
void tearDown(void) {
    // Reset mock state
    MockSensorFailure::setFailureMode(
        MockSensorFailure::FailureMode::NONE
    );
}
```

### 5. Test Edge Cases

```cpp
void test_handles_zero_input()
void test_handles_negative_input()
void test_handles_maximum_value()
void test_handles_null_pointer()
```

## Continuous Integration

Tests can be run in CI/CD pipelines:

```yaml
# Example GitHub Actions workflow
- name: Run Unit Tests
  run: pio test -e native
```

## Troubleshooting

### Test Not Found

**Problem:** `Error: Test not found`

**Solution:** Ensure test file name starts with `test_` and is in the `test/` directory.

### Compilation Errors

**Problem:** Arduino types not available in native environment

**Solution:** Use conditional compilation:

```cpp
#ifdef UNIT_TEST
#include <cstdint>
#else
#include <Arduino.h>
#endif
```

### Mock Functions Not Working

**Problem:** Mock functions not being called

**Solution:** Ensure `UNIT_TEST` flag is defined and `MockSensor.h` is included.

## Further Reading

- [PlatformIO Unit Testing](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)
- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)
- [UNIT_TEST_GUIDE.md](../docs/UNIT_TEST_GUIDE.md) - Detailed guide on using the UNIT_TEST flag
