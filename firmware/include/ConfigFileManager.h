#ifndef CONFIG_FILE_MANAGER_H
#define CONFIG_FILE_MANAGER_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <cstdint>
#include <string>
using String = std::string;
#endif

enum class ConfigLoadResult {
    SUCCESS,         // File loaded, parsed, checksum OK (may still have missing required fields)
    FILE_NOT_FOUND,  // No config file exists
    PARSE_ERROR,     // JSON parsing failed
    SCHEMA_ERROR,    // schema_version field missing
    CHECKSUM_ERROR,  // Checksum validation failed
    FS_MOUNT_ERROR,  // LittleFS mount failed
    READ_ERROR,      // Filesystem read failure
    WRITE_ERROR      // Filesystem write failure
};

struct ConfigValidationResult {
    bool requiredFieldsPresent;
    String missingFields;  // Comma-separated list of missing required fields
};

struct ConfigFileData {
    // Schema metadata
    uint32_t schemaVersion;
    String checksum;

    // Required fields
    String wifiSsid;
    String wifiPassword;  // May be empty string for open networks
    String backendUrl;

    // Optional fields (with defaults)
    String friendlyName;          // default: "ESP32-Sensor-{hardware_id}"
    uint8_t displayBrightness;    // default: 128 (0-255)
    uint32_t dataUploadInterval;  // default: 60 seconds
    uint32_t sensorReadInterval;  // default: 10 seconds
    bool enableDeepSleep;         // default: false
};

class ConfigFileManager {
   public:
    ConfigFileManager();
    ~ConfigFileManager();

    // Initialize LittleFS
    bool initialize();

    // Load config from /config.json
    ConfigLoadResult loadConfig(ConfigFileData& outConfig);

    // Save config to /config.json (atomic write via temp file)
    bool saveConfig(const ConfigFileData& config);

    // Validate config data
    bool validateConfig(const ConfigFileData& config, String& outError);

    // Check if required fields are present
    ConfigValidationResult checkRequiredFields(const ConfigFileData& config);

    // Get default config
    ConfigFileData getDefaults(const String& hardwareId);

    // Get last error message
    String getLastError() const;

   private:
    String lastError;
    bool fsInitialized;

    // CRC32 checksum calculation
    String calculateChecksum(const String& jsonContent);

    // Field validation helpers
    bool validateStringLength(const String& str, uint16_t maxLen);
    bool validateIntegerRange(int32_t value, int32_t min, int32_t max);
    bool validateUrl(const String& url);
};

#endif  // CONFIG_FILE_MANAGER_H
