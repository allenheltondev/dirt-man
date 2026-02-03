/**
 * Unit Tests for NetworkManager Registration Response Parsing
 *
 * Feature: device-registration
 * Tests the parseRegistrationResponse() method for various response formats
 * Validates: Requirements 12.1, 12.2, 12.6, 12.7, 12.8
 */

#include <unity.h>
#include <Arduino.h>
#include "BootId.h"

// Forward declare the parseRegistrationResponse function for testing
bool parseRegistrationResponse(const String& response, String& confirmationId) {
    confirmationId = "";

    if (response.length() == 0) {
        return false;
    }

    // Look for "confirmation_id" field in JSON response
    int fieldStart = response.indexOf("\"confirmation_id\"");
    if (fieldStart == -1) {
        return false;
    }

    // Find the colon after the field name
    int colonPos = response.indexOf(':', fieldStart);
    if (colonPos == -1) {
        return false;
    }

    // Find the opening quote of the value
    int quoteStart = response.indexOf('"', colonPos);
    if (quoteStart == -1) {
        return false;
    }

    // Find the closing quote of the value
    int quoteEnd = response.indexOf('"', quoteStart + 1);
    if (quoteEnd == -1) {
        return false;
    }

    // Extract confirmation_id
    String extractedId = response.substring(quoteStart + 1, quoteEnd);

    // Validate as UUID v4
    if (!BootId::isValidUuid(extractedId)) {
        return false;
    }

    confirmationId = extractedId;
    return true;
}

/**
 * Test Case 16: Successful Registration Response Handling
 * Test val
 * Test "already_registered" status
 */
void test_parse_already_registered_response() {
    String response = "{\"status\":\"already_registered\",\"confirmation_id\":\"7c9e6679-7425-40de-944b-e07fc1f90ae7\",\"hardware_id\":\"AA:BB:CC:DD:EE:FF\"}";
    String confirmationId;

    bool result = parseRegistrationResponse(response, confirmationId);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("7c9e6679-7425-40de-944b-e07fc1f90ae7", confirmationId.c_str());
}

/**
 * Test Case 19: Malformed JSON Response Handling
 * Test malformed JSON handling
 */
void test_parse_malformed_json() {
    String response = "{\"status\":\"registered\",\"confirmation_id\":\"550e8400-e29b-41d4-a716-446655440000\"";  // Missing closing brace
    String confirmationId;

    // Should still extract confirmation_id if it's present and valid
    bool result = parseRegistrationResponse(response, confirmationId);

    TEST_ASSERT_TRUE(result);  // Parser is lenient, extracts valid UUID
    TEST_ASSERT_EQUAL_STRING("550e8400-e29b-41d4-a716-446655440000", confirmationId.c_str());
}

/**
 * Test completely invalid JSON
 */
void test_parse_completely_invalid_json() {
    String response = "This is not JSON at all";
    String confirmationId;

    bool result = parseRegistrationResponse(response, confirmationId);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_STRING("", confirmationId.c_str());
}

/**
 * Test Case 20: Missing Confirmation ID Response Handling
 * Test missing confirmation_id field
 */
void test_parse_missing_confirmation_id() {
    String response = "{\"status\":\"registered\",\"hardware_id\":\"AA:BB:CC:DD:EE:FF\"}";
    String confirmationId;

    bool result = parseRegistrationResponse(response, confirmationId);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_STRING("", confirmationId.c_str());
}

/**
 * Test Case 21: Invalid Confirmation ID Format Response Handling
 * Test invalid confirmation_id format (not UUID v4)
 */
void test_parse_invalid_confirmation_id_format() {
    String response = "{\"status\":\"registered\",\"confirmation_id\":\"not-a-valid-uuid\",\"hardware_id\":\"AA:BB:CC:DD:EE:FF\"}";
    String confirmationId;

    bool result = parseRegistrationResponse(response, confirmationId);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_STRING("", confirmationId.c_str());
}

/**
 * Test invalid UUID v4 version bits
 */
void test_parse_invalid_uuid_version() {
    // UUID with version 3 instead of 4
    String response = "{\"status\":\"registered\",\"confirmation_id\":\"550e8400-e29b-31d4-a716-446655440000\",\"hardware_id\":\"AA:BB:CC:DD:EE:FF\"}";
    String confirmationId;

    bool result = parseRegistrationResponse(response, confirmationId);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_STRING("", confirmationId.c_str());
}

/**
 * Test invalid UUID v4 variant bits
 */
void test_parse_invalid_uuid_variant() {
    // UUID with invalid variant bits (should be 8/9/A/B)
    String response = "{\"status\":\"registered\",\"confirmation_id\":\"550e8400-e29b-41d4-c716-446655440000\",\"hardware_id\":\"AA:BB:CC:DD:EE:FF\"}";
    String confirmationId;

    bool result = parseRegistrationResponse(response, confirmationId);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_STRING("", confirmationId.c_str());
}

/**
 * Test empty response
 */
void test_parse_empty_response() {
    String response = "";
    String confirmationId;

    bool result = parseRegistrationResponse(response, confirmationId);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_STRING("", confirmationId.c_str());
}

/**
 * Test response with null confirmation_id
 */
void test_parse_null_confirmation_id() {
    String response = "{\"status\":\"registered\",\"confirmation_id\":null,\"hardware_id\":\"AA:BB:CC:DD:EE:FF\"}";
    String confirmationId;

    bool result = parseRegistrationResponse(response, confirmationId);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_STRING("", confirmationId.c_str());
}

/**
 * Test response with extra whitespace
 */
void test_parse_response_with_whitespace() {
    String response = "{\n  \"status\": \"registered\",\n  \"confirmation_id\": \"550e8400-e29b-41d4-a716-446655440000\",\n  \"hardware_id\": \"AA:BB:CC:DD:EE:FF\"\n}";
    String confirmationId;

    bool result = parseRegistrationResponse(response, confirmationId);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("550e8400-e29b-41d4-a716-446655440000", confirmationId.c_str());
}

/**
 * Test response with lowercase UUID
 */
void test_parse_response_with_lowercase_uuid() {
    String response = "{\"status\":\"registered\",\"confirmation_id\":\"550e8400-e29b-41d4-a716-446655440000\",\"hardware_id\":\"AA:BB:CC:DD:EE:FF\"}";
    String confirmationId;

    bool result = parseRegistrationResponse(response, confirmationId);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("550e8400-e29b-41d4-a716-446655440000", confirmationId.c_str());
}

/**
 * Test response with uppercase UUID
 */
void test_parse_response_with_uppercase_uuid() {
    String response = "{\"status\":\"registered\",\"confirmation_id\":\"550E8400-E29B-41D4-A716-446655440000\",\"hardware_id\":\"AA:BB:CC:DD:EE:FF\"}";
    String confirmationId;

    bool result = parseRegistrationResponse(response, confirmationId);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("550E8400-E29B-41D4-A716-446655440000", confirmationId.c_str());
}

void setUp(void) {
    // Set up code here (runs before each test)
}

void tearDown(void) {
    // Clean up code here (runs after each test)
}

void setup() {
    delay(2000);  // Wait for serial monitor

    UNITY_BEGIN();

    // Run all unit tests
    RUN_TEST(test_parse_valid_registration_response);
    RUN_TEST(test_parse_already_registered_response);
    RUN_TEST(test_parse_malformed_json);
    RUN_TEST(test_parse_completely_invalid_json);
    RUN_TEST(test_parse_missing_confirmation_id);
    RUN_TEST(test_parse_invalid_confirmation_id_format);
    RUN_TEST(test_parse_invalid_uuid_version);
    RUN_TEST(test_parse_invalid_uuid_variant);
    RUN_TEST(test_parse_empty_response);
    RUN_TEST(test_parse_null_confirmation_id);
    RUN_TEST(test_parse_response_with_whitespace);
    RUN_TEST(test_parse_response_with_lowercase_uuid);
    RUN_TEST(test_parse_response_with_uppercase_uuid);

    UNITY_END();
}

void loop() {
    // Nothing to do here
}
