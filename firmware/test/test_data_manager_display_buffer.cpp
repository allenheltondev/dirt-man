#include <unity.h>

#include "DataManager.h"
#include "models/DisplayPoint.h"
#include "models/SensorReadings.h"
#include "models/SensorType.h"

// Test helper to create a sensor reading with specific timestamp
SensorReadings createReading(uint32_t timestamp, float value) {
    SensorReadings reading = {};
    reading.monotonicMs = timestamp;
    reading.bme280Temp = value;
    reading.ds18b20Temp = value + 1.0f;
    reading.humidity = value + 2.0f;
    reading.pressure = 1000.0f + value;
    reading.soilMoisture = value + 3.0f;
    reading.sensorStatus = 0xFF;
    return reading;
}

void test_display_buffer_initial_state() {
    DataManager dm;

    TEST_ASSERT_EQUAL_UINT16(0, dm.getDisplayDataCount(SensorType::BME280_TEMP));
    TEST_ASSERT_EQUAL_UINT16(0, dm.getDisplayDataCount(SensorType::DS18B20_TEMP));
    TEST_ASSERT_EQUAL_UINT16(0, dm.getDisplayDataCount(SensorType::HUMIDITY));
    TEST_ASSERT_EQUAL_UINT16(0, dm.getDisplayDataCount(SensorType::PRESSURE));
    TEST_ASSERT_EQUAL_UINT16(0, dm.getDisplayDataCount(SensorType::SOIL_MOISTURE));
}

void test_display_buffer_respects_1_minute_interval() {
    DataManager dm;

    SensorReadings reading1 = createReading(0, 20.0f);
    dm.addToDisplayBuffer(reading1);

    TEST_ASSERT_EQUAL_UINT16(1, dm.getDisplayDataCount(SensorType::BME280_TEMP));

    SensorReadings reading2 = createReading(30000, 21.0f);
    dm.addToDisplayBuffer(reading2);

    TEST_ASSERT_EQUAL_UINT16(1, dm.getDisplayDataCount(SensorType::BME280_TEMP));

    SensorReadings reading3 = createReading(60000, 22.0f);
    dm.addToDisplayBuffer(reading3);

    TEST_ASSERT_EQUAL_UINT16(2, dm.getDisplayDataCount(SensorType::BME280_TEMP));
}

void test_display_buffer_capacity_limit() {
    DataManager dm;

    for (uint16_t i = 0; i < 250; i++) {
        uint32_t timestamp = i * 60000;
        SensorReadings reading = createReading(timestamp, 20.0f + i);
        dm.addToDisplayBuffer(reading);
    }

    TEST_ASSERT_EQUAL_UINT16(240, dm.getDisplayDataCount(SensorType::BME280_TEMP));
    TEST_ASSERT_EQUAL_UINT16(240, dm.getDisplayDataCount(SensorType::DS18B20_TEMP));
    TEST_ASSERT_EQUAL_UINT16(240, dm.getDisplayDataCount(SensorType::HUMIDITY));
    TEST_ASSERT_EQUAL_UINT16(240, dm.getDisplayDataCount(SensorType::PRESSURE));
    TEST_ASSERT_EQUAL_UINT16(240, dm.getDisplayDataCount(SensorType::SOIL_MOISTURE));
}

void test_display_buffer_downsampling_to_120_points() {
    DataManager dm;

    for (uint16_t i = 0; i < 240; i++) {
        uint32_t timestamp = i * 60000;
        SensorReadings reading = createReading(timestamp, 20.0f + i);
        dm.addToDisplayBuffer(reading);
    }

    uint16_t count = 0;
    const DisplayPoint* data = dm.getDisplayData(SensorType::BME280_TEMP, count, 120);

    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_UINT16(120, count);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.0f, data[0].value);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 259.0f, data[119].value);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_display_buffer_initial_state);
    RUN_TEST(test_display_buffer_respects_1_minute_interval);
    RUN_TEST(test_display_buffer_capacity_limit);
    RUN_TEST(test_display_buffer_downsampling_to_120_points);

    return UNITY_END();
}
