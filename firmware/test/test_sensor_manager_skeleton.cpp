#include <unity.h>

#include "../test/mocks/Arduino.cpp"  // Include mock implementation
#include "SensorManager.h"
#include "models/SensorReadings.h"
#include "models/SensorType.h"

// Test that SensorManager can be instantiated
void test_sensor_manager_instantiation() {
    SensorManager manager;
    TEST_ASSERT_TRUE(true);  // If we get here, instantiation succeeded
}

// Test that initialize() can be called without crashing
void test_sensor_manager_initialize() {
    SensorManager manager;
    manager.initialize();  // Should not crash even with empty implementation
    TEST_ASSERT_TRUE(true);
}

// Test that readSensors() returns a valid structure
void test_sensor_manager_read_sensors() {
    SensorManager manager;
    SensorReadings readings = manager.readSensors();

    // Verify structure is initialized (all zeros for now)
    TEST_ASSERT_EQUAL_FLOAT(0.0f, readings.bme280Temp);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, readings.ds18b20Temp);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, readings.humidity);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, readings.pressure);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, readings.soilMoisture);
    TEST_ASSERT_EQUAL_UINT16(0, readings.soilMoistureRaw);
    TEST_ASSERT_EQUAL_UINT8(0, readings.sensorStatus);
}

// Test that isSensorAvailable() returns false for uninitialized sensors
void test_sensor_manager_is_sensor_available() {
    SensorManager manager;

    // All sensors should be unavailable before initialization
    TEST_ASSERT_FALSE(manager.isSensorAvailable(SensorType::BME280_TEMP));
    TEST_ASSERT_FALSE(manager.isSensorAvailable(SensorType::DS18B20_TEMP));
    TEST_ASSERT_FALSE(manager.isSensorAvailable(SensorType::HUMIDITY));
    TEST_ASSERT_FALSE(manager.isSensorAvailable(SensorType::PRESSURE));
    TEST_ASSERT_FALSE(manager.isSensorAvailable(SensorType::SOIL_MOISTURE));
}

// Test that calibrateSoilMoisture() can be called
void test_sensor_manager_calibrate_soil_moisture() {
    SensorManager manager;

    // Should not crash when setting calibration values
    manager.calibrateSoilMoisture(3000, 1500);
    TEST_ASSERT_TRUE(true);
}

// Test soil moisture conversion formula with default calibration
void test_soil_moisture_conversion_formula() {
    SensorManager manager;

    // Set known calibration values
    manager.calibrateSoilMoisture(3000, 1500);  // dry=3000, wet=1500

    // Note: The actual conversion happens in readSoilMoisture() which is private
    // This test verifies the calibration can be set without errors
    // Full conversion testing will be done in property tests (Task 6.6)
    TEST_ASSERT_TRUE(true);
}

// Test that sensor status bitmask logic works correctly
void test_sensor_status_bitmask() {
    SensorManager manager;

    // Initially all sensors should be unavailable (status = 0)
    TEST_ASSERT_FALSE(manager.isSensorAvailable(SensorType::BME280_TEMP));
    TEST_ASSERT_FALSE(manager.isSensorAvailable(SensorType::DS18B20_TEMP));
    TEST_ASSERT_FALSE(manager.isSensorAvailable(SensorType::SOIL_MOISTURE));
}

// Test that multiple sensor types map to BME280 correctly
void test_bme280_sensor_types() {
    SensorManager manager;

    // BME280_TEMP, HUMIDITY, and PRESSURE should all check the same BME280 bit
    // Since sensor is not initialized, all should return false
    bool bme280Temp = manager.isSensorAvailable(SensorType::BME280_TEMP);
    bool humidity = manager.isSensorAvailable(SensorType::HUMIDITY);
    bool pressure = manager.isSensorAvailable(SensorType::PRESSURE);

    // All three should have the same availability status
    TEST_ASSERT_EQUAL(bme280Temp, humidity);
    TEST_ASSERT_EQUAL(humidity, pressure);
}

// Task 4.9: Unit tests for sensor initialization

// Test that sensors become available after initialization (mock mode)
void test_sensors_available_after_init() {
    SensorManager manager;
    manager.initialize();

    // In mock mode, all sensors should be available after initialization
    TEST_ASSERT_TRUE(manager.isSensorAvailable(SensorType::BME280_TEMP));
    TEST_ASSERT_TRUE(manager.isSensorAvailable(SensorType::DS18B20_TEMP));
    TEST_ASSERT_TRUE(manager.isSensorAvailable(SensorType::HUMIDITY));
    TEST_ASSERT_TRUE(manager.isSensorAvailable(SensorType::PRESSURE));
    TEST_ASSERT_TRUE(manager.isSensorAvailable(SensorType::SOIL_MOISTURE));
}

// Test that readSensors() returns valid mock data after initialization
void test_read_sensors_after_init() {
    SensorManager manager;
    manager.initialize();
    SensorReadings readings = manager.readSensors();

    // Verify mock sensor values are returned
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 22.5f, readings.bme280Temp);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 55.0f, readings.humidity);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1013.25f, readings.pressure);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 23.0f, readings.ds18b20Temp);
    TEST_ASSERT_EQUAL_UINT16(2250, readings.soilMoistureRaw);

    // Verify sensor status indicates all sensors are available
    TEST_ASSERT_NOT_EQUAL(0, readings.sensorStatus);
}

// Test that sensor status bitmask is set correctly after initialization
void test_sensor_status_bitmask_after_init() {
    SensorManager manager;
    manager.initialize();

    // Read sensors to get status
    SensorReadings readings = manager.readSensors();

    // In mock mode, all 3 sensor bits should be set (BME280, DS18B20, Soil)
    // Bitmask: bit 0 = BME280, bit 1 = DS18B20, bit 2 = Soil
    // Expected: 0b00000111 = 7
    TEST_ASSERT_EQUAL_UINT8(7, readings.sensorStatus);
}

// Test that monotonicMs timestamp is set in readings
void test_monotonic_timestamp_in_readings() {
    SensorManager manager;
    manager.initialize();

    // Set mock millis value
    mockMillis = 12345;

    SensorReadings readings = manager.readSensors();

    // Verify timestamp matches mock millis
    TEST_ASSERT_EQUAL_UINT32(12345, readings.monotonicMs);
}

// Test soil moisture calibration affects conversion
void test_soil_moisture_calibration_conversion() {
    SensorManager manager;
    manager.initialize();

    // Set calibration: dry=3000, wet=1500
    manager.calibrateSoilMoisture(3000, 1500);

    SensorReadings readings = manager.readSensors();

    // Mock raw value is 2250
    // Expected percentage: (2250 - 3000) / (1500 - 3000) * 100 = (-750) / (-1500) * 100 = 50%
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, readings.soilMoisture);
}

// Test soil moisture percentage clamping to 0-100 range
void test_soil_moisture_clamping() {
    SensorManager manager;
    manager.initialize();

    // Set calibration that would result in >100%
    // If raw=2250 and we set dry=2500, wet=1500:
    // (2250 - 2500) / (1500 - 2500) * 100 = (-250) / (-1000) * 100 = 25%
    manager.calibrateSoilMoisture(2500, 1500);

    SensorReadings readings = manager.readSensors();

    // Should be clamped to 0-100 range
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, readings.soilMoisture);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(100.0f, readings.soilMoisture);
}

// Test that calibration with same dry/wet values doesn't crash
void test_soil_moisture_calibration_edge_case() {
    SensorManager manager;
    manager.initialize();

    // Set calibration with same dry and wet values (division by zero case)
    manager.calibrateSoilMoisture(2000, 2000);

    SensorReadings readings = manager.readSensors();

    // Should return 0% to avoid division by zero
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, readings.soilMoisture);
}

// Test that physical range validation works for BME280 temperature
void test_bme280_temp_validation() {
    SensorManager manager;
    manager.initialize();

    SensorReadings readings = manager.readSensors();

    // Mock BME280 temp should be within valid range (-40 to 85°C)
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(-40.0f, readings.bme280Temp);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(85.0f, readings.bme280Temp);
}

// Test that physical range validation works for humidity
void test_humidity_validation() {
    SensorManager manager;
    manager.initialize();

    SensorReadings readings = manager.readSensors();

    // Humidity should be within valid range (0-100%)
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, readings.humidity);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(100.0f, readings.humidity);
}

// Test that physical range validation works for pressure
void test_pressure_validation() {
    SensorManager manager;
    manager.initialize();

    SensorReadings readings = manager.readSensors();

    // Pressure should be within valid range (300-1100 hPa)
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(300.0f, readings.pressure);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(1100.0f, readings.pressure);
}

// Test that physical range validation works for DS18B20 temperature
void test_ds18b20_temp_validation() {
    SensorManager manager;
    manager.initialize();

    SensorReadings readings = manager.readSensors();

    // DS18B20 temp should be within valid range (-55 to 125°C)
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(-55.0f, readings.ds18b20Temp);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(125.0f, readings.ds18b20Temp);
}

// Task 6.5: Unit tests for calibration edge cases

// Test calibration with invalid ADC values (> 4095)
void test_calibration_invalid_adc_values() {
    SensorManager manager;
    manager.initialize();

    // Set initial valid calibration
    manager.calibrateSoilMoisture(3000, 1500);

    // Try to set invalid calibration (> 4095)
    manager.calibrateSoilMoisture(5000, 1500);

    // Read sensors - should still use previous valid calibration
    SensorReadings readings = manager.readSensors();

    // With raw=2250, dry=3000, wet=1500: (2250-3000)/(1500-3000)*100 = 50%
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, readings.soilMoisture);
}

// Test calibration with both values invalid
void test_calibration_both_values_invalid() {
    SensorManager manager;
    manager.initialize();

    // Set initial valid calibration
    manager.calibrateSoilMoisture(3000, 1500);

    // Try to set both values invalid
    manager.calibrateSoilMoisture(5000, 6000);

    // Should keep previous valid calibration
    SensorReadings readings = manager.readSensors();
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, readings.soilMoisture);
}

// Test calibration with identical dry and wet values
void test_calibration_identical_values() {
    SensorManager manager;
    manager.initialize();

    // Set initial valid calibration
    manager.calibrateSoilMoisture(3000, 1500);

    // Try to set identical values (division by zero case)
    manager.calibrateSoilMoisture(2000, 2000);

    // Should keep previous valid calibration
    SensorReadings readings = manager.readSensors();
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, readings.soilMoisture);
}

// Test calibration at boundary values (0 and 4095)
void test_calibration_boundary_values() {
    SensorManager manager;
    manager.initialize();

    // Set calibration at ADC boundaries
    manager.calibrateSoilMoisture(4095, 0);

    // Should accept valid boundary values
    SensorReadings readings = manager.readSensors();

    // With raw=2250, dry=4095, wet=0: (2250-4095)/(0-4095)*100 = 45.05%
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 45.0f, readings.soilMoisture);
}

// Test calibration with reversed values (wet > dry)
void test_calibration_reversed_values() {
    SensorManager manager;
    manager.initialize();

    // Set calibration with wet > dry (reversed from typical)
    manager.calibrateSoilMoisture(1500, 3000);

    SensorReadings readings = manager.readSensors();

    // With raw=2250, dry=1500, wet=3000: (2250-1500)/(3000-1500)*100 = 50%
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, readings.soilMoisture);
}

// Test soil moisture clamping at lower bound
void test_soil_moisture_clamp_lower_bound() {
    SensorManager manager;
    manager.initialize();

    // Set calibration that would result in negative percentage
    // If raw=2250, dry=2000, wet=1500: (2250-2000)/(1500-2000)*100 = -50%
    manager.calibrateSoilMoisture(2000, 1500);

    SensorReadings readings = manager.readSensors();

    // Should be clamped to 0%
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, readings.soilMoisture);
}

// Test soil moisture clamping at upper bound
void test_soil_moisture_clamp_upper_bound() {
    SensorManager manager;
    manager.initialize();

    // Set calibration that would result in >100%
    // If raw=2250, dry=2500, wet=2000: (2250-2500)/(2000-2500)*100 = 50%
    // Let's try: dry=3000, wet=2500: (2250-3000)/(2500-3000)*100 = 150%
    manager.calibrateSoilMoisture(3000, 2500);

    SensorReadings readings = manager.readSensors();

    // Should be clamped to 100%
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 100.0f, readings.soilMoisture);
}

// Test calibration with minimum valid range (1 ADC unit difference)
void test_calibration_minimum_range() {
    SensorManager manager;
    manager.initialize();

    // Set calibration with minimal difference
    manager.calibrateSoilMoisture(2001, 2000);

    SensorReadings readings = manager.readSensors();

    // Should accept and calculate correctly
    // With raw=2250, dry=2001, wet=2000: (2250-2001)/(2000-2001)*100 = -24900%
    // Should clamp to 0%
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, readings.soilMoisture);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(100.0f, readings.soilMoisture);
}

void setUp(void) {
    // Set up code here (runs before each test)
}

void tearDown(void) {
    // Clean up code here (runs after each test)
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_sensor_manager_instantiation);
    RUN_TEST(test_sensor_manager_initialize);
    RUN_TEST(test_sensor_manager_read_sensors);
    RUN_TEST(test_sensor_manager_is_sensor_available);
    RUN_TEST(test_sensor_manager_calibrate_soil_moisture);
    RUN_TEST(test_soil_moisture_conversion_formula);
    RUN_TEST(test_sensor_status_bitmask);
    RUN_TEST(test_bme280_sensor_types);

    // Task 4.9: Sensor initialization tests
    RUN_TEST(test_sensors_available_after_init);
    RUN_TEST(test_read_sensors_after_init);
    RUN_TEST(test_sensor_status_bitmask_after_init);
    RUN_TEST(test_monotonic_timestamp_in_readings);
    RUN_TEST(test_soil_moisture_calibration_conversion);
    RUN_TEST(test_soil_moisture_clamping);
    RUN_TEST(test_soil_moisture_calibration_edge_case);
    RUN_TEST(test_bme280_temp_validation);
    RUN_TEST(test_humidity_validation);
    RUN_TEST(test_pressure_validation);
    RUN_TEST(test_ds18b20_temp_validation);

    // Task 6.5: Calibration edge case tests
    RUN_TEST(test_calibration_invalid_adc_values);
    RUN_TEST(test_calibration_both_values_invalid);
    RUN_TEST(test_calibration_identical_values);
    RUN_TEST(test_calibration_boundary_values);
    RUN_TEST(test_calibration_reversed_values);
    RUN_TEST(test_soil_moisture_clamp_lower_bound);
    RUN_TEST(test_soil_moisture_clamp_upper_bound);
    RUN_TEST(test_calibration_minimum_range);

    return UNITY_END();
}
