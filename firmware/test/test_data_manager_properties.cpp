#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unity.h>

#include "DataManager.h"

/**
 * Property-Based Tests for DataManager
 *
 * Feature: esp32-sensor-firmware
 *
 * These tests verify universal properties across many generated inputs.
 * Minimum 100 iterations per property test.
 */

// Random number generator helpers
float randomFloat(float min, float max) {
    return min + (float)rand() / ((float)RAND_MAX / (max - min));
}

uint32_t randomUInt32(uint32_t min, uint32_t max) {
    return min + (rand() % (max - min + 1));
}

// Helper to create a random sensor reading
SensorReadings createRandomReading(uint32_t timestamp) {
    SensorReadings reading = {};
    reading.bme280Temp = randomFloat(-40.0f, 85.0f);
    reading.ds18b20Temp = randomFloat(-55.0f, 125.0f);
    reading.humidity = randomFloat(0.0f, 100.0f);
    reading.pressure = randomFloat(300.0f, 1100.0f);
    reading.soilMoisture = randomFloat(0.0f, 100.0f);
    reading.soilMoistureRaw = randomUInt32(0, 4095);
    reading.sensorStatus = 0xFF;
    reading.monotonicMs = timestamp;
    return reading;
}

/**
 * Property 4: Averaging Buffer Lifecycle
 *
 * For any sequence of N sensor readings (where N equals Publish_Interval),
 * after adding N readings to the Averaging_Buffer, calling getAveragedData()
 * should return averaged values and clear the buffer to empty state.
 *
 * **Feature: esp32-sensor-firmware, Property 4: Averaging buffer lifecycle maintains correct
 * state**
 *
 * Validates: Requirements 3.1, 3.2, 3.4
 */
void property_averaging_buffer_lifecycle() {
    const int NUM_ITERATIONS = 100;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        DataManager dm;

        // Random publish interval between 1 and MAX_PUBLISH_SAMPLES
        uint16_t publishInterval = randomUInt32(1, MAX_PUBLISH_SAMPLES);
        dm.setPublishIntervalSamples(publishInterval);

        // Add exactly publishInterval readings
        for (uint16_t i = 0; i < publishInterval; i++) {
            SensorReadings reading = createRandomReading(1000 + i * 1000);
            dm.addReading(reading);
        }

        // Property: shouldPublish() must return true after N readings
        TEST_ASSERT_TRUE_MESSAGE(
            dm.shouldPublish(),
            "shouldPublish() must return true after adding publishInterval readings");

        // Property: getCurrentSampleCount() must equal publishInterval
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(publishInterval, dm.getCurrentSampleCount(),
                                         "Sample count must equal publishInterval");

        // Calculate averages
        AveragedData avg = dm.calculateAverages();

        // Property: sampleCount in averaged data must equal publishInterval
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(publishInterval, avg.sampleCount,
                                         "Averaged data sampleCount must equal publishInterval");

        // Clear buffer
        dm.clearAveragingBuffer();

        // Property: After clearing, buffer must be empty
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(0, dm.getCurrentSampleCount(),
                                         "Buffer must be empty after clearing");

        // Property: After clearing, shouldPublish() must return false
        TEST_ASSERT_FALSE_MESSAGE(dm.shouldPublish(),
                                  "shouldPublish() must return false after clearing");
    }
}

/**
 * Property 5: Average Calculation Correctness
 *
 * For any set of sensor readings in the Averaging_Buffer, the calculated
 * average for each sensor should equal the arithmetic mean of all readings
 * for that sensor.
 *
 * **Feature: esp32-sensor-firmware, Property 5: Average calculation equals arithmetic mean**
 *
 * Validates: Requirements 3.3
 */
void property_average_calculation_correctness() {
    const int NUM_ITERATIONS = 100;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        DataManager dm;

        // Random number of samples between 2 and 50
        uint16_t numSamples = randomUInt32(2, 50);
        dm.setPublishIntervalSamples(numSamples);

        // Track sums for manual calculation
        float sumBme280Temp = 0;
        float sumDs18b20Temp = 0;
        float sumHumidity = 0;
        float sumPressure = 0;
        float sumSoilMoisture = 0;

        // Add readings and track sums
        for (uint16_t i = 0; i < numSamples; i++) {
            SensorReadings reading = createRandomReading(1000 + i * 1000);
            dm.addReading(reading);

            sumBme280Temp += reading.bme280Temp;
            sumDs18b20Temp += reading.ds18b20Temp;
            sumHumidity += reading.humidity;
            sumPressure += reading.pressure;
            sumSoilMoisture += reading.soilMoisture;
        }

        // Calculate expected averages
        float expectedBme280Temp = sumBme280Temp / numSamples;
        float expectedDs18b20Temp = sumDs18b20Temp / numSamples;
        float expectedHumidity = sumHumidity / numSamples;
        float expectedPressure = sumPressure / numSamples;
        float expectedSoilMoisture = sumSoilMoisture / numSamples;

        // Get averaged data
        AveragedData avg = dm.calculateAverages();

        // Property: Calculated averages must equal arithmetic mean (within floating point
        // precision)
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, expectedBme280Temp, avg.avgBme280Temp,
                                         "BME280 temp average must equal arithmetic mean");
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, expectedDs18b20Temp, avg.avgDs18b20Temp,
                                         "DS18B20 temp average must equal arithmetic mean");
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, expectedHumidity, avg.avgHumidity,
                                         "Humidity average must equal arithmetic mean");
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, expectedPressure, avg.avgPressure,
                                         "Pressure average must equal arithmetic mean");
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.001f, expectedSoilMoisture, avg.avgSoilMoisture,
                                         "Soil moisture average must equal arithmetic mean");
    }
}

/**
 * Property 10: Batch ID Uniqueness
 *
 * For any two different AveragedData structures with different timestamp
 * ranges or device IDs, their generated batch_ids should be unique.
 *
 * **Feature: esp32-sensor-firmware, Property 10: Batch IDs are unique for different data**
 *
 * Validates: Requirements 5.5
 */
void property_batch_id_uniqueness() {
    const int NUM_ITERATIONS = 100;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        DataManager dm1, dm2;

        // Set same publish interval
        uint16_t publishInterval = randomUInt32(2, 20);
        dm1.setPublishIntervalSamples(publishInterval);
        dm2.setPublishIntervalSamples(publishInterval);

        // Add readings with different timestamp ranges
        uint32_t baseTime1 = randomUInt32(1000, 100000);
        uint32_t baseTime2 = randomUInt32(200000, 300000);

        for (uint16_t i = 0; i < publishInterval; i++) {
            dm1.addReading(createRandomReading(baseTime1 + i * 1000));
            dm2.addReading(createRandomReading(baseTime2 + i * 1000));
        }

        // Calculate averages
        AveragedData avg1 = dm1.calculateAverages();
        AveragedData avg2 = dm2.calculateAverages();

        // Property: Batch IDs must be different for different timestamp ranges
        TEST_ASSERT_NOT_EQUAL(strcmp(avg1.batchId, avg2.batchId), 0);
    }
}

/**
 * Property: Buffer Capacity Constraint
 *
 * For any sequence of readings, the buffer should never exceed the
 * configured publishIntervalSamples, even if more readings are added.
 */
void property_buffer_capacity_constraint() {
    const int NUM_ITERATIONS = 100;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        DataManager dm;

        // Random publish interval
        uint16_t publishInterval = randomUInt32(5, 50);
        dm.setPublishIntervalSamples(publishInterval);

        // Try to add more readings than the publish interval
        uint16_t excessReadings = randomUInt32(publishInterval + 1, publishInterval + 50);

        for (uint16_t i = 0; i < excessReadings; i++) {
            dm.addReading(createRandomReading(1000 + i * 1000));
        }

        // Property: Buffer count must not exceed publishInterval
        TEST_ASSERT_LESS_OR_EQUAL_UINT16_MESSAGE(publishInterval, dm.getCurrentSampleCount(),
                                                 "Buffer count must not exceed publishInterval");
    }
}

/**
 * Property: Timestamp Ordering
 *
 * For any averaged data, the sample start timestamp must be less than
 * or equal to the sample end timestamp.
 */
void property_timestamp_ordering() {
    const int NUM_ITERATIONS = 100;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        DataManager dm;

        uint16_t numSamples = randomUInt32(2, 20);
        dm.setPublishIntervalSamples(numSamples);

        // Add readings with monotonically increasing timestamps
        uint32_t baseTime = randomUInt32(1000, 100000);
        for (uint16_t i = 0; i < numSamples; i++) {
            dm.addReading(createRandomReading(baseTime + i * 1000));
        }

        AveragedData avg = dm.calculateAverages();

        // Property: Start timestamp must be <= end timestamp
        TEST_ASSERT_LESS_OR_EQUAL_UINT32_MESSAGE(avg.sampleStartUptimeMs, avg.sampleEndUptimeMs,
                                                 "Sample start timestamp must be <= end timestamp");
    }
}

void setUp() {
    // Seed random number generator
    srand((unsigned int)time(NULL));
}

void tearDown() {
    // Nothing to tear down
}

void setup() {
    UNITY_BEGIN();

    RUN_TEST(property_averaging_buffer_lifecycle);
    RUN_TEST(property_average_calculation_correctness);
    RUN_TEST(property_batch_id_uniqueness);
    RUN_TEST(property_buffer_capacity_constraint);
    RUN_TEST(property_timestamp_ordering);

    UNITY_END();
}

void loop() {
    // Empty
}
