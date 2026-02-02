#include "DataManager.h"

#include <stdio.h>
#include <string.h>

DataManager::DataManager()
    : averagingBufferCount(0),
      publishIntervalSamples(20)  // Default value, will be set from config
      ,
      dataBufferHead(0),
      dataBufferTail(0),
      dataBufferCount(0),
      bufferOverflowCount(0),
      lastDisplayUpdate(0) {
    // Initialize buffers to zero
    memset(averagingBuffer, 0, sizeof(averagingBuffer));
    memset(dataBuffer, 0, sizeof(dataBuffer));
    memset(displayBuffer, 0, sizeof(displayBuffer));

    // Initialize display buffer indices
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        displayBufferHead[i] = 0;
        displayBufferCount[i] = 0;
    }
}

void DataManager::setPublishIntervalSamples(uint16_t samples) {
    // Enforce MAX_PUBLISH_SAMPLES constraint
    if (samples > MAX_PUBLISH_SAMPLES) {
        publishIntervalSamples = MAX_PUBLISH_SAMPLES;
    } else if (samples == 0) {
        publishIntervalSamples = 1;  // Minimum 1 sample
    } else {
        publishIntervalSamples = samples;
    }
}

uint16_t DataManager::getPublishIntervalSamples() const {
    return publishIntervalSamples;
}

uint16_t DataManager::getCurrentSampleCount() const {
    return averagingBufferCount;
}

void DataManager::addReading(const SensorReadings& reading) {
    // Only add if we haven't reached the publish interval
    if (averagingBufferCount < publishIntervalSamples) {
        averagingBuffer[averagingBufferCount] = reading;
        averagingBufferCount++;
    }
}

bool DataManager::shouldPublish() {
    return averagingBufferCount >= publishIntervalSamples;
}

AveragedData DataManager::calculateAverages() {
    AveragedData avg = {};

    if (averagingBufferCount == 0) {
        return avg;
    }

    // Accumulate sums
    float sumBme280Temp = 0;
    float sumDs18b20Temp = 0;
    float sumHumidity = 0;
    float sumPressure = 0;
    float sumSoilMoisture = 0;

    for (uint16_t i = 0; i < averagingBufferCount; i++) {
        sumBme280Temp += averagingBuffer[i].bme280Temp;
        sumDs18b20Temp += averagingBuffer[i].ds18b20Temp;
        sumHumidity += averagingBuffer[i].humidity;
        sumPressure += averagingBuffer[i].pressure;
        sumSoilMoisture += averagingBuffer[i].soilMoisture;
    }

    // Calculate averages
    avg.avgBme280Temp = sumBme280Temp / averagingBufferCount;
    avg.avgDs18b20Temp = sumDs18b20Temp / averagingBufferCount;
    avg.avgHumidity = sumHumidity / averagingBufferCount;
    avg.avgPressure = sumPressure / averagingBufferCount;
    avg.avgSoilMoisture = sumSoilMoisture / averagingBufferCount;

    // Set timestamps from first and last readings
    avg.sampleStartUptimeMs = averagingBuffer[0].monotonicMs;
    avg.sampleEndUptimeMs = averagingBuffer[averagingBufferCount - 1].monotonicMs;

    // Epoch timestamps will be set to 0 for now (time sync in Task 7)
    avg.sampleStartEpochMs = 0;
    avg.sampleEndEpochMs = 0;
    avg.deviceBootEpochMs = 0;

    // Set metadata
    avg.sampleCount = averagingBufferCount;
    avg.sensorStatus = averagingBuffer[averagingBufferCount - 1].sensorStatus;
    avg.timeSynced = false;  // Will be updated when time sync is implemented

    // Get current uptime (use last reading's timestamp as approximation)
    avg.uptimeMs = averagingBuffer[averagingBufferCount - 1].monotonicMs;

    // Generate batch ID
    generateBatchId(avg.batchId, sizeof(avg.batchId), avg, avg.timeSynced);

    return avg;
}

void DataManager::clearAveragingBuffer() {
    averagingBufferCount = 0;
    // No need to zero memory, we track count
}

void DataManager::generateBatchId(char* batchId, size_t bufferSize, const AveragedData& data,
                                  bool timeSynced) {
    // Format: device_e_epochStart_epochEnd (synced) or device_u_uptimeStart_uptimeEnd (unsynced)
    // For now, use "device" as placeholder until device_id is available from config

    if (timeSynced && data.sampleStartEpochMs > 0) {
        // Use epoch timestamps
        snprintf(batchId, bufferSize, "device_e_%llu_%llu",
                 (unsigned long long)data.sampleStartEpochMs,
                 (unsigned long long)data.sampleEndEpochMs);
    } else {
        // Use uptime timestamps
        snprintf(batchId, bufferSize, "device_u_%lu_%lu", (unsigned long)data.sampleStartUptimeMs,
                 (unsigned long)data.sampleEndUptimeMs);
    }
}

// ============================================================================
// Transmission Ring Buffer Operations (Task 10)
// ============================================================================

void DataManager::bufferForTransmission(const AveragedData& data) {
    // Check if buffer is at 90% capacity (45 out of 50)
    const uint16_t OVERFLOW_THRESHOLD = (MAX_DATA_BUFFER_SIZE * 9) / 10;  // 90%

    if (dataBufferCount >= OVERFLOW_THRESHOLD) {
        // Buffer is at or above 90% - discard oldest entry
        if (dataBufferCount > 0) {
            advanceTail();  // Remove oldest
            dataBufferCount--;
            bufferOverflowCount++;  // Increment overflow counter
        }
    }

    // Add new data at head position
    dataBuffer[dataBufferHead] = data;
    advanceHead();

    // Increment count if not at max capacity
    if (dataBufferCount < MAX_DATA_BUFFER_SIZE) {
        dataBufferCount++;
    }
}

uint16_t DataManager::getBufferedDataCount() const {
    return dataBufferCount;
}

const AveragedData* DataManager::getBufferedData(uint16_t& count) const {
    count = dataBufferCount;

    // If buffer is empty, return nullptr
    if (dataBufferCount == 0) {
        return nullptr;
    }

    // Return pointer to the tail (oldest entry)
    // Caller must iterate through the ring buffer properly
    return &dataBuffer[dataBufferTail];
}

void DataManager::clearAcknowledgedData(const char* batchIds[], uint16_t batchIdCount) {
    if (batchIdCount == 0 || dataBufferCount == 0) {
        return;
    }

    // We need to remove entries that match any of the acknowledged batch IDs
    // Since this is a ring buffer, we'll compact it by keeping only unacknowledged entries

    // Create a temporary buffer to hold unacknowledged entries
    AveragedData tempBuffer[MAX_DATA_BUFFER_SIZE];
    uint16_t tempCount = 0;

    // Iterate through current buffer
    uint16_t currentIndex = dataBufferTail;
    for (uint16_t i = 0; i < dataBufferCount; i++) {
        bool isAcknowledged = false;

        // Check if this entry's batch ID matches any acknowledged ID
        for (uint16_t j = 0; j < batchIdCount; j++) {
            if (strcmp(dataBuffer[currentIndex].batchId, batchIds[j]) == 0) {
                isAcknowledged = true;
                break;
            }
        }

        // If not acknowledged, keep it
        if (!isAcknowledged) {
            tempBuffer[tempCount] = dataBuffer[currentIndex];
            tempCount++;
        }

        // Move to next entry in ring buffer
        currentIndex = (currentIndex + 1) % MAX_DATA_BUFFER_SIZE;
    }

    // Copy unacknowledged entries back to main buffer
    dataBufferHead = 0;
    dataBufferTail = 0;
    dataBufferCount = tempCount;

    for (uint16_t i = 0; i < tempCount; i++) {
        dataBuffer[i] = tempBuffer[i];
    }

    // Update head to point after last entry
    if (tempCount > 0) {
        dataBufferHead = tempCount % MAX_DATA_BUFFER_SIZE;
    }
}

bool DataManager::isBufferNearFull() const {
    // Return true if buffer is > 80% full (warning threshold)
    const uint16_t WARNING_THRESHOLD = (MAX_DATA_BUFFER_SIZE * 8) / 10;  // 80%
    return dataBufferCount > WARNING_THRESHOLD;
}

uint16_t DataManager::getBufferOverflowCount() const {
    return bufferOverflowCount;
}

// ============================================================================
// Ring Buffer Helper Methods
// ============================================================================

void DataManager::advanceHead() {
    dataBufferHead = (dataBufferHead + 1) % MAX_DATA_BUFFER_SIZE;
}

void DataManager::advanceTail() {
    dataBufferTail = (dataBufferTail + 1) % MAX_DATA_BUFFER_SIZE;
}

// ============================================================================
// Display Buffer Operations (Task 11)
// ============================================================================

void DataManager::addToDisplayBuffer(const SensorReadings& reading) {
    // Check if enough time has elapsed since last display update (1 minute)
    uint32_t currentTime = reading.monotonicMs;

    if (lastDisplayUpdate == 0 || (currentTime - lastDisplayUpdate) >= DISPLAY_INTERVAL_MS) {
        // Update timestamp
        lastDisplayUpdate = currentTime;

        // Add data point for each sensor type
        // BME280 Temperature
        uint8_t sensorIdx = static_cast<uint8_t>(SensorType::BME280_TEMP);
        displayBuffer[sensorIdx][displayBufferHead[sensorIdx]].value = reading.bme280Temp;
        displayBuffer[sensorIdx][displayBufferHead[sensorIdx]].timestamp = currentTime;
        displayBufferHead[sensorIdx] = (displayBufferHead[sensorIdx] + 1) % MAX_DISPLAY_POINTS;
        if (displayBufferCount[sensorIdx] < MAX_DISPLAY_POINTS) {
            displayBufferCount[sensorIdx]++;
        }

        // DS18B20 Temperature
        sensorIdx = static_cast<uint8_t>(SensorType::DS18B20_TEMP);
        displayBuffer[sensorIdx][displayBufferHead[sensorIdx]].value = reading.ds18b20Temp;
        displayBuffer[sensorIdx][displayBufferHead[sensorIdx]].timestamp = currentTime;
        displayBufferHead[sensorIdx] = (displayBufferHead[sensorIdx] + 1) % MAX_DISPLAY_POINTS;
        if (displayBufferCount[sensorIdx] < MAX_DISPLAY_POINTS) {
            displayBufferCount[sensorIdx]++;
        }

        // Humidity
        sensorIdx = static_cast<uint8_t>(SensorType::HUMIDITY);
        displayBuffer[sensorIdx][displayBufferHead[sensorIdx]].value = reading.humidity;
        displayBuffer[sensorIdx][displayBufferHead[sensorIdx]].timestamp = currentTime;
        displayBufferHead[sensorIdx] = (displayBufferHead[sensorIdx] + 1) % MAX_DISPLAY_POINTS;
        if (displayBufferCount[sensorIdx] < MAX_DISPLAY_POINTS) {
            displayBufferCount[sensorIdx]++;
        }

        // Pressure
        sensorIdx = static_cast<uint8_t>(SensorType::PRESSURE);
        displayBuffer[sensorIdx][displayBufferHead[sensorIdx]].value = reading.pressure;
        displayBuffer[sensorIdx][displayBufferHead[sensorIdx]].timestamp = currentTime;
        displayBufferHead[sensorIdx] = (displayBufferHead[sensorIdx] + 1) % MAX_DISPLAY_POINTS;
        if (displayBufferCount[sensorIdx] < MAX_DISPLAY_POINTS) {
            displayBufferCount[sensorIdx]++;
        }

        // Soil Moisture
        sensorIdx = static_cast<uint8_t>(SensorType::SOIL_MOISTURE);
        displayBuffer[sensorIdx][displayBufferHead[sensorIdx]].value = reading.soilMoisture;
        displayBuffer[sensorIdx][displayBufferHead[sensorIdx]].timestamp = currentTime;
        displayBufferHead[sensorIdx] = (displayBufferHead[sensorIdx] + 1) % MAX_DISPLAY_POINTS;
        if (displayBufferCount[sensorIdx] < MAX_DISPLAY_POINTS) {
            displayBufferCount[sensorIdx]++;
        }
    }
}

uint16_t DataManager::getDisplayDataCount(SensorType type) const {
    uint8_t sensorIdx = static_cast<uint8_t>(type);
    if (sensorIdx >= NUM_SENSORS) {
        return 0;
    }
    return displayBufferCount[sensorIdx];
}

const DisplayPoint* DataManager::getDisplayData(SensorType type, uint16_t& count,
                                                uint16_t maxPoints) const {
    uint8_t sensorIdx = static_cast<uint8_t>(type);
    if (sensorIdx >= NUM_SENSORS) {
        count = 0;
        return nullptr;
    }

    uint16_t availableCount = displayBufferCount[sensorIdx];

    if (availableCount == 0) {
        count = 0;
        return nullptr;
    }

    // If maxPoints is 0 or >= available count, return all data without downsampling
    if (maxPoints == 0 || maxPoints >= availableCount) {
        count = availableCount;
        return displayBuffer[sensorIdx];
    }

    // Need to downsample - copy to linear buffer first, then downsample
    // For ring buffer, we need to copy in correct order
    DisplayPoint linearBuffer[MAX_DISPLAY_POINTS];
    uint16_t head = displayBufferHead[sensorIdx];

    if (availableCount < MAX_DISPLAY_POINTS) {
        // Buffer not full yet, data is from index 0 to head-1
        memcpy(linearBuffer, displayBuffer[sensorIdx], availableCount * sizeof(DisplayPoint));
    } else {
        // Buffer is full, need to copy in ring order: from head to end, then from 0 to head-1
        uint16_t firstPartSize = MAX_DISPLAY_POINTS - head;
        memcpy(linearBuffer, &displayBuffer[sensorIdx][head], firstPartSize * sizeof(DisplayPoint));
        memcpy(&linearBuffer[firstPartSize], displayBuffer[sensorIdx], head * sizeof(DisplayPoint));
    }

    // Downsample to maxPoints
    downsampleDisplayData(linearBuffer, availableCount, downsampledBuffer, maxPoints);
    count = maxPoints;
    return downsampledBuffer;
}

void DataManager::downsampleDisplayData(const DisplayPoint* source, uint16_t sourceCount,
                                        DisplayPoint* dest, uint16_t maxPoints) const {
    if (sourceCount <= maxPoints) {
        // No downsampling needed
        memcpy(dest, source, sourceCount * sizeof(DisplayPoint));
        return;
    }

    // Simple downsampling: pick evenly spaced points
    // Always include first and last points
    dest[0] = source[0];
    dest[maxPoints - 1] = source[sourceCount - 1];

    // Fill in intermediate points
    for (uint16_t i = 1; i < maxPoints - 1; i++) {
        // Calculate source index for this destination point
        uint16_t sourceIdx = (i * (sourceCount - 1)) / (maxPoints - 1);
        dest[i] = source[sourceIdx];
    }
}
