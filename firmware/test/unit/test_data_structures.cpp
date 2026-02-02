#include <unity.h>
#include "../include/models/SensorType.h"
#include "../include/models/SensorHealth.h"
#include "../include/models/DisplayPage.h"
#include "../include/models/SensorReadings.h"
#include "../include/models/ErrorCounters.h"
#include "../include/models/AveragedData.h"
#include "../include/models/DisplayPoint.h"
#include "../include/models/SystemStatus.h"
#include "../include/models/Config.h"

void test_sensor_readings_structure() {
    SensorReadings readings = {};
    readings.bme280Temp = 22.5f;
    readings.ds18b20Temp = 21.8f;
    readings.humidity = 45.2f;
    readings.pressure = 1013.25f;
    readings.soilMoisture = 62.3f;
    readings.soilMoistureRaw = 2048;
    readings.sensorStatus = 0xFF;
    readings.monotonicMs = 1000;

    TEST_ASSERT_EQUAL_FLOAT(22.5f, readings.bme280Temp);
    TEST_ASSERT_EQUAL_FLOAT(21.8f, readings.ds18b20Temp);
    TEST_ASSERT_EQUAL_UINT16(2048, readings.soilMoistureRaw);
}

void test_averaged_data_structure() {
    AveragedData data = {};
    data.avgBme280Temp = 22.0f;
    data.sampleCount = 20;
    data.timeSynced = false;

    TEST_ASSERT_EQUAL_FLOAT(22.0f, data.avgBme280Temp);
    TEST_ASSERT_EQUAL_UINT16(20, data.sampleCount);
    TEST_ASSERT_FALSE(data.timeSynced);
}

void test_error_counters_structure() {
    ErrorCounters errors = {};
    errors.sensorReadFailures = 5;
    errors.networkFailures = 2;
    errors.bufferOverflows = 0;

    TEST_ASSERT_EQUAL_UINT16(5, errors.sensorReadFailures);
    TEST_ASSERT_EQUAL_UINT16(2, errors.networkFailures);
    TEST_ASSERT_EQUAL_UINT16(0, errors.bufferOverflows);
}

void test_display_point_structure() {
    DisplayPoint point = {};
    point.value = 23.5f;
    point.timestamp = 60000;

    TEST_ASSERT_EQUAL_FLOAT(23.5f, point.value);
    TEST_ASSERT_EQUAL_UINT32(60000, point.timestamp);
}

void test_sensor_type_enum() {
    SensorType type = SensorType::BME280_TEMP;
    TEST_ASSERT_EQUAL(0, static_cast<uint8_t>(type));

    type = SensorType::SOIL_MOISTURE;
    TEST_ASSERT_EQUAL(4, static_cast<uint8_t>(type));

    TEST_ASSERT_EQUAL_UINT8(5, NUM_SENSORS);
}

void test_sensor_health_enum() {
    SensorHealth health = SensorHealth::GREEN;
    TEST_ASSERT_EQUAL(0, static_cast<uint8_t>(health));

    health = SensorHealth::YELLOW;
    TEST_ASSERT_EQUAL(1, static_cast<uint8_t>(health));

    health = SensorHealth::RED;
    TEST_ASSERT_EQUAL(2, static_cast<uint8_t>(health));
}

void test_display_page_enum() {
    DisplayPage page = DisplayPage::SUMMARY;
    TEST_ASSERT_EQUAL(0, static_cast<uint8_t>(page));

    page = DisplayPage::SYSTEM_HEALTH;
    TEST_ASSERT_EQUAL(6, static_cast<uint8_t>(page));
}

void test_system_status_structure() {
    SystemStatus status = {};
    status.uptimeMs = 3600000;
    status.freeHeap = 245760;
    status.wifiRssi = -65;
    status.queueDepth = 5;
    status.bootCount = 10;

    TEST_ASSERT_EQUAL_UINT32(3600000, status.uptimeMs);
    TEST_ASSERT_EQUAL_UINT32(245760, status.freeHeap);
    TEST_ASSERT_EQUAL_INT8(-65, status.wifiRssi);
    TEST_ASSERT_EQUAL_UINT16(5, status.queueDepth);
    TEST_ASSERT_EQUAL_UINT16(10, status.bootCount);
}

void test_config_structure() {
    Config config = {};
    config.readingIntervalMs = 5000;
    config.publishIntervalSamples = 20;
    config.pageCycleIntervalMs = 10000;
    config.soilDryAdc = 3000;
    config.soilWetAdc = 1500;
    config.temperatureInFahrenheit = false;
    config.batteryMode = false;

    TEST_ASSERT_EQUAL_UINT32(5000, config.readingIntervalMs);
    TEST_ASSERT_EQUAL_UINT16(20, config.publishIntervalSamples);
    TEST_ASSERT_EQUAL_UINT16(3000, config.soilDryAdc);
    TEST_ASSERT_FALSE(config.batteryMode);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_sensor_readings_structure);
    RUN_TEST(test_averaged_data_structure);
    RUN_TEST(test_error_counters_structure);
    RUN_TEST(test_display_point_structure);
    RUN_TEST(test_sensor_type_enum);
    RUN_TEST(test_sensor_health_enum);
    RUN_TEST(test_display_page_enum);
    RUN_TEST(test_system_status_structure);
    RUN_TEST(test_config_structure);

    return UNITY_END();
}
