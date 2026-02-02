#ifndef SYSTEM_STATUS_MANAGER_H
#define SYSTEM_STATUS_MANAGER_H

#ifdef UNIT_TEST
#include "../test/mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "models/ErrorCounters.h"
#include "models/SensorReadings.h"
#include "models/SystemStatus.h"

/**
 * SystemStatusManager aggregates system state and metrics for display,
 * networking, and diagnostics.
 *
 * Tracks:
 * - Uptime since boot
 * - Free heap memory
 * - WiFi RSSI
 * - Queue depth (buffered readings)
 * - Error counters (sensor, network, buffer overflow)
 * - Last successful sensor read time
 * - Last successful transmission time
 * - Last error string
 * - Min/max sensor values since boot
 */
class SystemStatusManager {
   public:
    SystemStatusManager();
    ~SystemStatusManager();

    /**
     * Initialize the system status manager.
     * Sets boot time and initializes counters.
     */
    void initialize();

    /**
     * Update system metrics (uptime, heap, etc.).
     * Should be called periodically in main loop.
     */
    void update();

    /**
     * Get current system status.
     * @return SystemStatus structure with all metrics
     */
    SystemStatus getStatus() const;

    /**
     * Update WiFi RSSI value.
     * @param rssi WiFi signal strength in dBm
     */
    void setWiFiRSSI(int8_t rssi);

    /**
     * Update queue depth (number of buffered readings).
     * @param depth Number of readings in transmission buffer
     */
    void setQueueDepth(uint16_t depth);

    /**
     * Increment sensor read failure counter.
     */
    void incrementSensorFailures();

    /**
     * Increment network failure counter.
     */
    void incrementNetworkFailures();

    /**
     * Increment buffer overflow counter.
     */
    void incrementBufferOverflows();

    /**
     * Update last successful sensor read timestamp.
     * @param timestampMs Monotonic timestamp in milliseconds
     */
    void setLastSensorReadTime(unsigned long timestampMs);

    /**
     * Update last successful transmission timestamp.
     * @param timestampMs Monotonic timestamp in milliseconds
     */
    void setLastTransmissionTime(unsigned long timestampMs);

    /**
     * Set last error string (non-sensitive).
     * @param error Error message (max 128 chars)
     */
    void setLastError(const char* error);

    /**
     * Update min/max sensor values.
     * @param readings Current sensor readings
     */
    void updateMinMax(const SensorReadings& readings);

    /**
     * Reset min/max values to current readings.
     * @param readings Current sensor readings
     */
    void resetMinMax(const SensorReadings& readings);

    /**
     * Get uptime in milliseconds.
     * @return Uptime since boot in milliseconds
     */
    unsigned long getUptimeMs() const;

    /**
     * Get free heap memory in bytes.
     * @return Free heap memory
     */
    uint32_t getFreeHeap() const;

    /**
     * Get last successful sensor read time.
     * @return Monotonic timestamp in milliseconds
     */
    unsigned long getLastSensorReadTime() const;

    /**
     * Get last successful transmission time.
     * @return Monotonic timestamp in milliseconds
     */
    unsigned long getLastTransmissionTime() const;

    /**
     * Get last error string.
     * @return Error message (null-terminated)
     */
    const char* getLastError() const;

   private:
    SystemStatus status;
    unsigned long bootTimeMs;
    unsigned long lastSensorReadMs;
    unsigned long lastTransmissionMs;
    char lastErrorStr[128];

    void updateUptime();
    void updateHeapMemory();
};

#endif  // SYSTEM_STATUS_MANAGER_H
