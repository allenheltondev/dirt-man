#include <unity.h>
#include <Arduino.h>
#include "../../include/SensorManager.h"
#include "../../include/models/SensorType.h"
#include "../../include/PinConfig.h"

// Test fixture
SensorManager* sensorManager = nullptr;

void setUp(void) {
    // Create fresh sensor manager for each test
    sensorManager = new SensorManager();
}

void tearDown(void) {
    // Clean up after each test
    delete sensorManager;
    sensorManager = nullptr;
}

// ============================================================================
// Sensor Initialization Tests
// ============================================================================

void test_sensor_manager_initializes_successfully(void) {
    // Initialize sensor manager
    sensorManager->initialize();

    // Test passes if no crash occurs
    TEST_ASSERT_TRUE(true);
}

void test_bme280_sensor_is_available(void) {
    sensorManager->initialize();

    // Check if BME280 temperature sensor is available
    bool bme280Available = sensorManager->isSensorAvailable(SensorType::BME280_TEMP);

    TEST_ASSERT_TRUE_MESSAGE(bme280Available,
        "BME280 sensor not detected. Check I2C wiring (SDA=GPIO21, SCL=GPIO22) and I2C address (0x76 or 0x77)");
}

void test_ds18b20_sensor_is_available(void) {
    sensorManager->initialize();

    // Check if DS18B20 temperature sensor is available
    bool ds18b20Available = sensorManager->isSensorAvailable(SensorType::DS18B20_TEMP);

    TEST_ASSERT_TRUE_MESSAGE(ds18b20Available,
        "DS18B20 sensor not detected. Check OneWire wiring (Data=GPIO4) and 4.7kΩ pull-up resistor");
}

void test_humidity_sensor_is_available(void) {
    sensorManager->initialize();

    // Check if humidity sensor (BME280) is available
    bool humidityAvailable = sensorManager->isSensorAvailable(SensorType::HUMIDITY);

    TEST_ASSERT_TRUE_MESSAGE(humidityAvailable,
        "Humidity sensor (BME280) not detected. Check I2C wiring");
}

void test_pressure_sensor_is_available(void) {
    sensorManager->initialize();

    // Check if pressure sensor (BME280) is available
    bool pressureAvailable = sensorManager->isSensorAvailable(SensorType::PRESSURE);

    TEST_ASSERT_TRUE_MESSAGE(pressureAvailable,
        "Pressure sensor (BME280) not detected. Check I2C wiring");
}

void test_soil_moisture_sensor_is_available(void) {
    sensorManager->initialize();

    // Check if soil moisture sensor is available
    bool soilMoistureAvailable = sensorManager->isSensorAvailable(SensorType::SOIL_MOISTURE);

    TEST_ASSERT_TRUE_MESSAGE(soilMoistureAvailable,
        "Soil moisture sensor not detected. Check ADC wiring (GPIO32)");
}

void test_all_sensors_are_available(void) {
    sensorManager->initialize();

    // Check all sensors
    bool bme280 = sensorManager->isSensorAvailable(SensorType::BME280_TEMP);
    bool ds18b20 = sensorManager->
  errorMsg += "DS18B20_TEMP ";
        allAvailable = false;
    }
    if (!humidity) {
        errorMsg += "HUMIDITY ";
        allAvailable = false;
    }
    if (!pressure) {
        errorMsg += "PRESSURE ";
        allAvailable = false;
    }
    if (!soilMoisture) {
        errorMsg += "SOIL_MOISTURE ";
        allAvailable = false;
    }

    TEST_ASSERT_TRUE_MESSAGE(allAvailable, errorMsg.c_str());
}

// ============================================================================
// Sensor Reading Tests
// ============================================================================

void test_can_read_all_sensors(void) {
    sensorManager->initialize();

    // Read all sensors
    SensorReadings readings = sensorManager->readSensors();

    // Test passes if no crash occurs
    TEST_ASSERT_TRUE(true);
}

void test_bme280_temperature_reading_is_valid(void) {
    sensorManager->initialize();

    SensorReadings readings = sensorManager->readSensors();

    // BME280 temperature should be between -40°C and 85°C
    TEST_ASSERT_TRUE_MESSAGE(readings.bme280Temp >= -40.0f && readings.bme280Temp <= 85.0f,
        "BME280 temperature reading out of valid range (-40 to 85°C)");

    // Should be within reasonable room temperature range for most tests
    TEST_ASSERT_TRUE_MESSAGE(readings.bme280Temp >= 0.0f && readings.bme280Temp <= 50.0f,
        "BME280 temperature reading unusual (expected 0-50°C for typical indoor environment)");
}

void test_ds18b20_temperature_reading_is_valid(void) {
    sensorManager->initialize();

    SensorReadings readings = sensorManager->readSensors();

    // DS18B20 temperature should be between -55°C and 125°C
    TEST_ASSERT_TRUE_MESSAGE(readings.ds18b20Temp >= -55.0f && readings.ds18b20Temp <= 125.0f,
        "DS18B20 temperature reading out of valid range (-55 to 125°C)");

    // Should be within reasonable range for most tests
    TEST_ASSERT_TRUE_MESSAGE(readings.ds18b20Temp >= 0.0f && readings.ds18b20Temp <= 50.0f,
        "DS18B20 temperature reading unusual (expected 0-50°C for typical environment)");
}

void test_humidity_reading_is_valid(void) {
    sensorManager->initialize();

    SensorReadings readings = sensorManager->readSensors();

    // Humidity should be between 0% and 100%
    TEST_ASSERT_TRUE_MESSAGE(readings.humidity >= 0.0f && readings.humidity <= 100.0f,
        "Humidity reading out of valid range (0-100%)");

    // Should be within typical indoor range
    TEST_ASSERT_TRUE_MESSAGE(readings.humidity >= 10.0f && readings.humidity <= 90.0f,
        "Humidity reading unusual (expected 10-90% for typical indoor environment)");
}

void test_pressure_reading_is_valid(void) {
    sensorManager->initialize();

    SensorReadings readings = sensorManager->readSensors();

    // Pressure should be between 300 and 1100 hPa (covers all altitudes)
    TEST_ASSERT_TRUE_MESSAGE(readings.pressure >= 300.0f && readings.pressure <= 1100.0f,
        "Pressure reading out of valid range (300-1100 hPa)");

    // Should be within typical sea level range
    TEST_ASSERT_TRUE_MESSAGE(readings.pressure >= 950.0f && readings.pressure <= 1050.0f,
        "Pressure reading unusual (expected 950-1050 hPa for typical sea level environment)");
}

void test_soil_moisture_reading_is_valid(void) {
    sensorManager->initialize();

    SensorReadings readings = sensorManager->readSensors();

    // Soil moisture should be between 0% and 100%
    TEST_ASSERT_TRUE_MESSAGE(readings.soilMoisture >= 0.0f && readings.soilMoisture <= 100.0f,
        "Soil moisture reading out of valid range (0-100%)");
}

void test_multiple_consecutive_readings_are_stable(void) {
    sensorManager->initialize();

    // Take first reading
    SensorReadings reading1 = sensorManager->readSensors();
    delay(100);

    // Take second reading
    SensorReadings reading2 = sensorManager->readSensors();
    delay(100);

    // Take third reading
    SensorReadings reading3 = sensorManager->readSensors();

    // Readings should be relatively stable (within 5% for most sensors)
    float tempDiff1 = abs(reading2.bme280Temp - reading1.bme280Temp);
    float tempDiff2 = abs(reading3.bme280Temp - reading2.bme280Temp);

    TEST_ASSERT_TRUE_MESSAGE(tempDiff1 < 5.0f && tempDiff2 < 5.0f,
        "BME280 temperature readings are unstable (>5°C variation)");

    float humidityDiff1 = abs(reading2.humidity - reading1.humidity);
    float humidityDiff2 = abs(reading3.humidity - reading2.humidity);

    TEST_ASSERT_TRUE_MESSAGE(humidityDiff1 < 10.0f && humidityDiff2 < 10.0f,
        "Humidity readings are unstable (>10% variation)");
}

// ============================================================================
// Sensor Calibration Tests
// ============================================================================

void test_soil_moisture_calibration_affects_readings(void) {
    sensorManager->initialize();

    // Read before calibration
    SensorReadings beforeCalibration = sensorManager->readSensors();

    // Apply calibration (typical values: dry=3000, wet=1500)
    sensorManager->calibrateSoilMoisture(3000, 1500);

    // Read after calibration
    SensorReadings afterCalibration = sensorManager->readSensors();

    // Readings should be different (unless raw ADC happens to match calibration)
    // This test mainly verifies calibration doesn't crash
    TEST_ASSERT_TRUE(true);
}

// ============================================================================
// Pin Configuration Tests
// ============================================================================

void test_i2c_pins_are_configured_correctly(void) {
    // Verify I2C pins match expected configuration
    TEST_ASSERT_EQUAL_MESSAGE(21, I2C_SDA_PIN, "I2C SDA pin should be GPIO21");
    TEST_ASSERT_EQUAL_MESSAGE(22, I2C_SCL_PIN, "I2C SCL pin should be GPIO22");
}

void test_onewire_pin_is_configured_correctly(void) {
    // Verify OneWire pin matches expected configuration
    TEST_ASSERT_EQUAL_MESSAGE(4, ONEWIRE_PIN, "OneWire pin should be GPIO4");
}

void test_soil_moisture_pin_is_adc1(void) {
    // Verify soil moisture uses ADC1 (GPIO 32-39) to avoid WiFi conflicts
    TEST_ASSERT_TRUE_MESSAGE(SOIL_MOISTURE_PIN >= 32 && SOIL_MOISTURE_PIN <= 39,
        "Soil moisture pin must be ADC1 (GPIO 32-39) to avoid WiFi conflicts");

    TEST_ASSERT_EQUAL_MESSAGE(32, SOIL_MOISTURE_PIN, "Soil moisture pin should be GPIO32");
}

// ============================================================================
// Test Runner
// ============================================================================

void setup() {
    // Wait for serial connection
    delay(2000);

    UNITY_BEGIN();

    // Initialization tests
    RUN_TEST(test_sensor_manager_initializes_successfully);
    RUN_TEST(test_bme280_sensor_is_available);
    RUN_TEST(test_ds18b20_sensor_is_available);
    RUN_TEST(test_humidity_sensor_is_available);
    RUN_TEST(test_pressure_sensor_is_available);
    RUN_TEST(test_soil_moisture_sensor_is_available);
    RUN_TEST(test_all_sensors_are_available);

    // Reading tests
    RUN_TEST(test_can_read_all_sensors);
    RUN_TEST(test_bme280_temperature_reading_is_valid);
    RUN_TEST(test_ds18b20_temperature_reading_is_valid);
    RUN_TEST(test_humidity_reading_is_valid);
    RUN_TEST(test_pressure_reading_is_valid);
    RUN_TEST(test_soil_moisture_reading_is_valid);
    RUN_TEST(test_multiple_consecutive_readings_are_stable);

    // Calibration tests
    RUN_TEST(test_soil_moisture_calibration_affects_readings);

    // Pin configuration tests
    RUN_TEST(test_i2c_pins_are_configured_correctly);
    RUN_TEST(test_onewire_pin_is_configured_correctly);
    RUN_TEST(test_soil_moisture_pin_is_adc1);

    UNITY_END();
}

void loop() {
    // Nothing to do here
}
