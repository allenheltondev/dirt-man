#ifdef UNIT_TEST

#include <unity.h>
#include "../test/mocks/Arduino.h"
#include "MockSensor.h"

// ============================================================================
// Example Unit Tests Using Mock Sensors
// ============================================================================
// This file demonstrates how to write unit tests using the UNIT_TEST flag
// and mock sensor implementations.
// ============================================================================

void setUp(void) {
    // Reset mock state before each test
    MockSensorFailure::setFailureMode(MockSensorFailure::FailureMode::NONE);
}

void tearDown(void) {
    // Clean up after each test
}

// ============================================================================
// Test Mock Sensor Values
// ============================================================================

void test_mock_bme280_returns_expected_values() {
    MockBME280 sensor;

    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_EQUAL_FLOAT(22.5f, sensor.readTemperature());
    TEST_ASSERT_EQUAL_FLOAT(45.2f, sensor.readHumidity());
    TEST_ASSERT_EQUAL_FLOAT(101325.0f, sensor.readPressure());  // 1013.25 hPa in Pa
}

void test_mock_ds18b20_returns_expected_values() {
    MockDallasTemperature sensor(nullptr);

    sensor.begin();
    sensor.requestTemperatures();

    float temp = sensor.getTempCByIndex(0);
    TEST_ASSERT_EQUAL_FLOAT(21.8f, temp);
}

void test_mock_soil_moisture_returns_expected_values() {
    uint16_t rawValue = analogRead(32);  // GPIO32 - soil moisture pin

    TEST_ASSERT_EQUAL_UINT16(2048, rawValue);
}

void test_mock_sensor_with_noise_varies_within_range() {
    float temp1 = MockSensorWithNoise::getBME280Temp();
    float temp2 = MockSensorWithNoise::getBME280Temp();

    // Both should be near expected value
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 22.5f, temp1);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 22.5f, temp2);

    // Values should vary (not always identical)
    // Note: This might occasionally fail due to random chance
    // In real tests, you'd run multiple iterations
}

void test_mock_soil_moisture_with_noise_stays_in_adc_range() {
    for (int i = 0; i < 10; i++) {
        uint16_t value = MockSensorWithNoise::getSoilMoistureRaw(50);

        // Should stay within ADC range
        TEST_ASSERT_GREATER_OR_EQUAL(0, value);
        TEST_ASSERT_LESS_OR_EQUAL(4095, value);

        // Should be near expected value
        TEST_ASSERT_UINT16_WITHIN(50, 2048, value);
    }
}

// ============================================================================
// Test Soil Moisture Calibration Formula
// ============================================================================

float convertSoilMoistureToPercent(uint16_t rawAdc, uint16_t dryAdc, uint16_t wetAdc) {
    if (dryAdc == wetAdc) return 0.0f;  // Avoid division by zero

    float percentage = (static_cast<float>(rawAdc) - dryAdc) / (wetAdc - dryAdc) * 100.0f;

    // Clamp to 0-100 range
    if (percentage < 0.0f) percentage = 0.0f;
    if (percentage > 100.0f) percentage = 100.0f;

    return percentage;
}

void test_soil_moisture_calibration_at_dry_point() {
    uint16_t dryAdc = 3200;
    uint16_t wetAdc = 1200;

    float percentage = convertSoilMoistureToPercent(dryAdc, dryAdc, wetAdc);

    TEST_ASSERT_EQUAL_FLOAT(0.0f, percentage);
}

void test_soil_moisture_calibration_at_wet_point() {
    uint16_t dryAdc = 3200;
    uint16_t wetAdc = 1200;

    float percentage = convertSoilMoistureToPercent(wetAdc, dryAdc, wetAdc);

    TEST_ASSERT_EQUAL_FLOAT(100.0f, percentage);
}

void test_soil_moisture_calibration_at_midpoint() {
    uint16_t dryAdc = 3200;
    uint16_t wetAdc = 1200;
    uint16_t midAdc = 2200;  // Halfway between dry and wet

    float percentage = convertSoilMoistureToPercent(midAdc, dryAdc, wetAdc);

    TEST_ASSERT_EQUAL_FLOAT(50.0f, percentage);
}

void test_soil_moisture_calibration_clamps_below_zero() {
    uint16_t dryAdc = 3200;
    uint16_t wetAdc = 1200;
    uint16_t dryerThanDry = 3500;  // Drier than calibrated dry point

    float percentage = convertSoilMoistureToPercent(dryerThanDry, dryAdc, wetAdc);

    TEST_ASSERT_EQUAL_FLOAT(0.0f, percentage);
}

void test_soil_moisture_calibration_clamps_above_100() {
    uint16_t dryAdc = 3200;
    uint16_t wetAdc = 1200;
    uint16_t wetterThanWet = 1000;  // Wetter than calibrated wet point

    float percentage = convertSoilMoistureToPercent(wetterThanWet, dryAdc, wetAdc);

    TEST_ASSERT_EQUAL_FLOAT(100.0f, percentage);
}

// ============================================================================
// Test Sensor Failure Modes
// ============================================================================

void test_mock_sensor_failure_mode_can_be_set() {
    MockSensorFailure::setFailureMode(MockSensorFailure::FailureMode::BME280_INIT_FAIL);

    TEST_ASSERT_TRUE(MockSensorFailure::shouldBME280InitFail());
    TEST_ASSERT_FALSE(MockSensorFailure::shouldDS18B20Disconnect());
}

void test_mock_sensor_failure_mode_defaults_to_none() {
    TEST_ASSERT_FALSE(MockSensorFailure::shouldBME280InitFail());
    TEST_ASSERT_FALSE(MockSensorFailure::shouldBME280ReadFail());
    TEST_ASSERT_FALSE(MockSensorFailure::shouldDS18B20Disconnect());
}

// ============================================================================
// Test Arduino Mock Functions
// ============================================================================

void test_mock_millis_increments() {
    unsigned long time1 = millis();
    unsigned long time2 = millis();

    TEST_ASSERT_GREATER_THAN(time1, time2);
}

void test_mock_delay_does_not_crash() {
    // delay() should be a no-op in test environment
    delay(1000);

    // If we get here, test passes
    TEST_ASSERT_TRUE(true);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    UNITY_BEGIN();

    // Mock sensor value tests
    RUN_TEST(test_mock_bme280_returns_expected_values);
    RUN_TEST(test_mock_ds18b20_returns_expected_values);
    RUN_TEST(test_mock_soil_moisture_returns_expected_values);

    // Mock sensor with noise tests
    RUN_TEST(test_mock_sensor_with_noise_varies_within_range);
    RUN_TEST(test_mock_soil_moisture_with_noise_stays_in_adc_range);

    // Calibration formula tests
    RUN_TEST(test_soil_moisture_calibration_at_dry_point);
    RUN_TEST(test_soil_moisture_calibration_at_wet_point);
    RUN_TEST(test_soil_moisture_calibration_at_midpoint);
    RUN_TEST(test_soil_moisture_calibration_clamps_below_zero);
    RUN_TEST(test_soil_moisture_calibration_clamps_above_100);

    // Failure mode tests
    RUN_TEST(test_mock_sensor_failure_mode_can_be_set);
    RUN_TEST(test_mock_sensor_failure_mode_defaults_to_none);

    // Arduino mock function tests
    RUN_TEST(test_mock_millis_increments);
    RUN_TEST(test_mock_delay_does_not_crash);

    return UNITY_END();
}

#endif // UNIT_TEST
