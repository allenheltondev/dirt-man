#include "TimeManager.h"

#ifndef UNIT_TEST
#include <WiFi.h>
#include <time.h>
#endif

// NTP server configuration
static const char* NTP_SERVER = "pool.ntp.org";
static const long GMT_OFFSET_SEC = 0;  // UTC
static const int DAYLIGHT_OFFSET_SEC = 0;

TimeManager::TimeManager() : ntpSynced(false), ntpEpochMs(0), ntpSyncMillis(0), bootMillis(0) {}

TimeManager::~TimeManager() {}

void TimeManager::initialize() {
    bootMillis = millis();
    ntpSynced = false;
    ntpEpochMs = 0;
    ntpSyncMillis = 0;
}

uint32_t TimeManager::monotonicMs() const {
    return millis();
}

uint32_t TimeManager::uptimeMs() const {
    return millis() - bootMillis;
}

void TimeManager::tryNtpSync() {
#ifndef UNIT_TEST
    if (WiFi.status() == WL_CONNECTED) {
        if (syncWithNtpServer()) {
            ntpSynced = true;
        }
    }
#endif
}

void TimeManager::onWiFiConnected() {
    // Attempt NTP sync when WiFi connects
    tryNtpSync();
}

bool TimeManager::timeSynced() const {
    return ntpSynced;
}

uint64_t TimeManager::epochMsOrZero() const {
    if (!ntpSynced) {
        return 0;
    }

    // Calculate current epoch time based on NTP sync point
    uint32_t millisSinceSync = millis() - ntpSyncMillis;
    return ntpEpochMs + millisSinceSync;
}

uint64_t TimeManager::deviceBootEpochMs() const {
    if (!ntpSynced) {
        return 0;
    }

    // Calculate boot time by subtracting uptime from current epoch
    uint64_t currentEpoch = epochMsOrZero();
    uint32_t uptime = uptimeMs();

    if (currentEpoch >= uptime) {
        return currentEpoch - uptime;
    }

    return 0;
}

bool TimeManager::syncWithNtpServer() {
#ifndef UNIT_TEST
    // Configure NTP
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

    // Wait for time to be set (with timeout)
    int retries = 0;
    const int MAX_RETRIES = 10;

    while (retries < MAX_RETRIES) {
        time_t now = time(nullptr);
        if (now > 1000000000) {  // Valid timestamp (after year 2001)
            // Get current time in milliseconds
            struct timeval tv;
            gettimeofday(&tv, nullptr);

            ntpEpochMs = (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
            ntpSyncMillis = millis();

            return true;
        }

        delay(500);
        retries++;
    }

    return false;
#else
    // In unit test mode, simulate successful sync
    ntpEpochMs = 1704067200000ULL;  // Example: 2024-01-01 00:00:00 UTC
    ntpSyncMillis = millis();
    return true;
#endif
}
