#ifdef UNIT_TEST

#include <unity.h>

#include "../include/BootId.h"
#include "../include/ConfigManager.h"

ConfigManager* configManager;

void setUp(void) {
    configManager = new ConfigManager();
}

void tearDown(void) {
    delete configManager;
}

/**
 * Test valid UUID v4 acceptance
 * **Validates: Requirements 2.3, 6.3**
 */
void test_valid_uuid_v4_accepted(void) {
    // Generate a valid UUID v4
    String validUuid = BootId::generate();

    // Verify it's recognized as valid
    TEST_ASSERT_TRUE(BootId::isValidUuid(validUuid));
}

/**
 * Test invalid format rejection - wrong length
 * **Validates: Requirements 2.3, 6.3**
 */
void test_invalid_uuid_wrong_length_rejected(void) {
    // Too short
    String tooShort = "550e8400-e29b-41d4-a716";
    TEST_ASSERT_FALSE(BootId::isValidUuid(tooShort));

    // Too long
    String tooLong = "550e8400-e29b-41d4-a716-446655440000-extra";
    TEST_ASSERT_FALSE(BootId::isValidUuid(tooLong));
}

/**
 * Test invalid format rejection - missing hyphens
 * **Validates: Requirements 2.3, 6.3**
 */
void test_invalid_uuid_missing_hyphens_rejected(void) {
    // No hyphens
    String noHyphens = "550e8400e29b41d4a716446655440000";
    TEST_ASSERT_FALSE(BootId::isValidUuid(noHyphens));

    // Wrong hyphen positions
    String wrongHyphens = "550e8400e-29b-41d4-a716-446655440000";
    TEST_ASSERT_FALSE(BootId::isValidUuid(wrongHyphens));
}

/**
 * Test invalid format rejection - wrong version
 * **Validates: Requirements 2.3, 6.3**
 */
void test_invalid_uuid_wrong_version_rejected(void) {
    // Version 3 (should be 4)
    String version3 = "550e8400-e29b-31d4-a716-446655440000";
    TEST_ASSERT_FALSE(BootId::isValidUuid(version3));

    // Version 5 (should be 4)
    String version5 = "550e8400-e29b-51d4-a716-446655440000";
    TEST_ASSERT_FALSE(BootId::isValidUuid(version5));
}

/**
 * Test invalid format rejection - wrong variant
 * **Validates: Requirements 2.3, 6.3**
 */
void test_invalid_uuid_wrong_variant_rejected(void) {
    // Variant bits should be 8, 9, A, or B
    // Test with invalid variant (0-7, C-F)
    String invalidVariant1 = "550e8400-e29b-41d4-0716-446655440000";  // 0
    TEST_ASSERT_FALSE(BootId::isValidUuid(invalidVariant1));

    String invalidVariant2 = "550e8400-e29b-41d4-c716-446655440000";  // C
    TEST_ASSERT_FALSE(BootId::isValidUuid(invalidVariant2));

    String invalidVariant3 = "550e8400-e29b-41d4-f716-446655440000";  // F
    TEST_ASSERT_FALSE(BootId::isValidUuid(invalidVariant3));
}

/**
 * Test empty string handling
 * **Validates: Requirements 2.3, 6.3**
 */
void test_empty_string_rejected(void) {
    String empty = "";
    TEST_ASSERT_FALSE(BootId::isValidUuid(empty));
}

/**
 * Test invalid characters
 * **Validates: Requirements 2.3, 6.3**
 */
void test_invalid_characters_rejected(void) {
    // Non-hex characters
    String invalidChars = "550e8400-e29b-41d4-a716-44665544000g";
    TEST_ASSERT_FALSE(BootId::isValidUuid(invalidChars));

    // Special characters
    String specialChars = "550e8400-e29b-41d4-a716-44665544000!";
    TEST_ASSERT_FALSE(BootId::isValidUuid(specialChars));
}

/**
 * Test valid variants (8, 9, A, B)
 * **Validates: Requirements 2.3, 6.3**
 */
void test_valid_variants_accepted(void) {
    // Test all valid variant values
    String variant8 = "550e8400-e29b-41d4-8716-446655440000";
    TEST_ASSERT_TRUE(BootId::isValidUuid(variant8));

    String variant9 = "550e8400-e29b-41d4-9716-446655440000";
    TEST_ASSERT_TRUE(BootId::isValidUuid(variant9));

    String variantA = "550e8400-e29b-41d4-A716-446655440000";
    TEST_ASSERT_TRUE(BootId::isValidUuid(variantA));

    String variantB = "550e8400-e29b-41d4-B716-446655440000";
    TEST_ASSERT_TRUE(BootId::isValidUuid(variantB));

    // Test lowercase variants
    String variantALower = "550e8400-e29b-41d4-a716-446655440000";
    TEST_ASSERT_TRUE(BootId::isValidUuid(variantALower));

    String variantBLower = "550e8400-e29b-41d4-b716-446655440000";
    TEST_ASSERT_TRUE(BootId::isValidUuid(variantBLower));
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Valid UUID tests
    RUN_TEST(test_valid_uuid_v4_accepted);
    RUN_TEST(test_valid_variants_accepted);

    // Invalid format tests
    RUN_TEST(test_invalid_uuid_wrong_length_rejected);
    RUN_TEST(test_invalid_uuid_missing_hyphens_rejected);
    RUN_TEST(test_invalid_uuid_wrong_version_rejected);
    RUN_TEST(test_invalid_uuid_wrong_variant_rejected);
    RUN_TEST(test_invalid_characters_rejected);
    RUN_TEST(test_empty_string_rejected);

    return UNITY_END();
}

#endif  // UNIT_TEST
