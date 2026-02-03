#ifdef ARDUINO

#include <unity.h>

#include "../../include/ConfigManager.h"

ConfigManager* configManager;

void setUp(void) {
    configManager = new ConfigManager();
    configManager->initialize();
}

void tearDown(void) {
    delete configManager;
}

/**
 * Test friendly_name write to NVS and read back
 * **Validates: Requirements 3.2, 6.1**
 */
void test_friendlyName_writeAndRead_exactMatch(void) {
    String testFriendlyName = "greenhouse-sensor-01";

    // Write friendly_name to NVS via deviceId field
    Config& config = configManager->getConfig();
    config.deviceId = testFriendlyName;

    bool saved = configManager->saveConfig();
    TEST_ASSERT_TRUE(saved);

    // Create new ConfigManager instance to simulate reboot
    delete configManager;
    configManager = new ConfigManager();
    configManager->initialize();

    // Read back and verify exact match
    Config& loadedConfig = configManager->getConfig();
    TEST_ASSERT_EQUAL_STRING(testFriendlyName.c_str(), loadedConfig.deviceId.c_str());
}

/**
 * Test empty friendly_name roundtrip
 * **Validates: Requirements 3.2, 6.1**
 */
void test_friendlyName_emptyString_roundtrip(void) {
    String emptyFriendlyName = "";

    Config& config = configManager->getConfig();
    config.deviceId = emptyFriendlyName;

    bool saved = configManager->saveConfig();
    TEST_ASSERT_TRUE(saved);

    // Create new ConfigManager instance
    delete configManager;
    configManager = new ConfigManager();
    configManager->initialize();

    // Read back and verify
    Config& loadedConfig = configManager->getConfig();
    TEST_ASSERT_EQUAL_STRING(emptyFriendlyName.c_str(), loadedConfig.deviceId.c_str());
}

/**
 * Test friendly_name with special characters roundtrip
 * **Validates: Requirements 3.2, 6.1**
 */
void test_friendlyName_specialCharacters_roundtrip(void) {
    String specialFriendlyName = "sensor-01_test.device";

    Config& config = configManager->getConfig();
    config.deviceId = specialFriendlyName;

    bool saved = configManager->saveConfig();
    TEST_ASSERT_TRUE(saved);

    // Create new ConfigManager instance
    delete configManager;
    configManager = new ConfigManager();
    configManager->initialize();

    // Read back and verify exact match
    Config& loadedConfig = configManager->getConfig();
    TEST_ASSERT_EQUAL_STRING(specialFriendlyName.c_str(), loadedConfig.deviceId.c_str());
}

/**
 * Test friendly_name at maximum length (64 characters) roundtrip
 * **Validates: Requirements 3.2, 6.1**
 */
void test_friendlyName_maxLength_roundtrip(void) {
    String maxLengthName = "sensor-device-with-a-very-long-name-that-is-exactly-64-chars";
    TEST_ASSERT_EQUAL(64, maxLengthName.length());

    Config& config = configManager->getConfig();
    config.deviceId = maxLengthName;

    bool saved = configManager->saveConfig();
    TEST_ASSERT_TRUE(saved);

    // Create new ConfigManager instance
    delete configManager;
    configManager = new ConfigManager();
    configManager->initialize();

    // Read back and verify exact match
    Config& loadedConfig = configManager->getConfig();
    TEST_ASSERT_EQUAL_STRING(maxLengthName.c_str(), loadedConfig.deviceId.c_str());
}

void setup() {
    delay(2000);  // Wait for serial monitor

    UNITY_BEGIN();

    RUN_TEST(test_friendlyName_writeAndRead_exactMatch);
    RUN_TEST(test_friendlyName_emptyString_roundtrip);
    RUN_TEST(test_friendlyName_specialCharacters_roundtrip);
    RUN_TEST(test_friendlyName_maxLength_roundtrip);

    UNITY_END();
}

void loop() {
    // Empty loop for Arduino
}

#endif  // ARDUINO
