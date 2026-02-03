# Implementation Plan: Touch Screen Detection and Configuration File

## Overview

This implementation adds runtime touch screen detection and a configuration file system to the ESP32 firmware. The system will detect touch controllers at boot, conditionally enable the touch-based config page, and provide a JSON configuration file mechanism for devices without touch screens.

The implementation follows the existing firmware architecture with two new components:
- **TouchDetector**: Probes SPI and I2C touch controllers with timeout enforcement
- **ConfigFileManager**: Manages JSON config files on LittleFS with atomic writes and checksum validation

## Tasks

- [x] 1. Set up firmware and test environments
  - [x] 1.1 Set up firmware dependencies and filesystem
    - Add ArduinoJson@^6 to firmware library dependencies in platformio.ini
    - Ensure LittleFS is enabled for the target framework and board
    - Add or update partition table with LittleFS region (recommend 128KB)
    - _Requirements: 3.1, 4.1_

  - [x] 1.2 Set up native test environment
    - Add native test environment in platformio.ini for host-side property tests
    - Configure native environment with mocked SPI/I2C/LittleFS adapters for testing
    - Add RapidCheck library to native test environment dependencies only (not firmware)
    - _Requirements: N/A (testing infrastructure)_

- [x] 2. Implement ConfigFileManager component
  - [x] 2.1 Create ConfigFileManager header and data structures
    - Define ConfigLoadResult enum (SUCCESS, FILE_NOT_FOUND, PARSE_ERROR, SCHEMA_ERROR, CHECKSUM_ERROR, FS_MOUNT_ERROR, READ_ERROR, WRITE_ERROR)
    - Define ConfigValidationResult struct with requiredFieldsPresent flag and missingFields list
    - Define ConfigFileData struct with all config fields
    - Define ConfigFileManager class interface
    - Create firmware/include/ConfigFileManager.h
    - _Requirements: 3.1, 3.2, 4.1, 8.1, 8.2_

  - [x] 2.2 Implement LittleFS initialization
    - Implement initialize() method to mount LittleFS
    - Handle mount failures gracefully
    - Set fsInitialized flag on success
    - Return false and set lastError on failure
    - Create firmware/src/ConfigFileManager.cpp
    - _Requirements: 3.1_

  - [x] 2.3 Implement CRC32 checksum calculation
    - Implement calculateChecksum() using a portable CRC32 implementation (works in firmware and native tests)
    - Serialize JSON to canonical form with these rules:
      - UTF-8 encoding
      - No whitespace (minified)
      - Stable key order (fixed field order as documented)
      - Numbers serialized without trailing .0
      - Checksum field set to empty string during calculation
    - Calculate CRC32 over canonical UTF-8 bytes
    - Return 8-character hex string
    - _Requirements: 4.2, 4.6_

  - [x] 2.4 Implement config file loading
    - Implement loadConfig() to read /config.json
    - Parse JSON using ArduinoJson
    - Validate checksum before accepting data
    - Return appropriate ConfigLoadResult (including READ_ERROR for filesystem read failures)
    - Populate ConfigFileData output parameter
    - _Requirements: 3.2, 4.1, 4.2_

  - [x] 2.5 Implement field validation
    - Implement validateStringLength() for 128 char limit
    - Implement validateIntegerRange() for numeric bounds
    - Implement validateUrl() for http:// or https:// prefix
    - Implement validateConfig() to check all fields
    - Return detailed error messages via outError parameter
    - _Requirements: 4.3, 8.3, 8.4, 8.5_

  - [x] 2.6 Implement required field validation
    - Implement checkRequiredFields() to verify wifi_ssid, wifi_password, backend_url
    - Return ConfigValidationResult with missing field list
    - Note: wifi_password may be empty string (open networks)
    - _Requirements: 3.5, 3.6_

  - [x] 2.7 Implement atomic config file save
    - Implement saveConfig() with temp file write
    - Write to /config.tmp first
    - Calculate and store checksum
    - Flush filesystem buffers
    - Atomically rename /config.tmp to /config.json
    - Keep existing config if any step fails
    - Return WRITE_ERROR on any filesystem write failure (temp file creation, flush, or rename)
    - _Requirements: 5.3, 5.4_

  - [x] 2.8 Implement default config generation
    - Implement getDefaults() to create ConfigFileData with defaults
    - Use hardware ID for friendly_name default
    - Set all optional fields to documented defaults
    - Leave required fields empty (must be provided by user)
    - _Requirements: 6.6, 7.1, 8.2_

  - [x] 2.9 Implement schema version handling
    - Check schema_version field during parsing
    - Handle missing schema_version as SCHEMA_ERROR
    - Handle newer versions (> 1) with forward compatibility
    - Handle older versions (< 1) with backward compatibility
    - Log appropriate warnings for version mismatches
    - _Requirements: 4.5_

  - [ ]* 2.10 Write property test for config file loading (host-native)
    - **Property 8: Config File Loading**
    - **Validates: Requirements 3.2**
    - Test that valid config files are parsed correctly
    - Generate random valid config files
    - Tag: Feature: touch-screen-detection-and-config-file, Property 8

  - [ ]* 2.11 Write property test for invalid config handling (host-native)
    - **Property 9: Invalid Config Handling with Defaults**
    - **Validates: Requirements 3.3, 7.2**
    - Test that invalid optional fields use defaults
    - Generate random configs with invalid optional fields
    - Tag: Feature: touch-screen-detection-and-config-file, Property 9

  - [ ]* 2.12 Write property test for required field validation (host-native)
    - **Property 10: Required Field Validation and Provisioning**
    - **Validates: Requirements 3.5, 3.6**
    - Test that missing required fields trigger provisioning mode
    - Generate random configs with missing required fields
    - Tag: Feature: touch-screen-detection-and-config-file, Property 10

  - [ ]* 2.13 Write property test for checksum validation (host-native)
    - **Property 11: Checksum Validation**
    - **Validates: Requirements 4.2, 4.6**
    - Test that checksum mismatches are detected
    - Generate random config files and corrupt checksums
    - Tag: Feature: touch-screen-detection-and-config-file, Property 11

  - [ ]* 2.14 Write property test for field type and range validation (host-native)
    - **Property 12: Field Type and Range Validation**
    - **Validates: Requirements 4.3**
    - Test validation of all field types and ranges
    - Generate random field values including out-of-range values
    - Tag: Feature: touch-screen-detection-and-config-file, Property 12

  - [ ]* 2.15 Write property test for validation error messages (host-native)
    - **Property 13: Validation Error Messages**
    - **Validates: Requirements 4.4**
    - Test that error messages are descriptive
    - Generate random validation failures
    - Tag: Feature: touch-screen-detection-and-config-file, Property 13

  - [ ]* 2.16 Write property test for atomic config save (host-native)
    - **Property 15: Atomic Config Save**
    - **Validates: Requirements 5.3, 5.4**
    - Test that failed saves don't corrupt existing config
    - Simulate filesystem failures during save
    - Tag: Feature: touch-screen-detection-and-config-file, Property 15

  - [ ]* 2.17 Write property test for forward compatibility (host-native)
    - **Property 17: Forward Compatibility**
    - **Validates: Requirements 6.5**
    - Test that unknown fields are ignored
    - Generate configs with random unknown fields
    - Tag: Feature: touch-screen-detection-and-config-file, Property 17

  - [ ]* 2.18 Write property test for optional field defaults (host-native)
    - **Property 18: Optional Field Defaults**
    - **Validates: Requirements 6.6, 7.1**
    - Test that missing optional fields use defaults
    - Generate configs with random missing optional fields
    - Tag: Feature: touch-screen-detection-and-config-file, Property 18

  - [ ]* 2.19 Write property test for string length validation (host-native)
    - **Property 21: String Length Validation**
    - **Validates: Requirements 8.3**
    - Test trimming of optional fields and rejection of required fields
    - Generate random strings exceeding 128 characters
    - Tag: Feature: touch-screen-detection-and-config-file, Property 21

  - [ ]* 2.20 Write property test for integer range validation (host-native)
    - **Property 22: Integer Range Validation**
    - **Validates: Requirements 8.4**
    - Test clamping of optional fields and rejection of required fields
    - Generate random out-of-range integers
    - Tag: Feature: touch-screen-detection-and-config-file, Property 22

  - [ ]* 2.21 Write property test for URL format validation (host-native)
    - **Property 23: URL Format Validation**
    - **Validates: Requirements 8.5**
    - Test rejection of invalid URL schemes
    - Generate random invalid URLs
    - Tag: Feature: touch-screen-detection-and-config-file, Property 23

  - [ ]* 2.22 Write property test for config persistence round-trip (host-native)
    - **Property 25: Config Persistence Round-Trip**
    - **Validates: System reliability and data integrity**
    - Test that saved configs can be loaded successfully
    - Generate random valid configs, save, then load
    - Tag: Feature: touch-screen-detection-and-config-file, Property 25

  - [ ]* 2.23 Write unit tests for ConfigFileManager
    - Test JSON parsing with specific valid examples
    - Test checksum calculation with known values
    - Test atomic write sequence with mocked filesystem
    - Test error handling for corrupt files
    - Test default value application
    - Test schema version handling

- [x] 3. Integrate ConfigFileManager with ConfigManager
  - [x] 3.1 Add config file loading to ConfigManager
    - Add ConfigFileManager member to ConfigManager
    - Add loadFromFile() method to ConfigManager
    - Initialize LittleFS during ConfigManager initialization or inside loadFromFile() (not in constructor)
    - Update firmware/include/ConfigManager.h and firmware/src/ConfigManager.cpp
    - _Requirements: 3.1, 3.2, 5.1_

  - [x] 3.2 Implement config loading priority logic
    - Modify ConfigManager initialization to try config file first
    - Fall back to NVS if config file missing or invalid
    - Apply defaults for optional fields only if both fail
    - Implement NVS to file migration for first boot after update
    - _Requirements: 5.1, 7.1, 7.2_

  - [x] 3.3 Implement required field validation and provisioning mode
    - Check required fields after loading from any source
    - If file loads successfully but required fields missing: treat as "valid parse" but "invalid for run"
    - Enter provisioning mode if required fields missing
    - Display specific error on TFT: "Config: missing required" (separate from ConfigLoadResult errors)
    - Do NOT use placeholder defaults for required fields
    - Log clear error messages with list of missing required fields
    - _Requirements: 3.5, 3.6, 7.1_

  - [x] 3.4 Implement config save integration
    - Update existing config save methods to write to both file and NVS
    - Use atomic write via ConfigFileManager
    - Handle save failures gracefully
    - _Requirements: 5.3, 5.4_

  - [ ]* 3.5 Write property test for configuration priority (host-native)
    - **Property 14: Configuration Priority**
    - **Validates: Requirements 5.1**
    - Test that config file takes priority over NVS
    - Generate random configs in both file and NVS
    - Tag: Feature: touch-screen-detection-and-config-file, Property 14

  - [ ]* 3.6 Write property test for configuration coverage parity (host-native)
    - **Property 16: Configuration Coverage Parity**
    - **Validates: Requirements 6.1, 6.2, 6.3**
    - Test that all config page settings exist in file format
    - Compare ConfigFileData fields with Config struct fields
    - Tag: Feature: touch-screen-detection-and-config-file, Property 16

  - [ ]* 3.7 Write unit tests for config file integration
    - Test config loading from file with specific examples
    - Test NVS fallback when file missing
    - Test default application for optional fields
    - Test required field validation
    - Test provisioning mode entry
    - Test config save to both file and NVS

- [ ] 4. Checkpoint - ConfigFileManager complete
  - Native tests pass (CI green for host environment)
  - Firmware compiles for all target boards
  - Static analysis/lint clean (if configured)

- [x] 5. Implement TouchDetector component
  - [x] 5.1 Create TouchDetector header and basic structure
    - Define TouchControllerType enum (NONE, XPT2046, FT6236, CST816, GT911)
    - Add invariant comment: detected == false if and only if type == TouchControllerType::NONE
    - Define TouchDetectionResult struct with detected, type, detectionTimeMs fields
    - Define TouchDetector class with detect(), getLastResult() signatures
    - Define private probe methods: probeXPT2046, probeFT6236, probeCST816, probeGT911
    - Define private verifyStability() method
    - Create firmware/include/TouchDetector.h
    - _Requirements: 1.1, 1.2, 1.3_

  - [x] 5.2 Implement SPI touch controller probe
    - Implement probeXPT2046() for SPI resistive touch with 150ms timeout
    - Attempt to read controller ID register or generate test touch sample
    - Treat communication failures as "not present"
    - Add timeout enforcement using millis() tracking
    - Create firmware/src/TouchDetector.cpp
    - _Requirements: 1.1, 1.2, 1.3_

  - [x] 5.3 Implement I2C touch controller probes
    - Implement probeFT6236() for I2C address 0x38 with 150ms timeout
    - Implement probeCST816() for I2C address 0x15 with 150ms timeout
    - Implement probeGT911() for I2C addresses 0x5D/0x14 with 150ms timeout
    - Each probe reads known ID register, treats NACK as "not present"
    - Add timeout enforcement for each probe
    - _Requirements: 1.1, 1.2, 1.3_

  - [x] 5.4 Implement stability verification
    - Implement verifyStability() requiring 2 consecutive successful reads
    - Add 10ms delay between stability check reads
    - Return true only after both reads succeed
    - _Requirements: 1.5_

  - [x] 5.5 Implement main detect() method with priority order
    - Probe SPI controllers first (XPT2046)
    - Probe I2C controllers second (FT6236, CST816, GT911)
    - Enforce 500ms total timeout with watchdog
    - Call verifyStability() before reporting positive detection
    - Store result in lastResult member
    - Return TouchDetectionResult with timing information
    - _Requirements: 1.1, 1.4, 1.5_

  - [ ]* 5.6 Write property test for detection priority order (host-native)
    - **Property 1: Detection Priority Order**
    - **Validates: Requirements 1.1**
    - Test that SPI probes execute before I2C probes across random hardware configurations
    - Use mocked hardware to verify probe order
    - Tag: Feature: touch-screen-detection-and-config-file, Property 1

  - [ ]* 5.7 Write property test for per-probe timeout enforcement (host-native)
    - **Property 2: Per-Probe Timeout Enforcement**
    - **Validates: Requirements 1.2**
    - Test that no individual probe exceeds 150ms across random scenarios
    - Use mocked hardware with configurable delays
    - Tag: Feature: touch-screen-detection-and-config-file, Property 2

  - [ ]* 5.8 Write property test for detection criteria (host-native)
    - **Property 3: Detection Criteria**
    - **Validates: Requirements 1.3**
    - Test that detection succeeds only with valid ID or touch sample
    - Generate random probe responses
    - Tag: Feature: touch-screen-detection-and-config-file, Property 3

  - [ ]* 5.9 Write property test for total detection timeout (host-native)
    - **Property 4: Total Detection Timeout**
    - **Validates: Requirements 1.4**
    - Test that complete detection never exceeds 500ms
    - Test across all controller combinations
    - Tag: Feature: touch-screen-detection-and-config-file, Property 4

  - [ ]* 5.10 Write property test for detection stability (host-native)
    - **Property 5: Detection Stability**
    - **Validates: Requirements 1.5**
    - Test that detection requires 2 consecutive successful reads
    - Generate random sequences of successful/failed reads
    - Tag: Feature: touch-screen-detection-and-config-file, Property 5

  - [ ]* 5.11 Write unit tests for TouchDetector
    - Test each probe path via detect() with mocked SPI/I2C hardware
    - Test timeout handling for hung hardware
    - Test detection result caching
    - Test edge cases (no controllers, multiple controllers)

- [x] 6. Integrate TouchDetector with ConfigManager
  - [x] 6.1 Add touch detection support to ConfigManager
    - Add setTouchDetected() method to ConfigManager
    - Add touchDetected and touchType member variables
    - Add isConfigPageEnabled() method
    - Update firmware/include/ConfigManager.h
    - _Requirements: 2.1, 2.2_

  - [x] 6.2 Implement touch detection state management
    - Implement setTouchDetected() to store detection result
    - Implement isConfigPageEnabled() to return touchDetected flag
    - Update firmware/src/ConfigManager.cpp
    - _Requirements: 2.1, 2.2_

  - [ ]* 6.3 Write property test for config page state consistency (host-native)
    - **Property 6: Config Page State Consistency**
    - **Validates: Requirements 2.1, 2.2**
    - Test that config page enabled state matches touch detection
    - Generate random touch detection results
    - Tag: Feature: touch-screen-detection-and-config-file, Property 6

  - [ ]* 6.4 Write unit tests for touch detection integration
    - Test setTouchDetected() with various controller types
    - Test isConfigPageEnabled() returns correct state
    - Test integration with existing ConfigManager functionality

- [x] 7. Integrate with DisplayManager
  - [x] 7.1 Add touch enable/disable to DisplayManager
    - Add setTouchEnabled() method to DisplayManager
    - Add touchEnabled and touchType member variables
    - Add conditional touch driver initialization
    - Update firmware/include/DisplayManager.h
    - _Requirements: 2.2, 2.3_

  - [x] 7.2 Implement conditional touch driver initialization
    - Modify initializeTouchDriver() to check touchEnabled flag
    - Only allocate touch driver resources if touchEnabled is true
    - Skip touch polling task creation if touchEnabled is false
    - Skip touch IRQ handler attachment if touchEnabled is false
    - Update firmware/src/DisplayManager.cpp
    - _Requirements: 2.3_

  - [x] 7.3 Implement config error display
    - Add showConfigError() method to DisplayManager
    - Display one-line error message at bottom of screen
    - Map ConfigLoadResult to user-friendly messages (including READ_ERROR and WRITE_ERROR)
    - Add separate method or parameter to display "Config: missing required" for validation failures
    - Use yellow text color for visibility
    - _Requirements: 7.4_

  - [ ]* 7.4 Write property test for resource conservation (host-native)
    - **Property 7: Resource Conservation**
    - **Validates: Requirements 2.3**
    - Test that touch resources are not allocated when disabled
    - Check driver initialization, polling task, and IRQ handlers
    - Tag: Feature: touch-screen-detection-and-config-file, Property 7

  - [ ]* 7.5 Write property test for config error display (host-native)
    - **Property 20: Config Error Display**
    - **Validates: Requirements 7.4**
    - Test that error messages are displayed for all ConfigLoadResult values and for missing required fields
    - Generate random ConfigLoadResult values and validation failure scenarios
    - Tag: Feature: touch-screen-detection-and-config-file, Property 20

  - [ ]* 7.6 Write unit tests for DisplayManager integration
    - Test setTouchEnabled() with various controller types
    - Test conditional touch driver initialization
    - Test showConfigError() with all error types
    - Test resource allocation when touch disabled

- [x] 8. Update main.cpp boot sequence
  - [x] 8.1 Add touch detection to boot sequence
    - Create TouchDetector instance in setup()
    - Call detect() after display initialization
    - Pass result to ConfigManager via setTouchDetected()
    - Log detection result with controller type and timing
    - Note: Touch detection happens early but independently of config loading
    - Update firmware/src/main.cpp
    - _Requirements: 1.1, 2.1_

  - [x] 8.2 Update config loading sequence
    - Call ConfigManager::loadFromFile() before NVS load
    - Handle ConfigLoadResult and display errors if needed
    - Validate required fields after loading
    - Enter provisioning mode if required fields missing
    - Note: Config loading does NOT depend on touch detection state
    - _Requirements: 3.1, 3.2, 3.5, 3.6_

  - [x] 8.3 Update DisplayManager initialization
    - Pass touch detection result to DisplayManager
    - Call setTouchEnabled() with detection result
    - Conditionally initialize touch drivers based on result
    - _Requirements: 2.2, 2.3_

  - [ ]* 8.4 Write property test for touch detection fallback (host-native)
    - **Property 19: Touch Detection Fallback**
    - **Validates: Requirements 7.3**
    - Test that failed touch detection doesn't prevent config loading
    - Simulate touch detection failures
    - Tag: Feature: touch-screen-detection-and-config-file, Property 19

  - [ ]* 8.5 Write integration tests for boot sequence (on-device smoke tests)
    - Test complete boot sequence with touch detected
    - Test complete boot sequence without touch
    - Test boot with config file present
    - Test boot with config file missing
    - Test boot with invalid config file
    - Test provisioning mode entry

- [x] 9. Implement provisioning mode
  - [x] 9.1 Add provisioning mode state to StateManager
    - Add PROVISIONING state to StateManager enum
    - Add methods to enter and exit provisioning mode
    - Update firmware/include/StateManager.h and firmware/src/StateManager.cpp
    - _Requirements: 3.6, 7.1_

  - [x] 9.2 Implement serial console provisioning
    - Add serial command handlers for config field entry
    - Validate input as fields are entered
    - Save to both config file and NVS when complete
    - Reboot after successful provisioning
    - _Requirements: 3.6_

  - [ ]* 9.3 Implement touch-based provisioning (optional/MVP+)
    - Enable config page UI during provisioning (if touch detected)
    - Allow field entry via touch interface
    - Validate required fields before allowing save
    - Save to both config file and NVS when complete
    - Reboot after successful provisioning
    - _Requirements: 3.6_

  - [x] 9.4 Add provisioning mode display
    - Show "Provisioning Mode" message on TFT
    - Display instructions for serial or touch configuration
    - Update display as fields are entered
    - _Requirements: 3.6_

  - [ ]* 9.5 Write unit tests for provisioning mode
    - Test provisioning mode entry conditions
    - Test serial console provisioning flow
    - Test touch-based provisioning flow (if implemented)
    - Test config save and reboot after provisioning

- [x] 10. Implement deep sleep integration
  - [x] 10.1 Add deep sleep trigger to PowerManager
    - Check enable_deep_sleep config flag after data upload
    - Enter deep sleep if flag is true
    - Configure wake-up timer based on data_upload_interval
    - Update firmware/src/PowerManager.cpp
    - _Requirements: 8.6_

  - [ ]* 10.2 Write property test for deep sleep trigger (host-native)
    - **Property 24: Deep Sleep Trigger**
    - **Validates: Requirements 8.6**
    - Test that deep sleep is entered when flag is true
    - Generate random config with enable_deep_sleep variations
    - Tag: Feature: touch-screen-detection-and-config-file, Property 24

  - [ ]* 10.3 Write unit tests for deep sleep integration
    - Test deep sleep entry after successful upload
    - Test deep sleep skipped when flag is false
    - Test wake-up timer configuration

- [ ] 11. Checkpoint - Integration complete
  - Native tests pass (CI green for host environment)
  - Firmware compiles for all target boards
  - Static analysis/lint clean (if configured)

- [x] 12. Add logging and error handling
  - [x] 12.1 Add logging for touch detection
    - Log detection start and completion with timing
    - Log each controller probe attempt and result
    - Log final detection result with controller type
    - Use appropriate log levels (INFO for success, WARN for failures)
    - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5_

  - [x] 12.2 Add logging for config file operations
    - Log config file load attempts and results
    - Log validation errors with field names and values
    - Log checksum validation results
    - Log config save operations
    - Log NVS migration operations
    - Use appropriate log levels (INFO, WARN, ERROR)
    - _Requirements: 3.2, 3.3, 4.2, 4.3, 4.4, 7.2_

  - [x] 12.3 Add logging for provisioning mode
    - Log provisioning mode entry with reason
    - Log missing required fields
    - Log provisioning completion and reboot
    - _Requirements: 3.6, 7.1_

  - [ ]* 12.4 Write unit tests for logging
    - Test that appropriate log messages are generated
    - Test log levels are correct for each scenario
    - Test log messages contain relevant details

- [x] 13. Update documentation
  - [x] 13.1 Update README with config file documentation
    - Document config file format and location
    - Document all config fields with types and defaults
    - Document checksum calculation method
    - Document provisioning mode usage
    - Update firmware/README.md
    - _Requirements: 4.1, 4.2, 8.1, 8.2_

  - [x] 13.2 Create config file example
    - Create example config.json with all fields
    - Include comments explaining each field (in separate doc)
    - Document required vs optional fields
    - Create firmware/docs/CONFIG_FILE_EXAMPLE.md
    - _Requirements: 4.1, 8.1, 8.2_

  - [x] 13.3 Update DEPLOYMENT.md with touch detection info
    - Document touch controller support
    - Document detection process and timing
    - Document behavior with and without touch
    - Update firmware/docs/DEPLOYMENT.md
    - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.1, 2.2, 2.3_

  - [x] 13.4 Update SERIAL_CONSOLE.md with provisioning commands
    - Document provisioning mode commands
    - Document config field entry via serial
    - Document validation and save process
    - Update firmware/docs/SERIAL_CONSOLE.md
    - _Requirements: 3.6_

- [ ] 14. Final checkpoint - Full validation
  - Native tests pass (CI green for host environment)
  - Firmware compiles for all target boards
  - Static analysis/lint clean (if configured)
  - On-device smoke tests pass (touch detected/not detected scenarios)
  - All targeted correctness properties validated (see property coverage matrix below)

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- Property tests run host-native with RapidCheck + mocks (minimum 100 iterations each)
- Unit tests validate specific examples and edge cases
- On-device tests are smoke/integration tests only
- Implementation follows existing firmware architecture and coding standards
- ConfigFileManager implemented first (deterministic, easier to test)
- TouchDetector implemented second (hardware-dependent)
- Touch detection happens early in boot but independently of config loading
- Config file is source of truth after first boot (NVS migrated to file automatically)
- Required fields (wifi_ssid, wifi_password, backend_url) must be satisfied before normal operation
- Provisioning mode provides serial console configuration (touch-based is optional/MVP+)
- ConfigLoadResult enum includes READ_ERROR and WRITE_ERROR for filesystem failures
- DisplayManager showConfigError() maps all ConfigLoadResult values plus separate handling for missing required fields
- CRC32 implementation must be portable (works in both firmware and native test environments)
- LittleFS initialization happens during ConfigManager initialization or inside loadFromFile(), not in constructor

## Correctness Property Coverage Matrix

| Property | Description | Test Task | Status |
|----------|-------------|-----------|--------|
| Property 1 | Detection Priority Order | 5.6 | Planned |
| Property 2 | Per-Probe Timeout Enforcement | 5.7 | Planned |
| Property 3 | Detection Criteria | 5.8 | Planned |
| Property 4 | Total Detection Timeout | 5.9 | Planned |
| Property 5 | Detection Stability | 5.10 | Planned |
| Property 6 | Config Page State Consistency | 6.3 | Planned |
| Property 7 | Resource Conservation | 7.4 | Planned |
| Property 8 | Config File Loading | 2.10 | Planned |
| Property 9 | Invalid Config Handling with Defaults | 2.11 | Planned |
| Property 10 | Required Field Validation and Provisioning | 2.12 | Planned |
| Property 11 | Checksum Validation | 2.13 | Planned |
| Property 12 | Field Type and Range Validation | 2.14 | Planned |
| Property 13 | Validation Error Messages | 2.15 | Planned |
| Property 14 | Configuration Priority | 3.5 | Planned |
| Property 15 | Atomic Config Save | 2.16 | Planned |
| Property 16 | Configuration Coverage Parity | 3.6 | Planned |
| Property 17 | Forward Compatibility | 2.17 | Planned |
| Property 18 | Optional Field Defaults | 2.18 | Planned |
| Property 19 | Touch Detection Fallback | 8.4 | Planned |
| Property 20 | Config Error Display | 7.5 | Planned (includes ConfigLoadResult + missing required) |
| Property 21 | String Length Validation | 2.19 | Planned |
| Property 22 | Integer Range Validation | 2.20 | Planned |
| Property 23 | URL Format Validation | 2.21 | Planned |
| Property 24 | Deep Sleep Trigger | 10.2 | Planned |
| Property 25 | Config Persistence Round-Trip | 2.22 | Planned |

All 25 correctness properties from the design document have corresponding test tasks. Properties marked as optional (*) in their test tasks can be deferred for MVP.
