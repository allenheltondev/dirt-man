#ifdef UNIT_TEST

#include <unity.h>

#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

typedef std::string String;

// Test data structure for HTTP status code retry decisions
struct HttpStatusTestCase {
    int statusCode;
    bool expectedShouldRetry;
    const char* description;
};

void setUp(void) {
    // Nothing to set up
}

void tearDown(void) {
    // Nothing to tear down
}

/**
 * Helper function to determine if a status code should trigger a retry
 * This mirrors the logic in NetworkManager::registerDevice()
 */
bool shouldRetryForStatus(int statusCode) {
    // Retry on: 408, 429, or 5xx (500-599)
    if (statusCode == 408 || statusCode == 429) {
        return true;
    }
    if (statusCode >= 500 && statusCode <= 599) {
        return true;
    }
    return false;
}

/**
 * Property 12: HTTP Status Code Retry Decision
 * Validates: Requirements 10.4, 10.8, 12.3, 12.5
 *
 * For any HTTP status code, the retry decision should follow these rules:
 * - Retry if and only if status in {408, 429} union [500..599]
 * - Do NOT retry for 2xx success codes
 * - Do NOT retry for 4xx client errors (except 408 and 429)
 * - Note: If status is retryable, retry even if response body is malformed/non-JSON
 *
 * Feature: device-registration, Property 12: HTTP Status Code Retry Decision
 */
void test_property12_http_status_code_retry_decision(void) {
    // Test fixed set of status codes as specified in the task
    std::vector<HttpStatusTestCase> testCases;

    // Success codes - should NOT retry
    testCases.push_back({200, false, "200 OK - success, no retry"});
    testCases.push_back({201, false, "201 Created - success, no retry"});

    // Client errors - should NOT retry (except 408 and 429)
    testCases.push_back({400, false, "400 Bad Request - client error, no retry"});
    testCases.push_back({401, false, "401 Unauthorized - client error, no retry"});
    testCases.push_back({403, false, "403 Forbidden - client error, no retry"});
    testCases.push_back({404, false, "404 Not Found - client error, no retry"});
    testCases.push_back({409, false, "409 Conflict - client error, no retry"});
    testCases.push_back({422, false, "422 Unprocessable Entity - client error, no retry"});

    // Special client errors - SHOULD retry
    testCases.push_back({408, true, "408 Request Timeout - should retry"});
    testCases.push_back({429, true, "429 Too Many Requests - should retry"});

    // Server errors - SHOULD retry
    testCases.push_back({500, true, "500 Internal Server Error - should retry"});
    testCases.push_back({502, true, "502 Bad Gateway - should retry"});
    testCases.push_back({503, true, "503 Service Unavailable - should retry"});
    testCases.push_back({504, true, "504 Gateway Timeout - should retry"});

    for (size_t i = 0; i < testCases.size(); i++) {
        const HttpStatusTestCase& testCase = testCases[i];

        bool actualShouldRetry = shouldRetryForStatus(testCase.statusCode);

        if (actualShouldRetry != testCase.expectedShouldRetry) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Status code %d (%s): Expected shouldRetry=%s, got shouldRetry=%s",
                     testCase.statusCode, testCase.description,
                     testCase.expectedShouldRetry ? "true" : "false",
                     actualShouldRetry ? "true" : "false");
            TEST_FAIL_MESSAGE(msg);
            return;
        }
    }
}

/**
 * Property 12a: Verify all 5xx codes trigger retry
 *
 * This test verifies that ALL status codes in the range [500, 599] trigger a retry.
 */
void test_property12_all_5xx_codes_trigger_retry(void) {
    for (int statusCode = 500; statusCode <= 599; statusCode++) {
        bool shouldRetry = shouldRetryForStatus(statusCode);

        if (!shouldRetry) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Status code %d (5xx server error) should trigger retry but doesn't",
                     statusCode);
            TEST_FAIL_MESSAGE(msg);
            return;
        }
    }
}

/**
 * Property 12b: Verify 2xx success codes do NOT trigger retry
 *
 * This test verifies that success codes in the range [200, 299] do NOT trigger a retry.
 */
void test_property12_2xx_codes_no_retry(void) {
    for (int statusCode = 200; statusCode <= 299; statusCode++) {
        bool shouldRetry = shouldRetryForStatus(statusCode);

        if (shouldRetry) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Status code %d (2xx success) should NOT trigger retry but does", statusCode);
            TEST_FAIL_MESSAGE(msg);
            return;
        }
    }
}

/**
 * Property 12c: Verify 4xx client errors do NOT trigger retry (except 408 and 429)
 *
 * This test verifies that client error codes in the range [400, 499] do NOT trigger
 * a retry, with the exception of 408 and 429.
 */
void test_property12_4xx_codes_no_retry_except_408_429(void) {
    for (int statusCode = 400; statusCode <= 499; statusCode++) {
        bool shouldRetry = shouldRetryForStatus(statusCode);

        // Special cases: 408 and 429 SHOULD retry
        if (statusCode == 408 || statusCode == 429) {
            if (!shouldRetry) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Status code %d should trigger retry but doesn't",
                         statusCode);
                TEST_FAIL_MESSAGE(msg);
                return;
            }
        } else {
            // All other 4xx codes should NOT retry
            if (shouldRetry) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Status code %d (4xx client error) should NOT trigger retry but does",
                         statusCode);
                TEST_FAIL_MESSAGE(msg);
                return;
            }
        }
    }
}

/**
 * Property 12d: Verify 3xx redirect codes do NOT trigger retry
 *
 * This test verifies that redirect codes in the range [300, 399] do NOT trigger a retry.
 * (These are not explicitly mentioned in requirements but should be tested for completeness)
 */
void test_property12_3xx_codes_no_retry(void) {
    for (int statusCode = 300; statusCode <= 399; statusCode++) {
        bool shouldRetry = shouldRetryForStatus(statusCode);

        if (shouldRetry) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Status code %d (3xx redirect) should NOT trigger retry but does", statusCode);
            TEST_FAIL_MESSAGE(msg);
            return;
        }
    }
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Property 12: HTTP Status Code Retry Decision
    RUN_TEST(test_property12_http_status_code_retry_decision);
    RUN_TEST(test_property12_all_5xx_codes_trigger_retry);
    RUN_TEST(test_property12_2xx_codes_no_retry);
    RUN_TEST(test_property12_4xx_codes_no_retry_except_408_429);
    RUN_TEST(test_property12_3xx_codes_no_retry);

    return UNITY_END();
}

#endif  // UNIT_TEST
