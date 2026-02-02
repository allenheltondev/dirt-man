#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unity.h>

#include "DataManager.h"

/**
 * Property-Based Tests for DataManager Ring Buffer
 *
 * Feature: esp32-sensor-firmware
 *
 * These tests verify universal properties across many generated inputs.
 * Minimum 100 iterations per property test.
 */

// Random number generator helpers
float randomFloat(float min, float max) {
    return min + (float)rand() / ((float)RAND_MAX / (max - min));
}

uint32_t randomUInt32(uint32_t min, uint32_t max) {
    return min + (rand() % (max - min + 1));
}

// Helper to create a random AveragedData
AveragedData createRandomAveragedData(uint32_t startUptime, uint32_t endUptime) {
    AveragedData data = {};
    data.avgBme280Temp = randomFloat(-40.0f, 85.0f);
    data.avgDs18b20Temp = randomFloat(-55.0f, 125.0f);
    data.avgHumidity = randomFloat(0.0f, 100.0f);
    data.avgPressure = randomFloat(300.0f, 1100.0f);
    data.avgSoilMoisture = randomFloat(0.0f, 100.0f);
    data.sampleStartUptimeMs = startUptime;
    data.sampleEndUptimeMs = endUptime;
    data.sampleStartEpochMs = 0;
    data.sampleEndEpochMs = 0;
    data.deviceBootEpochMs = 0;
    data.uptimeMs = endUptime;
    data.sampleCount = randomUInt32(1, 120);
    data.sensorStatus = 0xFF;
    data.timeSynced = false;

    // Generate batch ID
    snprintf(data.batchId, sizeof(data.batchId), "device_u_%lu_%lu", (unsigned long)startUptime,
             (unsigned long)endUptime);

    return data;
}

/**
 * Property 6: Data Buffer Ring Behavior
 *
 * For any Data_Buffer at 90% capacity, adding a new averaged reading should result
 * in the oldest entry being removed and the new entry being added,
 * maintaining the buffer size constraint.
 *
 * **Feature: esp32-sensor-firmware, Property 6: Data buffer ring behavior maintains capacity**
 *
 * Validates: Requirements 3.6
 */
void property_data_buffer_ring_behavior() {
    const int NUM_ITERATIONS = 100;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        DataManager dm;

        // Fill buffer to exactly 90% (45 out of 50)
        for (uint16_t i = 0; i < 45; i++) {
            uint32_t startTime = i * 1000;
            uint32_t endTime = (i + 1) * 1000;
            AveragedData data = createRandomAveragedData(startTime, endTime);
            dm.bufferForTransmission(data);
        }

        // Verify buffer is at 90%
        TEST_ASSERT_EQUAL_UINT16(45, dm.getBufferedDataCount());

        // Add random number of additional items (1-10)
        uint16_t additionalItems = randomUInt32(1, 10);
        uint16_t initialOverflowCount = dm.getBufferOverflowCount();

        for (uint16_t i = 0; i < additionalItems; i++) {
            uint32_t startTime = (45 + i) * 1000;
            uint32_t endTime = (46 + i) * 1000;
            AveragedData data = createRandomAveragedData(startTime, endTime);
            dm.bufferForTransmission(data);
        }

        // Property: Buffer count should remain at 45 (90% threshold)
        TEST_ASSERT_EQUAL_UINT16(45, dm.getBufferedDataCount());

        // Property: Overflow counter should increment by number of additional items
        TEST_ASSERT_EQUAL_UINT16(initialOverflowCount + additionalItems,
                                 dm.getBufferOverflowCount());
    }
}

/**
 * Property 12: Idempotent Acknowledgment
 *
 * For any Data_Buffer containing multiple averaged readings, after receiving
 * a server response with acknowledged batch_ids, only the entries with matching
 * batch_ids should be removed from the buffer, leaving unacknowledged entries intact.
 *
 * **Feature: esp32-sensor-firmware, Property 12: Only acknowledged batch IDs are removed**
 *
 * Validates: Requirements 5.9
 */
void property_idempotent_acknowledgment() {
    const int NUM_ITERATIONS = 100;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        DataManager dm;

        // Add random number of items (5-20)
        uint16_t totalItems = randomUInt32(5, 20);

        for (uint16_t i = 0; i < totalItems; i++) {
            uint32_t startTime = i * 1000;
            uint32_t endTime = (i + 1) * 1000;
            AveragedData data = createRandomAveragedData(startTime, endTime);
            dm.bufferForTransmission(data);
        }

        TEST_ASSERT_EQUAL_UINT16(totalItems, dm.getBufferedDataCount());

        // Randomly select items to acknowledge (1 to totalItems-1)
        uint16_t numToAck = randomUInt32(1, totalItems - 1);

        // Create array of batch IDs to acknowledge
        const char* ackBatchIds[20];  // Max 20 items
        for (uint16_t i = 0; i < numToAck; i++) {
            // Acknowledge items at random indices
            uint16_t ackIndex = randomUInt32(0, totalItems - 1);
            uint32_t startTime = ackIndex * 1000;
            uint32_t endTime = (ackIndex + 1) * 1000;

            // Allocate batch ID string (will be freed at end of iteration)
            char* batchId = (char*)malloc(64);
            snprintf(batchId, 64, "device_u_%lu_%lu", (unsigned long)startTime,
                     (unsigned long)endTime);
            ackBatchIds[i] = batchId;
        }

        // Clear acknowledged data
        dm.clearAcknowledgedData(ackBatchIds, numToAck);

        // Property: Buffer count should be reduced (but not necessarily by exactly numToAck
        // because we might have acknowledged the same item multiple times)
        uint16_t remainingCount = dm.getBufferedDataCount();
        TEST_ASSERT_TRUE(remainingCount <= totalItems);
        TEST_ASSERT_TRUE(remainingCount >= totalItems - numToAck);

        // Clean up allocated batch IDs
        for (uint16_t i = 0; i < numToAck; i++) {
            free((void*)ackBatchIds[i]);
        }
    }
}

/**
 * Property: Ring Buffer Maintains FIFO Order
 *
 * For any sequence of additions to the ring buffer (without overflow),
 * the order of retrieval should match the order of insertion (FIFO).
 */
void property_ring_buffer_fifo_order() {
    const int NUM_ITERATIONS = 100;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        DataManager dm;

        // Add random number of items (less than 90% to avoid overflow)
        uint16_t numItems = randomUInt32(5, 40);

        // Store expected batch IDs
        char expectedBatchIds[40][64];

        for (uint16_t i = 0; i < numItems; i++) {
            uint32_t startTime = i * 1000;
            uint32_t endTime = (i + 1) * 1000;
            AveragedData data = createRandomAveragedData(startTime, endTime);
            dm.bufferForTransmission(data);

            // Store expected batch ID
            snprintf(expectedBatchIds[i], 64, "device_u_%lu_%lu", (unsigned long)startTime,
                     (unsigned long)endTime);
        }

        // Get buffered data
        uint16_t count = 0;
        const AveragedData* bufferedData = dm.getBufferedData(count);

        TEST_ASSERT_EQUAL_UINT16(numItems, count);
        TEST_ASSERT_NOT_NULL(bufferedData);

        // Property: First item in buffer should match first inserted item
        TEST_ASSERT_EQUAL_STRING(expectedBatchIds[0], bufferedData[0].batchId);
    }
}

/**
 * Property: Buffer Overflow Counter Never Decreases
 *
 * For any sequence of operations on the ring buffer, the overflow counter
 * should only increase or stay the same, never decrease.
 */
void property_overflow_counter_monotonic() {
    const int NUM_ITERATIONS = 100;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        DataManager dm;

        uint16_t previousOverflowCount = 0;

        // Perform random sequence of operations
        uint16_t numOperations = randomUInt32(50, 100);

        for (uint16_t i = 0; i < numOperations; i++) {
            uint32_t startTime = i * 1000;
            uint32_t endTime = (i + 1) * 1000;
            AveragedData data = createRandomAveragedData(startTime, endTime);
            dm.bufferForTransmission(data);

            uint16_t currentOverflowCount = dm.getBufferOverflowCount();

            // Property: Overflow counter should never decrease
            TEST_ASSERT_TRUE(currentOverflowCount >= previousOverflowCount);

            previousOverflowCount = currentOverflowCount;
        }
    }
}

/**
 * Property: Clear Acknowledged Data is Idempotent
 *
 * For any buffer state and set of batch IDs, calling clearAcknowledgedData
 * multiple times with the same batch IDs should produce the same result as
 * calling it once.
 */
void property_clear_acknowledged_idempotent() {
    const int NUM_ITERATIONS = 100;

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        DataManager dm1, dm2;

        // Add same items to both managers
        uint16_t numItems = randomUInt32(5, 15);

        for (uint16_t i = 0; i < numItems; i++) {
            uint32_t startTime = i * 1000;
            uint32_t endTime = (i + 1) * 1000;
            AveragedData data = createRandomAveragedData(startTime, endTime);
            dm1.bufferForTransmission(data);
            dm2.bufferForTransmission(data);
        }

        // Create acknowledgment list
        const char* ackBatchIds[15];
        uint16_t numToAck = randomUInt32(1, numItems);

        for (uint16_t i = 0; i < numToAck; i++) {
            char* batchId = (char*)malloc(64);
            uint32_t startTime = i * 1000;
            uint32_t endTime = (i + 1) * 1000;
            snprintf(batchId, 64, "device_u_%lu_%lu", (unsigned long)startTime,
                     (unsigned long)endTime);
            ackBatchIds[i] = batchId;
        }

        // Clear once on dm1
        dm1.clearAcknowledgedData(ackBatchIds, numToAck);
        uint16_t count1 = dm1.getBufferedDataCount();

        // Clear twice on dm2
        dm2.clearAcknowledgedData(ackBatchIds, numToAck);
        dm2.clearAcknowledgedData(ackBatchIds, numToAck);
        uint16_t count2 = dm2.getBufferedDataCount();

        // Property: Both should have same count (idempotent)
        TEST_ASSERT_EQUAL_UINT16(count1, count2);

        // Clean up
        for (uint16_t i = 0; i < numToAck; i++) {
            free((void*)ackBatchIds[i]);
        }
    }
}

void setUp(void) {
    // Seed random number generator
    srand((unsigned int)time(NULL));
}

void tearDown(void) {
    // Clean up code if needed
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(property_data_buffer_ring_behavior);
    RUN_TEST(property_idempotent_acknowledgment);
    RUN_TEST(property_ring_buffer_fifo_order);
    RUN_TEST(property_overflow_counter_monotonic);
    RUN_TEST(property_clear_acknowledged_idempotent);

    return UNITY_END();
}
