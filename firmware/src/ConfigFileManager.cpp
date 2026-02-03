#include "ConfigFileManager.h"

#ifdef ARDUINO
#include <ArduinoJson.h>
#include <LittleFS.h>
#else
// Mock implementations for native testing
#include <fstream>
#include <sstream>
#endif

ConfigFileManager::ConfigFileManager() : fsInitialized(false) {}

ConfigFileManager::~ConfigFileManager() {
#ifdef ARDUINO
    if (fsInitialized) {
        LittleFS.end();
    }
#endif
}

bool ConfigFileManager::initialize() {
#ifdef ARDUINO
    Serial.printf("[INFO] ConfigFileManager: Initializing LittleFS\n");
    if (!LittleFS.begin(true)) {  // true = format on mount failure
        lastError = "Failed to mount LittleFS";
        fsInitialized = false;
        Serial.printf("[ERROR] ConfigFileManager: %s\n", lastError.c_str());
        return false;
    }
    fsInitialized = true;
    Serial.printf("[INFO] ConfigFileManager: LittleFS initialized successfully\n");
    return true;
#else
    // For native testing, assume filesystem is always available
    fsInitialized = true;
    return true;
#endif
}

ConfigLoadResult ConfigFileManager::loadConfig(ConfigFileData& outConfig) {
    if (!fsInitialized) {
        lastError = "Filesystem not initialized";
        Serial.printf("[ERROR] ConfigFileManager: %s\n", lastError.c_str());
        return ConfigLoadResult::FS_MOUNT_ERROR;
    }

#ifdef ARDUINO
    Serial.printf("[INFO] ConfigFileManager: Loading config from /config.json\n");

    // Check if file exists
    if (!LittleFS.exists("/config.json")) {
        lastError = "Config file not found";
        Serial.printf("[INFO] ConfigFileManager: %s\n", lastError.c_str());
        return ConfigLoadResult::FILE_NOT_FOUND;
    }

    // Open file for reading
    File file = LittleFS.open("/config.json", "r");
    if (!file) {
        lastError = "Failed to open config file for reading";
        Serial.printf("[ERROR] ConfigFileManager: %s\n", lastError.c_str());
        return ConfigLoadResult::READ_ERROR;
    }

    // Read file content
    String fileContent = file.readString();
    size_t fileSize = file.size();
    file.close();

    Serial.printf("[INFO] ConfigFileManager: Read %u bytes from config file\n", fileSize);

    if (fileContent.length() == 0) {
        lastError = "Config file is empty";
        Serial.printf("[ERROR] ConfigFileManager: %s\n", lastError.c_str());
        return ConfigLoadResult::READ_ERROR;
    }

    // Parse JSON
    Serial.printf("[INFO] ConfigFileManager: Parsing JSON\n");
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, fileContent);

    if (error) {
        lastError = "JSON parsing failed: ";
        lastError += error.c_str();
        Serial.printf("[ERROR] ConfigFileManager: %s\n", lastError.c_str());
        return ConfigLoadResult::PARSE_ERROR;
    }

    Serial.printf("[INFO] ConfigFileManager: JSON parsed successfully\n");

    // Check for schema_version field
    if (!doc.containsKey("schema_version")) {
        lastError = "Missing schema_version field";
        Serial.printf("[ERROR] ConfigFileManager: %s\n", lastError.c_str());
        return ConfigLoadResult::SCHEMA_ERROR;
    }

    // Handle schema version
    uint32_t schemaVersion = doc["schema_version"].as<uint32_t>();
    const uint32_t CURRENT_SCHEMA_VERSION = 1;

    Serial.printf("[INFO] ConfigFileManager: Schema version: %u (current: %u)\n", schemaVersion,
                  CURRENT_SCHEMA_VERSION);

    if (schemaVersion > CURRENT_SCHEMA_VERSION) {
        // Newer schema version - forward compatibility
        Serial.printf(
            "[WARN] ConfigFileManager: Config file has newer schema version (%u), current version "
            "is %u. Unknown fields will be ignored.\n",
            schemaVersion, CURRENT_SCHEMA_VERSION);
    } else if (schemaVersion < CURRENT_SCHEMA_VERSION) {
        // Older schema version - backward compatibility
        Serial.printf(
            "[WARN] ConfigFileManager: Config file has older schema version (%u), current version "
            "is %u. Using defaults for missing fields.\n",
            schemaVersion, CURRENT_SCHEMA_VERSION);
    }

    // Extract and validate checksum
    if (!doc.containsKey("checksum")) {
        lastError = "Missing checksum field";
        Serial.printf("[ERROR] ConfigFileManager: %s\n", lastError.c_str());
        return ConfigLoadResult::CHECKSUM_ERROR;
    }

    String storedChecksum = doc["checksum"].as<String>();
    Serial.printf("[INFO] ConfigFileManager: Stored checksum: %s\n", storedChecksum.c_str());

    // Create a copy of the JSON with checksum set to empty string for validation
    StaticJsonDocument<1024> docForChecksum;
    docForChecksum = doc;
    docForChecksum["checksum"] = "";

    // Serialize to canonical form (minified, fixed field order)
    String canonicalJson;
    serializeJson(docForChecksum, canonicalJson);

    // Calculate checksum
    String calculatedChecksum = calculateChecksum(canonicalJson);
    Serial.printf("[INFO] ConfigFileManager: Calculated checksum: %s\n",
                  calculatedChecksum.c_str());

    // Validate checksum
    if (storedChecksum != calculatedChecksum) {
        lastError = "Checksum mismatch: expected " + storedChecksum + ", got " + calculatedChecksum;
        Serial.printf("[ERROR] ConfigFileManager: %s\n", lastError.c_str());
        return ConfigLoadResult::CHECKSUM_ERROR;
    }

    Serial.printf("[INFO] ConfigFileManager: Checksum validation passed\n");

    // Populate output structure
    outConfig.schemaVersion = schemaVersion;
    outConfig.checksum = storedChecksum;

    // Required fields
    outConfig.wifiSsid = doc["wifi_ssid"].as<String>();
    outConfig.wifiPassword = doc["wifi_password"].as<String>();
    outConfig.backendUrl = doc["backend_url"].as<String>();

    // Optional fields with defaults
    outConfig.friendlyName = doc["friendly_name"] | "";
    outConfig.displayBrightness = doc["display_brightness"] | 128;
    outConfig.dataUploadInterval = doc["data_upload_interval"] | 60;
    outConfig.sensorReadInterval = doc["sensor_read_interval"] | 10;
    outConfig.enableDeepSleep = doc["enable_deep_sleep"] | false;

    Serial.printf("[INFO] ConfigFileManager: Config loaded successfully\n");
    Serial.printf("[INFO]   wifi_ssid: %s\n", outConfig.wifiSsid.c_str());
    Serial.printf("[INFO]   backend_url: %s\n", outConfig.backendUrl.c_str());
    Serial.printf("[INFO]   friendly_name: %s\n", outConfig.friendlyName.c_str());
    Serial.printf("[INFO]   display_brightness: %u\n", outConfig.displayBrightness);
    Serial.printf("[INFO]   data_upload_interval: %u\n", outConfig.dataUploadInterval);
    Serial.printf("[INFO]   sensor_read_interval: %u\n", outConfig.sensorReadInterval);
    Serial.printf("[INFO]   enable_deep_sleep: %d\n", outConfig.enableDeepSleep);

    return ConfigLoadResult::SUCCESS;

#else
    // Native testing mock implementation
    lastError = "Native testing not fully implemented";
    return ConfigLoadResult::FILE_NOT_FOUND;
#endif
}

bool ConfigFileManager::saveConfig(const ConfigFileData& config) {
    if (!fsInitialized) {
        lastError = "Filesystem not initialized";
        Serial.printf("[ERROR] ConfigFileManager: %s\n", lastError.c_str());
        return false;
    }

#ifdef ARDUINO
    Serial.printf("[INFO] ConfigFileManager: Saving config to /config.json\n");

    // Create JSON document in canonical form (fixed field order)
    StaticJsonDocument<1024> doc;

    doc["schema_version"] = config.schemaVersion;
    doc["checksum"] = "";  // Will be calculated after serialization
    doc["wifi_ssid"] = config.wifiSsid;
    doc["wifi_password"] = config.wifiPassword;
    doc["backend_url"] = config.backendUrl;
    doc["friendly_name"] = config.friendlyName;
    doc["display_brightness"] = config.displayBrightness;
    doc["data_upload_interval"] = config.dataUploadInterval;
    doc["sensor_read_interval"] = config.sensorReadInterval;
    doc["enable_deep_sleep"] = config.enableDeepSleep;

    // Serialize to canonical form (minified)
    String jsonContent;
    serializeJson(doc, jsonContent);

    // Calculate checksum
    String checksum = calculateChecksum(jsonContent);
    Serial.printf("[INFO] ConfigFileManager: Calculated checksum: %s\n", checksum.c_str());

    // Update checksum in document
    doc["checksum"] = checksum;

    // Serialize final JSON with checksum
    String finalJson;
    serializeJson(doc, finalJson);

    Serial.printf("[INFO] ConfigFileManager: Final JSON size: %u bytes\n", finalJson.length());

    // Write to temporary file
    Serial.printf("[INFO] ConfigFileManager: Writing to temporary file /config.tmp\n");
    File tempFile = LittleFS.open("/config.tmp", "w");
    if (!tempFile) {
        lastError = "Failed to create temporary config file";
        Serial.printf("[ERROR] ConfigFileManager: %s\n", lastError.c_str());
        return false;
    }

    size_t bytesWritten = tempFile.print(finalJson);
    if (bytesWritten != finalJson.length()) {
        tempFile.close();
        LittleFS.remove("/config.tmp");
        lastError = "Failed to write complete config to temporary file";
        Serial.printf("[ERROR] ConfigFileManager: %s (wrote %u of %u bytes)\n", lastError.c_str(),
                      bytesWritten, finalJson.length());
        return false;
    }

    Serial.printf("[INFO] ConfigFileManager: Wrote %u bytes to temporary file\n", bytesWritten);

    // Flush filesystem buffers
    Serial.printf("[INFO] ConfigFileManager: Flushing filesystem buffers\n");
    tempFile.flush();
    tempFile.close();

    // Atomically rename temp file to config.json
    Serial.printf("[INFO] ConfigFileManager: Renaming /config.tmp to /config.json\n");
    if (LittleFS.exists("/config.json")) {
        Serial.printf("[INFO] ConfigFileManager: Removing existing /config.json\n");
        LittleFS.remove("/config.json");
    }

    if (!LittleFS.rename("/config.tmp", "/config.json")) {
        lastError = "Failed to rename temporary file to config.json";
        Serial.printf("[ERROR] ConfigFileManager: %s\n", lastError.c_str());
        LittleFS.remove("/config.tmp");
        return false;
    }

    Serial.printf("[INFO] ConfigFileManager: Config saved successfully\n");
    return true;

#else
    // Native testing mock implementation
    lastError = "Native testing not fully implemented";
    return false;
#endif
}

bool ConfigFileManager::validateConfig(const ConfigFileData& config, String& outError) {
    outError = "";
    Serial.printf("[INFO] ConfigFileManager: Validating config\n");

    // Validate required string fields (must not exceed 128 chars)
    if (!validateStringLength(config.wifiSsid, 128)) {
#ifdef ARDUINO
        outError =
            "wifi_ssid exceeds 128 characters (length: " + String(config.wifiSsid.length()) + ")";
#else
        outError =
            "wifi_ssid exceeds 128 characters (length: " + std::to_string(config.wifiSsid.size()) +
            ")";
#endif
        Serial.printf("[ERROR] ConfigFileManager: %s\n", outError.c_str());
        return false;
    }

    if (!validateStringLength(config.wifiPassword, 128)) {
#ifdef ARDUINO
        outError = "wifi_password exceeds 128 characters (length: " +
                   String(config.wifiPassword.length()) + ")";
#else
        outError = "wifi_password exceeds 128 characters (length: " +
                   std::to_string(config.wifiPassword.size()) + ")";
#endif
        Serial.printf("[ERROR] ConfigFileManager: %s\n", outError.c_str());
        return false;
    }

    if (!validateStringLength(config.backendUrl, 128)) {
#ifdef ARDUINO
        outError =
            "backend_url exceeds 128 characters (length: " + String(config.backendUrl.length()) +
            ")";
#else
        outError = "backend_url exceeds 128 characters (length: " +
                   std::to_string(config.backendUrl.size()) + ")";
#endif
        Serial.printf("[ERROR] ConfigFileManager: %s\n", outError.c_str());
        return false;
    }

    // Validate backend_url format
    if (!validateUrl(config.backendUrl)) {
        outError = "backend_url must start with http:// or https://";
        Serial.printf("[ERROR] ConfigFileManager: %s (value: %s)\n", outError.c_str(),
                      config.backendUrl.c_str());
        return false;
    }

    // Validate optional string fields (trim if needed, log warning)
    if (!validateStringLength(config.friendlyName, 128)) {
#ifdef ARDUINO
        outError = "Warning: friendly_name exceeds 128 characters (length: " +
                   String(config.friendlyName.length()) + "), will be trimmed";
        Serial.printf("[WARN] ConfigFileManager: %s\n", outError.c_str());
#else
        outError = "Warning: friendly_name exceeds 128 characters (length: " +
                   std::to_string(config.friendlyName.size()) + "), will be trimmed";
#endif
        // This is a warning, not an error - continue validation
    }

    // Validate integer ranges for optional fields (clamp if needed, log warning)
    if (!validateIntegerRange(config.displayBrightness, 0, 255)) {
#ifdef ARDUINO
        String warning = String("Warning: display_brightness out of range (") +
                         String(config.displayBrightness) + "), will be clamped to 0-255";
        Serial.printf("[WARN] ConfigFileManager: %s\n", warning.c_str());
        outError += (outError.length() > 0 ? "; " : "") + warning;
#else
        outError += (outError.size() > 0 ? "; " : "") +
                    std::string("Warning: display_brightness out of range (") +
                    std::to_string(config.displayBrightness) + "), will be clamped to 0-255";
#endif
    }

    if (!validateIntegerRange(config.dataUploadInterval, 10, 86400)) {
#ifdef ARDUINO
        String warning = String("Warning: data_upload_interval out of range (") +
                         String(config.dataUploadInterval) + "), will be clamped to 10-86400";
        Serial.printf("[WARN] ConfigFileManager: %s\n", warning.c_str());
        outError += (outError.length() > 0 ? "; " : "") + warning;
#else
        outError += (outError.size() > 0 ? "; " : "") +
                    std::string("Warning: data_upload_interval out of range (") +
                    std::to_string(config.dataUploadInterval) + "), will be clamped to 10-86400";
#endif
    }

    if (!validateIntegerRange(config.sensorReadInterval, 1, 3600)) {
#ifdef ARDUINO
        String warning = String("Warning: sensor_read_interval out of range (") +
                         String(config.sensorReadInterval) + "), will be clamped to 1-3600";
        Serial.printf("[WARN] ConfigFileManager: %s\n", warning.c_str());
        outError += (outError.length() > 0 ? "; " : "") + warning;
#else
        outError += (outError.size() > 0 ? "; " : "") +
                    std::string("Warning: sensor_read_interval out of range (") +
                    std::to_string(config.sensorReadInterval) + "), will be clamped to 1-3600";
#endif
    }

    // Validation passed (warnings are not errors)
    Serial.printf("[INFO] ConfigFileManager: Config validation passed\n");
    return true;
}

ConfigValidationResult ConfigFileManager::checkRequiredFields(const ConfigFileData& config) {
    ConfigValidationResult result;
    result.requiredFieldsPresent = true;
    result.missingFields = "";

    // Check wifi_ssid (must be non-empty)
#ifdef ARDUINO
    if (config.wifiSsid.length() == 0) {
#else
    if (config.wifiSsid.empty()) {
#endif
        result.requiredFieldsPresent = false;
        result.missingFields = "wifi_ssid";
    }

    // Check wifi_password (required field, but may be empty string for open networks)
    // No validation needed - empty string is valid

    // Check backend_url (must be non-empty)
#ifdef ARDUINO
    if (config.backendUrl.length() == 0) {
#else
    if (config.backendUrl.empty()) {
#endif
        result.requiredFieldsPresent = false;
#ifdef ARDUINO
        if (result.missingFields.length() > 0) {
            result.missingFields += ", ";
        }
#else
        if (!result.missingFields.empty()) {
            result.missingFields += ", ";
        }
#endif
        result.missingFields += "backend_url";
    }

    return result;
}

ConfigFileData ConfigFileManager::getDefaults(const String& hardwareId) {
    ConfigFileData defaults;

    // Schema metadata
    defaults.schemaVersion = 1;
    defaults.checksum = "";  // Will be calculated when saving

    // Required fields - leave empty (must be provided by user)
    defaults.wifiSsid = "";
    defaults.wifiPassword = "";
    defaults.backendUrl = "";

    // Optional fields with documented defaults
    defaults.friendlyName = "ESP32-Sensor-" + hardwareId;
    defaults.displayBrightness = 128;
    defaults.dataUploadInterval = 60;
    defaults.sensorReadInterval = 10;
    defaults.enableDeepSleep = false;

    return defaults;
}

String ConfigFileManager::getLastError() const {
    return lastError;
}

String ConfigFileManager::calculateChecksum(const String& jsonContent) {
    // Portable CRC32 implementation (works in firmware and native tests)
    static const uint32_t crc32_table[256] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535,
        0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd,
        0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d,
        0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
        0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
        0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac,
        0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab,
        0xb6662d3d, 0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
        0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb,
        0x086d3d2d, 0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea,
        0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65, 0x4db26158, 0x3ab551ce,
        0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
        0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409,
        0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739,
        0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
        0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344, 0x8708a3d2, 0x1e01f268,
        0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0,
        0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8,
        0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
        0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703,
        0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7,
        0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
        0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae,
        0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6,
        0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
        0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d,
        0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5,
        0x47b2cf7f, 0x30b5ffe9, 0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
        0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};

    uint32_t crc = 0xFFFFFFFF;

#ifdef ARDUINO
    const uint8_t* data = (const uint8_t*)jsonContent.c_str();
    size_t length = jsonContent.length();
#else
    const uint8_t* data = (const uint8_t*)jsonContent.data();
    size_t length = jsonContent.size();
#endif

    for (size_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }

    crc = crc ^ 0xFFFFFFFF;

    // Convert to 8-character hex string
    char hexStr[9];
#ifdef ARDUINO
    sprintf(hexStr, "%08X", (unsigned int)crc);
#else
    snprintf(hexStr, sizeof(hexStr), "%08X", (unsigned int)crc);
#endif

    return String(hexStr);
}

bool ConfigFileManager::validateStringLength(const String& str, uint16_t maxLen) {
#ifdef ARDUINO
    return str.length() <= maxLen;
#else
    return str.size() <= maxLen;
#endif
}

bool ConfigFileManager::validateIntegerRange(int32_t value, int32_t min, int32_t max) {
    return value >= min && value <= max;
}

bool ConfigFileManager::validateUrl(const String& url) {
#ifdef ARDUINO
    return url.startsWith("http://") || url.startsWith("https://");
#else
    return url.find("http://") == 0 || url.find("https://") == 0;
#endif
}
