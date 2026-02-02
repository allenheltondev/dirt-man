#include <unity.h>

#include "../include/ConfigManager.h"
#include "../include/models/Config.h"

ConfigManager* configManager;

void setUp(void) {
    configManager = new ConfigManager();
}

void tearDown(void) {
    delete configManager;
}

void test_setDefaults_setsValidDefaults(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    TEST_ASSERT_EQUAL_STRING("esp32-sensor-001", config.deviceId.c_str());
    TEST_ASSERT_EQUAL_UINT32(5000, config.readingIntervalMs);
    TEST_ASSERT_EQUAL_UINT16(20, config.publishIntervalSamples);
}

void test_validateConfig_validConfig_returnsTrue(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();
    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";

    TEST_ASSERT_TRUE(configManager->validateConfig());
}

void test_validateConfig_emptyWiFiSsid_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_shortWiFiPassword_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "short";
    config.apiEndpoint = "https://api.example.com/data";

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_longWiFiSsid_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "ThisIsAVeryLongSSIDThatExceedsTheMaximumAllowedLengthOf32Characters";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_emptyApiEndpoint_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "";

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_invalidApiEndpointProtocol_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "ftp://api.example.com/data";

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_httpApiEndpoint_returnsTrue(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "http://api.example.com/data";

    TEST_ASSERT_TRUE(configManager->validateConfig());
}

void test_validateConfig_httpsApiEndpoint_returnsTrue(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";

    TEST_ASSERT_TRUE(configManager->validateConfig());
}

void test_validateConfig_readingIntervalTooLow_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";
    config.readingIntervalMs = 500;

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_readingIntervalTooHigh_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";
    config.readingIntervalMs = 4000000;

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_publishIntervalTooLow_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";
    config.publishIntervalSamples = 0;

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_publishIntervalTooHigh_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";
    config.publishIntervalSamples = 150;

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_pageCycleIntervalTooLow_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";
    config.pageCycleIntervalMs = 500;

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_pageCycleIntervalTooHigh_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";
    config.pageCycleIntervalMs = 70000;

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_soilDryAdcTooHigh_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";
    config.soilDryAdc = 5000;

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_soilWetAdcTooHigh_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";
    config.soilWetAdc = 5000;

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_soilDryLessThanWet_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";
    config.soilDryAdc = 1000;
    config.soilWetAdc = 2000;

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_soilDryEqualToWet_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";
    config.soilDryAdc = 2000;
    config.soilWetAdc = 2000;

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_thresholdLowGreaterThanHigh_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";
    config.soilMoistureThresholdLow = 80;
    config.soilMoistureThresholdHigh = 60;

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_thresholdHighOver100_returnsFalse(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";
    config.soilMoistureThresholdHigh = 150;

    TEST_ASSERT_FALSE(configManager->validateConfig());
}

void test_validateConfig_boundaryValues_returnsTrue(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";
    config.readingIntervalMs = 1000;
    config.publishIntervalSamples = 1;
    config.pageCycleIntervalMs = 1000;
    config.soilDryAdc = 4095;
    config.soilWetAdc = 0;
    config.soilMoistureThresholdLow = 0;
    config.soilMoistureThresholdHigh = 100;

    TEST_ASSERT_TRUE(configManager->validateConfig());
}

void test_validateConfig_maxBoundaryValues_returnsTrue(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "TestNetwork";
    config.wifiPassword = "password123";
    config.apiEndpoint = "https://api.example.com/data";
    config.readingIntervalMs = 3600000;
    config.publishIntervalSamples = 120;
    config.pageCycleIntervalMs = 60000;

    TEST_ASSERT_TRUE(configManager->validateConfig());
}

void test_validateConfig_emptyWiFiPassword_returnsTrue(void) {
    configManager->setDefaults();
    Config& config = configManager->getConfig();

    config.wifiSsid = "OpenNetwork";
    config.wifiPassword = "";
    config.apiEndpoint = "https://api.example.com/data";

    TEST_ASSERT_TRUE(configManager->validateConfig());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_setDefaults_setsValidDefaults);
    RUN_TEST(test_validateConfig_validConfig_returnsTrue);
    RUN_TEST(test_validateConfig_emptyWiFiSsid_returnsFalse);
    RUN_TEST(test_validateConfig_shortWiFiPassword_returnsFalse);
    RUN_TEST(test_validateConfig_longWiFiSsid_returnsFalse);
    RUN_TEST(test_validateConfig_emptyApiEndpoint_returnsFalse);
    RUN_TEST(test_validateConfig_invalidApiEndpointProtocol_returnsFalse);
    RUN_TEST(test_validateConfig_httpApiEndpoint_returnsTrue);
    RUN_TEST(test_validateConfig_httpsApiEndpoint_returnsTrue);
    RUN_TEST(test_validateConfig_readingIntervalTooLow_returnsFalse);
    RUN_TEST(test_validateConfig_readingIntervalTooHigh_returnsFalse);
    RUN_TEST(test_validateConfig_publishIntervalTooLow_returnsFalse);
    RUN_TEST(test_validateConfig_publishIntervalTooHigh_returnsFalse);
    RUN_TEST(test_validateConfig_pageCycleIntervalTooLow_returnsFalse);
    RUN_TEST(test_validateConfig_pageCycleIntervalTooHigh_returnsFalse);
    RUN_TEST(test_validateConfig_soilDryAdcTooHigh_returnsFalse);
    RUN_TEST(test_validateConfig_soilWetAdcTooHigh_returnsFalse);
    RUN_TEST(test_validateConfig_soilDryLessThanWet_returnsFalse);
    RUN_TEST(test_validateConfig_soilDryEqualToWet_returnsFalse);
    RUN_TEST(test_validateConfig_thresholdLowGreaterThanHigh_returnsFalse);
    RUN_TEST(test_validateConfig_thresholdHighOver100_returnsFalse);
    RUN_TEST(test_validateConfig_boundaryValues_returnsTrue);
    RUN_TEST(test_validateConfig_maxBoundaryValues_returnsTrue);
    RUN_TEST(test_validateConfig_emptyWiFiPassword_returnsTrue);

    return UNITY_END();
}
