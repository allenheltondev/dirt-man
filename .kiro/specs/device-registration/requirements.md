# Requirements Document: Device Registration

## Introduction

This specification defines the device registration functionality for ESP32 sensor firmware. The system shall automatically register devices with a backend API using the ESP32's immutable hardware MAC address as the unique device identifier, along with an optional user-configurable friendly name. This ensures proper device lifecycle tracking and reduces accidental device duplication. The Hardware_ID provides a stable identifier but does not constitute cryptographic authentication or prevent intentional device spoofing.

## Glossary

- **Device**: The ESP32-based sensor hardware unit
- **Hardware_ID**: The immutable MAC address of the ESP32, formatted as a colon-separated hexadecimal string (e.g., "AA:BB:CC:DD:EE:FF")
- **Friendly_Name**: An optional user-configurable human-readable device identifier, sent as a separate field in API payloads
- **Backend_API**: The remote HTTP/HTTPS server that receives device registration and sensor data
- **Registration_Status**: A persistent string containing the confirmation ID returned by the backend upon successful registration, or empty if not registered
- **Confirmation_ID**: A unique identifier returned by the Backend_API upon successful registration, must be a valid UUID v4 format
- **Boot_ID**: A randomly generated UUID v4 string created on each device boot. The Boot_ID SHALL be stored only in RAM and MUST NOT be persisted to NVS. It is used to correlate sensor data with a specific runtime session and detect device restarts.
- **NVS**: Non-Volatile Storage, the ESP32's persistent storage mechanism
- **Network_Manager**: The firmware component responsible for HTTP/HTTPS communication
- **Config_Manager**: The firmware component responsible for configuration storage and retrieval
- **Serial_Console**: The command-line interface accessible via serial connection for device configuration
- **Registration_Endpoint**: The API endpoint for device registration, derived from the data endpoint by replacing the last path segment
- **API_Key**: The authentication key used for API requests, sent in the X-API-Key header

## Requirements

### Requirement 1: Hardware Identification

**User Story:** As a device owner, I want my device to use its immutable hardware MAC address as the primary identifier, so that devices cannot be spoofed or duplicated.

#### Acceptance Criteria

1. THE Device SHALL extract its MAC address using ESP32 hardware functions
2. THE Device SHALL format the MAC address as a colon-separated uppercase hexadecimal string (AA:BB:CC:DD:EE:FF)
3. THE Device SHALL use the Hardware_ID as the immutable device identifier in all API communications
4. THE Device SHALL provide a method to retrieve the Hardware_ID for display and logging purposes

### Requirement 2: Automatic Registration

**User Story:** As a device owner, I want my device to automatically register with the backend API on first boot, so that the backend knows about my device without manual intervention.

#### Acceptance Criteria

1. WHEN the Device connects to WiFi for the first time after boot, THE Device SHALL check the Registration_Status in NVS
2. WHEN the Registration_Status is empty, THE Device SHALL attempt registration with the Backend_API
3. WHEN the Registration_Status contains a value that is not a valid UUID v4 format, THE Device SHALL treat it as empty and attempt registration
4. WHEN registration succeeds, THE Device SHALL persist the Confirmation_ID returned by the backend in NVS as the Registration_Status
5. WHEN the Device reboots and Registration_Status contains a valid UUID v4 Confirmation_ID, THE Device SHALL NOT attempt re-registration
6. THE Device SHALL attempt registration once before the first sensor data transmission
7. WHEN the registration attempt fails, THE Device SHALL proceed with sensor data transmission and retry registration asynchronously in the background according to Requirement 10 retry rules

### Requirement 3: Registration Payload

**User Story:** As a backend developer, I want to receive device registration information including hardware ID, friendly name, firmware version, boot ID, and initial capabilities, so that I can track device lifecycle and online status.

#### Acceptance Criteria

1. WHEN sending a registration request, THE Device SHALL include the Hardware_ID in the payload
2. WHEN a Friendly_Name is configured, THE Device SHALL include it as a separate "friendly_name" field in the registration payload
3. WHEN a Friendly_Name is not configured, THE Device SHALL omit the "friendly_name" field from the registration payload
4. THE Device SHALL include the firmware version string in the registration payload
5. THE Device SHALL generate a unique Boot_ID on each boot and include it in the registration payload
6. THE Device SHALL generate the Boot_ID using a UUID v4 compliant random generator or a 128-bit cryptographically secure random value formatted as 8-4-4-4-12 hexadecimal characters
7. THE Device SHALL regenerate the Boot_ID on every power cycle or restart
8. THE Device SHALL NOT persist the Boot_ID to NVS or any non-volatile storage
9. THE Device SHALL include a capabilities object in the registration payload with the following structure:
   - "sensors" array containing sensor type strings (e.g., ["bme280", "ds18b20"])
   - "features" object containing boolean flags for each feature (e.g., {"tft_display": true, "offline_buffering": false})
10. THE Device SHALL format the registration payload as valid JSON

### Requirement 4: Registration Endpoint Configuration

**User Story:** As a device owner, I want the registration endpoint to be automatically derived from my configured API endpoint, so that I don't need to configure multiple URLs.

#### Acceptance Criteria

1. THE Device SHALL derive the Registration_Endpoint by replacing the last path segment of the configured API endpoint with "register"
2. WHEN the configured API endpoint is "https://api.example.com/v1/sensor-data", THE Device SHALL use "https://api.example.com/v1/register" as the Registration_Endpoint
3. WHEN the configured API endpoint is "https://api.example.com/sensor-data", THE Device SHALL use "https://api.example.com/register" as the Registration_Endpoint
4. THE Device SHALL preserve the protocol (HTTP/HTTPS), domain, port, and all path segments except the last when deriving the Registration_Endpoint
5. THE Device SHALL support both HTTP and HTTPS protocols for the Registration_Endpoint

### Requirement 5: Registration Authentication

**User Story:** As a backend developer, I want device registration to use the same authentication mechanism as sensor data, so that I can maintain consistent security policies.

#### Acceptance Criteria

1. THE Device SHALL use the configured API_Key for registration requests
2. THE Device SHALL include the API_Key in the X-API-Key header
3. THE Device SHALL use the same TLS validation settings for registration as for sensor data transmission

### Requirement 6: Registration Persistence

**User Story:** As a device owner, I want the device to remember its registration status across reboots, so that it doesn't unnecessarily re-register on every boot.

#### Acceptance Criteria

1. WHEN registration succeeds, THE Device SHALL write the Confirmation_ID to NVS as the Registration_Status
2. THE Device SHALL read the Registration_Status from NVS on boot
3. THE Device SHALL validate that the Registration_Status is a valid UUID v4 format (8-4-4-4-12 hexadecimal characters)
4. THE Registration_Status SHALL persist across power cycles and reboots
5. WHEN NVS is erased or corrupted, THE Device SHALL treat Registration_Status as empty and attempt registration

### Requirement 7: Manual Registration Trigger

**User Story:** As a device owner, I want to manually trigger device registration via serial console, so that I can re-register if needed or test the registration process.

#### Acceptance Criteria

1. THE Serial_Console SHALL provide a "register" command
2. WHEN the "register" command is executed, THE Device SHALL attempt registration regardless of current Registration_Status
3. WHEN the "register" command succeeds, THE Device SHALL update the Registration_Status in NVS
4. THE Serial_Console SHALL display the registration result to the user

### Requirement 8: Hardware ID Display

**User Story:** As a device owner, I want to view my device's hardware ID via serial console, so that I can identify my device in the backend system.

#### Acceptance Criteria

1. THE Serial_Console SHALL provide a "hwid" command
2. WHEN the "hwid" command is executed, THE Device SHALL display the Hardware_ID
3. THE Device SHALL format the Hardware_ID output in a human-readable format

### Requirement 9: Friendly Name Configuration

**User Story:** As a device owner, I want to optionally configure a friendly name for my device, so that I can easily identify it in the backend dashboard.

#### Acceptance Criteria

1. THE Config_Manager SHALL store the Friendly_Name in NVS
2. THE Serial_Console SHALL allow configuration of the Friendly_Name via the existing "deviceid" command
3. WHEN the Friendly_Name is changed, THE Device SHALL include the new name in subsequent API communications
4. THE Device SHALL support Friendly_Name strings up to 64 characters in length

### Requirement 10: Graceful Degradation

**User Story:** As a device owner, I want the device to continue normal sensor operation even if registration fails, so that temporary network issues don't prevent data collection.

#### Acceptance Criteria

1. WHEN registration fails due to network errors, THE Device SHALL log a warning message
2. WHEN registration fails, THE Device SHALL NOT block sensor readings or data transmission
3. WHEN registration fails, THE Device SHALL continue normal operation with unregistered status
4. WHEN registration fails with HTTP 5xx errors or network timeouts, THE Device SHALL retry with exponential backoff
5. THE Device SHALL limit registration retry attempts to a maximum of 5 attempts
6. THE Device SHALL use exponential backoff with base delay of 1 second, doubling on each retry (1s, 2s, 4s, 8s, 16s)
7. THE Device SHALL add random jitter of 0-500ms to each retry delay to prevent thundering herd
8. WHEN registration fails with HTTP 4xx errors (except 408 Request Timeout and 429 Too Many Requests), THE Device SHALL NOT retry
9. WHEN registration fails with HTTP 408 or 429 errors, THE Device SHALL retry with exponential backoff
10. Registration retries SHALL occur in the background and SHALL NOT delay sensor data transmission or block the main loop

### Requirement 11: Enhanced Sensor Data Format

**User Story:** As a backend developer, I want sensor data transmissions to include hardware ID, friendly name, and boot ID, so that I can correlate data with registered devices and track device online status.

#### Acceptance Criteria

1. WHEN sending sensor data, THE Device SHALL always include the Hardware_ID in the payload
2. WHEN a Friendly_Name is configured, THE Device SHALL include it as a separate "friendly_name" field in the sensor data payload
3. WHEN a Friendly_Name is not configured, THE Device SHALL omit the "friendly_name" field from the sensor data payload
4. THE Device SHALL include the current Boot_ID in every sensor data payload until the next reboot generates a new Boot_ID
5. THE Device SHALL maintain backward compatibility with existing sensor data format structure by adding new fields without removing existing ones

### Requirement 12: Registration Response Handling

**User Story:** As a device owner, I want the device to properly handle different registration responses from the backend, so that registration status is accurately tracked.

#### Acceptance Criteria

1. WHEN the Backend_API returns HTTP 200 with a "confirmation_id" field containing a valid UUID v4, THE Device SHALL extract and store the Confirmation_ID in NVS
2. WHEN the Backend_API returns HTTP 200 with status "already_registered" and a valid "confirmation_id", THE Device SHALL store the Confirmation_ID in NVS
3. WHEN the Backend_API returns HTTP 4xx client errors (except 408 and 429), THE Device SHALL log the error and NOT retry registration
4. WHEN the Backend_API returns HTTP 408 or 429 errors, THE Device SHALL retry with exponential backoff up to 5 attempts
5. WHEN the Backend_API returns HTTP 5xx server errors, THE Device SHALL log the error and retry with exponential backoff up to 5 attempts
6. WHEN the Backend_API returns malformed JSON, THE Device SHALL log the error and treat it as a registration failure without retry
7. WHEN the Backend_API returns JSON without a "confirmation_id" field, THE Device SHALL log the error and treat it as a registration failure without retry
8. WHEN the Backend_API returns a "confirmation_id" that is not a valid UUID v4 format, THE Device SHALL log the error and treat it as a registration failure without retry

### Requirement 13: Non-Blocking Operation

**User Story:** As a firmware developer, I want registration to not block the main loop or sensor readings, so that device responsiveness is maintained.

#### Acceptance Criteria

1. THE Device SHALL perform registration in a separate FreeRTOS task OR use network timeouts and yield calls to maintain main loop responsiveness
2. THE Device SHALL configure HTTP client timeouts to a maximum of 10 seconds for registration requests
3. WHEN registration is in progress, THE Device SHALL continue processing sensor readings
4. THE Device SHALL not delay sensor data transmission waiting for registration completion beyond the configured timeout period

### Requirement 14: Cross-Platform Compatibility

**User Story:** As a firmware developer, I want MAC address extraction to work on all ESP32 variants, so that the firmware is portable across hardware versions.

#### Acceptance Criteria

1. THE Device SHALL extract the Hardware_ID using the ESP32 WiFi Station MAC address via esp_read_mac() with ESP_MAC_WIFI_STA or equivalent SDK function
2. THE Device SHALL use the WiFi Station MAC as the canonical Hardware_ID across all ESP32 variants (ESP32, ESP32-S2, ESP32-S3, ESP32-C3)
3. THE Device SHALL NOT use the Access Point MAC or Bluetooth MAC as the Hardware_ID
4. THE Device SHALL validate that the extracted MAC address is non-zero and not equal to 00:00:00:00:00:00 before use
