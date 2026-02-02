#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include <stdint.h>

#include "models/AveragedData.h"
#include "models/DisplayPoint.h"
#include "models/SensorReadings.h"
#include "models/SensorType.h"

// Compile-time constant for maximum publish samples
#define MAX_PUBLISH_SAMPLES 120

// Compile-time constant for transmission ring buffer size
#define MAX_DATA_BUFFER_SIZE 50

// Compile-time constant for display buffer size (240 points = 4 hours at 1-minute intervals)
#define MAX_DISPLAY_POINTS 240

/**
 * DataManager handles three types of data buffers:
 * 1. Averaging Buffer: Fixed-size array for accumulating sensor readings
 * 2. Data Buffer: Ring buffer for transmission queue (Task 10)
 * 3. Display Buffer: Ring buffer for graph history (Task 11)
 */
class DataManager {
   public:
    DataManager();

    // Averaging Buffer operations (Task 9)
    void addReading(const SensorReadings& reading);
    bool shouldPublish();
    AveragedData calculateAverages();
    void clearAveragingBuffer();

    // Transmission Ring Buffer operations (Task 10)
    void bufferForTransmission(const AveragedData& data);
    uint16_t getBufferedDataCount() const;
    const AveragedData* getBufferedData(uint16_t& count) const;
    void clearAcknowledgedData(const char* batchIds[], uint16_t batchIdCount);
    bool isBufferNearFull() const;  // Returns true if > 80% full (warning threshold)

    // Display Buffer operations (Task 11)
    void addToDisplayBuffer(const SensorReadings& reading);
    uint16_t getDisplayDataCount(SensorType type) const;
    const DisplayPoint* getDisplayData(SensorType type, uint16_t& count,
                                       uint16_t maxPoints = 0) const;

    // Configuration
    void setPublishIntervalSamples(uint16_t samples);
    uint16_t getPublishIntervalSamples() const;
    uint16_t getCurrentSampleCount() const;

    // Buffer overflow tracking
    uint16_t getBufferOverflowCount() const;

   private:
    // Fixed-size averaging buffer (no heap allocation)
    SensorReadings averagingBuffer[MAX_PUBLISH_SAMPLES];
    uint16_t averagingBufferCount;  // Current number of readings in buffer
    uint16_t
        publishIntervalSamples;  // Effective length from config (must be <= MAX_PUBLISH_SAMPLES)

    // Fixed-size transmission ring buffer (no heap allocation)
    AveragedData dataBuffer[MAX_DATA_BUFFER_SIZE];
    uint16_t dataBufferHead;       // Index where next item will be written
    uint16_t dataBufferTail;       // Index of oldest item
    uint16_t dataBufferCount;      // Current number of items in buffer
    uint16_t bufferOverflowCount;  // Counter for buffer overflow events

    // Fixed-size display buffer (no heap allocation) - 2D array [sensor][points]
    DisplayPoint displayBuffer[NUM_SENSORS][MAX_DISPLAY_POINTS];
    uint16_t displayBufferHead[NUM_SENSORS];   // Head index for each sensor's ring buffer
    uint16_t displayBufferCount[NUM_SENSORS];  // Current count for each sensor
    uint32_t lastDisplayUpdate;                // Timestamp of last display buffer update (ms)

    // Display buffer interval (1 minute = 60000 ms)
    static constexpr uint32_t DISPLAY_INTERVAL_MS = 60000;

    // Downsampling buffer for getDisplayData (max 120 points as per requirements)
    mutable DisplayPoint downsampledBuffer[120];

    // Batch ID generation
    void generateBatchId(char* batchId, size_t bufferSize, const AveragedData& data,
                         bool timeSynced);

    // Ring buffer helpers
    void advanceHead();
    void advanceTail();

    // Display buffer helper
    void downsampleDisplayData(const DisplayPoint* source, uint16_t sourceCount, DisplayPoint* dest,
                               uint16_t maxPoints) const;
};

#endif  // DATA_MANAGER_H
