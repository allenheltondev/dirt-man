#include <unity.h>

#include "../include/TimeManager.h"
#include <cstdint>
#include <cstdlib>
#include <ctime>

/**
 * Property-Based Test for TimeManager
 *
 * Feature: esp32-sensor-firmware
 * Property 25: Timestamp Format Consistency
 *
 * **Validates: Requirements 5.3**
 *
 * Property: For any timestamp in the system, it should be represented as Unix epoch
 * milliseconds (positive integer), ensuring consistent time representation across all components.
 */

TimeManager* timeManager;

void setUp(void) {
    timeManager = new TimeManager();
    timeManager->initialize();
}

void tearDown(void) {
    delete timeManager;
}

// Helper function to generate random timestamps
uint64_t generateRandomTimestamp() {
    // Generate timestamps between year 2020 and year 2030
    // 2020-01-01: 1577836800000 ms
    // 2030-01-01: 1893456000000 ms
    uint64_t min_ts = 1577836800000ULL;
    uint64_t max_ts = 1893456000000ULL;

    uint64_t range = max_ts - min_ts;
    uint64_t random_offset = ((uint64_t)rand() << 32) | rand();
    random_offset = random_offset % range;

    return min_ts + random_offset;
}

/**
 * Property Test: Timestamp Format Consistency
 *
 * Tests that all timestamps returned by TimeManager are Unix epoch milliseconds.
 * Runs 100 iterations with different states.
 */
void test_property_timestamp_format_consistency(void) {
    const int NUM_ITERATIONS = 100;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Test monotonicMs() - should always return a positive value
        uint32_t monotonic = timeManager->monotonicMs();
        TEST_ASSERT_GREATER_THAN(0, monotonic);

        // Test uptimeMs() - should always return a positive value or zero
        uint32_t uptime = timeManager->uptimeMs();
        TEST_ASSERT_GREATER_OR_EQUAL(0, uptime);

        // Test epochMsOrZero() before sync - should return 0
        uint64_t epoch_before = timeManager->epochMsOrZero();
        TEST_ASSERT_EQUAL_UINT64(0, epoch_before);

        // Simulate NTP sync
        timeManager->tryNtpSync();

        // Test epochMsOrZero() after sync - should be Unix epoch milliseconds
        uint64_t epoch_after = timeManager->epochMsOrZero();

        // Should be greater than 0
        TEST_ASSERT_GREATER_THAN(0, epoch_after);

        // Should be a reasonable timestamp (after year 2020)
        TEST_ASSERT_GREATER_THAN(1577836800000ULL, epoch_after);

        // Should be less than year 2100 (4102444800000 ms)
        TEST_ASSERT_LESS_THAN(4102444800000ULL, epoch_after);

        // Test deviceBootEpochMs() - should also be Unix epoch milliseconds
        uint64_t boot_epoch = timeManager->deviceBootEpochMs();

        // Should be greater than 0
        TEST_ASSERT_GREATER_THAN(0, boot_epoch);

        // Should be a reasonable timestamp (after year 2020)
        TEST_ASSERT_GREATER_THAN(1577836800000ULL, boot_epoch);

        // Should be less than year 2100
        TEST_ASSERT_LESS_THAN(4102444800000ULL, boot_epoch);

        // Boot epoch should be less than or equal to current epoch
        TEST_ASSERT_LESS_OR_EQUAL(epoch_after, boot_epoch);

        // Small delay to simulate time passing
        delay(1);

        // Create a new TimeManager for next iteration
        delete timeManager;
        timeManager = new TimeManager();
        timeManager->initialize();
    }
}

/**
 * Property Test: Timestamp Monotonicity
 *
 * Tests that timestamps never go backwards.
 */
void test_property_timestamp_monotonicity(void) {
    const int NUM_ITERATIONS = 50;

    timeManager->tryNtpSync();

    uint32_t prev_monotonic = timeManager->monotonicMs();
    uint32_t prev_uptime = timeManager->uptimeMs();
    uint64_t prev_epoch = timeManager->epochMsOrZero();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        delay(5);  // Small delay

        uint32_t curr_monotonic = timeManager->monotonicMs();
        uint32_t curr_uptime = timeManager->uptimeMs();
        uint64_t curr_epoch = timeManager->epochMsOrZero();

        // Monotonic time should never decrease
        TEST_ASSERT_GREATER_OR_EQUAL(prev_monotonic, curr_monotonic);

        // Uptime should never decrease
        TEST_ASSERT_GREATER_OR_EQUAL(prev_uptime, curr_uptime);

        // Epoch time should never decrease (when synced)
        TEST_ASSERT_GREATER_OR_EQUAL(prev_epoch, curr_epoch);

        prev_monotonic = curr_monotonic;
        prev_uptime = curr_uptime;
        prev_epoch = curr_epoch;
    }
}

/**
 * Property Test: Timestamp Precision
 *
 * Tests that timestamps have millisecond precision.
 */
void test_property_timestamp_precision(void) {
    const int NUM_ITERATIONS = 50;

    timeManager->tryNtpSync();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        uint64_t epoch1 = timeManager->epochMsOrZero();
        delay(10);  // 10ms delay
        uint64_t epoch2 = timeManager->epochMsOrZero();

        // Difference should be at least 8ms (allowing for some timing variance)
        uint64_t diff = epoch2 - epoch1;
        TEST_ASSERT_GREATER_OR_EQUAL(8, diff);

        // Difference should be less than 20ms (10ms delay + some overhead)
        TEST_ASSERT_LESS_THAN(20, diff);
    }
}

/**
 * Property Test: Zero Before Sync
 *
 * Tests that epoch timestamps are always zero before NTP sync.
 */
void test_property_zero_before_sync(void) {
    const int NUM_ITERATIONS = 20;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Create fresh TimeManager
        TimeManager tm;
        tm.initialize();

        // Should not be synced initially
        TEST_ASSERT_FALSE(tm.timeSynced());

        // Epoch should be zero
        TEST_ASSERT_EQUAL_UINT64(0, tm.epochMsOrZero());

        // Boot epoch should be zero
        TEST_ASSERT_EQUAL_UINT64(0, tm.deviceBootEpochMs());

        // But monotonic and uptime should still work
        TEST_ASSERT_GREATER_THAN(0, tm.monotonicMs());
        TEST_ASSERT_GREATER_OR_EQUAL(0, tm.uptimeMs());
    }
}

/**
 * Property Test: Consistent After Sync
 *
 * Tests that once synced, timestamps remain consistent.
 */
void test_property_consistent_after_sync(void) {
    const int NUM_ITERATIONS = 30;

    timeManager->tryNtpSync();
    TEST_ASSERT_TRUE(timeManager->timeSynced());

    uint64_t boot_epoch = timeManager->deviceBootEpochMs();

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        delay(5);

        // Boot epoch should remain relatively stable
        uint64_t current_boot_epoch = timeManager->deviceBootEpochMs();

        // Should be within 10ms of original (accounting for calculation timing)
        int64_t diff = (int64_t)current_boot_epoch - (int64_t)boot_epoch;
        TEST_ASSERT_LESS_THAN(10, abs(diff));

        // Current epoch should always be greater than boot epoch
        uint64_t current_epoch = timeManager->epochMsOrZero();
        TEST_ASSERT_GREATER_THAN(boot_epoch, current_epoch);
    }
}

int main(int argc, char** argv) {
    // Seed random number generator
    srand(time(NULL));

    UNITY_BEGIN();

    RUN_TEST(test_property_timestamp_format_consistency);
    RUN_TEST(test_property_timestamp_monotonicity);
    RUN_TEST(test_property_timestamp_precision);
    RUN_TEST(test_property_zero_before_sync);
    RUN_TEST(test_property_consistent_after_sync);

    return UNITY_END();
}
