#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include "../test/mocks/Arduino.h"
#endif

#include <cstdint>

class TimeManager {
   public:
    TimeManager();
    ~TimeManager();

    void initialize();

    // Monotonic clock - always available, never goes backwards
    uint32_t monotonicMs() const;

    // Uptime tracking - milliseconds since boot
    uint32_t uptimeMs() const;

    // NTP synchronization
    void tryNtpSync();
    void onWiFiConnected();

    // Time sync status
    bool timeSynced() const;

    // Unix epoch milliseconds (0 if not synced)
    uint64_t epochMsOrZero() const;

    // Get device boot timestamp (0 if not synced)
    uint64_t deviceBootEpochMs() const;

   private:
    bool ntpSynced;
    uint64_t ntpEpochMs;     // Unix epoch ms when NTP was synced
    uint32_t ntpSyncMillis;  // millis() value when NTP was synced
    uint32_t bootMillis;     // millis() at boot (should be 0, but tracked for clarity)

    bool syncWithNtpServer();
};

#endif  // TIME_MANAGER_H
