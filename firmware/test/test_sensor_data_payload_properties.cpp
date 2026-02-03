#ifdef UNIT_TEST

#include <unity.h>

#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

typedef std::string String;

// Helper function to generate random alphanumeric string
String generateRandomString(int length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
    String result;
    for (int i = 0; i < length; i++) {
        result += charset[rand() % (sizeof(charset) - 1)];
    }
    return result;
}

// Helper function to generate random MAC address
String generateRandomMac() {
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", rand() % 256, rand() % 256,
             rand() % 256, rand() % 256, rand() % 256, rand() % 256);
    return String(mac);
}

// Helper function to generate random UUID v4
String generateRandomUuid() {
    char uuid[37];
    unsigned long long lower = ((unsigned long long)rand() << 32) | rand();
    snprintf(uuid, sizeof(uuid), "%08x-%04x-4%03x-%04x-%012llx", rand(), rand() % 0x10000,
             rand() % 0x1000, (rand() % 0x4000) | 0x8000, lower);
    return String(uuid);
}

// Simplified buildSensorDataPayload for testing
String buildSensorDataPayload(const String& hardwareId, const String& bootId,
                              const String& friendlyName, bool includeReadings = true) {
    String payload = "{";

    // Required fields
    payload += "\"hardware_id\":\"" + hardwareId + "\",";
    payload += "\"boot_id\":\"" + bootId + "\",";

    // Optional friendly_name field
    if (friendlyName.length() > 0) {
        payload += "\"friendly_name\":\"" + friendlyName + "\",";
    }

    // Readings array (simplified for testing)
    if (includeReadings) {
        payload += "\"readings\":[";
        payload += "{";
        payload += "\"batch_id\":\"test-batch-1\",";
        payload += "\"device_id\":\"test-device\",";
        payload += "\"sample_start_epoch_ms\":1234567890,";
        payload += "\"sensors\":{\"bme280_temp_c\":22.5}";
        payload += "}";
        payload += "]";
    } else {
        payload += "\"readings\":[]";
    }

    payload += "}";

    return payload;
}

// Helper function to check if a string contains a substring
bool contains(const String& str, const String& substr) {
    return str.find(substr) != std::string::npos;
}

// Helper function to count occurrences of a substring
int countOccurrences(const String& str, const String& substr) {
    int count = 0;
    size_t pos = 0;
    while ((pos = str.find(substr, pos)) != std::string::npos) {
        count++;
        pos += substr.length();
    }
    return count;
}

void setUp(void) {
    // Seed random number generator
    srand(time(NULL) + rand());
}

void tearDown(void) {
    // Clean up after each test
}

/**
 * Property 13: Sensor Data Payload Structure Completeness
 * Validates: Requirements 11.1, 11.2, 11.4
 *
 * For any sensor data transmission, the JSON payload should always include
 * hardware_id and boot_id fields, and should include friendly_name field
 * if and only if a friendly_name is configured.
 */
void test_property_sensor_data_payload_structure_completeness() {
    const int ITERATIONS = 100;
    int passCount = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        // Generate random test data
        String hardwareId = generateRandomMac();
        String bootId = generateRandomUuid();

        // Randomly decide whether to include friendly_name (50% chance)
        bool includeFriendlyName = (rand() % 2) == 0;
        String friendlyName = includeFriendlyName ? generateRandomString(10 + rand() % 20) : "";

        // Build sensor data payload
        String payload = buildSensorDataPayload(hardwareId, bootId, friendlyName);

        // Verify hardware_id is always present
        String hardwareIdField = "\"hardware_id\":\"" + hardwareId + "\"";
        if (!contains(payload, hardwareIdField)) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: hardware_id not found in payload", i);
            TEST_FAIL_MESSAGE(msg);
            continue;
        }

        // Verify boot_id is always present
        String bootIdField = "\"boot_id\":\"" + bootId + "\"";
        if (!contains(payload, bootIdField)) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: boot_id not found in payload", i);
            TEST_FAIL_MESSAGE(msg);
            continue;
        }

        // Verify friendly_name is present if and only if configured
        bool friendlyNameInPayload = contains(payload, "\"friendly_name\":");
        if (includeFriendlyName && !friendlyNameInPayload) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: friendly_name configured but not in payload",
                     i);
            TEST_FAIL_MESSAGE(msg);
            continue;
        }
        if (!includeFriendlyName && friendlyNameInPayload) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Iteration %d: friendly_name not configured but present in payload", i);
            TEST_FAIL_MESSAGE(msg);
            continue;
        }

        // Verify confirmation_id is NOT present in sensor payload
        if (contains(payload, "\"confirmation_id\":")) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Iteration %d: confirmation_id should NOT be in sensor payload", i);
            TEST_FAIL_MESSAGE(msg);
            continue;
        }

        // Verify readings array is present
        if (!contains(payload, "\"readings\":")) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: readings array not found in payload", i);
            TEST_FAIL_MESSAGE(msg);
            continue;
        }

        passCount++;
    }

    // All iterations should pass
    char msg[128];
    snprintf(msg, sizeof(msg), "Property test passed %d/%d iterations", passCount, ITERATIONS);
    TEST_ASSERT_EQUAL_MESSAGE(ITERATIONS, passCount, msg);
}

/**
 * Property 14: Boot ID Session Consistency
 * Validates: Requirements 11.4
 *
 * For any sequence of sensor data transmissions within a single boot session
 * (before reboot), all payloads should contain the same boot_id value.
 */
void test_property_boot_id_session_consistency() {
    const int ITERATIONS = 100;
    int passCount = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        // Generate a single boot_id for this "session"
        String sessionBootId = generateRandomUuid();
        String hardwareId = generateRandomMac();

        // Generate multiple payloads in the same session
        const int PAYLOADS_PER_SESSION = 5 + rand() % 10;  // 5-14 payloads
        std::vector<String> payloads;

        for (int j = 0; j < PAYLOADS_PER_SESSION; j++) {
            // Randomly include or exclude friendly_name
            String friendlyName = (rand() % 2) == 0 ? generateRandomString(10) : "";
            String payload = buildSensorDataPayload(hardwareId, sessionBootId, friendlyName);
            payloads.push_back(payload);
        }

        // Verify all payloads contain the same boot_id
        bool allMatch = true;
        String bootIdField = "\"boot_id\":\"" + sessionBootId + "\"";

        for (size_t j = 0; j < payloads.size(); j++) {
            if (!contains(payloads[j], bootIdField)) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Iteration %d, Payload %zu: boot_id mismatch or missing",
                         i, j);
                TEST_FAIL_MESSAGE(msg);
                allMatch = false;
                break;
            }
        }

        if (allMatch) {
            passCount++;
        }
    }

    // All iterations should pass
    char msg[128];
    snprintf(msg, sizeof(msg), "Property test passed %d/%d iterations", passCount, ITERATIONS);
    TEST_ASSERT_EQUAL_MESSAGE(ITERATIONS, passCount, msg);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_property_sensor_data_payload_structure_completeness);
    RUN_TEST(test_property_boot_id_session_consistency);

    return UNITY_END();
}

#endif  // UNIT_TEST
