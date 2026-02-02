#include <unity.h>

#include "../include/TimeManager.h"

TimeManager* timeManager;

void setUp(void) {
    timeManager = new TimeManager();
    timeManager->initialize();
}

void tearDown(void) {
    delete timeManager;
}

void test_monotonicMs_returnsNonZero(void) {
    uint32_t time1 = timeManager->monotonicMs();
    TEST_ASSERT_GREATER_THAN(0, time1);
}

void test_monotonicMs_neverGoesBackwards(void) {
    uint32_t time1 = timeManager->monotonicMs();
    delay(10);
    uint32_t time2 = timeManager->monotonicMs();

    TEST_ASSERT_GREATER_OR_EQUAL(time1, time2);
}

void test_uptimeMs_startsAtZero(void) {
    // Create a fresh TimeManager
    TimeManager tm;
    tm.initialize();

    uint32_t uptime = tm.uptimeMs();
    // Uptime should be very small (close to 0) right after initialization
    TEST_ASSERT_LESS_THAN(100, uptime);
}

void test_uptimeMs_increases(void) {
    uint32_t uptime1 = timeManager->uptimeMs();
    delay(50);
    uint32_t uptime2 = timeManager->uptimeMs();

    TEST_ASSERT_GREATER_THAN(uptime1, uptime2);
    TEST_ASSERT_GREATER_OR_EQUAL(40, uptime2 - uptime1);  // At least 40ms passed
}

void test_timeSynced_initiallyFalse(void) {
    TEST_ASSERT_FALSE(timeManager->timeSynced());
}

void test_epochMsOrZero_returnsZeroWhenNotSynced(void) {
    TEST_ASSERT_EQUAL_UINT64(0, timeManager->epochMsOrZero());
}

void test_deviceBootEpochMs_returnsZeroWhenNotSynced(void) {
    TEST_ASSERT_EQUAL_UINT64(0, timeManager->deviceBootEpochMs());
}

void test_tryNtpSync_setsTimeSynced(void) {
    // In UNIT_TEST mode, tryNtpSync should simulate successful sync
    timeManager->tryNtpSync();

    TEST_ASSERT_TRUE(timeManager->timeSynced());
}

void test_epochMsOrZero_returnsNonZeroAfterSync(void) {
    timeManager->tryNtpSync();

    uint64_t epoch = timeManager->epochMsOrZero();
    TEST_ASSERT_GREATER_THAN(0, epoch);

    // Should be a reasonable timestamp (after year 2020)
    TEST_ASSERT_GREATER_THAN(1577836800000ULL, epoch);  // 2020-01-01
}

void test_epochMsOrZero_increases(void) {
    timeManager->tryNtpSync();

    uint64_t epoch1 = timeManager->epochMsOrZero();
    delay(50);
    uint64_t epoch2 = timeManager->epochMsOrZero();

    TEST_ASSERT_GREATER_THAN(epoch1, epoch2);
    TEST_ASSERT_GREATER_OR_EQUAL(40, epoch2 - epoch1);  // At least 40ms passed
}

void test_deviceBootEpochMs_returnsNonZeroAfterSync(void) {
    timeManager->tryNtpSync();

    uint64_t bootEpoch = timeManager->deviceBootEpochMs();
    TEST_ASSERT_GREATER_THAN(0, bootEpoch);

    // Boot epoch should be reasonable (after year 2020)
    TEST_ASSERT_GREATER_THAN(1577836800000ULL, bootEpoch);  // 2020-01-01
}

void test_deviceBootEpochMs_consistentAcrossCalls(void) {
    timeManager->tryNtpSync();

    uint64_t bootEpoch1 = timeManager->deviceBootEpochMs();
    delay(50);
    uint64_t bootEpoch2 = timeManager->deviceBootEpochMs();

    // Boot epoch should be relatively stable (within a few ms due to calculation timing)
    int64_t diff = (int64_t)bootEpoch2 - (int64_t)bootEpoch1;
    TEST_ASSERT_LESS_THAN(10, abs(diff));
}

void test_onWiFiConnected_triggersNtpSync(void) {
    TEST_ASSERT_FALSE(timeManager->timeSynced());

    timeManager->onWiFiConnected();

    TEST_ASSERT_TRUE(timeManager->timeSynced());
}

void test_timestampFormat_isUnixEpochMilliseconds(void) {
    timeManager->tryNtpSync();

    uint64_t epoch = timeManager->epochMsOrZero();

    // Unix epoch milliseconds should be a large positive number
    TEST_ASSERT_GREATER_THAN(1000000000000ULL, epoch);  // After year 2001

    // Should be less than year 2100 (4102444800000 ms)
    TEST_ASSERT_LESS_THAN(4102444800000ULL, epoch);
}

void test_uptimeMs_independentOfNtpSync(void) {
    uint32_t uptime1 = timeManager->uptimeMs();

    timeManager->tryNtpSync();

    uint32_t uptime2 = timeManager->uptimeMs();

    // Uptime should continue increasing regardless of NTP sync
    TEST_ASSERT_GREATER_OR_EQUAL(uptime1, uptime2);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_monotonicMs_returnsNonZero);
    RUN_TEST(test_monotonicMs_neverGoesBackwards);
    RUN_TEST(test_uptimeMs_startsAtZero);
    RUN_TEST(test_uptimeMs_increases);
    RUN_TEST(test_timeSynced_initiallyFalse);
    RUN_TEST(test_epochMsOrZero_returnsZeroWhenNotSynced);
    RUN_TEST(test_deviceBootEpochMs_returnsZeroWhenNotSynced);
    RUN_TEST(test_tryNtpSync_setsTimeSynced);
    RUN_TEST(test_epochMsOrZero_returnsNonZeroAfterSync);
    RUN_TEST(test_epochMsOrZero_increases);
    RUN_TEST(test_deviceBootEpochMs_returnsNonZeroAfterSync);
    RUN_TEST(test_deviceBootEpochMs_consistentAcrossCalls);
    RUN_TEST(test_onWiFiConnected_triggersNtpSync);
    RUN_TEST(test_timestampFormat_isUnixEpochMilliseconds);
    RUN_TEST(test_uptimeMs_independentOfNtpSync);

    return UNITY_END();
}
