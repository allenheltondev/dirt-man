#include <string.h>
#include <unity.h>

#include "DataManager.h"

// Test helper to create a sensor reading
SensorReadings createTestReading(float temp, uint32_t timestamp) {
    SensorReadings reading = {};
    reading.bme280Temp = temp;
    reading.ds18b20Temp = temp + 1.0f;
    reading.humidity = 50.0f;
    reading.pressure = 1013.25f;
    reading.soilMoisture = 60.0f;
    reading.soilMoistureRaw = 2000;
    reading.sensorStatus = 0xFF;  // All sensors available
    reading.monotonicMs = timestamp;
    return reading;
}

void test_data_manager_initialization() {
    DataManager dm;

    TEST_ASSERT_EQUAL_UINT16(20, dm.getPublishIntervalSamples());
    TEST_ASSERT_EQUAL_UINT16(0, dm.getCurrentSampleCount());
    TEST_ASSERT_FALSE(dm.shouldPublish());
}

void test_add_single_reading() {
    DataManager dm;
    SensorReadings reading = createTestReading(22.5f, 1000);

    dm.addReading(reading);

    TEST_ASSERT_EQUAL_UINT16(1, dm.getCurrentSampleCount());
    TEST_ASSERT_FALSE(dm.shouldPublish());
}

void test_add_readings_until_publish() {
    DataManager dm;
    dm.setPublishIntervalSamples(5);

    for (int i = 0; i < 5; i++) {
        SensorReadings reading = createTestReading(20.0f + i, 1000 + i * 1000);
        dm.addReading(reading);
    }

    TEST_ASSERT_EQUAL_UINT16(5, dm.getCurrentSampleCount());
    TEST_ASSERT_TRUE(dm.shouldPublish());
}

void test_calculate_averages() {
    DataManager dm;
    dm.setPublishIntervalSamples(3);

    // Add 3 readings with known values
    dm.addReading(createTestReading(20.0f, 1000));
    dm.addReading(createTestReading(22.0f, 2000));
    dm.addReading(createTestReading(24.0f, 3000));

    AveragedData avg = dm.calculateAverages();

    // Check averages (20 + 22 + 24) / 3 = 22.0
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.0f, avg.avgBme280Temp);
    // DS18B20 is temp + 1.0, so (21 + 23 + 25) / 3 = 23.0
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.0f, avg.avgDs18b20Temp);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, avg.avgHumidity);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1013.25f, avg.avgPressure);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 60.0f, avg.avgSoilMoisture);

    // Check timestamps
    TEST_ASSERT_EQUAL_UINT32(1000, avg.sampleStartUptimeMs);
    TEST_ASSERT_EQUAL_UINT32(3000, avg.sampleEndUptimeMs);

    // Check metadata
    TEST_ASSERT_EQUAL_UINT16(3, avg.sampleCount);
    TEST_ASSERT_EQUAL_UINT8(0xFF, avg.sensorStatus);
    TEST_ASSERT_FALSE(avg.timeSynced);
}

void test_clear_averaging_buffer() {
    DataManager dm;
    dm.setPublishIntervalSamples(3);

    // Add readings
    for (int i = 0; i < 3; i++) {
        dm.addReading(createTestReading(20.0f, 1000 + i * 1000));
    }

    TEST_ASSERT_EQUAL_UINT16(3, dm.getCurrentSampleCount());

    // Clear buffer
    dm.clearAveragingBuffer();

    TEST_ASSERT_EQUAL_UINT16(0, dm.getCurrentSampleCount());
    TEST_ASSERT_FALSE(dm.shouldPublish());
}

void test_averaging_buffer_lifecycle() {
    DataManager dm;
    dm.setPublishIntervalSamples(5);

    // Add 5 readings
    for (int i = 0; i < 5; i++) {
        dm.addReading(createTestReading(20.0f + i, 1000 + i * 1000));
    }

    TEST_ASSERT_TRUE(dm.shouldPublish());

    // Calculate averages
    AveragedData avg = dm.calculateAverages();
    TEST_ASSERT_EQUAL_UINT16(5, avg.sampleCount);

    // Clear buffer
    dm.clearAveragingBuffer();
    TEST_ASSERT_EQUAL_UINT16(0, dm.getCurrentSampleCount());
    TEST_ASSERT_FALSE(dm.shouldPublish());

    // Add new readings
    for (int i = 0; i < 5; i++) {
        dm.addReading(createTestReading(30.0f + i, 6000 + i * 1000));
    }

    TEST_ASSERT_TRUE(dm.shouldPublish());

    // Calculate new averages
    AveragedData avg2 = dm.calculateAverages();
    TEST_ASSERT_EQUAL_UINT16(5, avg2.sampleCount);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 32.0f, avg2.avgBme280Temp);  // (30+31+32+33+34)/5 = 32
}

void test_set_publish_interval_samples() {
    DataManager dm;

    // Test normal value
    dm.setPublishIntervalSamples(10);
    TEST_ASSERT_EQUAL_UINT16(10, dm.getPublishIntervalSamples());

    // Test max value
    dm.setPublishIntervalSamples(MAX_PUBLISH_SAMPLES);
    TEST_ASSERT_EQUAL_UINT16(MAX_PUBLISH_SAMPLES, dm.getPublishIntervalSamples());

    // Test exceeding max (should clamp to MAX_PUBLISH_SAMPLES)
    dm.setPublishIntervalSamples(MAX_PUBLISH_SAMPLES + 10);
    TEST_ASSERT_EQUAL_UINT16(MAX_PUBLISH_SAMPLES, dm.getPublishIntervalSamples());

    // Test zero (should set to 1)
    dm.setPublishIntervalSamples(0);
    TEST_ASSERT_EQUAL_UINT16(1, dm.getPublishIntervalSamples());
}

void test_batch_id_generation_unsynced() {
    DataManager dm;
    dm.setPublishIntervalSamples(2);

    dm.addReading(createTestReading(20.0f, 1000));
    dm.addReading(createTestReading(22.0f, 2000));

    AveragedData avg = dm.calculateAverages();

    // Batch ID should use uptime format: device_u_uptimeStart_uptimeEnd
    TEST_ASSERT_TRUE(strstr(avg.batchId, "device_u_") != NULL);
    TEST_ASSERT_TRUE(strstr(avg.batchId, "1000") != NULL);
    TEST_ASSERT_TRUE(strstr(avg.batchId, "2000") != NULL);
}

void test_empty_buffer_averages() {
    DataManager dm;

    // Calculate averages on empty buffer
    AveragedData avg = dm.calculateAverages();

    // Should return zeroed structure
    TEST_ASSERT_EQUAL_UINT16(0, avg.sampleCount);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, avg.avgBme280Temp);
}

void test_buffer_does_not_overflow() {
    DataManager dm;
    dm.setPublishIntervalSamples(5);

    // Add more readings than publish interval
    for (int i = 0; i < 10; i++) {
        dm.addReading(createTestReading(20.0f + i, 1000 + i * 1000));
    }

    // Should only have 5 readings (publish interval)
    TEST_ASSERT_EQUAL_UINT16(5, dm.getCurrentSampleCount());
}

void setup() {
    UNITY_BEGIN();

    RUN_TEST(test_data_manager_initialization);
    RUN_TEST(test_add_single_reading);
    RUN_TEST(test_add_readings_until_publish);
    RUN_TEST(test_calculate_averages);
    RUN_TEST(test_clear_averaging_buffer);
    RUN_TEST(test_averaging_buffer_lifecycle);
    RUN_TEST(test_set_publish_interval_samples);
    RUN_TEST(test_batch_id_generation_unsynced);
    RUN_TEST(test_empty_buffer_averages);
    RUN_TEST(test_buffer_does_not_overflow);

    UNITY_END();
}

void loop() {
    // Empty
}
