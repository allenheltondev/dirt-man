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

ConfigManager::ConfigManager()
    : registrationCallback(nullptr),
      bootIdRef(nullptr),
      touchDetected(false),
      touchType(TouchControllerType::NONE),
      provisioningMode(false) {}

ConfigManager::~ConfigManager() {
#ifdef ARDUINO
    nvs.end();
#endif
}

void ConfigManager::initialize() {
#ifdef ARDUINO
    nvs.begin("config", false);
#endif

    // Initialize LittleFS via ConfigFileManager
    if (!fileManager.initialize()) {
#ifdef ARDUINO
        Serial.println("[WARN] Failed to initialize LittleFS");
#endif
    }

    if (!loadConfig()) {
        setDefaults();
        saveConfig();
    }

    // Check required fields after loading
#ifdef ARDUINO
    if (!hasRequiredFields()) {
        String missing = getMissingRequiredFields();
        Serial.println("[WARN] ========================================");
        Serial.println("[WARN] PROVISIONING MODE REQUIRED");
        Serial.print("[WARN] Missing required fields: ");
        Serial.println(missing);
        Serial.println("[WARN] Use serial console to configure device");
        Serial.println("[WARN] Commands: wifi, api, save");
        Serial.println("[WARN] ========================================");
    }
#endif
}

bool ConfigManager::loadFromFile() {
#ifdef ARDUINO
    ConfigFileData fileData;
    ConfigLoadResult result = fileManager.loadConfig(fileData);

    if (result == ConfigLoadResult::SUCCESS) {
        Serial.println("[INFO] Config file loaded successfully");
        applyConfigFileData(fileData);

        // Validate config structure
        if (!validateConfig()) {
            Serial.println("[ERROR] Config file validation failed");
            return false;
        }

        // Check required fields separately
        if (!hasRequiredFields()) {
            String missing = getMissingRequiredFields();
            Serial.print("[ERROR] Config file missing required fields: ");
            Serial.println(missing);
            // Return true to indicate file was parsed successfully
            // but caller should check hasRequiredFields() separately
            return true;
        }

        return true;
    } else {
        // Log the specific error
        switch (result) {
            case ConfigLoadResult::FILE_NOT_FOUND:
                Serial.println("[INFO] Config file not found");
                break;
            case ConfigLoadResult::PARSE_ERROR:
                Serial.println("[ERROR] Config file parse error");
                break;
            case ConfigLoadResult::SCHEMA_ERROR:
                Serial.println("[ERROR] Config file schema error");
                break;
            case ConfigLoadResult::CHECKSUM_ERROR:
                Serial.println("[ERROR] Config file checksum error");
                break;
            case ConfigLoadResult::FS_MOUNT_ERROR:
                Serial.println("[ERROR] Filesystem mount error");
                break;
            case ConfigLoadResult::READ_ERROR:
                Serial.println("[ERROR] Config file read error");
                break;
            default:
                Serial.println("[ERROR] Unknown config file error");
                break;
        }
        Serial.print("[ERROR] ");
        Serial.println(fileManager.getLastError());
        return false;
    }
#else
    return false;
#endif
}

bool ConfigManager::loadConfig() {
#ifdef ARDUINO
    // Priority 1: Try loading from config file
    if (loadFromFile()) {
        Serial.println("[INFO] Configuration loaded from file");
        return true;
    }

    // Priority 2: Try loading from NVS
    if (nvs.isKey("initialized")) {
        Serial.println("[INFO] Loading configuration from NVS");

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

        if (validateConfig()) {
            // Migrate NVS config to file for future boots
            Serial.println("[INFO] Migrating NVS config to file");
            migrateNvsToFile();
            return true;
        } else {
            Serial.println("[WARN] NVS config validation failed");
        }
    }

    // Priority 3: No valid config found, will use defaults
    Serial.println("[INFO] No valid configuration found, will use defaults");
    return false;
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

    // Save to NVS first (existing behavior)
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

    // Also save to config file (atomic write)
    ConfigFileData fileData;
    fileData.schemaVersion = 1;
    fileData.wifiSsid = config.wifiSsid;
    fileData.wifiPassword = config.wifiPassword;
    fileData.backendUrl = config.apiEndpoint;
    fileData.friendlyName = config.deviceId;

    // Convert intervals
    fileData.sensorReadInterval = config.readingIntervalMs / 1000;  // ms to seconds

    // Calculate upload interval from publish samples and reading interval
    uint32_t uploadIntervalMs = config.publishIntervalSamples * config.readingIntervalMs;
    fileData.dataUploadInterval = uploadIntervalMs / 1000;  // ms to seconds

    // Display brightness - use default since not in Config struct
    fileData.displayBrightness = 128;

    // Battery mode maps to enableDeepSleep
    fileData.enableDeepSleep = config.batteryMode;

    // Save to file
    if (fileManager.saveConfig(fileData)) {
        Serial.println("[INFO] Configuration saved to both NVS and file");
        return true;
    } else {
        Serial.println("[WARN] Configuration saved to NVS but failed to save to file");
        Serial.print("[ERROR] ");
        Serial.println(fileManager.getLastError());
        // Still return true since NVS save succeeded
        return true;
    }
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

    // If in provisioning mode, handle provisioning commands
    if (provisioningMode) {
        handleProvisioningSerialInput();
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
    } else if (command == "register") {
        // Trigger manual registration via callback
        Serial.println("Triggering manual registration...");
        if (registrationCallback) {
            registrationCallback();
        } else {
            Serial.println("ERROR: Registration callback not set");
        }
    } else if (command == "hwid") {
// Display hardware ID (MAC address)
#include "HardwareId.h"
        Serial.print("Hardware ID: ");
        Serial.println(HardwareId::getHardwareId());
    } else if (command == "bootid") {
        // Display current Boot ID
        if (bootIdRef) {
            Serial.print("Boot ID: ");
            Serial.println(*bootIdRef);
        } else {
            Serial.println("ERROR: Boot ID not available");
        }
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
    Serial.println("register  - Manually trigger device registration");
    Serial.println("hwid      - Display hardware ID (MAC address)");
    Serial.println("bootid    - Display current boot ID");
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

String ConfigManager::getConfirmationId() {
#ifdef ARDUINO
    return nvs.getString("dev.confirm_id", "");
#else
    return "";
#endif
}

void ConfigManager::setConfirmationId(const String& confirmationId) {
#ifdef ARDUINO
    nvs.putString("dev.confirm_id", confirmationId);
#endif
}

bool ConfigManager::hasValidConfirmationId() {
    String confirmationId = getConfirmationId();

    if (confirmationId.length() == 0) {
        return false;
    }

    // Validate UUID v4 format using BootId utility
    // We need to include BootId.h for this
    // Check length: 8-4-4-4-12 = 36 characters including hyphens
    if (confirmationId.length() != 36) {
#ifdef ARDUINO
        Serial.printf("[WARN] Invalid confirmation_id length: %d (expected 36)\n",
                      confirmationId.length());
#endif
        return false;
    }

    // Check hyphen positions
    if (confirmationId[8] != '-' || confirmationId[13] != '-' || confirmationId[18] != '-' ||
        confirmationId[23] != '-') {
#ifdef ARDUINO
        Serial.printf("[WARN] Invalid confirmation_id format: missing hyphens\n");
#endif
        return false;
    }

    // Check all other characters are hex digits
    for (size_t i = 0; i < confirmationId.length(); i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            continue;  // Skip hyphens
        }

        char c = confirmationId[i];
        bool isHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');

        if (!isHex) {
#ifdef ARDUINO
            Serial.printf("[WARN] Invalid confirmation_id: non-hex character at position %d\n", i);
#endif
            return false;
        }
    }

    // Check version bits: character at position 14 must be '4'
    if (confirmationId[14] != '4') {
#ifdef ARDUINO
        Serial.printf("[WARN] Invalid confirmation_id version: expected '4', got '%c'\n",
                      confirmationId[14]);
#endif
        return false;
    }

    // Check variant bits: character at position 19 must be 8, 9, A, B (or lowercase)
    char variantChar = confirmationId[19];
    bool validVariant = (variantChar == '8' || variantChar == '9' || variantChar == 'A' ||
                         variantChar == 'a' || variantChar == 'B' || variantChar == 'b');

    if (!validVariant) {
#ifdef ARDUINO
        Serial.printf("[WARN] Invalid confirmation_id variant: expected 8/9/A/B, got '%c'\n",
                      variantChar);
#endif
    }

    return validVariant;
}

void ConfigManager::setRegistrationCallback(RegistrationCallback callback) {
    registrationCallback = callback;
}

void ConfigManager::setBootIdReference(const String* bootId) {
    bootIdRef = bootId;
}

void ConfigManager::applyConfigFileData(const ConfigFileData& fileData) {
    // Map ConfigFileData fields to Config struct
    config.wifiSsid = fileData.wifiSsid;
    config.wifiPassword = fileData.wifiPassword;
    config.apiEndpoint = fileData.backendUrl;
    config.deviceId = fileData.friendlyName;

    // Convert intervals
    // dataUploadInterval (seconds) -> publishIntervalSamples
    // Assuming readingIntervalMs is used to calculate samples
    config.readingIntervalMs = fileData.sensorReadInterval * 1000;  // seconds to ms

    // Calculate publish interval samples based on upload interval and reading interval
    if (config.readingIntervalMs > 0) {
        uint32_t uploadIntervalMs = fileData.dataUploadInterval * 1000;
        config.publishIntervalSamples = uploadIntervalMs / config.readingIntervalMs;

        // Ensure at least 1 sample
        if (config.publishIntervalSamples < 1) {
            config.publishIntervalSamples = 1;
        }
    } else {
        config.publishIntervalSamples = 20;  // Default
    }

    // Display brightness is not in Config struct, so we skip it for now
    // It would be used by DisplayManager if needed

    // Battery mode maps to enableDeepSleep
    config.batteryMode = fileData.enableDeepSleep;

    // Keep existing values for fields not in ConfigFileData
    // (soilDryAdc, soilWetAdc, temperatureInFahrenheit, thresholds, pageCycleIntervalMs, etc.)
    // These will be loaded from NVS or use defaults
}

void ConfigManager::migrateNvsToFile() {
#ifdef ARDUINO
    Serial.printf("[INFO] ConfigManager: Migrating NVS config to file\n");

    // Convert current Config to ConfigFileData
    ConfigFileData fileData;

    fileData.schemaVersion = 1;
    fileData.wifiSsid = config.wifiSsid;
    fileData.wifiPassword = config.wifiPassword;
    fileData.backendUrl = config.apiEndpoint;
    fileData.friendlyName = config.deviceId;

    // Convert intervals
    fileData.sensorReadInterval = config.readingIntervalMs / 1000;  // ms to seconds

    // Calculate upload interval from publish samples and reading interval
    uint32_t uploadIntervalMs = config.publishIntervalSamples * config.readingIntervalMs;
    fileData.dataUploadInterval = uploadIntervalMs / 1000;  // ms to seconds

    // Display brightness - use default since not in Config struct
    fileData.displayBrightness = 128;

    // Battery mode maps to enableDeepSleep
    fileData.enableDeepSleep = config.batteryMode;

    Serial.printf("[INFO] ConfigManager: NVS config values:\n");
    Serial.printf("[INFO]   wifi_ssid: %s\n", fileData.wifiSsid.c_str());
    Serial.printf("[INFO]   backend_url: %s\n", fileData.backendUrl.c_str());
    Serial.printf("[INFO]   friendly_name: %s\n", fileData.friendlyName.c_str());
    Serial.printf("[INFO]   sensor_read_interval: %u\n", fileData.sensorReadInterval);
    Serial.printf("[INFO]   data_upload_interval: %u\n", fileData.dataUploadInterval);
    Serial.printf("[INFO]   enable_deep_sleep: %d\n", fileData.enableDeepSleep);

    // Save to file
    if (fileManager.saveConfig(fileData)) {
        Serial.printf("[INFO] ConfigManager: NVS config migrated to file successfully\n");
    } else {
        Serial.printf("[ERROR] ConfigManager: Failed to migrate NVS config to file\n");
        Serial.printf("[ERROR] %s\n", fileManager.getLastError().c_str());
    }
#endif
}

bool ConfigManager::hasRequiredFields() {
    // Required fields: wifi_ssid, wifi_password (can be empty for open networks), backend_url
    // wifi_ssid must be non-empty
    if (config.wifiSsid.length() == 0) {
        return false;
    }

    // backend_url (apiEndpoint) must be non-empty
    if (config.apiEndpoint.length() == 0) {
        return false;
    }

    // wifi_password is required but can be empty string for open networks
    // So we don't check its length

    return true;
}

String ConfigManager::getMissingRequiredFields() {
    String missing = "";

    if (config.wifiSsid.length() == 0) {
        if (missing.length() > 0)
            missing += ", ";
        missing += "wifi_ssid";
    }

    if (config.apiEndpoint.length() == 0) {
        if (missing.length() > 0)
            missing += ", ";
        missing += "backend_url";
    }

    return missing;
}

void ConfigManager::setTouchDetected(bool detected, TouchControllerType type) {
    touchDetected = detected;
    touchType = type;

#ifdef ARDUINO
    if (detected) {
        Serial.print("[INFO] Touch controller detected: ");
        switch (type) {
            case TouchControllerType::XPT2046:
                Serial.println("XPT2046 (SPI)");
                break;
            case TouchControllerType::FT6236:
                Serial.println("FT6236 (I2C)");
                break;
            case TouchControllerType::CST816:
                Serial.println("CST816 (I2C)");
                break;
            case TouchControllerType::GT911:
                Serial.println("GT911 (I2C)");
                break;
            default:
                Serial.println("Unknown");
                break;
        }
    } else {
        Serial.println("[INFO] No touch controller detected");
    }
#endif
}

bool ConfigManager::isConfigPageEnabled() const {
    return touchDetected;
}

void ConfigManager::enterProvisioningMode() {
#ifdef ARDUINO
    provisioningMode = true;
    provisioningWifiSsid = "";
    provisioningWifiPassword = "";
    provisioningBackendUrl = "";

    Serial.printf("\n========================================\n");
    Serial.printf("=== PROVISIONING MODE ===\n");
    Serial.printf("========================================\n");
    Serial.printf("[INFO] ConfigManager: Entered provisioning mode\n");
    Serial.printf("[INFO] Reason: Missing required configuration fields\n");

    // Log which fields are missing
    String missing = getMissingRequiredFields();
    if (missing.length() > 0) {
        Serial.printf("[INFO] Missing required fields: %s\n", missing.c_str());
    }

    Serial.printf("Device requires configuration.\n");
    Serial.printf("Please provide the following required fields:\n");
    Serial.printf("  1. WiFi SSID\n");
    Serial.printf("  2. WiFi Password (can be empty for open networks)\n");
    Serial.printf("  3. Backend URL\n");
    Serial.printf("\n");
    Serial.printf("Commands:\n");
    Serial.printf("  provision_wifi <ssid> <password>\n");
    Serial.printf("  provision_url <url>\n");
    Serial.printf("  provision_save\n");
    Serial.printf("  provision_cancel\n");
    Serial.printf("  provision_status\n");
    Serial.printf("========================================\n\n");
#endif
}

void ConfigManager::handleProvisioningSerialInput() {
#ifdef ARDUINO
    if (!Serial.available()) {
        return;
    }

    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.startsWith("provision_wifi ")) {
        // Parse: provision_wifi <ssid> <password>
        String params = command.substring(15);  // Skip "provision_wifi "
        int spaceIndex = params.indexOf(' ');

        if (spaceIndex > 0) {
            provisioningWifiSsid = params.substring(0, spaceIndex);
            provisioningWifiPassword = params.substring(spaceIndex + 1);
            provisioningWifiPassword.trim();

            Serial.print("WiFi SSID set to: ");
            Serial.println(provisioningWifiSsid);
            Serial.println("WiFi Password set");

            // Validate
            if (provisioningWifiSsid.length() == 0) {
                Serial.println("[ERROR] WiFi SSID cannot be empty");
            } else if (provisioningWifiSsid.length() > 128) {
                Serial.println("[ERROR] WiFi SSID too long (max 128 characters)");
            } else {
                Serial.println("[OK] WiFi credentials validated");
            }
        } else {
            Serial.println("[ERROR] Usage: provision_wifi <ssid> <password>");
            Serial.println("Example: provision_wifi MyNetwork MyPassword123");
            Serial.println("For open networks: provision_wifi MyNetwork \"\"");
        }
    } else if (command.startsWith("provision_url ")) {
        // Parse: provision_url <url>
        provisioningBackendUrl = command.substring(14);  // Skip "provision_url "
        provisioningBackendUrl.trim();

        Serial.print("Backend URL set to: ");
        Serial.println(provisioningBackendUrl);

        // Validate
        if (provisioningBackendUrl.length() == 0) {
            Serial.println("[ERROR] Backend URL cannot be empty");
        } else if (!provisioningBackendUrl.startsWith("http://") &&
                   !provisioningBackendUrl.startsWith("https://")) {
            Serial.println("[ERROR] Backend URL must start with http:// or https://");
        } else if (provisioningBackendUrl.length() > 128) {
            Serial.println("[ERROR] Backend URL too long (max 128 characters)");
        } else {
            Serial.println("[OK] Backend URL validated");
        }
    } else if (command == "provision_status") {
        Serial.println("\n=== Provisioning Status ===");
        Serial.print("WiFi SSID: ");
        Serial.println(provisioningWifiSsid.length() > 0 ? provisioningWifiSsid : "[NOT SET]");
        Serial.print("WiFi Password: ");
        Serial.println(provisioningWifiPassword.length() > 0 ? "[SET]" : "[NOT SET]");
        Serial.print("Backend URL: ");
        Serial.println(provisioningBackendUrl.length() > 0 ? provisioningBackendUrl : "[NOT SET]");
        Serial.println("");

        // Check if all required fields are set
        bool allSet = (provisioningWifiSsid.length() > 0 && provisioningBackendUrl.length() > 0);
        if (allSet) {
            Serial.println("Status: Ready to save");
        } else {
            Serial.println("Status: Missing required fields");
            if (provisioningWifiSsid.length() == 0) {
                Serial.println("  - WiFi SSID required");
            }
            if (provisioningBackendUrl.length() == 0) {
                Serial.println("  - Backend URL required");
            }
        }
        Serial.println("===========================\n");
    } else if (command == "provision_save") {
        // Validate all required fields
        bool valid = true;
        String errors = "";

        if (provisioningWifiSsid.length() == 0) {
            valid = false;
            errors += "  - WiFi SSID is required\n";
        } else if (provisioningWifiSsid.length() > 128) {
            valid = false;
            errors += "  - WiFi SSID too long (max 128 characters)\n";
        }

        if (provisioningBackendUrl.length() == 0) {
            valid = false;
            errors += "  - Backend URL is required\n";
        } else if (!provisioningBackendUrl.startsWith("http://") &&
                   !provisioningBackendUrl.startsWith("https://")) {
            valid = false;
            errors += "  - Backend URL must start with http:// or https://\n";
        } else if (provisioningBackendUrl.length() > 128) {
            valid = false;
            errors += "  - Backend URL too long (max 128 characters)\n";
        }

        if (!valid) {
            Serial.println("[ERROR] Cannot save configuration:");
            Serial.print(errors);
            Serial.println("Use 'provision_status' to check current values");
            Serial.printf("[ERROR] ConfigManager: Provisioning save failed - validation errors\n");
            return;
        }

        Serial.printf(
            "[INFO] ConfigManager: Provisioning validation passed, saving configuration\n");

        // Apply provisioned values to config
        config.wifiSsid = provisioningWifiSsid;
        config.wifiPassword = provisioningWifiPassword;
        config.apiEndpoint = provisioningBackendUrl;

        // Save to both NVS and config file
        bool nvsSuccess = saveConfig();
        bool fileSuccess = false;

        // Save to config file
        ConfigFileData fileData = fileManager.getDefaults(config.deviceId);
        fileData.wifiSsid = provisioningWifiSsid;
        fileData.wifiPassword = provisioningWifiPassword;
        fileData.backendUrl = provisioningBackendUrl;
        fileData.friendlyName = config.deviceId;
        fileData.displayBrightness = 128;
        fileData.dataUploadInterval = 60;
        fileData.sensorReadInterval = 10;
        fileData.enableDeepSleep = false;

        fileSuccess = fileManager.saveConfig(fileData);

        if (nvsSuccess && fileSuccess) {
            Serial.println("\n[SUCCESS] Configuration saved successfully!");
            Serial.printf(
                "[INFO] ConfigManager: Provisioning complete - configuration saved to NVS and "
                "file\n");
            Serial.printf("[INFO] ConfigManager: Rebooting device in 3 seconds...\n");
            Serial.println("Rebooting in 3 seconds...");
            delay(3000);
            ESP.restart();
        } else {
            Serial.println("\n[ERROR] Failed to save configuration:");
            Serial.printf("[ERROR] ConfigManager: Provisioning save failed\n");
            if (!nvsSuccess) {
                Serial.println("  - NVS save failed");
                Serial.printf("[ERROR]   - NVS save failed\n");
            }
            if (!fileSuccess) {
                Serial.println("  - Config file save failed");
                Serial.print("  - Error: ");
                Serial.println(fileManager.getLastError());
                Serial.printf("[ERROR]   - Config file save failed: %s\n",
                              fileManager.getLastError().c_str());
            }
        }
    } else if (command == "provision_cancel") {
        Serial.println("Provisioning cancelled. Returning to normal mode.");
        Serial.println("Note: Device is not fully configured and may not function properly.");
        Serial.printf("[WARN] ConfigManager: Provisioning cancelled by user\n");
        provisioningMode = false;
    } else if (command == "help" || command == "?") {
        Serial.println("\n=== Provisioning Commands ===");
        Serial.println("provision_wifi <ssid> <password> - Set WiFi credentials");
        Serial.println("provision_url <url>              - Set backend URL");
        Serial.println("provision_status                 - Show current values");
        Serial.println("provision_save                   - Save and reboot");
        Serial.println("provision_cancel                 - Cancel provisioning");
        Serial.println("help or ?                        - Show this help");
        Serial.println("============================\n");
    } else {
        Serial.println("[ERROR] Unknown command. Type 'help' for available commands.");
    }
#endif
}

bool ConfigManager::isInProvisioningMode() const {
    return provisioningMode;
}
