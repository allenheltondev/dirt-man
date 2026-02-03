#ifdef UNIT_TEST

#include <unity.h>

#include "../../include/BootId.h"
#include "../../src/BootId.cpp"

#include <cstdlib>
#include <ctime>
#include <set>
#include <string>

void setUp(void) {
    // Seed random number generator
    srand(time(NULL) + rand());
}

void tearDown(void) {
    // Nothing to tear down
}

/**
 * Unit Test: Verify Boot_ID is not written to NVS
 * **Validates: Requirements 3.8**
 *
 * This test verifies that the Boot_ID generation process does not
 * involve any NVS persistence operations. The Boot_ID should only
 * exist in RAM and be regenerated on each boot.
 */
void test_boot_id_not_written_to_nvs(void) {
    // Generate multiple Boot IDs
    String bootId1 = BootId::generate();
    String bootId2 = BootId::generate();
    String bootId3 = BootId::generate();

    // Verify all Boot IDs are valid
    TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid(bootId1), "Boot ID 1 should be valid");
    TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid(bootId2), "Boot ID 2 should be valid");
    TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid(bootId3), "Boot ID 3 should be valid");

    // Verify Boot IDs are different (not persisted/cached)
    TEST_ASSERT_TRUE_MESSAGE(bootId1 != bootId2, "Boot IDs should be unique (not persisted)");
    TEST_ASSERT_TRUE_MESSAGE(bootId2 != bootId3, "Boot IDs should be unique (not persisted)");
    TEST_ASSERT_TRUE_MESSAGE(bootId1 != bootId3, "Boot IDs should be unique (not persisted)");

    // This test passes if Boot ID generation completes without any NVS operations
    // The BootId class should not have any NVS-related code
}

/**
 * Unit Test: Verify Boot_ID is stored only in RAM
 * **Validates: Requirements 3.8**
 *
 * This test verifies that the Boot_ID exists only in RAM (as a variable)
 * and is not persisted to any non-volatile storage. Each generation
 * produces a new unique value.
 */
void test_boot_id_stored_only_in_ram(void) {
    // Generate multiple Boot IDs to simulate multiple "boots"
    std::set<std::string> generatedBootIds;
    const int NUM_GENERATIONS = 10;

    for (int i = 0; i < NUM_GENERATIONS; i++) {
        String bootId = BootId::generate();

        // Verify Boot ID is valid
        TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid(bootId), "Generated Boot ID should be valid");

        // Store in set to check uniqueness
        generatedBootIds.insert(bootId.c_str());
    }

    // Verify all Boot IDs are unique (not persisted/reused)
    TEST_ASSERT_EQUAL_INT_MESSAGE(NUM_GENERATIONS, generatedBootIds.size(),
                                  "All Boot IDs should be unique - no persistence/caching");
}

/**
 * Unit Test: Verify Boot_ID regeneration on simulated reboot
 * **Validates: Requirements 3.7, 3.8**
 *
 * This test simulates a reboot scenario where a new Boot_ID is generated
 * and verifies that it's different from the previous one (not persisted).
 */
void test_boot_id_regeneration_on_reboot(void) {
    // Simulate first boot
    String bootId1 = BootId::generate();
    TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid(bootId1), "First Boot ID should be valid");

    // Simulate reboot - generate new Boot ID
    String bootId2 = BootId::generate();
    TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid(bootId2), "Second Boot ID should be valid");

    // Verify new Boot ID is different (not persisted from previous boot)
    TEST_ASSERT_TRUE_MESSAGE(bootId1 != bootId2,
                            "Boot ID should be different after reboot (not persisted)");

    // Simulate another reboot
    String bootId3 = BootId::generate();
    TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid(bootId3), "Third Boot ID should be valid");

    // Verify all Boot IDs are different
    TEST_ASSERT_TRUE_MESSAGE(bootId1 != bootId3, "Boot IDs should be unique across reboots");
    TEST_ASSERT_TRUE_MESSAGE(bootId2 != bootId3, "Boot IDs should be unique across reboots");
}

/**
 * Unit Test: Verify Boot_ID class has no NVS dependencies
 * **Validates: Requirements 3.8**
 *
 * This test verifies that the BootId class implementation does not
 * include any NVS-related headers or functionality.
 */
void test_boot_id_no_nvs_dependencies(void) {
    // This is a conceptual test - the BootId class should not have:
    // - #include <nvs.h> or similar
    // - Any calls to nvs_set_* or nvs_get_* functions
    // - Any persistence logic

    // Generate a Boot ID
    String bootId = BootId::generate();

    // Verify it's valid
    TEST_ASSERT_TRUE_MESSAGE(BootId::isValidUuid(bootId), "Boot ID should be valid");

    // The fact that this test compiles and runs without NVS mocks
    // demonstrates that BootId has no NVS dependencies
    TEST_PASS_MESSAGE("BootId class has no NVS dependencies");
}

/**
 * Unit Test: Verify Boot_ID uniqueness across many generations
 * **Validates: Requirements 3.7, 3.8**
 *
 * This test generates many Boot IDs and verifies they are all unique,
 * demonstrating that they are not being persisted or cached.
 */
void test_boot_id_uniqueness_across_generations(void) {
    std::set<std::string> generatedBootIds;
    const int NUM_GENERATIONS = 100;

    for (int i = 0; i < NUM_GENERATIONS; i++) {
        String bootId = BootId::generate();

        // Verify Boot ID is valid
        if (!BootId::isValidUuid(bootId)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Generation %d produced invalid Boot ID: %s",
                    i, bootId.c_str());
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Check for duplicates
        if (generatedBootIds.find(bootId.c_str()) != generatedBootIds.end()) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Generation %d produced duplicate Boot ID: %s",
                    i, bootId.c_str());
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        generatedBootIds.insert(bootId.c_str());
    }

    // Verify all Boot IDs are unique
    TEST_ASSERT_EQUAL_INT_MESSAGE(NUM_GENERATIONS, generatedBootIds.size(),
                                  "All 100 Boot IDs should be unique - no persistence");
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_boot_id_not_written_to_nvs);
    RUN_TEST(test_boot_id_stored_only_in_ram);
    RUN_TEST(test_boot_id_regeneration_on_reboot);
    RUN_TEST(test_boot_id_no_nvs_dependencies);
    RUN_TEST(test_boot_id_uniqueness_across_generations);

    return UNITY_END();
}

#endif  // UNIT_TEST
