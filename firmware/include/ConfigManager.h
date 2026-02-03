#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#ifdef ARDUINO
#include <Arduino.h>

#include <Preferences.h>
#else
#include <cstdint>
#include <functional>
#include <string>
#endif

#include "ConfigFileManager.h"
#include "TouchDetector.h"
#include "models/Config.h"

// Callback type for registration command
typedef std::function<void()> RegistrationCallback;

class ConfigManager {
   public:
    ConfigManager();
    ~ConfigManager();

    void initialize();
    bool loadConfig();
    bool saveConfig();
    void setDefaults();
    bool validateConfig();

    // New: Load from config file (called before NVS load)
    bool loadFromFile();

    // New: Check if required fields are present
    bool hasRequiredFields();

    // New: Get list of missing required fields
    String getMissingRequiredFields();

    void handleSerialConfig();

    Config& getConfig();

    // Public for testing
    String sanitizeSensitiveData(const String& data);

    // Registration persistence methods
    String getConfirmationId();
    void setConfirmationId(const String& confirmationId);
    bool hasValidConfirmationId();

    // Set callback for registration command (called from main.cpp)
    void setRegistrationCallback(RegistrationCallback callback);

    // Set Boot ID reference (called from main.cpp)
    void setBootIdReference(const String* bootId);

    // New: Set touch detection result
    void setTouchDetected(bool detected, TouchControllerType type);

    // New: Check if config page should be enabled
    bool isConfigPageEnabled() const;

    // Provisioning mode methods
    void enterProvisioningMode();
    void handleProvisioningSerialInput();
    bool isInProvisioningMode() const;

   private:
    Config config;
#ifdef ARDUINO
    Preferences nvs;
#endif

    // ConfigFileManager for file-based configuration
    ConfigFileManager fileManager;

    // Callback for registration command
    RegistrationCallback registrationCallback;

    // Reference to Boot ID (owned by main.cpp)
    const String* bootIdRef;

    // Touch detection state
    bool touchDetected;
    TouchControllerType touchType;

    // Provisioning mode state
    bool provisioningMode;
    String provisioningWifiSsid;
    String provisioningWifiPassword;
    String provisioningBackendUrl;

    // Helper to convert ConfigFileData to Config
    void applyConfigFileData(const ConfigFileData& fileData);

    // Migrate NVS config to file
    void migrateNvsToFile();

    void printConfigMenu();
    void printCurrentConfig();
    void updateWiFiCredentials();
    void updateApiEndpoint();
    void updateIntervals();
    void updateCalibration();
    void updateDeviceId();

    bool validateWiFiCredentials();
    bool validateApiEndpoint();
    bool validateIntervals();
    bool validateCalibration();
};

#endif  // CONFIG_MANAGER_H
