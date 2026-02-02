#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "DataManager.h"

// Test helper to create a sample AveragedData with unique batch ID
AveragedData createSampleData(uint32_t startUptime, uint32_t endUptime) {
    AveragedData data = {};
    data.avgBme280Temp = 22.5f;
    data.avgDs18b20Temp = 21.8f;
    data.avgHumidity = 45.2f;
    data.avgPressure = 1013.25f;
    data.avgSoilMoisture = 62.3f;
    data.sampleStartUptimeMs = startUptime;
    data.sampleEndUptimeMs = endUptime;
    data.sampleCount = 20;
    data.sensorStatus = 0xFF;
    data.timeSynced = false;

    // Generate batch ID
    snprintf(data.batchId, sizeof(data.batchId), "device_u_%lu_%lu", (unsigned long)startUptime,
             (unsigned long)endUptime);

    return data;
}

// Test: Empty buffer returns zero count
void test_empty_buffer_returns_zero_count() {
    DataManager dm;
    TEST_ASSERT_EQUAL_UINT16(0, dm.getBufferedDataCount());
}

// Test: Adding single item increases count
void test_add_single_item_increases_count() {
    DataManager dm;
    AveragedData data = createSampleData(1000, 2000);

    dm.bufferForTransmission(data);

    TEST_ASSERT_EQUAL_UINT16(1, dm.getBufferedDataCount());
}

// Test: Adding multiple items increases count
void test_add_multiple_items_increases_count() {
    DataManager dm;

    for (uint16_t i = 0; i < 10; i++) {
        AveragedData data = createSampleData(i * 1000, (i + 1) * 1000);
        dm.bufferForTransmission(data);
    }

    TEST_ASSERT_EQUAL_UINT16(10, dm.getBufferedDataCount());
}

// Test: Buffer overflow at 90% threshold discards oldest
void test_buffer_overflow_at_90_percent_discards_oldest() {
    DataManager dm;

    // Fill buffer to 90% (45 out of 50)
    for (uint16_t i = 0; i < 45; i++) {
        AveragedData data = createSampleData(i * 1000, (i + 1) * 1000);
        dm.bufferForTransmission(data);
    }

    TEST_ASSERT_EQUAL_UINT16(45, dm.getBufferedDataCount());
    TEST_ASSERT_EQUAL_UINT16(0, dm.getBufferOverflowCount());

    // Add one more - should trigger overflow
    AveragedData newData = createSampleData(45000, 46000);
    dm.bufferForTransmission(newData);

    // Count should still be 45 (oldest removed, new added)
    TEST_ASSERT_EQUAL_UINT16(45, dm.getBufferedDataCount());
    TEST_ASSERT_EQUAL_UINT16(1, dm.getBufferOverflowCount());
}

// Test: Multiple overflows increment counter
void test_multiple_overflows_increment_counter() {
    DataManager dm;

    // Fill to 90%
    for (uint16_t i = 0; i < 45; i++) {
        AveragedData data = createSampleData(i * 1000, (i + 1) * 1000);
        dm.bufferForTransmission(data);
    }

    // Add 5 more items - each should trigger overflow
    for (uint16_t i = 45; i < 50; i++) {
        AveragedData data = createSampleData(i * 1000, (i + 1) * 1000);
        dm.bufferForTransmission(data);
    }

    TEST_ASSERT_EQUAL_UINT16(45, dm.getBufferedDataCount());
    TEST_ASSERT_EQUAL_UINT16(5, dm.getBufferOverflowCount());
}

// Test: isBufferNearFull returns true above 80%
void test_is_buffer_near_full_above_80_percent() {
    DataManager dm;

    // Add 40 items (80%)
    for (uint16_t i = 0; i < 40; i++) {
        AveragedData data = createSampleData(i * 1000, (i + 1) * 1000);
        dm.bufferForTransmission(data);
    }

    TEST_ASSERT_FALSE(dm.isBufferNearFull());  // Exactly 80% is not "near full"

    // Add one more (82%)
    AveragedData data = createSampleData(40000, 41000);
    dm.bufferForTransmission(data);

    TEST_ASSERT_TRUE(dm.isBufferNearFull());
}

// Test: clearAcknowledgedData removes matching entries
void test_clear_acknowledged_data_removes_matching_entries() {
    DataManager dm;

    // Add 5 items
    for (uint16_t i = 0; i < 5; i++) {
        AveragedData data = createSampleData(i * 1000, (i + 1) * 1000);
        dm.bufferForTransmission(data);
    }

    TEST_ASSERT_EQUAL_UINT16(5, dm.getBufferedDataCount());

    // Acknowledge items 1 and 3 (batch IDs: device_u_1000_2000 and device_u_3000_4000)
    const char* ackBatchIds[] = {"device_u_1000_2000", "device_u_3000_4000"};

    dm.clearAcknowledgedData(ackBatchIds, 2);

    // Should have 3 items remaining
    TEST_ASSERT_EQUAL_UINT16(3, dm.getBufferedDataCount());
}

// Test: clearAcknowledgedData with no matches keeps all entries
void test_clear_acknowledged_data_no_matches_keeps_all() {
    DataManager dm;

    // Add 3 items
    for (uint16_t i = 0; i < 3; i++) {
        AveragedData data = createSampleData(i * 1000, (i + 1) * 1000);
        dm.bufferForTransmission(data);
    }

    TEST_ASSERT_EQUAL_UINT16(3, dm.getBufferedDataCount());

    // Try to acknowledge non-existent batch IDs
    const char* ackBatchIds[] = {"device_u_99000_100000", "device_u_88000_89000"};

    dm.clearAcknowledgedData(ackBatchIds, 2);

    // Should still have 3 items
    TEST_ASSERT_EQUAL_UINT16(3, dm.getBufferedDataCount());
}

// Test: clearAcknowledgedData with all matches empties buffer
void test_clear_acknowledged_data_all_matches_empties_buffer() {
    DataManager dm;

    // Add 3 items
    for (uint16_t i = 0; i < 3; i++) {
        AveragedData data = createSampleData(i * 1000, (i + 1) * 1000);
        dm.bufferForTransmission(data);
    }

    TEST_ASSERT_EQUAL_UINT16(3, dm.getBufferedDataCount());

    // Acknowledge all batch IDs
    const char* ackBatchIds[] = {"device_u_0_1000", "device_u_1000_2000", "device_u_2000_3000"};

    dm.clearAcknowledgedData(ackBatchIds, 3);

    // Buffer should be empty
    TEST_ASSERT_EQUAL_UINT16(0, dm.getBufferedDataCount());
}

// Test: getBufferedData returns nullptr for empty buffer
void test_get_buffered_data_empty_returns_null() {
    DataManager dm;
    uint16_t count = 0;

    const AveragedData* data = dm.getBufferedData(count);

    TEST_ASSERT_NULL(data);
    TEST_ASSERT_EQUAL_UINT16(0, count);
}

// Test: getBufferedData returns correct count for non-empty buffer
void test_get_buffered_data_returns_correct_count() {
    DataManager dm;

    // Add 5 items
    for (uint16_t i = 0; i < 5; i++) {
        AveragedData data = createSampleData(i * 1000, (i + 1) * 1000);
        dm.bufferForTransmission(data);
    }

    uint16_t count = 0;
    const AveragedData* data = dm.getBufferedData(count);

    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_UINT16(5, count);
}

// Test: Ring buffer wraps around correctly
void test_ring_buffer_wraps_around() {
    DataManager dm;

    // Fill buffer completely (50 items)
    for (uint16_t i = 0; i < 50; i++) {
        AveragedData data = createSampleData(i * 1000, (i + 1) * 1000);
        dm.bufferForTransmission(data);
    }

    TEST_ASSERT_EQUAL_UINT16(50, dm.getBufferedDataCount());

    // Add more items - should wrap around and trigger overflow
    for (uint16_t i = 50; i < 55; i++) {
        AveragedData data = createSampleData(i * 1000, (i + 1) * 1000);
        dm.bufferForTransmission(data);
    }

    // Count should be at 90% threshold (45)
    TEST_ASSERT_EQUAL_UINT16(45, dm.getBufferedDataCount());
    TEST_ASSERT_TRUE(dm.getBufferOverflowCount() > 0);
}

void setUp(void) {
    // Set up code if needed
}

void tearDown(void) {
    // Clean up code if needed
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_empty_buffer_returns_zero_count);
    RUN_TEST(test_add_single_item_increases_count);
    RUN_TEST(test_add_multiple_items_increases_count);
    RUN_TEST(test_buffer_overflow_at_90_percent_discards_oldest);
    RUN_TEST(test_multiple_overflows_increment_counter);
    RUN_TEST(test_is_buffer_near_full_above_80_percent);
    RUN_TEST(test_clear_acknowledged_data_removes_matching_entries);
    RUN_TEST(test_clear_acknowledged_data_no_matches_keeps_all);
    RUN_TEST(test_clear_acknowledged_data_all_matches_empties_buffer);
    RUN_TEST(test_get_buffered_data_empty_returns_null);
    RUN_TEST(test_get_buffered_data_returns_correct_count);
    RUN_TEST(test_ring_buffer_wraps_around);

    return UNITY_END();
}
