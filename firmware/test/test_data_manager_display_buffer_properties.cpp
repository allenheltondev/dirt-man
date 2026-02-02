#include <stdlib.h>
#include <time.h>
#include <unity.h>

#include "DataManager.h"
#include "models/DisplayPoint.h"
#include "models/SensorReadings.h"
#include "models/SensorType.h"

// Random number generators
float randomFloat(float min, float max) {
    float scale = rand() / (float)RAND_MAX;
    return min + scale * (max - min);
}

uint32_t randomUInt32(uint32_t min, uint32_t max) {
    return min + (rand() % (max - min + 1));
}

// Helper to create a sensor reading
SensorReadings createReading(uint32_t timestamp, float baseValue) {
    SensorReadings reading = {};
    reading.monotonicMs = timestamp;
    reading.bme280Temp = baseValue;
    reading.ds18b20Temp = baseValue + 1.0f;
    reading.humidity = baseValue + 2.0f;
    reading.pressure = 1000.0f + baseValue;
    reading.soilMoisture = baseValue + 3.0f;
    reading.sensorStatus = 0xFF;
    return reading;
}

/**
 * Property 7: Display Buffer Fixed Interval
 *
 * For any sequence of sensor readings collected at Reading_Interval, the Display_Buffer
 * should only be updated when at least 1 minute has elapsed since the last display buffer
 * update, independent of Reading_Interval.
 *
 * **Feature: esp32-sensor-firmware, Property 7: Display buffer updates at fixed 1-minute
 * intervals**
 *
 * Validates: Requirements 3.7, 7.9
 */
void property_display_buffer_fixed_interval() {
    const int NUM_ITERATIONS = 100;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        DataManager dm;

        // Generate random reading interval (between 1 second and 2 minutes)
        uint32_t readingInterval = randomUInt32(1000, 120000);

        // Add readings at the random interval for 10 minutes
        uint32_t currentTime = 0;
        uint32_t endTime = 600000;  // 10 minutes
        int readingsAdded = 0;

        while (currentTime <= endTime) {
            SensorReadings reading = createReading(currentTime, 20.0f + readingsAdded);
            dm.addToDisplayBuffer(reading);

            currentTime += readingInterval;
            readingsAdded++;
        }

        // Calculate expected number of display buffer updates
        // Should be approximately endTime / 60000 (1 minute intervals)
        uint16_t expectedCount = (endTime / 60000) + 1;  // +1 for the first reading at t=0

        // Get actual count
        uint16_t actualCount = dm.getDisplayDataCount(SensorType::BME280_TEMP);

        // Allow some tolerance due to timing
        TEST_ASSERT_INT_WITHIN(2, expectedCount, actualCount);

        // Verify that updates happened at approximately 1-minute intervals
        uint16_t count = 0;
        const DisplayPoint* data = dm.getDisplayData(SensorType::BME280_TEMP, count);

        if (count > 1) {
            for (uint16_t i = 1; i < count; i++) {
                uint32_t timeDiff = data[i].timestamp - data[i - 1].timestamp;
                // Should be approximately 60000ms (1 minute), allow 10% tolerance
                TEST_ASSERT_INT_WITHIN(6000, 60000, timeDiff);
            }
        }
    }
}

/**
 * Property 8: Display Buffer Capacity Limit
 *
 * For any sequence of display buffer updates, the Display_Buffer should never exceed
 * 240 data points per sensor, removing oldest points when capacity is reached.
 *
 * **Feature: esp32-sensor-firmware, Property 8: Display buffer capacity limited to 240 points**
 *
 * Validates: Requirements 7.10
 */
void property_display_buffer_capacity_limit() {
    const int NUM_ITERATIONS = 100;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        DataManager dm;

        // Generate random number of readings (between 250 and 500)
        uint16_t numReadings = randomUInt32(250, 500);

        // Add readings at 1-minute intervals
        for (uint16_t i = 0; i < numReadings; i++) {
            uint32_t timestamp = i * 60000;
            SensorReadings reading = createReading(timestamp, 20.0f + i);
            dm.addToDisplayBuffer(reading);
        }

        // Verify all sensors are capped at 240 points
        TEST_ASSERT_EQUAL_UINT16(240, dm.getDisplayDataCount(SensorType::BME280_TEMP));
        TEST_ASSERT_EQUAL_UINT16(240, dm.getDisplayDataCount(SensorType::DS18B20_TEMP));
        TEST_ASSERT_EQUAL_UINT16(240, dm.getDisplayDataCount(SensorType::HUMIDITY));
        TEST_ASSERT_EQUAL_UINT16(240, dm.getDisplayDataCount(SensorType::PRESSURE));
        TEST_ASSERT_EQUAL_UINT16(240, dm.getDisplayDataCount(SensorType::SOIL_MOISTURE));

        // Verify that the oldest data was discarded (ring buffer behavior)
        uint16_t count = 0;
        const DisplayPoint* data = dm.getDisplayData(SensorType::BME280_TEMP, count);

        TEST_ASSERT_EQUAL_UINT16(240, count);

        // The first value should be from reading (numReadings - 240)
        uint16_t expectedFirstIndex = numReadings - 240;
        float expectedFirstValue = 20.0f + expectedFirstIndex;

        TEST_ASSERT_FLOAT_WITHIN(0.1f, expectedFirstValue, data[0].value);

        // The last value should be from the last reading
        float expectedLastValue = 20.0f + (numReadings - 1);
        TEST_ASSERT_FLOAT_WITHIN(0.1f, expectedLastValue, data[239].value);
    }
}

void setUp(void) {
    srand(time(NULL));
}

void tearDown(void) {}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(property_display_buffer_fixed_interval);
    RUN_TEST(property_display_buffer_capacity_limit);

    return UNITY_END();
}
