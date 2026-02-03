/**
 * Unit Tests for NetworkManager Endpoint Derivation
 *
 * Feature: device-registration
 * Tests the deriveEndpoint() method for various URL formats
 * Validates: Requirements 4.1, 4.2, 4.3, 4.4
 */

#include <unity.h>
#include <Arduino.h>

// Forward declare the deriveEndpoint function for testing
// We'll test it as a standalone function
String deriveEndpoint(const String& dataEndpoint) {
    String endpoint = dataEndpoint;

    // Strip trailing slashes
    while (endpoint.endsWith("/")) {
        endpoint = endpoint.substring(0, endpoint.length() - 1);
    }

    // Remove query string and fragment at earliest occurrence
    int queryPos = endpoint.indexOf('?');
    int fragmentPos = endpoint.indexOf('#');
    int cutPos = -1;

    if (queryPos != -1 && fragmentPos != -1) {
        cutPos = min(queryPos, fragmentPos);
    } else if (queryPos != -1) {
        cutPos = queryPos;
    } else if (fragmentPos != -1) {
        cutPos = fragmentPos;
    }

    if (cutPos != -1) {
        endpoint = endpoint.substring(0, cutPos);
    }

    // Find the last '/' in the path
    int lastSlash = endpoint.lastIndexOf('/');

    if (lastSlash == -1) {
        // No path, append /register
        return endpoint + "/register";
    }

    // Check if last slash is part of protocol (http://)
    if (lastSlash < 8) {
        // No path after domain, append /register
        return endpoint + "/register";
    }

    // Replace last segment with "register"
    return endpoint.substring(0, lastSlash + 1) + "register";
}

/**
 * Test Case 6: Endpoint Derivation Example 1
 * Given: API endpoint "https://api.example.com/v1/sensor-data"
 * When: Deriving registration endpoint
 * Then: Result should be "https://api.example.com/v1/register"
 */
void test_endpoint_derivation_with_path() {
    String input = "https://api.example.com/v1/sensor-data";
    String expected = "https://api.example.com/v1/register";
    String result = deriveEndpoint(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

/**
 * Test Case 7: Endpoint Derivation Example 2
 * Given: API endpoint "https://api.example.com/sensor-data"
 * When: Deriving registration endpoint
 * Then: Result should be "https://api.example.com/register"
 */
void test_endpoint_derivation_single_path_segment() {
    String input = "https://api.example.com/sensor-data";
    String expected = "https://api.example.com/register";
    String result = deriveEndpoint(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

/**
 * Test URL with trailing slash
 */
void test_endpoint_derivation_trailing_slash() {
    String input = "https://api.example.com/v1/sensor-data/";
    String expected = "https://api.example.com/v1/register";
    String result = deriveEndpoint(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

/**
 * Test URL with query string
 */
void test_endpoint_derivation_with_query_string() {
    String input = "https://api.example.com/v1/sensor-data?key=value";
    String expected = "https://api.example.com/v1/register";
    String result = deriveEndpoint(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

/**
 * Test URL with fragment
 */
void test_endpoint_derivation_with_fragment() {
    String input = "https://api.example.com/v1/sensor-data#section";
    String expected = "https://api.example.com/v1/register";
    String result = deriveEndpoint(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

/**
 * Test URL containing both ? and # in either order: .../data#frag?x=y
 */
void test_endpoint_derivation_fragment_then_query() {
    String input = "https://api.example.com/v1/sensor-data#frag?x=y";
    String expected = "https://api.example.com/v1/register";
    String result = deriveEndpoint(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

/**
 * Test URL containing both ? and # in either order: .../data?x=y#frag
 */
void test_endpoint_derivation_query_then_fragment() {
    String input = "https://api.example.com/v1/sensor-data?x=y#frag";
    String expected = "https://api.example.com/v1/register";
    String result = deriveEndpoint(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

/**
 * Test URL with no path
 */
void test_endpoint_derivation_no_path() {
    String input = "https://api.example.com";
    String expected = "https://api.example.com/register";
    String result = deriveEndpoint(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

/**
 * Test URL with port
 */
void test_endpoint_derivation_with_port() {
    String input = "http://192.168.1.100:8080/api/v2/data";
    String expected = "http://192.168.1.100:8080/api/v2/register";
    String result = deriveEndpoint(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

/**
 * Test HTTP protocol preservation
 */
void test_endpoint_derivation_http_protocol() {
    String input = "http://api.example.com/data";
    String expected = "http://api.example.com/register";
    String result = deriveEndpoint(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
}

/**
 * Test HTTPS protocol preservation
 */
void test_endpoint_derivation_https_protocol() {
    String input = "https://api.example.com/data";
    String expected = "https://api.example.com/register";
    String result = deriveEndpoint(input);

    TEST_ASSERT_EQUAL_STRING(expected.c_str(), result.c_str());
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
    RUN_TEST(test_endpoint_derivation_with_path);
    RUN_TEST(test_endpoint_derivation_single_path_segment);
    RUN_TEST(test_endpoint_derivation_trailing_slash);
    RUN_TEST(test_endpoint_derivation_with_query_string);
    RUN_TEST(test_endpoint_derivation_with_fragment);
    RUN_TEST(test_endpoint_derivation_fragment_then_query);
    RUN_TEST(test_endpoint_derivation_query_then_fragment);
    RUN_TEST(test_endpoint_derivation_no_path);
    RUN_TEST(test_endpoint_derivation_with_port);
    RUN_TEST(test_endpoint_derivation_http_protocol);
    RUN_TEST(test_endpoint_derivation_https_protocol);

    UNITY_END();
}

void loop() {
    // Nothing to do here
}
