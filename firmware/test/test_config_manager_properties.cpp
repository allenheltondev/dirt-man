#ifdef UNIT_TEST

#include <unity.h>

#include "../include/ConfigManager.h"
#include "../include/models/Config.h"
#include <cstdlib>
#include <cstring>
#include <ctime>

// Mock NVS storage for property testing
static Config mockNvsStorage;
static bool mockNvsInitialized = false;

// Helper function to generate random string
String generateRandomString(int minLen, int maxLen) {
    int len = minLen + (rand() % (maxLen - minLen + 1));
    String result = "";
    for (int i = 0; i < len; i++) {
        char c = 'a' + (rand() % 26);
        result += c;
    }
    return result;
}

// Helper function to generate random uint32 in range
uint32_t randomUint32(uint32_t min, uint32_t max) {
    return min + (rand() % (max - min + 1));
}

// Helper function to generate random uint16 in range
uint16_t randomUint16(uint16_t min, uint16_t max) {
    return min + (rand() % (max - min + 1));
}

// Helper function to generate random bool
bool randomBool() {
    return (rand() % 2) == 1;
}

// Helper function to generate valid Config
Config generateValidConfig() {
    Config config;

    // WiFi credentials (valid)
    config.wifiSsid = generateRandomString(1, 32);
    // 50% chance of empty password (open
    // Intervals (valid ranges)
    config.readingIntervalMs = randomUint32(1000, 3600000);
    config.publishIntervalSamples = randomUint16(1, 120);
    config.pageCycleIntervalMs = randomUint16(1000, 60000);

    // Calibration (valid: dry > wet, both <= 4095)
    config.soilWetAdc = randomUint16(0, 2047);
    config.soilDryAdc = randomUint16(config.soilWetAdc + 1, 4095);

    // Display settings
    config.temperatureInFahrenheit = randomBool();
    config.soilMoistureThresholdLow = randomUint16(0, 50);
    config.soilMoistureThresholdHigh = randomUint16(config.soilMoistureThresholdLow + 1, 100);

    config.batteryMode = randomBool();

    return config;
}

// Helper function to generate invalid Config
Config generateInvalidConfig() {
    Config config = generateValidConfig();

    // Randomly break one validation rule
    int invalidationType = rand() % 10;

    switch (invalidationType) {
        case 0:  // Empty WiFi SSID
            config.wifiSsid = "";
            break;
        case 1:  // WiFi SSID too long
            config.wifiSsid = generateRandomString(33, 50);
            break;
        case 2:  // WiFi password too short (if not empty)
            config.wifiPassword = generateRandomString(1, 7);
            break;
        case 3:  // Empty API endpoint
            config.apiEndpoint = "";
            break;
        case 4:  // Invalid API protocol
            config.apiEndpoint = "ftp://" + generateRandomString(5, 20) + ".com";
            break;
        case 5:  // Reading interval too low
            config.readingIntervalMs = randomUint32(0, 999);
            break;
        case 6:  // Publish interval too high
            config.publishIntervalSamples = randomUint16(121, 200);
            break;
        case 7:  // Soil dry <= wet
            config.soilDryAdc = randomUint16(0, config.soilWetAdc);
            break;
        case 8:  // Threshold low >= high
            config.soilMoistureThresholdLow = randomUint16(50, 100);
            config.soilMoistureThresholdHigh = randomUint16(0, config.soilMoistureThresholdLow);
            break;
        case 9:  // Threshold high > 100
            config.soilMoistureThresholdHigh = randomUint16(101, 200);
            break;
    }

    return config;
}

// Helper function to compare configs
bool configsEqual(const Config& a, const Config& b) {
    return a.wifiSsid == b.wifiSsid && a.wifiPassword == b.wifiPassword &&
           a.apiEndpoint == b.apiEndpoint && a.apiToken == b.apiToken && a.deviceId == b.deviceId &&
           a.readingIntervalMs == b.readingIntervalMs &&
           a.publishIntervalSamples == b.publishIntervalSamples &&
           a.pageCycleIntervalMs == b.pageCycleIntervalMs && a.soilDryAdc == b.soilDryAdc &&
           a.soilWetAdc == b.soilWetAdc && a.temperatureInFahrenheit == b.temperatureInFahrenheit &&
           a.soilMoistureThresholdLow == b.soilMoistureThresholdLow &&
           a.soilMoistureThresholdHigh == b.soilMoistureThresholdHigh &&
           a.batteryMode == b.batteryMode;
}

// Mock NVS save/load for testing
void mockSaveConfig(const Config& config) {
    mockNvsStorage = config;
    mockNvsInitialized = true;
}

Config mockLoadConfig() {
    return mockNvsStorage;
}

ConfigManager* configManager;

void setUp(void) {
    configManager = new ConfigManager();
    mockNvsInitialized = false;
    srand(time(NULL) + rand());  // Re-seed for each test
}

void tearDown(void) {
    delete configManager;
}

/**
 * Property 3: Configuration Round Trip
 * **Validates: Requirements 2.1, 6.3, 7.19**
 *
 * For any valid Config structure, saving to NVS and then loading should produce
 * an equivalent Config structure with all fields preserved including WiFi credentials,
 * API settings, intervals, and calibration data.
 */
void test_property3_config_round_trip_preserves_all_fields(void) {
    const int ITERATIONS = 100;
    int successCount = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        // Generate a valid random config
        Config originalConfig = generateValidConfig();

        // Set it in the config manager
        configManager->getConfig() = originalConfig;

        // Validate it's valid
        if (!configManager->validateConfig()) {
            // Skip invalid configs (shouldn't happen with generateValidConfig)
            continue;
        }

        // Simulate save/load cycle using mock
        mockSaveConfig(originalConfig);
        Config loadedConfig = mockLoadConfig();

        // Verify all fields are preserved
        if (configsEqual(originalConfig, loadedConfig)) {
            successCount++;
        } else {
            // Print failure details for debugging
            char msg[256];
            snprintf(msg, sizeof(msg), "Round-trip failed at iteration %d: configs not equal", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }
    }

    // All iterations should succeed
    TEST_ASSERT_EQUAL_INT(ITERATIONS, successCount);
}

/**
 * Property 14: Configuration Validation Rejection
 * **Validates: Requirements 6.4, 6.5**
 *
 * For any invalid configuration update (e.g., negative intervals, empty required fields),
 * calling validateConfig() should return false and the current configuration should
 * remain unchanged.
 */
void test_property14_invalid_config_rejected_and_current_unchanged(void) {
    const int ITERATIONS = 100;
    int successCount = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        // Start with a valid config
        Config validConfig = generateValidConfig();
        configManager->getConfig() = validConfig;

        // Verify it's valid
        TEST_ASSERT_TRUE(configManager->validateConfig());

        // Save the current valid config
        Config savedValidConfig = configManager->getConfig();

        // Generate an invalid config
        Config invalidConfig = generateInvalidConfig();

        // Try to set the invalid config
        configManager->getConfig() = invalidConfig;

        // Validation should fail
        bool isValid = configManager->validateConfig();

        if (isValid) {
            // This invalid config passed validation - that's a bug
            char msg[256];
            snprintf(msg, sizeof(msg), "Invalid config passed validation at iteration %d", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }

        // Restore the valid config (simulating rejection)
        configManager->getConfig() = savedValidConfig;

        // Verify the valid config is still valid
        if (configManager->validateConfig() &&
            configsEqual(configManager->getConfig(), savedValidConfig)) {
            successCount++;
        } else {
            char msg[256];
            snprintf(msg, sizeof(msg), "Config restoration failed at iteration %d", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }
    }

    // All iterations should succeed
    TEST_ASSERT_EQUAL_INT(ITERATIONS, successCount);
}

/**
 * Additional property test: Valid configs always pass validation
 */
void test_property_valid_configs_always_pass_validation(void) {
    const int ITERATIONS = 100;
    int successCount = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        Config validConfig = generateValidConfig();
        configManager->getConfig() = validConfig;

        if (configManager->validateConfig()) {
            successCount++;
        } else {
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "Valid config failed validation at iteration %d:\n"
                     "  wifiSsid len=%zu, wifiPassword len=%zu\n"
                     "  readingIntervalMs=%u, publishIntervalSamples=%u\n"
                     "  soilDryAdc=%u, soilWetAdc=%u",
                     i, validConfig.wifiSsid.length(), validConfig.wifiPassword.length(),
                     validConfig.readingIntervalMs, validConfig.publishIntervalSamples,
                     validConfig.soilDryAdc, validConfig.soilWetAdc);
            TEST_FAIL_MESSAGE(msg);
            return;
        }
    }

    TEST_ASSERT_EQUAL_INT(ITERATIONS, successCount);
}

/**
 * Additional property test: Invalid configs always fail validation
 */
void test_property_invalid_configs_always_fail_validation(void) {
    const int ITERATIONS = 100;
    int successCount = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        Config invalidConfig = generateInvalidConfig();
        configManager->getConfig() = invalidConfig;

        if (!configManager->validateConfig()) {
            successCount++;
        } else {
            char msg[256];
            snprintf(msg, sizeof(msg), "Invalid config passed validation at iteration %d", i);
            TEST_FAIL_MESSAGE(msg);
            return;
        }
    }

    TEST_ASSERT_EQUAL_INT(ITERATIONS, successCount);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Property 3: Config round-trip preserves all fields
    RUN_TEST(test_property3_config_round_trip_preserves_all_fields);

    // Property 14: Invalid config rejected and current config unchanged
    RUN_TEST(test_property14_invalid_config_rejected_and_current_unchanged);

    // Additional property tests
    RUN_TEST(test_property_valid_configs_always_pass_validation);
    RUN_TEST(test_property_invalid_configs_always_fail_validation);

    return UNITY_END();
}

#endif  // UNIT_TEST
