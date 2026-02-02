#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#ifdef ARDUINO
#include <Arduino.h>

#include <Preferences.h>
#else
#include <cstdint>
#include <string>
#endif

#include "models/Config.h"

class ConfigManager {
   public:
    ConfigManager();
    ~ConfigManager();

    void initialize();
    bool loadConfig();
    bool saveConfig();
    void setDefaults();
    bool validateConfig();

    void handleSerialConfig();

    Config& getConfig();

    // Public for testing
    String sanitizeSensitiveData(const String& data);

   private:
    Config config;
#ifdef ARDUINO
    Preferences nvs;
#endif

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
