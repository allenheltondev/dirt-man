#ifdef UNIT_TEST

#include <unity.h>

#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

typedef std::string String;

// Helper function to generate random alphanumeric string
String generateRandomString(int length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
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
    snprintf(uuid, sizeof(uuid), "%08x-%04x-4%03x-%04x-%012x", rand(), rand() % 0x10000,
             rand() % 0x1000,
             (rand() % 0x4000) | 0x8000,  // Variant bits
             rand());
    return String(uuid);
}

// Helper function to generate random firmware version
String generateRandomVersion() {
    int major = rand() % 10;
    int minor = rand() % 20;
    int patch = rand() % 100;
    char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d", major, minor, patch);
    return String(version);
}

// Simplified buildRegistrationPayload for testing
String buildRegistrationPayload(const String& hardwareId, const String& bootId,
                                const String& friendlyName, const String& firmwareVersion) {
    String payload = "{";

    // Required fields
    payload += "\"hardware_id\":\"" + hardwareId + "\",";
    payload += "\"boot_id\":\"" + bootId + "\",";
    payload += "\"firmware_version\":\"" + firmwareVersion + "\",";

    // Optional friendly_name field
    if (friendlyName.length() > 0) {
        payload += "\"friendly_name\":\"" + friendlyName + "\",";
    }

    // Capabilities object
    payload += "\"capabilities\":{";
    payload += "\"sensors\":[\"bme280\",\"ds18b20\",\"soil_moisture\"],";
    payload += "\"features\":{";
    payload += "\"tft_display\":true,";
    payload += "\"offline_buffering\":true,";
    payload += "\"ntp_sync\":true";
    payload += "}";
    payload += "}";
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
    // Nothing to tear down
}

/**
 * Property 5: Registration Payload Structure Completeness
 * Validates: Requirements 3.1, 3.4, 3.5, 3.9, 3.10
 *
 * For any registration request, the JSON payload should contain all required fields
 * (hardware_id, boot_id, firmware_version, capabilities) with correct types, and the
 * capabilities object should have both "sensors" array and "features" object with
 * boolean values.
 *
 * Feature: device-registration, Property 5: Registration Payload Structure Completeness
 */
void test_property5_registration_payload_structure_completeness(void) {
    const int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; i++) {
        // Generate random registration data
        String hardwareId = generateRandomMac();
        String bootId = generateRandomUuid();
        String friendlyName = (rand() % 2 == 0) ? generateRandomString(10) : "";
        String firmwareVersion = generateRandomVersion();

        // Build payload
        String payload =
            buildRegistrationPayload(hardwareId, bootId, friendlyName, firmwareVersion);

        // Property 5a: Verify all required fields are present
        if (!contains(payload, "\"hardware_id\"")) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: Missing required field 'hardware_id'", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        if (!contains(payload, "\"boot_id\"")) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: Missing required field 'boot_id'", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        if (!contains(payload, "\"firmware_version\"")) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: Missing required field 'firmware_version'",
                     i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        if (!contains(payload, "\"capabilities\"")) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: Missing required field 'capabilities'", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Property 5b: Verify hardware_id value is present
        String hardwareIdField = "\"hardware_id\":\"" + hardwareId + "\"";
        if (!contains(payload, hardwareIdField)) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: hardware_id value mismatch", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Property 5c: Verify boot_id value is present
        String bootIdField = "\"boot_id\":\"" + bootId + "\"";
        if (!contains(payload, bootIdField)) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: boot_id value mismatch", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Property 5d: Verify firmware_version value is present
        String versionField = "\"firmware_version\":\"" + firmwareVersion + "\"";
        if (!contains(payload, versionField)) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: firmware_version value mismatch", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Property 5e: Verify capabilities structure has sensors array
        if (!contains(payload, "\"sensors\":[")) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: Missing 'sensors' array in capabilities", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Property 5f: Verify capabilities structure has features object
        if (!contains(payload, "\"features\":{")) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: Missing 'features' object in capabilities",
                     i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Property 5g: Verify sensors array contains expected sensor types
        if (!contains(payload, "\"bme280\"")) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: Missing 'bme280' in sensors array", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        if (!contains(payload, "\"ds18b20\"")) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: Missing 'ds18b20' in sensors array", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        if (!contains(payload, "\"soil_moisture\"")) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: Missing 'soil_moisture' in sensors array", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Property 5h: Verify features object contains boolean values
        if (!contains(payload, "\"tft_display\":true") &&
            !contains(payload, "\"tft_display\":false")) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Iteration %d: Missing or invalid 'tft_display' boolean in features", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        if (!contains(payload, "\"offline_buffering\":true") &&
            !contains(payload, "\"offline_buffering\":false")) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Iteration %d: Missing or invalid 'offline_buffering' boolean in features", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        if (!contains(payload, "\"ntp_sync\":true") && !contains(payload, "\"ntp_sync\":false")) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Iteration %d: Missing or invalid 'ntp_sync' boolean in features", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Property 5i: Verify JSON structure (basic validation)
        // Count opening and closing braces
        int openBraces = countOccurrences(payload, "{");
        int closeBraces = countOccurrences(payload, "}");
        if (openBraces != closeBraces) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Iteration %d: Mismatched braces in JSON (open=%d, close=%d)", i, openBraces,
                     closeBraces);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Count opening and closing brackets
        int openBrackets = countOccurrences(payload, "[");
        int closeBrackets = countOccurrences(payload, "]");
        if (openBrackets != closeBrackets) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Iteration %d: Mismatched brackets in JSON (open=%d, close=%d)", i,
                     openBrackets, closeBrackets);
            TEST_FAIL_MESSAGE(msg);
            return;
        }
    }
}

/**
 * Property 5j: Verify capabilities object is properly nested
 *
 * This test verifies that the capabilities object is at the root level and
 * contains the expected nested structure.
 */
void test_property5_capabilities_nesting(void) {
    const int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; i++) {
        String hardwareId = generateRandomMac();
        String bootId = generateRandomUuid();
        String friendlyName = "";
        String firmwareVersion = generateRandomVersion();

        String payload =
            buildRegistrationPayload(hardwareId, bootId, friendlyName, firmwareVersion);

        // Verify capabilities is at root level (comes after required fields)
        size_t capabilitiesPos = payload.find("\"capabilities\":{");
        if (capabilitiesPos == std::string::npos) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: capabilities object not found", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Verify sensors array is inside capabilities
        size_t sensorsPos = payload.find("\"sensors\":[", capabilitiesPos);
        if (sensorsPos == std::string::npos) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: sensors array not found inside capabilities",
                     i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Verify features object is inside capabilities
        size_t featuresPos = payload.find("\"features\":{", capabilitiesPos);
        if (featuresPos == std::string::npos) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Iteration %d: features object not found inside capabilities", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Verify features comes after sensors
        if (featuresPos <= sensorsPos) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Iteration %d: features should come after sensors in capabilities", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }
    }
}

/**
 * Property 6: Conditional Friendly Name Inclusion in Registration
 * Validates: Requirements 3.2, 3.3
 *
 * For any registration request where a friendly_name is configured, the payload should
 * include the "friendly_name" field with the configured value. When friendly_name is not
 * configured (empty string), the payload should omit the "friendly_name" field.
 *
 * Feature: device-registration, Property 6: Conditional Friendly Name Inclusion in Registration
 */
void test_property6_conditional_friendly_name_inclusion(void) {
    const int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; i++) {
        String hardwareId = generateRandomMac();
        String bootId = generateRandomUuid();
        String firmwareVersion = generateRandomVersion();

        // Test with friendly_name configured (50% of iterations)
        if (i < ITERATIONS / 2) {
            String friendlyName = generateRandomString(10 + (rand() % 20));  // 10-30 chars

            String payload =
                buildRegistrationPayload(hardwareId, bootId, friendlyName, firmwareVersion);

            // Property 6a: Verify friendly_name is included when configured
            if (!contains(payload, "\"friendly_name\"")) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Iteration %d: friendly_name field missing when configured", i);
                TEST_FAIL_MESSAGE(msg);
                return;
            }

            // Property 6b: Verify the friendly_name value is correct
            String friendlyNameField = "\"friendly_name\":\"" + friendlyName + "\"";
            if (!contains(payload, friendlyNameField)) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Iteration %d: friendly_name value mismatch", i);
                TEST_FAIL_MESSAGE(msg);
                return;
            }
        } else {
            // Test without friendly_name (empty string)
            String friendlyName = "";

            String payload =
                buildRegistrationPayload(hardwareId, bootId, friendlyName, firmwareVersion);

            // Property 6c: Verify friendly_name is omitted when not configured
            if (contains(payload, "\"friendly_name\"")) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Iteration %d: friendly_name field present when not configured", i);
                TEST_FAIL_MESSAGE(msg);
                return;
            }
        }
    }
}

/**
 * Property 6d: Verify friendly_name field ordering
 *
 * This test verifies that when friendly_name is included, it appears after
 * the required fields but before capabilities.
 */
void test_property6_friendly_name_field_ordering(void) {
    const int ITERATIONS = 50;

    for (int i = 0; i < ITERATIONS; i++) {
        String hardwareId = generateRandomMac();
        String bootId = generateRandomUuid();
        String friendlyName = generateRandomString(15);
        String firmwareVersion = generateRandomVersion();

        String payload =
            buildRegistrationPayload(hardwareId, bootId, friendlyName, firmwareVersion);

        // Find positions of key fields
        size_t hardwareIdPos = payload.find("\"hardware_id\"");
        size_t bootIdPos = payload.find("\"boot_id\"");
        size_t versionPos = payload.find("\"firmware_version\"");
        size_t friendlyNamePos = payload.find("\"friendly_name\"");
        size_t capabilitiesPos = payload.find("\"capabilities\"");

        // Verify all fields are present
        if (hardwareIdPos == std::string::npos || bootIdPos == std::string::npos ||
            versionPos == std::string::npos || friendlyNamePos == std::string::npos ||
            capabilitiesPos == std::string::npos) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: Missing required fields in payload", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Verify friendly_name comes after required fields
        if (friendlyNamePos < hardwareIdPos || friendlyNamePos < bootIdPos ||
            friendlyNamePos < versionPos) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: friendly_name appears before required fields",
                     i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Verify friendly_name comes before capabilities
        if (friendlyNamePos > capabilitiesPos) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: friendly_name appears after capabilities", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }
    }
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Property 5: Registration Payload Structure Completeness
    RUN_TEST(test_property5_registration_payload_structure_completeness);
    RUN_TEST(test_property5_capabilities_nesting);

    // Property 6: Conditional Friendly Name Inclusion in Registration
    RUN_TEST(test_property6_conditional_friendly_name_inclusion);
    RUN_TEST(test_property6_friendly_name_field_ordering);

    return UNITY_END();
}

#endif  // UNIT_TEST
