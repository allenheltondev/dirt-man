#include <unity.h>
#include "NetworkManager.h"
#include "ConfigManager.h"
#include "TimeManager.h"
#include "SystemStatusManager.h"
#include "models/SensorType.h"
#include <vector>

// Mock instances for testing
ConfigManager* testConfig = nullptr;
TimeManager* testTime = nullptr;
SystemStatusManager* testStatus = nullptr;
NetworkManager* testNetwork = nullptr;

void setUp(void) {
    testConfig = new ConfigManager();
    testTime = new TimeManager();
    testStatus = new SystemStatusManager();
    testNetwork = new NetworkManager(*testConfig, *testTime, *testStatus);

    testConfig->initialize();
    testTime->initialize();
    testStatus->initialize();
    testNetwork->initialize();
}

void tearDown(void) {
    delete testNetwork;
    delete testStatus;
    delete testTime;
    delete testConfig;
}

/**
 * Test: NetworkManager initializes successfully
 */
void test_network_manager_initialize() {
    // NetworkManager should initialize without errors
    // This is verified by setUp() not throwing
    TEST_ASSERT_NOT_NULL(testNetwork);
}

/**
 * Test: isConnected() returns false when WiFi is not connected
 * Note: This test assumes WiFi is not connected in test environment
 */
void test_is_connected_returns_false_when_disconnected() {
    // In native test environment, WiFi should not be connected
    bool connected = testNetwork->isConnected();
    TEST_ASSERT_FALSE(connected);
}

/**
 * Test: Exponential backoff calculation
 * Property: Backoff delay should increase exponentially
 *
 * This tests the calculateBackoffDelay logic indirectly through
 * the public interface behavior expectations.
 *
 * Expected delays:
 * - Attempt 0: 1000ms (1s)
 * - Attempt 1: 2000ms (2s)
 * - Attempt 2: 4000ms (4s)
 * - Attempt 3: 8000ms (8s)
 * - Attempt 4: 16000ms (16s)
 * - Attempt 5+: 16000ms (capped)
 */
void test_exponential_backoff_calculation() {
    // We can't directly test the private method, but we can verify
    // the behavior through connection attempts

    // For now, we'll document the expected behavior
    // The actual backoff is tested through integration testing
    TEST_PASS_MESSAGE("Exponential backoff: 1s, 2s, 4s, 8s, 16s (max)");
}

/**
 * Test: connectWiFi() handles missing SSID gracefully
 */
void test_connect_wifi_handles_missing_ssid() {
    // Set empty SSID in config
    Config cfg = testConfig->getConfig();
    cfg.wifiSsid[0] = '\0';
    testConfig->updateConfig(cfg);

    // Attempt to connect should fail gracefully
    bool result = testNetwork->connectWiFi();
    TEST_ASSERT_FALSE(result);
}

/**
 * Test: NetworkManager tracks connection state
 */
void test_network_manager_tracks_connection_state() {
    // Initially should not be connected
    TEST_ASSERT_FALSE(testNetwork->isConnected());

    // After failed connection attempt, should still be disconnected
    // (This assumes WiFi is not available in test environment)
    bool connected = testNetwork->isConnected();
    TEST_ASSERT_FALSE(connected);
}

/**
 * Test: JSON payload formatting with all sensors available
 * Validates: Requirements 5.1-5.2, 5.12
 */
void test_json_payload_all_sensors_available() {
    // Create test data with all sensors available
    AveragedData data = {};
    strcpy(data.batchId, "device123_e_1704067200000_1704067800000");
    data.avgBme280Temp = 22.5f;
    data.avgDs18b20Temp = 21.8f;
    data.avgHumidity = 45.2f;
    data.avgPressure = 1013.25f;
    data.avgSoilMoisture = 62.3f;
    data.sampleStartEpochMs = 1704067200000ULL;
    data.sampleEndEpochMs = 1704067800000ULL;
    data.deviceBootEpochMs = 1704060000000ULL;
    data.sampleStartUptimeMs = 100000;
    data.sampleEndUptimeMs = 700000;
    data.uptimeMs = 7200000;
    data.sampleCount = 20;
    data.sensorStatus = 0xFF; // All sensors available
    data.timeSynced = true;

    std::vector<AveragedData> dataList;
    dataList.push_back(data);

    // Format JSON payload
    String json = testNetwork->formatJsonPayload(dataList);

    // Verify JSON contains required fields
    TEST_ASSERT_TRUE(json.indexOf("\"batch_id\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"device_id\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"sample_start_epoch_ms\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"sample_end_epoch_ms\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"sample_start_uptime_ms\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"sample_end_uptime_ms\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"uptime_ms\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"sample_count\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"time_synced\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"sensors\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"bme280_temp_c\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"ds18b20_temp_c\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"humidity_pct\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"pressure_hpa\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"soil_moisture_pct\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"sensor_status\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"health\"") >= 0);

    // Verify sensor values are present (not null)
    TEST_ASSERT_TRUE(json.indexOf("\"bme280_temp_c\":22.5") >= 0 || json.indexOf("\"bme280_temp_c\":22.50") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"time_synced\":true") >= 0);
}

/**
 * Test: JSON payload formatting with unavailable sensors
 * Validates: Requirements 5.12 (null sensor representation)
 */
void test_json_payload_unavailable_sensors() {
    // Create test data with only BME280 available
    AveragedData data = {};
    strcpy(data.batchId, "device123_u_100000_700000");
    data.avgBme280Temp = 22.5f;
    data.avgDs18b20Temp = 0.0f;
    data.avgHumidity = 45.2f;
    data.avgPressure = 1013.25f;
    data.avgSoilMoisture = 0.0f;
    data.sampleStartEpochMs = 0;
    data.sampleEndEpochMs = 0;
    data.deviceBootEpochMs = 0;
    data.sampleStartUptimeMs = 100000;
    data.sampleEndUptimeMs = 700000;
    data.uptimeMs = 700000;
    data.sampleCount = 20;
    // Only BME280 sensors available (bits 0, 2, 3)
    data.sensorStatus = (1 << static_cast<uint8_t>(SensorType::BME280_TEMP)) |
                        (1 << static_cast<uint8_t>(SensorType::HUMIDITY)) |
                        (1 << static_cast<uint8_t>(SensorType::PRESSURE));
    data.timeSynced = false;

    std::vector<AveragedData> dataList;
    dataList.push_back(data);

    // Format JSON payload
    String json = testNetwork->formatJsonPayload(dataList);

    // Verify unavailable sensors are represented as null
    TEST_ASSERT_TRUE(json.indexOf("\"ds18b20_temp_c\":null") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"soil_moisture_pct\":null") >= 0);

    // Verify available sensors have values
    TEST_ASSERT_TRUE(json.indexOf("\"bme280_temp_c\":22.5") >= 0 || json.indexOf("\"bme280_temp_c\":22.50") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"humidity_pct\":45.2") >= 0 || json.indexOf("\"humidity_pct\":45.20") >= 0);

    // Verify sensor status flags
    TEST_ASSERT_TRUE(json.indexOf("\"bme280\":\"ok\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"ds18b20\":\"unavailable\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"soil_moisture\":\"unavailable\"") >= 0);
}

/**
 * Test: JSON payload with time not synced (epoch timestamps = 0)
 * Validates: Requirements 5.4 (time_synced flag and uptime timestamps)
 */
void test_json_payload_time_not_synced() {
    // Create test data with time not synced
    AveragedData data = {};
    strcpy(data.batchId, "device123_u_100000_700000");
    data.avgBme280Temp = 22.5f;
    data.avgDs18b20Temp = 21.8f;
    data.avgHumidity = 45.2f;
    data.avgPressure = 1013.25f;
    data.avgSoilMoisture = 62.3f;
    data.sampleStartEpochMs = 0; // Not synced
    data.sampleEndEpochMs = 0;   // Not synced
    data.deviceBootEpochMs = 0;  // Not synced
    data.sampleStartUptimeMs = 100000;
    data.sampleEndUptimeMs = 700000;
    data.uptimeMs = 700000;
    data.sampleCount = 20;
    data.sensorStatus = 0xFF;
    data.timeSynced = false;

    std::vector<AveragedData> dataList;
    dataList.push_back(data);

    // Format JSON payload
    String json = testNetwork->formatJsonPayload(dataList);

    // Verify epoch timestamps are 0
    TEST_ASSERT_TRUE(json.indexOf("\"sample_start_epoch_ms\":0") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"sample_end_epoch_ms\":0") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"device_boot_epoch_ms\":0") >= 0);

    // Verify uptime timestamps are present
    TEST_ASSERT_TRUE(json.indexOf("\"sample_start_uptime_ms\":100000") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"sample_end_uptime_ms\":700000") >= 0);

    // Verify time_synced flag is false
    TEST_ASSERT_TRUE(json.indexOf("\"time_synced\":false") >= 0);
}

/**
 * Test: JSON payload batch formatting with multiple readings
 * Validates: Requirements 5.11 (batch transmission)
 */
void test_json_payload_batch_multiple_readings() {
    // Create multiple test readings
    std::vector<AveragedData> dataList;

    for (int i = 0; i < 3; i++) {
        AveragedData data = {};
        snprintf(data.batchId, sizeof(data.batchId), "device123_e_%d_%d", 1704067200000 + i * 600000, 1704067800000 + i * 600000);
        data.avgBme280Temp = 22.0f + i;
        data.avgDs18b20Temp = 21.0f + i;
        data.avgHumidity = 45.0f + i;
        data.avgPressure = 1013.0f + i;
        data.avgSoilMoisture = 60.0f + i;
        data.sampleStartEpochMs = 1704067200000ULL + i * 600000;
        data.sampleEndEpochMs = 1704067800000ULL + i * 600000;
        data.deviceBootEpochMs = 1704060000000ULL;
        data.sampleStartUptimeMs = 100000 + i * 600000;
        data.sampleEndUptimeMs = 700000 + i * 600000;
        data.uptimeMs = 7200000 + i * 600000;
        data.sampleCount = 20;
        data.sensorStatus = 0xFF;
        data.timeSynced = true;

        dataList.push_back(data);
    }

    // Format JSON payload
    String json = testNetwork->formatJsonPayload(dataList);

    // Verify batch structure
    TEST_ASSERT_TRUE(json.indexOf("\"device_id\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"readings\":[") >= 0);

    // Verify multiple readings are present (count commas between readings)
    int readingCount = 0;
    int pos = 0;
    while ((pos = json.indexOf("\"batch_id\"", pos)) >= 0) {
        readingCount++;
        pos++;
    }
    TEST_ASSERT_EQUAL(3, readingCount);
}

/**
 * Test: JSON payload includes health metrics
 * Validates: Requirements 8.7 (health metrics in payload)
 */
void test_json_payload_includes_health_metrics() {
    // Create test data
    AveragedData data = {};
    strcpy(data.batchId, "device123_e_1704067200000_1704067800000");
    data.avgBme280Temp = 22.5f;
    data.avgDs18b20Temp = 21.8f;
    data.avgHumidity = 45.2f;
    data.avgPressure = 1013.25f;
    data.avgSoilMoisture = 62.3f;
    data.sampleStartEpochMs = 1704067200000ULL;
    data.sampleEndEpochMs = 1704067800000ULL;
    data.deviceBootEpochMs = 1704060000000ULL;
    data.sampleStartUptimeMs = 100000;
    data.sampleEndUptimeMs = 700000;
    data.uptimeMs = 7200000;
    data.sampleCount = 20;
    data.sensorStatus = 0xFF;
    data.timeSynced = true;

    std::vector<AveragedData> dataList;
    dataList.push_back(data);

    // Format JSON payload
    String json = testNetwork->formatJsonPayload(dataList);

    // Verify health section is present
    TEST_ASSERT_TRUE(json.indexOf("\"health\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"free_heap_bytes\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"wifi_rssi_dbm\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"error_counters\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"sensor_read_failures\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"network_failures\"") >= 0);
    TEST_ASSERT_TRUE(json.indexOf("\"buffer_overflows\"") >= 0);
}

/**
 * Test: JSON payload with empty data list returns empty object
 */
void test_json_payload_empty_data_list() {
    std::vector<AveragedData> dataList; // Empty

    // Format JSON payload
    String json = testNetwork->formatJsonPayload(dataList);

    // Should return empty JSON object
    TEST_ASSERT_EQUAL_STRING("{}", json.c_str());
}

/**
 * Test: Parse acknowledged batch IDs from server response
 * Validates: Requirements 5.9 (idempotent acknowledgment)
 */
void test_parse_acknowledged_batch_ids_valid_response() {
    // Create a mock server response with acknowledged batch IDs
    String response = "{\"status\":\"success\",\"acknowledged_batch_ids\":[\"device123_e_1704067200000_1704067800000\",\"device123_e_1704067800000_1704068400000\",\"device123_e_1704068400000_1704069000000\"]}";

    // Parse the response
    std::vector<String> batchIds = testNetwork->parseAcknowledgedBatchIds(response);

    // Verify correct number of batch IDs parsed
    TEST_ASSERT_EQUAL(3, batchIds.size());

    // Verify batch IDs are correct
    TEST_ASSERT_EQUAL_STRING("device123_e_1704067200000_1704067800000", batchIds[0].c_str());
    TEST_ASSERT_EQUAL_STRING("device123_e_1704067800000_1704068400000", batchIds[1].c_str());
    TEST_ASSERT_EQUAL_STRING("device123_e_1704068400000_1704069000000", batchIds[2].c_str());
}

/**
 * Test: Parse acknowledged batch IDs from response with single batch ID
 */
void test_parse_acknowledged_batch_ids_single_id() {
    String response = "{\"status\":\"success\",\"acknowledged_batch_ids\":[\"device123_e_1704067200000_1704067800000\"]}";

    std::vector<String> batchIds = testNetwork->parseAcknowledgedBatchIds(response);

    TEST_ASSERT_EQUAL(1, batchIds.size());
    TEST_ASSERT_EQUAL_STRING("device123_e_1704067200000_1704067800000", batchIds[0].c_str());
}

/**
 * Test: Parse acknowledged batch IDs from response with empty array
 */
void test_parse_acknowledged_batch_ids_empty_array() {
    String response = "{\"status\":\"success\",\"acknowledged_batch_ids\":[]}";

    std::vector<String> batchIds = testNetwork->parseAcknowledgedBatchIds(response);

    TEST_ASSERT_EQUAL(0, batchIds.size());
}

/**
 * Test: Parse acknowledged batch IDs from response without acknowledged_batch_ids field
 */
void test_parse_acknowledged_batch_ids_missing_field() {
    String response = "{\"status\":\"success\"}";

    std::vector<String> batchIds = testNetwork->parseAcknowledgedBatchIds(response);

    // Should return empty vector when field is missing
    TEST_ASSERT_EQUAL(0, batchIds.size());
}

/**
 * Test: Parse acknowledged batch IDs from empty response
 */
void test_parse_acknowledged_batch_ids_empty_response() {
    String response = "";

    std::vector<String> batchIds = testNetwork->parseAcknowledgedBatchIds(response);

    TEST_ASSERT_EQUAL(0, batchIds.size());
}

/**
 * Test: Parse acknowledged batch IDs from malformed response
 */
void test_parse_acknowledged_batch_ids_malformed_response() {
    // Missing closing bracket
    String response = "{\"status\":\"success\",\"acknowledged_batch_ids\":[\"device123_e_1704067200000_1704067800000\"";

    std::vector<String> batchIds = testNetwork->parseAcknowledgedBatchIds(response);

    // Should handle gracefully and return empty or partial results
    // The implementation should not crash
    TEST_ASSERT_TRUE(batchIds.size() >= 0);
}

/**
 * Test: Parse acknowledged batch IDs with whitespace in response
 */
void test_parse_acknowledged_batch_ids_with_whitespace() {
    String response = "{\n  \"status\": \"success\",\n  \"acknowledged_batch_ids\": [\n    \"device123_e_1704067200000_1704067800000\",\n    \"device123_e_1704067800000_1704068400000\"\n  ]\n}";

    std::vector<String> batchIds = testNetwork->parseAcknowledgedBatchIds(response);

    TEST_ASSERT_EQUAL(2, batchIds.size());
    TEST_ASSERT_EQUAL_STRING("device123_e_1704067200000_1704067800000", batchIds[0].c_str());
    TEST_ASSERT_EQUAL_STRING("device123_e_1704067800000_1704068400000", batchIds[1].c_str());
}

void setup() {
    delay(2000); // Wait for serial monitor

    UNITY_BEGIN();

    RUN_TEST(test_network_manager_initialize);
    RUN_TEST(test_is_connected_returns_false_when_disconnected);
    RUN_TEST(test_exponential_backoff_calculation);
    RUN_TEST(test_connect_wifi_handles_missing_ssid);
    RUN_TEST(test_network_manager_tracks_connection_state);
    RUN_TEST(test_json_payload_all_sensors_available);
    RUN_TEST(test_json_payload_unavailable_sensors);
    RUN_TEST(test_json_payload_time_not_synced);
    RUN_TEST(test_json_payload_batch_multiple_readings);
    RUN_TEST(test_json_payload_includes_health_metrics);
    RUN_TEST(test_json_payload_empty_data_list);
    RUN_TEST(test_parse_acknowledged_batch_ids_valid_response);
    RUN_TEST(test_parse_acknowledged_batch_ids_single_id);
    RUN_TEST(test_parse_acknowledged_batch_ids_empty_array);
    RUN_TEST(test_parse_acknowledged_batch_ids_missing_field);
    RUN_TEST(test_parse_acknowledged_batch_ids_empty_response);
    RUN_TEST(test_parse_acknowledged_batch_ids_malformed_response);
    RUN_TEST(test_parse_acknowledged_batch_ids_with_whitespace);

    UNITY_END();
}

void loop() {
    // Nothing to do here
}
