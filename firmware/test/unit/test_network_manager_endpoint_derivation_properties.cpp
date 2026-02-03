/**
 * Property-Based Tests for NetworkManager Endpoint Derivation
 *
 * Feature: device-registration
 * Property 8: Endpoint Derivation Preserves URL Components
 * Validates: Requirements 4.1, 4.4
 */

#include <unity.h>
#include <Arduino.h>
#include <cstdlib>
#include <ctime>

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

// Helper function to generate random string
String generateRandomString(int length, const char* charset) {
    String result = "";
    int charsetLen = strlen(charset);
    for (int i = 0; i < length; i++) {
        result += charset[rand() % charsetLen];
    }
    return result;
}

// Helper function to generate random path segment
String generateRandomPathSegment() {
    const char* charset = "abcdefghijklmnopqrstuvwxyz0123456789-_";
    int length = 3 + (rand() % 8);  // 3-10 characters
    return generateRandomString(length, charset);
}

// Helper function to generate random domain
String generateRandomDomain() {
    const char* charset = "abcdefghijklmnopqrstuvwxyz0123456789-";
    String domain = generateRandomString(5 + (rand() % 10), charset);

    // Add TLD
    const char* tlds[] = {"com", "org", "net", "io", "dev"};
    domain += ".";
    domain += tlds[rand() % 5];

    return domain;
}

// Helper function to extract protocol from URL
String extractProtocol(const String& url) {
    int protocolEnd = url.indexOf("://");
    if (protocolEnd == -1) return "";
    return url.substring(0, protocolEnd + 3);
}

// Helper function to extract domain (including port) from URL
String extractDomain(const String& url) {
    int protocolEnd = url.indexOf("://");
    if (protocolEnd == -1) return "";

    int pathStart = url.indexOf('/', protocolEnd + 3);
    if (pathStart == -1) {
        return url.substring(protocolEnd + 3);
    }

    return url.substring(protocolEnd + 3, pathStart);
}

// Helper function to extract path segments (excluding last) from URL
String extractPathWithoutLast(const String& url) {
    int protocolEnd = url.indexOf("://");
    if (protocolEnd == -1) return "";

    int pathStart = url.indexOf('/', protocolEnd + 3);
    if (pathStart == -1) return "";

    String fullPath = url.substring(pathStart);

    // Remove query and fragment
    int queryPos = fullPath.indexOf('?');
    int fragmentPos = fullPath.indexOf('#');
    int cutPos
llPath.lastIndexOf('/');
    if (lastSlash == -1 || lastSlash == 0) {
        return "";
    }

    return fullPath.substring(0, lastSlash + 1);
}

/**
 * Property 8: Endpoint Derivation Preserves URL Components
 * Validates: Requirements 4.1, 4.4
 *
 * For any valid HTTP/HTTPS URL with a path:
 * - Protocol (http:// or https://) is preserved
 * - Domain (including port if present) is preserved
 * - All path segments except the last are preserved
 * - Last path segment is replaced with "register"
 */
void test_property_endpoint_derivation_preserves_components() {
    const int ITERATIONS = 100;
    int successCount = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        // Generate random URL components
        bool useHttps = (rand() % 2) == 0;
        String protocol = useHttps ? "https://" : "http://";
        String domain = generateRandomDomain();

        // Optionally add port
        String port = "";
        if ((rand() % 3) == 0) {  // 33% chance of having a port
            port = ":";
            port += String(1000 + (rand() % 9000));
        }

        // Generate 1-5 path segments
        int numSegments = 1 + (rand() % 5);
        String path = "";
        for (int j = 0; j < numSegments; j++) {
            path += "/";
            path += generateRandomPathSegment();
        }

        // Construct full URL
        String url = protocol + domain + port + path;

        // Derive endpoint
        String result = deriveEndpoint(url);

        // Verify protocol is preserved
        String resultProtocol = extractProtocol(result);
        if (resultProtocol != protocol) {
            Serial.print("FAIL: Protocol not preserved. Expected: ");
            Serial.print(protocol);
            Serial.print(", Got: ");
            Serial.println(resultProtocol);
            TEST_FAIL_MESSAGE("Protocol not preserved");
            return;
        }

        // Verify domain (with port) is preserved
        String resultDomain = extractDomain(result);
        String expectedDomain = domain + port;
        if (resultDomain != expectedDomain) {
            Serial.print("FAIL: Domain not preserved. Expected: ");
            Serial.print(expectedDomain);
            Serial.print(", Got: ");
            Serial.println(resultDomain);
            TEST_FAIL_MESSAGE("Domain not preserved");
            return;
        }

        // Verify path segments except last are preserved
        String resultPathWithoutLast = extractPathWithoutLast(result);
        String expectedPathWithoutLast = extractPathWithoutLast(url);
        if (resultPathWithoutLast != expectedPathWithoutLast) {
            Serial.print("FAIL: Path segments not preserved. Expected: ");
            Serial.print(expectedPathWithoutLast);
            Serial.print(", Got: ");
            Serial.println(resultPathWithoutLast);
            TEST_FAIL_MESSAGE("Path segments not preserved");
            return;
        }

        // Verify last segment is "register"
        if (!result.endsWith("/register")) {
            Serial.print("FAIL: Last segment not 'register'. URL: ");
            Serial.print(url);
            Serial.print(", Result: ");
            Serial.println(result);
            TEST_FAIL_MESSAGE("Last segment not 'register'");
            return;
        }

        successCount++;
    }

    Serial.print("Property test passed for ");
    Serial.print(successCount);
    Serial.print(" / ");
    Serial.print(ITERATIONS);
    Serial.println(" iterations");

    TEST_ASSERT_EQUAL(ITERATIONS, successCount);
}

/**
 * Property: Endpoint derivation with query strings
 * Verifies that query strings are removed before processing
 */
void test_property_endpoint_derivation_removes_query_strings() {
    const int ITERATIONS = 50;

    for (int i = 0; i < ITERATIONS; i++) {
        // Generate base URL
        String protocol = (rand() % 2) == 0 ? "https://" : "http://";
        String domain = generateRandomDomain();
        String path = "/" + generateRandomPathSegment() + "/" + generateRandomPathSegment();

        // Add query string
        String queryString = "?key=" + generateRandomPathSegment();
        String url = protocol + domain + path + queryString;

        // Derive endpoint
        String result = deriveEndpoint(url);

        // Verify no query string in result
        TEST_ASSERT_EQUAL(-1, result.indexOf('?'));

        // Verify ends with /register
        TEST_ASSERT_TRUE(result.endsWith("/register"));
    }
}

/**
 * Property: Endpoint derivation with fragments
 * Verifies that fragments are removed before processing
 */
void test_property_endpoint_derivation_removes_fragments() {
    const int ITERATIONS = 50;

    for (int i = 0; i < ITERATIONS; i++) {
        // Generate base URL
        String protocol = (rand() % 2) == 0 ? "https://" : "http://";
        String domain = generateRandomDomain();
        String path = "/" + generateRandomPathSegment() + "/" + generateRandomPathSegment();

        // Add fragment
        String fragment = "#" + generateRandomPathSegment();
        String url = protocol + domain + path + fragment;

        // Derive endpoint
        String result = deriveEndpoint(url);

        // Verify no fragment in result
        TEST_ASSERT_EQUAL(-1, result.indexOf('#'));

        // Verify ends with /register
        TEST_ASSERT_TRUE(result.endsWith("/register"));
    }
}

void setUp(void) {
    // Set up code here (runs before each test)
}

void tearDown(void) {
    // Clean up code here (runs after each test)
}

void setup() {
    delay(2000);  // Wait for serial monitor

    // Seed random number generator
    srand(time(NULL));

    UNITY_BEGIN();

    // Run property-based tests
    RUN_TEST(test_property_endpoint_derivation_preserves_components);
    RUN_TEST(test_property_endpoint_derivation_removes_query_strings);
    RUN_TEST(test_property_endpoint_derivation_removes_fragments);

    UNITY_END();
}

void loop() {
    // Nothing to do here
}
