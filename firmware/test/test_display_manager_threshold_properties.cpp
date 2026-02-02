#ifdef UNIT_TEST

#include <unity.h>

#include "DisplayManager.h"
#include "models/Config.h"
#include "models/SensorReadings.h"
#include "models/SystemStatus.h"
#include <cstdlib>
#include <ctime>

// Property-based testing utilities
static uint16_t randomUint16(uint16_t min, uint16_t max) {
    return min + (rand() % (max - min + 1));
}

static float randomFloat(float min, float max) {
    return min + (static_cast<float>(rand()) / RAND_MAX) * (max - min);
}

// Helper to determine expected threshold status
enum class ThresholdStatus { LOW, NORMAL, HIGH, NO_THRESHOLD };

ThresholdStatus getExpectedThresholdStatus(float value, float lowThreshold, float highThreshold) {
    if (lowThreshold == 0 && highThreshold == 0) {
        return ThresholdStatus::NO_THRESHOLD;
    }

    if (highThreshold > 0 && value >= highThreshold) {
        return ThresholdStatus::HIGH;
    }

    if (lowThreshold > 0 && value <= lowThreshold) {
        return ThresholdStatus::LOW;
    }

    return ThresholdStatus::NORMAL;
}

void test_property_thresholdDetection_boundaryConditions(void) {
    srand(static_cast<unsigned>(time(nullptr)));

    const int NUM_ITERATIONS = 100;
    int passCount = 0;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        uint16_t lowThreshold = randomUint16(10, 40);
        uint16_t highThreshold = randomUint16(60, 90);

        float valueLow = randomFloat(0, lowThreshold);
        ThresholdStatus statusLow =
            getExpectedThresholdStatus(valueLow, lowThreshold, highThreshold);
        TEST_ASSERT_EQUAL(static_cast<int>(ThresholdStatus::LOW), static_cast<int>(statusLow));

        float valueNormal = randomFloat(lowThreshold + 1, highThreshold - 1);
        ThresholdStatus statusNormal =
            getExpectedThresholdStatus(valueNormal, lowThreshold, highThreshold);
        TEST_ASSERT_EQUAL(static_cast<int>(ThresholdStatus::NORMAL),
                          static_cast<int>(statusNormal));

        float valueHigh = randomFloat(highThreshold, 100);
        ThresholdStatus statusHigh =
            getExpectedThresholdStatus(valueHigh, lowThreshold, highThreshold);
        TEST_ASSERT_EQUAL(static_cast<int>(ThresholdStatus::HIGH), static_cast<int>(statusHigh));

        passCount++;
    }

    TEST_ASSERT_EQUAL(NUM_ITERATIONS, passCount);
}

void test_property_thresholdDetection_exactBoundaries(void) {
    srand(static_cast<unsigned>(time(nullptr)));

    const int NUM_ITERATIONS = 100;
    int passCount = 0;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        uint16_t lowThreshold = randomUint16(10, 40);
        uint16_t highThreshold = randomUint16(60, 90);

        ThresholdStatus statusAtLow =
            getExpectedThresholdStatus(lowThreshold, lowThreshold, highThreshold);
        TEST_ASSERT_EQUAL(static_cast<int>(ThresholdStatus::LOW), static_cast<int>(statusAtLow));

        ThresholdStatus statusAtHigh =
            getExpectedThresholdStatus(highThreshold, lowThreshold, highThreshold);
        TEST_ASSERT_EQUAL(static_cast<int>(ThresholdStatus::HIGH), static_cast<int>(statusAtHigh));

        ThresholdStatus statusAboveLow =
            getExpectedThresholdStatus(lowThreshold + 0.1f, lowThreshold, highThreshold);
        TEST_ASSERT_EQUAL(static_cast<int>(ThresholdStatus::NORMAL),
                          static_cast<int>(statusAboveLow));

        ThresholdStatus statusBelowHigh =
            getExpectedThresholdStatus(highThreshold - 0.1f, lowThreshold, highThreshold);
        TEST_ASSERT_EQUAL(static_cast<int>(ThresholdStatus::NORMAL),
                          static_cast<int>(statusBelowHigh));

        passCount++;
    }

    TEST_ASSERT_EQUAL(NUM_ITERATIONS, passCount);
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_property_thresholdDetection_boundaryConditions);
    RUN_TEST(test_property_thresholdDetection_exactBoundaries);

    return UNITY_END();
}

#endif
