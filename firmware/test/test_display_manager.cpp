#include <unity.h>

#include "DisplayManager.h"
#include "models/SensorReadings.h"
#include "models/SystemStatus.h"

// Test DisplayManager initialization
void test_display_manager_initialization() {
    DisplayManager display;

    bool result = display.initialize();

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(display.isInitialized());
}

// Test startup screen rendering (should not crash)
void test_display_manager_startup_screen() {
    DisplayManager display;
    display.initialize();

    // Should not crash when showing startup screen
    display.showStartupScreen("v1.0.0");

    TEST_ASSERT_TRUE(display.isInitialized());
}

// Test page cycling
void test_display_manager_page_cycling() {
    DisplayManager display;
    display.initialize();

    // Start on SUMMARY page
    TEST_ASSERT_EQUAL(DisplayPage::SUMMARY, display.getCurrentPage());

    // Cycle to next page
    display.cyclePage();
    TEST_ASSERT_EQUAL(DisplayPage::GRAPH_BME280_TEMP, display.getCurrentPage());

    // Cycle again
    display.cyclePage();
    TEST_ASSERT_EQUAL(DisplayPage::GRAPH_DS18B20_TEMP, display.getCurrentPage());

    // Cycle again
    display.cyclePage();
    TEST_ASSERT_EQUAL(DisplayPage::GRAPH_HUMIDITY, display.getCurrentPage());

    // Cycle again
    display.cyclePage();
    TEST_ASSERT_EQUAL(DisplayPage::GRAPH_PRESSURE, display.getCurrentPage());

    // Cycle again
    display.cyclePage();
    TEST_ASSERT_EQUAL(DisplayPage::GRAPH_SOIL_MOISTURE, display.getCurrentPage());

    // Cycle back to SUMMARY (no debug mode)
    display.cyclePage();
    TEST_ASSERT_EQUAL(DisplayPage::SUMMARY, display.getCurrentPage());
}

// Test page cycling with debug mode
void test_display_manager_page_cycling_with_debug() {
    DisplayManager display;
    display.initialize();
    display.setDebugMode(true);

    // Start on SUMMARY page
    TEST_ASSERT_EQUAL(DisplayPage::SUMMARY, display.getCurrentPage());

    // Cycle through all pages
    display.cyclePage();  // GRAPH_BME280_TEMP
    display.cyclePage();  // GRAPH_DS18B20_TEMP
    display.cyclePage();  // GRAPH_HUMIDITY
    display.cyclePage();  // GRAPH_PRESSURE
    display.cyclePage();  // GRAPH_SOIL_MOISTURE
    display.cyclePage();  // SYSTEM_HEALTH (because debug mode is on)

    TEST_ASSERT_EQUAL(DisplayPage::SYSTEM_HEALTH, display.getCurrentPage());

    // Cycle back to SUMMARY
    display.cyclePage();
    TEST_ASSERT_EQUAL(DisplayPage::SUMMARY, display.getCurrentPage());
}

// Test update with sensor readings (should not crash)
void test_display_manager_update() {
    DisplayManager display;
    display.initialize();

    // Create mock sensor readings
    SensorReadings readings;
    readings.bme280Temp = 22.5f;
    readings.ds18b20Temp = 21.8f;
    readings.humidity = 45.2f;
    readings.pressure = 1013.25f;
    readings.soilMoisture = 62.3f;
    readings.soilMoistureRaw = 2048;
    readings.sensorStatus = 0xFF;  // All sensors OK
    readings.monotonicMs = 1000;

    // Create mock system status
    SystemStatus status;
    status.uptimeMs = 3600000;  // 1 hour
    status.freeHeap = 245760;
    status.wifiRssi = -65;
    status.queueDepth = 0;
    status.bootCount = 1;
    status.errors.sensorReadFailures = 0;
    status.errors.networkFailures = 0;
    status.errors.bufferOverflows = 0;

    // Should not crash when updating display
    display.update(readings, status);

    TEST_ASSERT_TRUE(display.isInitialized());
}

// Test debug mode toggle
void test_display_manager_debug_mode() {
    DisplayManager display;
    display.initialize();

    // Debug mode should be off by default
    display.setDebugMode(false);

    // Enable debug mode
    display.setDebugMode(true);

    // Disable debug mode
    display.setDebugMode(false);

    TEST_ASSERT_TRUE(display.isInitialized());
}

// Test burn-in protection (should not crash)
void test_display_manager_burn_in_protection() {
    DisplayManager display;
    display.initialize();

    // Should not crash when checking burn-in protection
    display.checkBurnInProtection();

    TEST_ASSERT_TRUE(display.isInitialized());
}

void setUp(void) {
    // Set up code here (runs before each test)
}

void tearDown(void) {
    // Clean up code here (runs after each test)
}

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_display_manager_initialization);
    RUN_TEST(test_display_manager_startup_screen);
    RUN_TEST(test_display_manager_page_cycling);
    RUN_TEST(test_display_manager_page_cycling_with_debug);
    RUN_TEST(test_display_manager_update);
    RUN_TEST(test_display_manager_debug_mode);
    RUN_TEST(test_display_manager_burn_in_protection);

    return UNITY_END();
}
