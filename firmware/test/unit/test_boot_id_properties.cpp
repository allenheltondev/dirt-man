#ifdef UNIT_TEST

#include <unity.h>

#include "../../src/BootId.cpp"
#include <cstdlib>
#include <ctime>

void setUp(void) {
    // Seed random number generator with time + additional entropy
    srand(time(NULL) + rand());
}

void tearDown(void) {
    // Nothing to tear down
}

/**
 * Property 7: Boot ID Format Compliance
 * **Validates: Requirements 3.6**
 *
 * For any generated Boot_ID, it should match the UUID v4 format:
 * 8-4-4-4-12 hexadecimal characters separated by hyphens, with the version
 * bits set to 4 in the third group and variant bits set to 8, 9, A, or B
 * in the fourth group.
 *
 * **Feature: device-registration, Property 7: Boot ID Format Compliance**
 */
void test_property7_boot_id_format_compliance(void) {
    const int ITERATIONS = 100;
    int successCount = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        // Generate Boot ID
        String bootId = BootId::generate();

        // Property 7a: Must be exactly 36 characters (8-4-4-4-12 with hyphens)
        if (bootId.length() != 36) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Iteration %d: Expected length 36, got %zu", i,
                     bootId.length());
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Property 7b: Hyphens must be at positions 8, 13, 18, 23
        int expectedHyphenPositions[] = {8, 13, 18, 23};
        for (int pos = 0; pos < 4; pos++) {
            int expectedPos = expectedHyphenPositions[pos];
            if (bootId[expectedPos] != '-') {
                char msg[128];
                snprintf(msg, sizeof(msg), "Iteration %d: Expected hyphen at position %d, got '%c'",
                         i, expectedPos, bootId[expectedPos]);
                TEST_FAIL_MESSAGE(msg);
                return;
            }
        }

        // Property 7c: All non-hyphen characters must be hex digits
        for (size_t pos = 0; pos < bootId.length(); pos++) {
            if (pos == 8 || pos == 13 || pos == 18 || pos == 23) {
                continue;  // Skip hyphens
            }

            char c = bootId[pos];
            bool isHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');

            if (!isHex) {
                char msg[128];
                snprintf(msg, sizeof(msg),
                         "Iteration %d: Invalid character '%c' at position %zu (expected hex)", i, c,
                         pos);
                TEST_FAIL_MESSAGE(msg);
                return;
            }
        }

        // Property 7d: Version bits must be 4 (position 14, first char of third group)
        if (bootId[14] != '4') {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "Iteration %d: Expected version bit '4' at position 14, got '%c'", i,
                     bootId[14]);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Property 7e: Variant bits must be 8, 9, A, or B (position 19, first char of fourth group)
        char variantChar = bootId[19];
        bool validVariant = (variantChar == '8' || variantChar == '9' || variantChar == 'A' ||
                             variantChar == 'a' || variantChar == 'B' || variantChar == 'b');

        if (!validVariant) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                     "Iteration %d: Expected variant bit 8/9/A/B at position 19, got '%c'", i,
                     variantChar);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        successCount++;
    }

    // All iterations should succeed
    TEST_ASSERT_EQUAL_INT_MESSAGE(ITERATIONS, successCount,
                                  "All Boot ID format compliance iterations should succeed");
}

/**
 * Additional property test: Verify isValidUuid correctly validates generated UUIDs
 */
void test_property_boot_id_validation_consistency(void) {
    const int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; i++) {
        // Generate Boot ID
        String bootId = BootId::generate();

        // Property: All generated Boot IDs should pass validation
        if (!BootId::isValidUuid(bootId)) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Iteration %d: Generated Boot ID '%s' failed validation", i,
                     bootId.c_str());
            TEST_FAIL_MESSAGE(msg);
            return;
        }
    }
}

/**
 * Additional property test: Verify group lengths are correct (8-4-4-4-12)
 */
void test_property_boot_id_group_lengths(void) {
    const int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; i++) {
        String bootId = BootId::generate();

        // Extract groups
        String group1 = bootId.substr(0, 8);    // 8 chars
        String group2 = bootId.substr(9, 4);    // 4 chars
        String group3 = bootId.substr(14, 4);   // 4 chars
        String group4 = bootId.substr(19, 4);   // 4 chars
        String group5 = bootId.substr(24, 12);  // 12 chars

        // Verify lengths
        if (group1.length() != 8) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Iteration %d: Group 1 should be 8 chars, got %zu", i,
                     group1.length());
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        if (group2.length() != 4) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Iteration %d: Group 2 should be 4 chars, got %zu", i,
                     group2.length());
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        if (group3.length() != 4) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Iteration %d: Group 3 should be 4 chars, got %zu", i,
                     group3.length());
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        if (group4.length() != 4) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Iteration %d: Group 4 should be 4 chars, got %zu", i,
                     group4.length());
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        if (group5.length() != 12) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Iteration %d: Group 5 should be 12 chars, got %zu", i,
                     group5.length());
            TEST_FAIL_MESSAGE(msg);
            return;
        }
    }
}

/**
 * Unit test: Verify isValidUuid rejects invalid formats
 */
void test_unit_invalid_uuid_rejection(void) {
    // Test various invalid formats
    TEST_ASSERT_FALSE_MESSAGE(BootId::isValidUuid(""), "Empty string should be invalid");

    TEST_ASSERT_FALSE_MESSAGE(BootId::isValidUuid("not-a-uuid"), "Random string should be invalid");

    TEST_ASSERT_FALSE_MESSAGE(BootId::isValidUuid("12345678-1234-1234-1234-123456789012"),
                              "Wrong version should be invalid");

    TEST_ASSERT_FALSE_MESSAGE(BootId::isValidUuid("12345678-1234-4234-7234-123456789012"),
                              "Wrong variant should be invalid");

    TEST_ASSERT_FALSE_MESSAGE(BootId::isValidUuid("12345678-1234-4234-8234-12345678901"),
                              "Too short should be invalid");

    TEST_ASSERT_FALSE_MESSAGE(BootId::isValidUuid("12345678-1234-4234-8234-1234567890123"),
                              "Too long should be invalid");

    TEST_ASSERT_FALSE_MESSAGE(BootId::isValidUuid("12345678_1234_4234_8234_123456789012"),
                              "Wrong separator should be invalid");

    TEST_ASSERT_FALSE_MESSAGE(BootId::isValidUuid("1234567G-1234-4234-8234-123456789012"),
                              "Invalid hex character should be invalid");
}

/**
 * Unit test: Verify isValidUuid accepts valid UUID v4 formats
 */
void test_unit_valid_uuid_acceptance(void) {
    // Test valid UUID v4 formats with different variant bits
    TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid("12345678-1234-4234-8234-123456789012"),
                             "Valid UUID with variant 8 should be accepted");

    TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid("12345678-1234-4234-9234-123456789012"),
                             "Valid UUID with variant 9 should be accepted");

    TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid("12345678-1234-4234-A234-123456789012"),
                             "Valid UUID with variant A should be accepted");

    TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid("12345678-1234-4234-B234-123456789012"),
                             "Valid UUID with variant B should be accepted");

    TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid("12345678-1234-4234-a234-123456789012"),
                             "Valid UUID with lowercase variant a should be accepted");

    TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid("12345678-1234-4234-b234-123456789012"),
                             "Valid UUID with lowercase variant b should be accepted");

    TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid("ABCDEF01-2345-4678-9ABC-DEF012345678"),
                             "Valid UUID with uppercase hex should be accepted");

    TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid("abcdef01-2345-4678-9abc-def012345678"),
                             "Valid UUID with lowercase hex should be accepted");
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Property 7: Boot ID Format Compliance
    RUN_TEST(test_property7_boot_id_format_compliance);

    // Additional property tests
    RUN_TEST(test_property_boot_id_validation_consistency);
    RUN_TEST(test_property_boot_id_group_lengths);

    // Unit tests for validation
    RUN_TEST(test_unit_invalid_uuid_rejection);
    RUN_TEST(test_unit_valid_uuid_acceptance);

    return UNITY_END();
}

#endif  // UNIT_TEST
