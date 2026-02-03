# Requirements Document

## Introduction

This feature enables the ESP32 firmware to detect touch screen capability at runtime and provide alternative configuration methods for devices without touch screens. The system will conditionally display the touch-based configuration page only when touch input is available, and provide a boot-time configuration file loading mechanism for non-touch devices.

## Glossary

- **Touch_Screen**: A display device that supports touch input detection
- **Config_Page**: The touch-based user interface for configuring device settings
- **Config_File**: A JSON file stored at `/config.json` on the LittleFS filesystem containing configuration settings
- **TFT_Display**: The TFT display managed by the TFT_eSPI library
- **Boot_Time**: The initialization phase when the ESP32 firmware starts
- **Touch_Detection**: The process of determining whether the display supports touch input by probing hardware interfaces
- **Firmware**: The ESP32 application software
- **Touch_Controller**: Hardware chip that handles touch input (e.g., XPT2046, FT6236, CST816, GT911)
- **Default_Settings**: Predefined configuration values used when Config_File is missing or invalid
- **Required_Field**: A configuration field that must be present for the device to function (e.g., WiFi credentials)
- **Optional_Field**: A configuration field that has a safe default value

## Requirements

### Requirement 1: Touch Screen Detection

**User Story:** As a firmware developer, I want to detect touch screen capability at runtime, so that the system can adapt to different hardware configurations.

#### Acceptance Criteria

1. WHEN the Firmware initializes, THE Touch_Detection SHALL attempt detection using the configured touch controller drivers in priority order: SPI (e.g., XPT2046) then I2C (e.g., FT6236, CST816, GT911)
2. FOR each supported controller probe, THE Touch_Detection SHALL use per-probe timeouts and SHALL NOT block longer than 150ms per controller
3. THE Touch_Detection SHALL consider touch "detected" only if it successfully reads a stable controller identity or produces at least one valid touch sample (per driver)
4. THE Touch_Detection SHALL complete within 500ms total (sum of all probes)
5. THE Touch_Detection SHALL require two consecutive successful identity or sample reads before reporting positive detection

### Requirement 2: Conditional Config Page Display

**User Story:** As a user with a touch screen device, I want to access the configuration page, so that I can configure device settings through the touch interface.

#### Acceptance Criteria

1. WHEN touch capability is detected, THE Firmware SHALL enable and display the Config_Page
2. WHEN touch capability is not detected, THE Firmware SHALL disable the Config_Page
3. WHILE the Config_Page is disabled, THE Firmware SHALL NOT initialize any touch driver, SHALL NOT start a touch polling task, and SHALL NOT attach/enable any touch IRQ interrupt handlers

### Requirement 3: Configuration File Loading

**User Story:** As a user with a non-touch device, I want to configure settings via a configuration file, so that I can set up the device without touch input.

#### Acceptance Criteria

1. WHEN the Firmware boots, THE Firmware SHALL check for the presence of a Config_File
2. IF a Config_File exists, THEN THE Firmware SHALL parse and load the configuration settings
3. WHEN the Config_File contains invalid data, THE Firmware SHALL log an error and use default settings for Optional_Fields
4. THE Config_File SHALL support all settings available in the Config_Page
5. THE Firmware SHALL validate that all Required_Fields are present after config load; if any Required_Field is missing, THE Firmware SHALL treat the Config_File as invalid
6. WHEN Required_Fields are missing or invalid, THE Firmware SHALL enter provisioning mode (serial console configuration) and SHALL NOT attempt normal operation using placeholder defaults

### Requirement 4: Configuration File Format

**User Story:** As a firmware developer, I want a well-defined configuration file format, so that configuration files are consistent and parseable.

#### Acceptance Criteria

1. THE Config_File SHALL be JSON
2. THE Config_File SHALL include validation markers or checksums to detect corruption
3. WHEN parsing the Config_File, THE Firmware SHALL validate all field types and ranges
4. THE Firmware SHALL provide clear error messages for malformed Config_File entries
5. THE Config_File SHALL include schema_version (integer) and a checksum field computed using CRC32 over the raw file bytes with the checksum field set to an empty string
6. WHEN checksum validation fails, THE Firmware SHALL treat the Config_File as invalid and proceed via fallback behavior (provisioning mode)

### Requirement 5: Configuration Priority

**User Story:** As a user, I want to understand which configuration method takes precedence, so that I can predict device behavior.

#### Acceptance Criteria

1. WHEN both touch capability exists and a Config_File is present, THE Firmware SHALL prioritize the Config_File settings at boot
2. WHEN touch capability exists, THE Config_Page SHALL allow runtime modification of settings
3. WHEN settings are modified via the Config_Page and the user selects "Save", THE Firmware SHALL write temp file, flush to filesystem, then rename over /config.json
4. THE Config_Page SHALL NOT apply changes permanently until "Save" succeeds; if save fails, the Firmware SHALL keep the prior persisted config

### Requirement 6: Configuration Settings Coverage

**User Story:** As a user, I want the configuration file to support all settings from the config page, so that I have feature parity regardless of input method.

#### Acceptance Criteria

1. THE Config_File SHALL support all network configuration settings available in the Config_Page
2. THE Config_File SHALL support all display configuration settings available in the Config_Page
3. THE Config_File SHALL support all device identification settings available in the Config_Page
4. WHEN a setting is added to the Config_Page, THE Config_File format SHALL be updated to include it
5. WHEN loading Config_File, THE Firmware SHALL ignore unknown fields (forward compatibility)
6. WHEN a supported Optional_Field is missing, THE Firmware SHALL use Default_Settings for that field

### Requirement 7: Error Handling and Fallback

**User Story:** As a user, I want the device to function with default settings if configuration fails, so that the device remains operational.

#### Acceptance Criteria

1. WHEN the Config_File is missing, THE Firmware SHALL use Default_Settings for Optional_Fields only
2. WHEN the Config_File parsing fails, THE Firmware SHALL log the error and use default values for Optional_Fields
3. WHEN touch detection fails, THE Firmware SHALL assume no touch capability and proceed with Config_File loading
4. WHEN configuration loading fails, THE Firmware SHALL display a one-line status on the TFT_Display indicating the failure reason: missing, parse_error, schema_error, or checksum_error

### Requirement 8: Configuration Field Definitions

**User Story:** As a firmware developer, I want clearly defined configuration fields, so that I can implement validation and ensure consistency.

#### Acceptance Criteria

1. THE Config_File SHALL support the following Required_Fields:
   - `wifi_ssid` (string): WiFi network name
   - `wifi_password` (string): WiFi network password
   - `backend_url` (string): Backend server URL for device registration and data upload

2. THE Config_File SHALL support the following Optional_Fields with defaults:
   - `friendly_name` (string, default: "ESP32-Sensor-{hardware_id}"): Human-readable device name
   - `display_brightness` (integer 0-255, default: 128): TFT display brightness level
   - `data_upload_interval` (integer seconds, default: 60): Interval between data uploads
   - `sensor_read_interval` (integer seconds, default: 10): Interval between sensor readings
   - `enable_deep_sleep` (boolean, default: false): Enable deep sleep power saving mode

3. THE Firmware SHALL validate that string fields do not exceed 128 characters
4. THE Firmware SHALL validate that integer fields are within their specified ranges
5. THE Firmware SHALL validate that backend_url begins with http:// or https://
6. WHEN enable_deep_sleep is true, THE Firmware SHALL enter deep sleep after each successful data upload cycle
