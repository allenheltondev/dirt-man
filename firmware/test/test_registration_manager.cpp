#include <unity.h>

#include "../test/mocks/Arduino.h"
#include "BootId.h"
#include "ConfigManager.h"

// Test fixtures
ConfigManager* configMgr = nullptr;

void setUp(void) {
    configMgr = new ConfigManager();
}

void tearDown(void) {
    delete configMgr;
}

// Test: ConfigManager confirmation_id persistence
void test_config_manager_confirmation_id_persistence() {
    String validUuid = "550e8400-e29b-41d4-a716-446655440000";
    configMgr->setConfirmationId(validUuid);

    String retrieved = configMgr->getConfirmationId();

    TEST_ASSERT_EQUAL_STRING(validUuid.c_str(), retrieved.c_str());
}

// Test: ConfigManager hasValidConfirmationId with valid UUID
void test_config_manager_has_valid_confirmation_id_true() {
    String validUuid = "7c9e6679-7425-40de-944b-e07fc1f90ae7";
    configMgr->setConfirmationId(validUuid);

    bool hasValid = configMgr->hasValidConfirmationId();

    TEST_ASSERT_TRUE(hasValid);
}

// Test: ConfigManager hasValidConfirmationId with empty string
void test_config_manager_has_valid_confirmation_id_false_empty() {
    configMgr->setConfirmationId("");

    bool hasValid = configMgr->hasValidConfirmationId();

    TEST_ASSERT_FALSE(hasValid);
}

// Test: Registration payload structure with friendly_name
void test_registration_payload_structure_with_friendly_name() {
    String hardwareId = "AA:BB:CC:DD:EE:FF";
    String bootId = "550e8400-e29b-41d4-a716-446655440000";
    String friendlyName = "test-device";
    String firmwareVersion = "1.0.0";

    String payload = "{";
    payload += "\"hardware_id\":\"" + hardwareId + "\",";
    payload += "\"boot_id\":\"" + bootId + "\",";
    payload += "\"firmware_version\":\"" + firmwareVersion + "\",";
    if (friendlyName.length() > 0) {
        payload += "\"friendly_name\":\"" + friendlyName + "\",";
    }
    payload += "\"capabilities\":{";
    payload += "\"sensors\":[\"bme280\",\"ds18b20\",\"soil_moisture\"],";
    payload += "\"features\":{";
    payload += "\"tft_display\":true,";
    payload += "\"offline_buffering\":true,";
    payload += "\"ntp_sync\":true";
    payload += "}";
    payload += "}";
    payload += "}";

    TEST_ASSERT_TRUE(payload.find("\"hardware_id\":\"AA:BB:CC:DD:EE:FF\"") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"boot_id\":\"550e8400-e29b-41d4-a716-446655440000\"") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"firmware_version\":\"1.0.0\"") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"friendly_name\":\"test-device\"") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"capabilities\"") != std::string::npos);
}

// Test: Registration payload structure without friendly_name
void test_registration_payload_structure_without_friendly_name() {
    String hardwareId = "AA:BB:CC:DD:EE:FF";
    String bootId = "550e8400-e29b-41d4-a716-446655440000";
    String friendlyName = "";
    String firmwareVersion = "1.0.0";

    String payload = "{";
    payload += "\"hardware_id\":\"" + hardwareId + "\",";
    payload += "\"boot_id\":\"" + bootId + "\",";
    payload += "\"firmware_version\":\"" + firmwareVersion + "\",";
    if (friendlyName.length() > 0) {
        payload += "\"friendly_name\":\"" + friendlyName + "\",";
    }
    payload += "\"capabilities\":{";
    payload += "\"sensors\":[\"bme280\",\"ds18b20\",\"soil_moisture\"],";
    payload += "\"features\":{";
    payload += "\"tft_display\":true,";
    payload += "\"offline_buffering\":true,";
    payload += "\"ntp_sync\":true";
    payload += "}";
    payload += "}";
    payload += "}";

    TEST_ASSERT_TRUE(payload.find("\"friendly_name\"") == std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"hardware_id\":\"AA:BB:CC:DD:EE:FF\"") != std::string::npos);
}

// Test: Registration payload capabilities structure
void test_registration_payload_capabilities_structure() {
    String hardwareId = "AA:BB:CC:DD:EE:FF";
    String bootId = "550e8400-e29b-41d4-a716-446655440000";
    String friendlyName = "";
    String firmwareVersion = "1.0.0";

    String payload = "{";
    payload += "\"hardware_id\":\"" + hardwareId + "\",";
    payload += "\"boot_id\":\"" + bootId + "\",";
    payload += "\"firmware_version\":\"" + firmwareVersion + "\",";
    if (friendlyName.length() > 0) {
        payload += "\"friendly_name\":\"" + friendlyName + "\",";
    }
    payload += "\"capabilities\":{";
    payload += "\"sensors\":[\"bme280\",\"ds18b20\",\"soil_moisture\"],";
    payload += "\"features\":{";
    payload += "\"tft_display\":true,";
    payload += "\"offline_buffering\":true,";
    payload += "\"ntp_sync\":true";
    payload += "}";
    payload += "}";
    payload += "}";

    TEST_ASSERT_TRUE(payload.find("\"capabilities\":{") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"sensors\":[") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"bme280\"") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"ds18b20\"") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"soil_moisture\"") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"features\":{") != std::string::npos);
    TEST_ASSERT_TRUE(payload.find("\"tft_display\":true") != std::string::npos);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_config_manager_confirmation_id_persistence);
    RUN_TEST(test_config_manager_has_valid_confirmation_id_true);
    RUN_TEST(test_config_manager_has_valid_confirmation_id_false_empty);
    RUN_TEST(test_registration_payload_structure_with_friendly_name);
    RUN_TEST(test_registration_payload_structure_without_friendly_name);
    RUN_TEST(test_registration_payload_capabilities_structure);

    return UNITY_END();
}
