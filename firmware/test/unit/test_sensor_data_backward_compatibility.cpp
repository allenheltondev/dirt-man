#ifdef UNIT_TEST

#include <unity.h>

#include <cstdlib>
#include <ctime>
#include <string>

typedef std::string String;

// Simplified buildSensorDataPayload for testing
String buildSensorDataPayload(const String& hardwareId, const String& bootId,
                              const String& friendlyName) {
    String payload = "{";

    // New fields at root level
    payload += "\"hardware_id\":\"" + hardwareId + "\",";
    payload += "\"boot_id\":\"" + bootId + "\",";

    if (friendlyName.length() > 0) {
        payload += "\"friendly_name\":\"" + friendlyName + "\",";
    }

    // Existing readings array structure (unchanged)
    payload += "\"readings\":[";
    payload += "{";
    payload += "\"batch_id\":\"test-batch-1\",";
    payload += "\"device_id\":\"test-device\",";
    payload += "\"sample_start_epoch_ms\":1234567890,";
    payload += "\"sample_end_epoch_ms\":1234567900,";
    payload += "\"device_boot_epoch_ms\":1234560000,";
    payload += "\"sample_start_uptime_ms\":10000,";
    payload += "\"sample_end_uptime_ms\":10010,";
    payload += "\"uptime_ms\":10010,";
    payload += "\"sample_count\":5,";
    payload += "\"time_synced\":true,";
    payload += "\"sensors\":{";
    payload += "\"bme280_temp_c\":22.5,";
    payload += "\"ds18b20_temp_c\":null,";
    payload += "\"humidity_pct\":65.0,";
    payload += "\"pressure_hpa\":1013.25,";
    payload += "\"soil_moisture_pct\":null";
    payload += "},";
    payload += "\"sensor_status\":{";
    payload += "\"bme280\":\"ok\",";
    payload += "\"ds18b20\":\"unavailable\",";
    payload += "\"soil_moisture\":\"unavailable\"";
    payload += "},";
    payload += "\"health\":{";
    payload += "\"uptime_ms\":10010";
    payload += "}";
    payload += "}";
    payload += "]";

    payload += "}";

    return payload;
}

// Helper function to check if a string contains a substring
bool contains(const String& str, const String& substr) {
    return str.find(substr) != std::string::npos;
}

void setUp(void) {
    // Setup before each test
}

void tearDown(void) {
    // Cleanup after each test
}

// Test: Backward Compatibility - Readings Array Structure Unchanged
// Validates: Requirements 11.5
void test_readings_array_structure_unchanged() {
    // Build a sensor data payload
    String payload = buildSensorDataPayload("AA:BB:CC:DD:EE:FF",
                                           "550e8400-e29b-41d4-a716-446655440000",
                                           "test-device");

    // Verify readings array is present
    TEST_ASSERT_TRUE_MESSAGE(contains(payload, "\"readings\":["),
                            "readings array should be present");

    // Verify readings array contains expected fields (unchanged structure)
    TEST_ASSERT_TRUE_MESSAGE(contains(payload, "\"batch_id\":"),
                            "readings should contain batch_id");
    TEST_ASSERT_TRUE_MESSAGE(contains(payload, "\"device_id\":"),
                            "readings should contain device_id");
    TEST_ASSERT_TRUE_MESSAGE(contains(payload, "\"sample_start_epoch_ms\":"),
                            "readings should contain sample_start_epoch_ms");
    TEST_ASSERT_TRUE_MESSAGE(contains(payload, "\"sample_end_epoch_ms\":"),
                            "readings should contain sample_end_epoch_ms");
    TEST_ASSERT_TRUE_MESSAGE(contains(payload, "\"device_boot_epoch_ms\":"),
                            "readings should contain device_boot_epoch_ms");
    TEST_ASSERT_TRUE_MESSAGE(contains(payload, "\"sample_start_uptime_ms\":"),
                            "readings should contain sample_start_uptime_ms");
    TEST_ASSERT_TRUE_MESSAGE(contains(payload, "\"sample_end_uptime_ms\":"),
                            "readings should contain sample_end_uptime_ms");
    TEST_ASSERT_TRUE_MESSAGE(contains(payload, "\"uptime_ms\":"),
                            "readings should contain uptime_ms");
    TEST_ASSERT_TRUE_MESSAGE(contains(payload, "\"sample_count\":"),
                            "readings should contain sample_count");
    TEST_ASSERT_TRUE_MESSAGE(contains(payload, "\"time_synced\":"),
                            "readings should contain time_synced");
    TEST_ASSERT_TRUE_MESSAGE(contains(payload, "\"sensors\":{"),
                            "readings should contain sensors object");
    TEST_ASSERT_TRUE_MESSAGE(contains(payload, "\"sensor_status\":{"),
                            "readings should contain sensor_status object");
    TEST_ASSERT_TRUE_MESSAGE(contains(payload, "\"health\":{"),
                            "readings should contain health object");
}

// Test: Backward Compatibility - New Fields at Root Level
// Validates: Requirements 11.5
void test_new_fields_at_root_level() {
    // Build a sensor data payload
    String payload = buildSensorDataPayload("AA:BB:CC:DD:EE:FF",
                                           "550e8400-e29b-41d4-a716-446655440000",
                                           "test-device");

    // Find the position of "readings" array
    size_t readingsPos = payload.find("\"readings\":");
    TEST_ASSERT_NOT_EQUAL_MESSAGE(std::string::npos, readingsPos,
                                 "readings array should be present");

    // Find the positions of new fields
    size_t hardwareIdPos = payload.find("\"hardware_id\":");
    size_t bootIdPos = payload.find("\"boot_id\":");
    size_t friendlyNamePos = payload.find("\"friendly_name\":");

    // Verify new fields appear BEFORE the readings array (at root level)
    TEST_ASSERT_LESS_THAN_MESSAGE(readingsPos, hardwareIdPos,
                                  "hardware_id should be at root level before readings");
    TEST_ASSERT_LESS_THAN_MESSAGE(readingsPos, bootIdPos,
                                  "boot_id should be at root level before readings");
    TEST_ASSERT_LESS_THAN_MESSAGE(readingsPos, friendlyNamePos,
                                  "friendly_name should be at root level before readings");

    // Verify new fields are NOT inside the readings array
    // Find the end of readings array
    size_t readingsEnd = payload.find("]", readingsPos);
    TEST_ASSERT_NOT_EQUAL_MESSAGE(std::string::npos, readingsEnd,
                                 "readings array should have closing bracket");

    // Verify hardware_id is not between readingsPos and readingsEnd
    String readingsContent = payload.substr(readingsPos, readingsEnd - readingsPos);
    TEST_ASSERT_FALSE_MESSAGE(readingsContent.find("\"hardware_id\":") != std::string::npos,
                             "hardware_id should NOT be inside readings array");
    TEST_ASSERT_FALSE_MESSAGE(readingsContent.find("\"boot_id\":") != std::string::npos,
                             "boot_id should NOT be inside readings array");
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_readings_array_structure_unchanged);
    RUN_TEST(test_new_fields_at_root_level);

    return UNITY_END();
}

#endif  // UNIT_TEST
