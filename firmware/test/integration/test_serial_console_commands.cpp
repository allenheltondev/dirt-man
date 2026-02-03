#ifdef ARDUINO

#include <unity.h>

#include "../../include/ConfigManager.h"
#include "../../include/HardwareId.h"
#include "../../include/BootId.h"
#include "../../include/RegistrationManager.h"
#include "../../include/NetworkManager.h"
#include "../../include/TimeManager.h"
#include "../../include/SystemStatusManager.h"

// Global instances for testing
ConfigManager* configManager;
TimeManager* timeManager;
SystemStatusManager* systemStatusManager;
NetworkManager* networkManager;
RegistrationManager* registrationManager;
String globalBootId;

void setUp(void) {
    configManager = new ConfigManager();
    configManager->initialize();

    timeManager = new TimeManager();
    timeManager->initialize();

    systemStatusManager = new SystemStatusManager();
    systemStatusManager->initialize();

    networkManager = new NetworkManager(*configManager, *timeManager, *systemStatusManager);
    networkManager->initialize();

    registrationManager = new RegistrationManager(networkManager, configManager);

    // Generate Boot ID once
    globalBootId = BootId::generate();
}

void tearDown(void) {
    delete registrationManager;
    delete networkManager;
    delete systemStatusManager;
    delete timeManager;
    delete configManager;
}

/**
 * Test "hwid" command displays correct MAC address
 * **Validates: Requirements 7.2, 7.3**
 */
void test_hwid_command_displays_mac_address(void) {
    // Get hardware ID
    String hardwareId = HardwareId::getHardwareId();

    // Verify it's a valid MAC address format (AA:BB:CC:DD:EE:FF)
    TEST_ASSERT_EQUAL(17, hardwareId.length());

    // Verify format: XX:XX:XX:XX:XX:XX
    TEST_ASSERT_EQUAL(':', hardwareId[2]);
    TEST_ASSERT_EQUAL(':', hardwareId[5]);
    TEST_ASSERT_EQUAL(':', hardwareId[8]);
    TEST_ASSERT_EQUAL(':', hardwareId[11]);
    TEST_ASSERT_EQUAL(':', hardwareId[14]);

    // Verify all characters are hex or colons
    for (int i = 0; i < hardwareId.length(); i++) {
        if (i == 2 || i == 5 || i == 8 || i == 11 || i == 14) {
            TEST_ASSERT_EQUAL(':', hardwareId[i]);
        } else {
            char c = hardwareId[i];
            bool isHex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
            TEST_ASSERT_TRUE(isHex);
        }
    }

    Serial.print("Hardware ID: ");
    Serial.println(hardwareId);
}

/**
 * Test "bootid" command displays current Boot_ID
 * **Validates: Requirements 7.2, 7.3**
 */
void test_bootid_command_displays_current_bootid(void) {
    // Verify Boot ID is valid UUID v4
    TEST_ASSERT_TRUE(BootId::isValidUuid(globalBootId));

    // Verify format: 8-4-4-4-12
    TEST_ASSERT_EQUAL(36, globalBootId.length());
    TEST_ASSERT_EQUAL('-', globalBootId[8]);
    TEST_ASSERT_EQUAL('-', globalBootId[13]);
    TEST_ASSERT_EQUAL('-', globalBootId[18]);
    TEST_ASSERT_EQUAL('-', globalBootId[23]);

    // Verify version bits (position 14 should be '4')
    TEST_ASSERT_EQUAL('4', globalBootId[14]);

    // Verify variant bits (position 19 should be 8, 9, A, or B)
    char variantChar = globalBootId[19];
    bool validVariant = (variantChar == '8' || variantChar == '9' ||
                         variantChar == 'A' || variantChar == 'a' ||
                         variantChar == 'B' || variantChar == 'b');
    TEST_ASSERT_TRUE(validVariant);

    Serial.print("Boot ID: ");
    Serial.println(globalBootId);
}

/**
 * Test "register" command triggers registration
 * **Validates: Requirements 7.2, 7.3**
 *
 * Note: This test verifies the registration flow can be triggered.
 * Actual network communication requires a backend server.
 */
void test_register_command_triggers_registration(void) {
    // Get hardware ID and firmware version
    String hardwareId = HardwareId::getHardwareId();
    String firmwareVersion = "1.0.0";  // Test version
    String friendlyName = "test-device";

    // Build registration payload
    String payload = registrationManager->buildRegistrationPayload(
        hardwareId, globalBootId, friendlyName, firmwareVersion
    );

    // Verify payload is valid JSON (basic check)
    TEST_ASSERT_TRUE(payload.length() > 0);
    TEST_ASSERT_TRUE(payload.indexOf("hardware_id") > 0);
    TEST_ASSERT_TRUE(payload.indexOf("boot_id") > 0);
    TEST_ASSERT_TRUE(payload.indexOf("firmware_version") > 0);
    TEST_ASSERT_TRUE(payload.indexOf("friendly_name") > 0);

    Serial.println("Registration payload built successfully");
    Serial.println(payload);

    // Note: Actual registration attempt would require network connectivity
    // and a backend server. This test verifies the payload can be built.
}

/**
 * Test "register" command updates NVS on success
 * **Validates: Requirements 7.2, 7.3**
 *
 * Note: This test simulates a successful registration by directly
 * setting the confirmation ID, as actual network communication
 * requires a backend server.
 */
void test_register_command_updates_nvs_on_success(void) {
    // Generate a valid confirmation ID (UUID v4)
    String testConfirmationId = BootId::generate();

    // Simulate successful registration by setting confirmation ID
    configManager->setConfirmationId(testConfirmationId);

    // Verify it was stored
    String storedId = configManager->getConfirmationId();
    TEST_ASSERT_EQUAL_STRING(testConfirmationId.c_str(), storedId.c_str());

    // Verify it's recognized as valid
    TEST_ASSERT_TRUE(configManager->hasValidConfirmationId());

    // Verify registration manager recognizes it
    TEST_ASSERT_TRUE(registrationManager->isRegistered());

    Serial.println("Confirmation ID stored successfully in NVS");
    Serial.print("Confirmation ID: ");
    Serial.println(storedId);
}

void setup() {
    delay(2000);  // Wait for serial monitor

    UNITY_BEGIN();

    RUN_TEST(test_hwid_command_displays_mac_address);
    RUN_TEST(test_bootid_command_displays_current_bootid);
    RUN_TEST(test_register_command_triggers_registration);
    RUN_TEST(test_register_command_updates_nvs_on_success);

    UNITY_END();
}

void loop() {
    // Empty loop for Arduino
}

#endif  // ARDUINO
