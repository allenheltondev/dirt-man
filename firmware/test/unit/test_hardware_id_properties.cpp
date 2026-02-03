#ifdef UNIT_TEST

#include <unity.h>

#include "../../src/HardwareId.cpp"
#include <cstdlib>
#include <ctime>

// Helper function to generate random byte
uint8_t randomByte() {
    return rand() % 256;
}

// Helper function to check if character is uppercase hex
bool isUppercaseHex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
}

void setUp(void) {
    // Seed random number generator with time + additional entropy
    srand(time(NULL) + rand());
}

void tearDown(void) {
    // Nothing to tear down
}

/**
 * Property 1: MAC Address Formatting Consistency
 * **Validates: Requirements 1.2**
 *
 * For any 6-byte MAC address array, when formatted by the HardwareId module,
 * the resulting string should be exactly 17 characters long, contain exactly
 * 5 colons at positions 2, 5, 8, 11, and 14, and contain only uppercase
 * hexadecimal characters (0-9, A-F) and colons.
 *
 * **Feature: device-registration, Property 1: MAC Address Formatting Consistency**
 */
void test_property1_mac_address_formatting_consistency(void) {
    const int ITERATIONS = 100;
    int successCount = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        // Generate random 6-byte MAC address
        uint8_t mac[6];
        for (int j = 0; j < 6; j++) {
            mac[j] = randomByte();
        }

        // Format MAC address
        String formatted = HardwareId::formatMac(mac);

        // Property 1a: Formatted string must be exactly 17 characters
        if (formatted.length() != 17) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Iteration %d: Expected length 17, got %zu", i,
                     formatted.length());
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Property 1b: Must contain exactly 5 colons at correct positions (2, 5, 8, 11, 14)
        int colonCount = 0;
        int expectedColonPositions[] = {2, 5, 8, 11, 14};
        for (int pos = 0; pos < 5; pos++) {
            int expectedPos = expectedColonPositions[pos];
            if (formatted[expectedPos] == ':') {
                colonCount++;
            } else {
                char msg[128];
                snprintf(msg, sizeof(msg), "Iteration %d: Expected colon at position %d", i,
                         expectedPos);
                TEST_FAIL_MESSAGE(msg);
                return;
            }
        }

        if (colonCount != 5) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Iteration %d: Expected 5 colons, got %d", i, colonCount);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Property 1c: All non-colon characters must be uppercase hex (0-9, A-F)
        for (size_t pos = 0; pos < formatted.length(); pos++) {
            char c = formatted[pos];
            if (c != ':' && !isUppercaseHex(c)) {
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "Iteration %d: Invalid character '%c' at position %zu", i, c, pos);
                TEST_FAIL_MESSAGE(msg);
                return;
            }
        }

        successCount++;
    }

    // All iterations should succeed
    TEST_ASSERT_EQUAL_INT_MESSAGE(ITERATIONS, successCount,
                                  "All MAC formatting iterations should succeed");
}

/**
 * Additional property test: Verify colon positions are exactly at 2, 5, 8, 11, 14
 * and all other positions contain hex digits
 */
void test_property_mac_format_structure(void) {
    const int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; i++) {
        uint8_t mac[6];
        for (int j = 0; j < 6; j++) {
            mac[j] = randomByte();
        }

        String formatted = HardwareId::formatMac(mac);

        // Check each position
        for (size_t pos = 0; pos < 17; pos++) {
            char c = formatted[pos];

            if (pos == 2 || pos == 5 || pos == 8 || pos == 11 || pos == 14) {
                // Should be colon
                if (c != ':') {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Iteration %d: Expected ':' at position %zu, got '%c'",
                             i, pos, c);
                    TEST_FAIL_MESSAGE(msg);
                    return;
                }
            } else {
                // Should be uppercase hex digit
                if (!isUppercaseHex(c)) {
                    char msg[128];
                    snprintf(msg, sizeof(msg),
                             "Iteration %d: Expected hex digit at position %zu, got '%c'", i, pos, c);
                    TEST_FAIL_MESSAGE(msg);
                    return;
                }
            }
        }
    }
}

/**
 * Additional property test: Verify formatted MAC matches expected format for known values
 */
void test_property_mac_format_correctness(void) {
    const int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; i++) {
        uint8_t mac[6];
        for (int j = 0; j < 6; j++) {
            mac[j] = randomByte();
        }

        String formatted = HardwareId::formatMac(mac);

        // Manually construct expected format
        char expected[18];
        snprintf(expected, sizeof(expected), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
                 mac[3], mac[4], mac[5]);

        // Compare
        if (formatted != expected) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: Expected '%s', got '%s'", i, expected,
                     formatted.c_str());
            TEST_FAIL_MESSAGE(msg);
            return;
        }
    }
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Property 1: MAC Address Formatting Consistency
    RUN_TEST(test_property1_mac_address_formatting_consistency);

    // Additional property tests
    RUN_TEST(test_property_mac_format_structure);
    RUN_TEST(test_property_mac_format_correctness);

    return UNITY_END();
}

#endif  // UNIT_TEST
