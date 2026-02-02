#ifndef AVERAGED_DATA_H
#define AVERAGED_DATA_H

#include <stdint.h>

/**
 * Structure representing averaged sensor data over a collection period.
 * Used for network transmission and buffering.
 *
 * Includes both epoch timestamps (nullable when time not synced) and
 * uptime timestamps (always available) for robust time tracking.
 */
struct AveragedData {
    char batchId[64];  // Unique identifier: device_e_epochStart_epochEnd or
                       // device_u_uptimeStart_uptimeEnd

    // Averaged sensor values
    float avgBme280Temp;    // Celsius
    float avgDs18b20Temp;   // Celsius
    float avgHumidity;      // Percent
    float avgPressure;      // hPa
    float avgSoilMoisture;  // Percent (0-100)

    // Epoch timestamps (0 when time not synced)
    uint64_t sampleStartEpochMs;  // Unix epoch milliseconds
    uint64_t sampleEndEpochMs;    // Unix epoch milliseconds
    uint64_t deviceBootEpochMs;   // Unix epoch milliseconds

    // Uptime timestamps (always available)
    uint32_t sampleStartUptimeMs;  // Milliseconds since boot
    uint32_t sampleEndUptimeMs;    // Milliseconds since boot
    uint32_t uptimeMs;             // Current uptime when averaged

    // Metadata
    uint16_t sampleCount;  // Number of readings averaged
    uint8_t sensorStatus;  // Bitmask of available sensors
    bool timeSynced;       // Whether epoch timestamps are valid
};

#endif  // AVERAGED_DATA_H
