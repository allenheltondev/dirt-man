#ifdef UNIT_TEST

#include <unity.h>

#include <cstdlib>
#include <ctime>
#include <string>

typedef std::string String;

// Standalone implementation of calculateBackoff for testing
// This is a copy of the algorithm from RegistrationManager
unsigned long calculateBackoff(int attemptIndex) {
    // Base delay: 1 second
    unsigned long baseDelay = 1000;

    // Exponential: 2^attemptIndex seconds
    // attemptIndex is zero-based: 0→1s, 1→2s, 2→4s, 3→8s, 4→16s
    unsigned long delay = baseDelay * (1UL << attemptIndex);

    // Clamp to maximum 30 seconds to prevent overflow
    if (delay > 30000) {
        delay = 30000;
    }

    // Add random jitter: 0-500ms
    unsigned long jitter = rand() % 501;

    return delay + jitter;
}

void setUp(void) {
    // Seed random number generator with time + additional entropy
    srand(time(NULL) + rand());
}

void tearDown(void) {
    // Nothing to tear down
}

/**
 * Property 11: Exponential Backoff Calculation
 * **Validates: Requirements 10.6, 10.7**
 *
 * For any retry attemptIndex n (where 0 ≤ n ≤ 4), the calculated backoff delay
 * should be (1000 * 2^n) milliseconds plus jitter, where jitter is in the range
 * [0, 500] milliseconds, and the total delay is clamped to a maximum of 30,000 milliseconds.
 *
 * **Feature: device-registration, Property 11: Exponential Backoff Calculation**
 */
void test_property11_exponential_backoff_calculation(void) {
    // Test attemptIndex 0-4 for correctness (100 iterations each)
    const int ITERATIONS = 100;

    for (int attemptIndex = 0; attemptIndex <= 4; attemptIndex++) {
        unsigned long expectedBaseDelay = 1000UL * (1UL << attemptIndex);

        for (int i = 0; i < ITERATIONS; i++) {
            // Calculate backoff
            unsigned long backoff = calculateBackoff(attemptIndex);

            // Property 11a: Verify base delay = 1000 * 2^attemptIndex
            // The backoff should be >= expectedBaseDelay (base delay)
            if (backoff < expectedBaseDelay) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Iteration %d, attemptIndex %d: Backoff %lu is less than expected base "
                         "delay %lu",
                         i, attemptIndex, backoff, expectedBaseDelay);
                TEST_FAIL_MESSAGE(msg);
                return;
            }

            // Property 11b: Verify jitter in range [0, 500]
            // The backoff should be <= expectedBaseDelay + 500 (base delay + max jitter)
            unsigned long maxDelay = expectedBaseDelay + 500;
            if (backoff > maxDelay) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Iteration %d, attemptIndex %d: Backoff %lu exceeds max expected %lu "
                         "(base %lu + jitter 500)",
                         i, attemptIndex, backoff, maxDelay, expectedBaseDelay);
                TEST_FAIL_MESSAGE(msg);
                return;
            }

            // Verify the jitter is within [0, 500]
            unsigned long jitter = backoff - expectedBaseDelay;
            if (jitter > 500) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Iteration %d, attemptIndex %d: Jitter %lu exceeds maximum 500ms", i,
                         attemptIndex, jitter);
                TEST_FAIL_MESSAGE(msg);
                return;
            }
        }
    }
}

/**
 * Property 11c: Verify attemptIndex 10 clamps to <= 30000 + jitter
 *
 * This tests that very large attemptIndex values are properly clamped to the
 * maximum delay of 30 seconds.
 */
void test_property11_backoff_clamping(void) {
    const int ITERATIONS = 100;
    const unsigned long MAX_DELAY = 30000;
    const unsigned long MAX_JITTER = 500;

    for (int i = 0; i < ITERATIONS; i++) {
        // Test with attemptIndex 10 (which would be 1024 seconds without clamping)
        unsigned long backoff = calculateBackoff(10);

        // Property: Backoff should be clamped to <= 30000 + 500
        if (backoff > MAX_DELAY + MAX_JITTER) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Iteration %d: Backoff %lu exceeds maximum %lu (30000 + 500)", i, backoff,
                     MAX_DELAY + MAX_JITTER);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Property: Backoff should be at least 30000 (the clamped base delay)
        if (backoff < MAX_DELAY) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "Iteration %d: Backoff %lu is less than minimum clamped delay %lu", i, backoff,
                     MAX_DELAY);
            TEST_FAIL_MESSAGE(msg);
            return;
        }
    }
}

/**
 * Additional property test: Verify jitter distribution is reasonable
 *
 * This test verifies that the jitter is actually random and not always
 * the same value.
 */
void test_property_backoff_jitter_distribution(void) {
    const int ITERATIONS = 100;
    const int attemptIndex = 0;  // Use attemptIndex 0 for simplicity
    const unsigned long baseDelay = 1000;

    // Collect jitter values
    bool seenDifferentJitters = false;
    unsigned long firstJitter = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        unsigned long backoff = calculateBackoff(attemptIndex);
        unsigned long jitter = backoff - baseDelay;

        if (i == 0) {
            firstJitter = jitter;
        } else if (jitter != firstJitter) {
            seenDifferentJitters = true;
        }
    }

    // Property: Over 100 iterations, we should see at least some variation in jitter
    // (This is a probabilistic test - there's a tiny chance it could fail even with correct
    // implementation)
    TEST_ASSERT_TRUE_MESSAGE(seenDifferentJitters,
                             "Jitter should vary across iterations (probabilistic test)");
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Property 11: Exponential Backoff Calculation
    RUN_TEST(test_property11_exponential_backoff_calculation);
    RUN_TEST(test_property11_backoff_clamping);

    // Additional property tests
    RUN_TEST(test_property_backoff_jitter_distribution);

    return UNITY_END();
}

#endif  // UNIT_TEST
