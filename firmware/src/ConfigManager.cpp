#include "ConfigManager.h"

#ifdef ARDUINO
#define MAX_PUBLISH_SAMPLES 120
#define MIN_READING_INTERVAL_MS 1000
#define MAX_READING_INTERVAL_MS 3600000
#define MIN_PUBLISH_INTERVAL_SAMPLES 1
#define MIN_PAGE_CYCLE_INTERVAL_MS 1000
#define MAX_PAGE_CYCLE_INTERVAL_MS 60000
#define MAX_ADC_VALUE 4095
#endif

ConfigManager::ConfigManager() {}

ConfigManager::~ConfigManager() {
#ifdef ARDUINO
    nvs.end();
#endif
}

void ConfigManager::initialize() {
#ifdef ARDUINO
    nvs.begin("config", false);
#endif

    if (!loadConfig()) {
        setDefaults();
        saveConfig();
    }
}

bool ConfigManager::loadConfig() {
#ifdef ARDUINO
    if (!nvs.isKey("initialized")) {
        return false;
    }

    config.wifiSsid = nvs.getString("wifiSsid", "");
    config.wifiPassword = nvs.getString("wifiPass", "");
    config.apiEndpoint = nvs.getString("apiEndpoint", "");
    config.apiToken = nvs.getString("apiToken", "");
    config.deviceId = nvs.getString("deviceId", "");

    config.readingIntervalMs = nvs.getUInt("readingInt", 5000);
    config.publishIntervalSamples = nvs.getUShort("publishInt", 20);
    config.pageCycleIntervalMs = nvs.getUShort("pageCycle", 10000);

    config.soilDryAdc = nvs.getUShort("soilDryAdc", 3000);
    config.soilWetAdc = nvs.getUShort("soilWetAdc", 1500);

    config.temperatureInFahrenheit = nvs.getBool("tempF", false);
    config.soilMoistureThresholdLow = nvs.getUShort("soilThreshLow", 30);
    config.soilMoistureThresholdHigh = nvs.getUShort("soilThreshHigh", 70);

    config.batteryMode = nvs.getBool("batteryMode", false);

    // TLS/HTTPS configuration
    config.tlsValidateServer = nvs.getBool("tlsValidate", true);
    config.allowHttpFallback = nvs.getBool("httpFallback", false);

    return validateConfig();
#else
    return false;
#endif
}

bool ConfigManager::saveConfig() {
#ifdef ARDUINO
    if (!validateConfig()) {
        Serial.println("Cannot save invalid configuration");
        return false;
    }

    nvs.putString("wifiSsid", config.wifiSsid);
    nvs.putString("wifiPass", config.wifiPassword);
    nvs.putString("apiEndpoint", config.apiEndpoint);
    nvs.putString("apiToken", config.apiToken);
    nvs.putString("deviceId", config.deviceId);

    nvs.putUInt("readingInt", config.readingIntervalMs);
    nvs.putUShort("publishInt", config.publishIntervalSamples);
    nvs.putUShort("pageCycle", config.pageCycleIntervalMs);

    nvs.putUShort("soilDryAdc", config.soilDryAdc);
    nvs.putUShort("soilWetAdc", config.soilWetAdc);

    nvs.putBool("tempF", config.temperatureInFahrenheit);
    nvs.putUShort("soilThreshLow", config.soilMoistureThresholdLow);
    nvs.putUShort("soilThreshHigh", config.soilMoistureThresholdHigh);

    nvs.putBool("batteryMode", config.batteryMode);

    // TLS/HTTPS configuration
    nvs.putBool("tlsValidate", config.tlsValidateServer);
    nvs.putBool("httpFallback", config.allowHttpFallback);

    nvs.putBool("initialized", true);

    return true;
#else
    return false;
#endif
}

void ConfigManager::setDefaults() {
    config.wifiSsid = "";
    config.wifiPassword = "";
    config.apiEndpoint = "https://api.example.com/sensor-data";
    config.apiToken = "";
    config.deviceId = "esp32-sensor-001";

    config.readingIntervalMs = 5000;
    config.publishIntervalSamples = 20;
    config.pageCycleIntervalMs = 10000;

    config.soilDryAdc = 3000;
    config.soilWetAdc = 1500;

    config.temperatureInFahrenheit = false;
    config.soilMoistureThresholdLow = 30;
    config.soilMoistureThresholdHigh = 70;

    config.batteryMode = false;

    // TLS/HTTPS defaults
    config.tlsValidateServer = true;   // Enable certificate validation by default
    config.allowHttpFallback = false;  // Disable HTTP fallback by default
}

bool ConfigManager::validateConfig() {
    return validateWiFiCredentials() && validateApiEndpoint() && validateIntervals() &&
           validateCalibration();
}

bool ConfigManager::validateWiFiCredentials() {
    if (config.wifiSsid.length() == 0 || config.wifiSsid.length() > 32) {
        return false;
    }

    if (config.wifiPassword.length() > 0 && config.wifiPassword.length() < 8) {
        return false;
    }

    if (config.wifiPassword.length() > 63) {
        return false;
    }

    return true;
}

bool ConfigManager::validateApiEndpoint() {
    if (config.apiEndpoint.length() == 0) {
        return false;
    }

// Check if starts with http:// or https://
#ifdef ARDUINO
    if (!config.apiEndpoint.startsWith("http://") && !config.apiEndpoint.startsWith("https://")) {
        return false;
    }
#else
    // std::string version
    if (config.apiEndpoint.find("http://") != 0 && config.apiEndpoint.find("https://") != 0) {
        return false;
    }
#endif

    if (config.apiEndpoint.length() > 256) {
        return false;
    }

    return true;
}

bool ConfigManager::validateIntervals() {
#ifdef ARDUINO
    if (config.readingIntervalMs < MIN_READING_INTERVAL_MS ||
        config.readingIntervalMs > MAX_READING_INTERVAL_MS) {
        return false;
    }

    if (config.publishIntervalSamples < MIN_PUBLISH_INTERVAL_SAMPLES ||
        config.publishIntervalSamples > MAX_PUBLISH_SAMPLES) {
        return false;
    }

    if (config.pageCycleIntervalMs < MIN_PAGE_CYCLE_INTERVAL_MS ||
        config.pageCycleIntervalMs > MAX_PAGE_CYCLE_INTERVAL_MS) {
        return false;
    }
#endif

    if (config.soilMoistureThresholdLow >= config.soilMoistureThresholdHigh) {
        return false;
    }

    if (config.soilMoistureThresholdHigh > 100) {
        return false;
    }

    return true;
}

bool ConfigManager::validateCalibration() {
#ifdef ARDUINO
    if (config.soilDryAdc > MAX_ADC_VALUE || config.soilWetAdc > MAX_ADC_VALUE) {
        return false;
    }
#endif

    if (config.soilDryAdc <= config.soilWetAdc) {
        return false;
    }

    return true;
}

void ConfigManager::handleSerialConfig() {
#ifdef ARDUINO
    if (!Serial.available()) {
        return;
    }

    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "config") {
        printConfigMenu();
    } else if (command == "show") {
        printCurrentConfig();
    } else if (command == "wifi") {
        updateWiFiCredentials();
    } else if (command == "api") {
        updateApiEndpoint();
    } else if (command == "intervals") {
        updateIntervals();
    } else if (command == "calibrate") {
        updateCalibration();
    } else if (command == "deviceid") {
        updateDeviceId();
    } else if (command == "save") {
        if (validateConfig()) {
            if (saveConfig()) {
                Serial.println("Configuration saved successfully");
            } else {
                Serial.println("Failed to save configuration");
            }
        } else {
            Serial.println("Invalid configuration - cannot save");
        }
    } else if (command == "defaults") {
        setDefaults();
        Serial.println("Configuration reset to defaults");
    } else if (command == "help") {
        printConfigMenu();
    }
#endif
}

void ConfigManager::printConfigMenu() {
#ifdef ARDUINO
    Serial.println("\n=== Configuration Menu ===");
    Serial.println("config    - Show this menu");
    Serial.println("show      - Display current configuration");
    Serial.println("wifi      - Update WiFi credentials");
    Serial.println("api       - Update API endpoint and token");
    Serial.println("intervals - Update timing intervals");
    Serial.println("calibrate - Update soil moisture calibration");
    Serial.println("deviceid  - Update device identifier");
    Serial.println("save      - Save configuration to NVS");
    Serial.println("defaults  - Reset to default configuration");
    Serial.println("diag      - Show system diagnostics");
    Serial.println("help      - Show this menu");
    Serial.println("========================\n");
#endif
}

void ConfigManager::printCurrentConfig() {
#ifdef ARDUINO
    Serial.println("\n=== Current Configuration ===");
    Serial.print("WiFi SSID: ");
    Serial.println(config.wifiSsid);
    Serial.print("WiFi Password: ");
    Serial.println(sanitizeSensitiveData(config.wifiPassword));
    Serial.print("API Endpoint: ");
    Serial.println(config.apiEndpoint);
    Serial.print("API Token: ");
    Serial.println(sanitizeSensitiveData(config.apiToken));
    Serial.print("Device ID: ");
    Serial.println(config.deviceId);
    Serial.print("Reading Interval (ms): ");
    Serial.println(config.readingIntervalMs);
    Serial.print("Publish Interval (samples): ");
    Serial.println(config.publishIntervalSamples);
    Serial.print("Page Cycle Interval (ms): ");
    Serial.println(config.pageCycleIntervalMs);
    Serial.print("Soil Dry ADC: ");
    Serial.println(config.soilDryAdc);
    Serial.print("Soil Wet ADC: ");
    Serial.println(config.soilWetAdc);
    Serial.print("Temperature in Fahrenheit: ");
    Serial.println(config.temperatureInFahrenheit ? "Yes" : "No");
    Serial.print("Soil Moisture Threshold Low: ");
    Serial.println(config.soilMoistureThresholdLow);
    Serial.print("Soil Moisture Threshold High: ");
    Serial.println(config.soilMoistureThresholdHigh);
    Serial.print("Battery Mode: ");
    Serial.println(config.batteryMode ? "Yes" : "No");
    Serial.println("============================\n");
#endif
}

void ConfigManager::updateWiFiCredentials() {
#ifdef ARDUINO
    Serial.println("\n=== Update WiFi Credentials ===");
    Serial.print("Enter WiFi SSID: ");
    while (!Serial.available()) {
    }
    config.wifiSsid = Serial.readStringUntil('\n');
    config.wifiSsid.trim();

    Serial.print("Enter WiFi Password: ");
    while (!Serial.available()) {
    }
    config.wifiPassword = Serial.readStringUntil('\n');
    config.wifiPassword.trim();

    if (validateWiFiCredentials()) {
        Serial.println("WiFi credentials updated");
    } else {
        Serial.println("Invalid WiFi credentials");
    }
#endif
}

void ConfigManager::updateApiEndpoint() {
#ifdef ARDUINO
    Serial.println("\n=== Update API Configuration ===");
    Serial.print("Enter API Endpoint URL: ");
    while (!Serial.available()) {
    }
    config.apiEndpoint = Serial.readStringUntil('\n');
    config.apiEndpoint.trim();

    Serial.print("Enter API Token: ");
    while (!Serial.available()) {
    }
    config.apiToken = Serial.readStringUntil('\n');
    config.apiToken.trim();

    if (validateApiEndpoint()) {
        Serial.println("API configuration updated");
    } else {
        Serial.println("Invalid API endpoint");
    }
#endif
}

void ConfigManager::updateIntervals() {
#ifdef ARDUINO
    Serial.println("\n=== Update Intervals ===");
    Serial.print("Enter Reading Interval (ms, 1000-3600000): ");
    while (!Serial.available()) {
    }
    config.readingIntervalMs = Serial.readStringUntil('\n').toInt();

    Serial.print("Enter Publish Interval (samples, 1-120): ");
    while (!Serial.available()) {
    }
    config.publishIntervalSamples = Serial.readStringUntil('\n').toInt();

    Serial.print("Enter Page Cycle Interval (ms, 1000-60000): ");
    while (!Serial.available()) {
    }
    config.pageCycleIntervalMs = Serial.readStringUntil('\n').toInt();

    if (validateIntervals()) {
        Serial.println("Intervals updated");
    } else {
        Serial.println("Invalid interval values");
    }
#endif
}

void ConfigManager::updateCalibration() {
#ifdef ARDUINO
    Serial.println("\n=== Update Soil Moisture Calibration ===");
    Serial.println("Place sensor in DRY soil and press Enter");
    while (!Serial.available()) {
    }
    Serial.readStringUntil('\n');

    Serial.print("Enter Dry ADC value (0-4095): ");
    while (!Serial.available()) {
    }
    config.soilDryAdc = Serial.readStringUntil('\n').toInt();

    Serial.println("Place sensor in WET soil and press Enter");
    while (!Serial.available()) {
    }
    Serial.readStringUntil('\n');

    Serial.print("Enter Wet ADC value (0-4095): ");
    while (!Serial.available()) {
    }
    config.soilWetAdc = Serial.readStringUntil('\n').toInt();

    if (validateCalibration()) {
        Serial.println("Calibration updated");
        Serial.print("Dry ADC: ");
        Serial.println(config.soilDryAdc);
        Serial.print("Wet ADC: ");
        Serial.println(config.soilWetAdc);
    } else {
        Serial.println("Invalid calibration values (Dry must be > Wet, both 0-4095)");
    }
#endif
}

void ConfigManager::updateDeviceId() {
#ifdef ARDUINO
    Serial.println("\n=== Update Device ID ===");
    Serial.print("Enter Device ID: ");
    while (!Serial.available()) {
    }
    config.deviceId = Serial.readStringUntil('\n');
    config.deviceId.trim();

    if (config.deviceId.length() > 0) {
        Serial.println("Device ID updated");
    } else {
        Serial.println("Invalid Device ID");
        config.deviceId = "esp32-sensor-001";
    }
#endif
}

String ConfigManager::sanitizeSensitiveData(const String& data) {
    if (data.length() == 0) {
        return "<not set>";
    }

    if (data.length() <= 4) {
        return "****";
    }

    String sanitized = "";
    for (size_t i = 0; i < data.length() - 2; i++) {
        sanitized += "*";
    }

// Get last 2 characters
#ifdef ARDUINO
    sanitized += data.substring(data.length() - 2);
#else
    // std::string version
    sanitized += data.substr(data.length() - 2);
#endif

    return sanitized;
}

Config& ConfigManager::getConfig() {
    return config;
}
