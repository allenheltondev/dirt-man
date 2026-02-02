#ifndef MOCK_SENSOR_H
#define MOCK_SENSOR_H

#ifdef UNIT_TEST
#include <cstdint>
#else
#include <Arduino.h>
#endif

// ============================================================================
// Mock Sensor Support for Unit Testing
// ============================================================================
// This file provides mock sensor implementations when UNIT_TEST flag is set.
// Mock sensors return realistic test data without requiring physical hardware.
// ============================================================================

#ifdef UNIT_TEST

// ============================================================================
// Mock Arduino Functions
// ============================================================================
// Provide minimal Arduino API for native testing environment
// Note: These are declared in Arduino.h mock, not defined here to avoid conflicts

// ============================================================================
// Mock Sensor Values
// ============================================================================
// Realistic sensor values for testing

namespace MockSensor {
// BME280 Environmental Sensor
constexpr float BME280_TEMP_C = 22.5f;
constexpr float BME280_HUMIDITY_PCT = 45.2f;
constexpr float BME280_PRESSURE_HPA = 1013.25f;

// DS18B20 Temperature Sensor
constexpr float DS18B20_TEMP_C = 21.8f;

// Soil Moisture Sensor
constexpr uint16_t SOIL_MOISTURE_RAW = 2048;  // Mid-range ADC value
constexpr float SOIL_MOISTURE_PCT = 62.3f;

// Sensor Status (all sensors available)
constexpr uint8_t SENSOR_STATUS_ALL_OK = 0xFF;

// Calibration Values
constexpr uint16_t SOIL_DRY_ADC = 3200;  // Dry soil ADC value
constexpr uint16_t SOIL_WET_ADC = 1200;  // Wet soil ADC value
}  // namespace MockSensor

// ============================================================================
// Mock BME280 Class
// ============================================================================
// Minimal mock implementation of Adafruit_BME280 for testing

class MockBME280 {
   public:
    bool begin(uint8_t addr = 0x76) {
        (void)addr;
        return true;  // Always succeeds in mock mode
    }

    float readTemperature() { return MockSensor::BME280_TEMP_C; }

    float readHumidity() { return MockSensor::BME280_HUMIDITY_PCT; }

    float readPressure() {
        return MockSensor::BME280_PRESSURE_HPA * 100.0f;  // Convert hPa to Pa
    }
};

// ============================================================================
// Mock DallasTemperature Class
// ============================================================================
// Minimal mock implementation of DallasTemperature for testing

class MockDallasTemperature {
   public:
    MockDallasTemperature(void* oneWire) { (void)oneWire; }

    void begin() {
        // No-op in mock mode
    }

    void requestTemperatures() {
        // No-op in mock mode
    }

    float getTempCByIndex(uint8_t index) {
        (void)index;
        return MockSensor::DS18B20_TEMP_C;
    }

    static constexpr float DEVICE_DISCONNECTED_C = -127.0f;
};

// ============================================================================
// Mock ADC Functions
// ============================================================================
// Mock analog read for soil moisture sensor
// Note: analogRead is declared in Arduino.h mock, not defined here to avoid conflicts

// ============================================================================
// Mock Sensor Reading with Noise
// ============================================================================
// For property-based testing, provide readings with controlled variation

class MockSensorWithNoise {
   public:
    static float getBME280Temp(float variation = 0.5f) {
        return MockSensor::BME280_TEMP_C + getRandomVariation(variation);
    }

    static float getDS18B20Temp(float variation = 0.3f) {
        return MockSensor::DS18B20_TEMP_C + getRandomVariation(variation);
    }

    static float getHumidity(float variation = 2.0f) {
        return MockSensor::BME280_HUMIDITY_PCT + getRandomVariation(variation);
    }

    static float getPressure(float variation = 1.0f) {
        return MockSensor::BME280_PRESSURE_HPA + getRandomVariation(variation);
    }

    static uint16_t getSoilMoistureRaw(uint16_t variation = 50) {
        int32_t value = MockSensor::SOIL_MOISTURE_RAW + getRandomVariation(variation);
        if (value < 0)
            value = 0;
        if (value > 4095)
            value = 4095;
        return static_cast<uint16_t>(value);
    }

   private:
    static float getRandomVariation(float maxVariation) {
        // Simple pseudo-random variation for testing
        static uint32_t seed = 12345;
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        float normalized = static_cast<float>(seed) / 0x7fffffff;
        return (normalized - 0.5f) * 2.0f * maxVariation;
    }

    static int32_t getRandomVariation(int32_t maxVariation) {
        static uint32_t seed = 54321;
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        int32_t normalized = static_cast<int32_t>(seed % (maxVariation * 2 + 1));
        return normalized - maxVariation;
    }
};

// ============================================================================
// Mock Sensor Failure Modes
// ============================================================================
// For testing error handling

class MockSensorFailure {
   public:
    enum class FailureMode {
        NONE,
        BME280_INIT_FAIL,
        BME280_READ_FAIL,
        DS18B20_DISCONNECTED,
        SOIL_MOISTURE_OUT_OF_RANGE
    };

    static void setFailureMode(FailureMode mode) { currentFailureMode = mode; }

    static FailureMode getFailureMode() { return currentFailureMode; }

    static bool shouldBME280InitFail() {
        return currentFailureMode == FailureMode::BME280_INIT_FAIL;
    }

    static bool shouldBME280ReadFail() {
        return currentFailureMode == FailureMode::BME280_READ_FAIL;
    }

    static bool shouldDS18B20Disconnect() {
        return currentFailureMode == FailureMode::DS18B20_DISCONNECTED;
    }

    static bool shouldSoilMoistureBeOutOfRange() {
        return currentFailureMode == FailureMode::SOIL_MOISTURE_OUT_OF_RANGE;
    }

   private:
    static FailureMode currentFailureMode;
};

// Initialize static member
inline MockSensorFailure::FailureMode MockSensorFailure::currentFailureMode =
    MockSensorFailure::FailureMode::NONE;

#endif  // UNIT_TEST

// ============================================================================
// Usage Example
// ============================================================================
//
// In your sensor manager code:
//
// #ifdef UNIT_TEST
// #include "MockSensor.h"
// using BME280 = MockBME280;
// using DallasTemperature = MockDallasTemperature;
// #else
// #include <Adafruit_BME280.h>
// #include <DallasTemperature.h>
// #endif
//
// class SensorManager {
// private:
//     BME280 bme280;  // Will be MockBME280 in tests, real in production
//     DallasTemperature ds18b20;
// };
//
// ============================================================================

#endif  // MOCK_SENSOR_H
