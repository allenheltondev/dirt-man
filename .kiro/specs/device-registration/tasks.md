# Implementation Plan: Device Registration

## Overview

This implementation plan breaks down the device registration feature into discrete coding tasks. The plan follows an incremental approach: create utility modules first, extend existing components, integrate registration flow, add serial console commands, and finally implement comprehensive testing.

## Tasks

- [x] 1. Create Hardware ID utility module
  - Create `firmware/src/HardwareId.h` and `firmware/src/HardwareId.cpp`
  - Implement `getHardwareId()` to extract WiFi Station MAC using `esp_read_mac(ESP_MAC_WIFI_STA)`
  - Implement `formatMac()` to format as uppercase colon-separated hex (AA:BB:CC:DD:EE:FF)
  - Implement `isValidMac()` to reject 00:00:00:00:00:00
  - _Requirements: 1.2, 1.3, 14.1, 14.4_

- [x] 1.1 Write property test for MAC address formatting
  - **Property 1: MAC Address Formatting Consistency**
  - **Validates: Requirements 1.2**
  - Generate random 6-byte MAC addresses (100 iterations)
  - Verify formatted string is exactly 17 characters
  - Verify exactly 5 colons at correct positions
  - Verify only uppercase hex characters and colons
  - _Requirements: 1.2_

- [x] 2. Create Boot ID utility module
  - Create `firmware/src/BootId.h` and `firmware/src/BootId.cpp`
  - Implement `generate()` to create UUID v4 using `esp_random()`
  - Set version bits (4) and variant bits (8/9/A/B) per RFC 4122
  - Implement `isValidUuid()` to validate UUID v4 format with strict version/variant checks
  - _Requirements: 3.6, 3.7_

- [x] 2.1 Write property test for Boot ID format compliance
  - **Property 7: Boot ID Format Compliance**
  - **Validates: Requirements 3.6**
  - Generate multiple Boot IDs (100 iterations)
  - Verify 8-4-4-4-12 format with hyphens
  - Verify version bits = 4 in third group
  - Verify variant bits = 8/9/A/B in fourth group
  - _Requirements: 3.6_

- [x] 3. Extend ConfigManager for registration persistence
  - Add `getConfirmationId()` method to read from NVS key "dev.confirm_id"
  - Add `setConfirmationId()` method to write to NVS
  - Add `hasValidConfirmationId()` method with UUID v4 validation (version + variant checks)
  - Update NVS key constant to use namespaced key "dev.confirm_id"
  - _Requirements: 6.1, 6.3, 6.4_

- [x] 3.1 Write property test for NVS persistence round-trip
  - **Property 4: NVS Persistence Round-Trip**
  - **Validates: Requirements 2.4, 6.1**
  - Generate confirmation IDs by calling BootId UUID v4 generator (100 iterations)
  - Write confirmation_id to NVS, read back, verify exact match
  - Only test confirmation_id (friendly_name tested separately in Task 11)
  - _Requirements: 2.4, 6.1_

- [x] 3.2 Write unit tests for UUID v4 validation
  - Test valid UUID v4 acceptance
  - Test invalid format rejection (wrong length, missing hyphens, wrong version/variant)
  - Test empty string handling
  - _Requirements: 2.3, 6.3_

- [x] 4. Extend NetworkManager for registration endpoint derivation
  - Add `deriveEndpoint()` method to replace last path segment with "register"
  - Strip trailing slashes before processing
  - Remove query string and fragment at earliest occurrence of ? or #
  - Add `getRegistrationEndpoint()` public method
  - _Requirements: 4.1, 4.4_

- [x] 4.1 Write property test for endpoint derivation
  - **Property 8: Endpoint Derivation Preserves URL Components**
  - **Validates: Requirements 4.1, 4.4**
  - Generate random valid HTTP/HTTPS URLs (100 iterations)
  - Verify protocol, domain, port preserved
  - Verify all path segments except last preserved
  - Verify last segment replaced with "register"
  - _Requirements: 4.1, 4.4_

- [x] 4.2 Write unit tests for endpoint derivation edge cases
  - Test URL with trailing slash
  - Test URL with query string
  - Test URL with fragment
  - Test URL containing both ? and # in either order: .../data#frag?x=y and .../data?x=y#frag
  - Test URL with no path
  - Test URL with single path segment
  - _Requirements: 4.2, 4.3_

- [x] 5. Extend NetworkManager for registration HTTP requests
  - Add `registerDevice(const String& payload) -> RegistrationResult` method to POST registration payload
  - Return struct: `RegistrationResult { int statusCode; String confirmationId; bool shouldRetry; }`
  - `shouldRetry` is computed from (network_error || status in {408,429} || 500-599) before parsing JSON
  - Configure 10-second timeout for registration requests
  - Add X-API-Key header using configured API key
  - Add `parseRegistrationResponse()` to extract confirmation_id from JSON
  - Validate confirmation_id as UUID v4 before returning (empty string if invalid or missing)
  - Handle retryable errors (408, 429, 5xx) even with malformed response bodies
  - _Requirements: 5.1, 5.2, 12.1, 12.2, 13.2_

- [x] 5.1 Write unit tests for response parsing
  - Test valid JSON with confirmation_id
  - Test "already_registered" status
  - Test malformed JSON handling
  - Test missing confirmation_id field
  - Test invalid confirmation_id format
  - Test retryable status codes with malformed bodies
  - _Requirements: 12.1, 12.2, 12.6, 12.7, 12.8_

- [x] 6. Create RegistrationManager component
  - Create `firmware/src/RegistrationManager.h` and `firmware/src/RegistrationManager.cpp`
  - Implement constructor taking NetworkManager and ConfigManager pointers
  - Implement `isRegistered()` to check for valid confirmation_id in NVS
  - Implement `getConfirmationId()` to retrieve stored value
  - Implement `buildRegistrationPayload()` and cache the exact payload string for retries
  - RegistrationManager calls `NetworkManager.registerDevice(payload)` and passes the exact cached payload string for retries
  - NetworkManager does not rebuild payload - RegistrationManager owns payload construction
  - _Requirements: 2.1, 2.2, 2.4_

- [x] 7. Implement registration retry logic with exponential backoff
  - Implement `calculateBackoff()` with zero-based attemptIndex (0→1s, 1→2s, 2→4s, 3→8s, 4→16s)
  - Add random jitter 0-500ms to each delay
  - Clamp maximum delay to 30 seconds
  - Implement `queueRetry()` to schedule next attempt
  - Implement `processRetries()` to check timing and execute retries
  - Limit to maximum 5 retry attempts
  - _Requirements: 10.4, 10.5, 10.6, 10.7, 10.8, 10.9_

- [x] 7.1 Write property test for exponential backoff calculation
  - **Property 11: Exponential Backoff Calculation**
  - **Validates: Requirements 10.6, 10.7**
  - Test attemptIndex 0-4 for correctness (100 iterations each)
  - Verify base delay = 1000 * 2^attemptIndex
  - Verify jitter in range [0, 500]
  - Add one additional test: attemptIndex 10 clamps to <= 30000 + jitter
  - _Requirements: 10.6, 10.7_

- [x] 7.2 Write property test for HTTP status code retry decisions
  - **Property 12: HTTP Status Code Retry Decision**
  - **Validates: Requirements 10.4, 10.8, 12.3, 12.5**
  - Test fixed set of status codes (no need for random): 200, 201, 400, 401, 403, 404, 408, 409, 422, 429, 500, 502, 503, 504
  - Assert: retry if and only if status ∈ {408, 429} ∪ [500..599]
  - Note: If status is retryable, retry even if response body is malformed / non-JSON
  - _Requirements: 10.4, 10.8, 12.3, 12.5_

- [x] 8. Build registration payload (single source of truth)
  - Implement `buildRegistrationPayload()` method in RegistrationManager
  - Always include: hardware_id, boot_id, firmware_version, capabilities
  - Conditionally include: friendly_name (omit if empty)
  - Cache the exact payload string for retries
  - Include capabilities object with sensors array and features object
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.9, 3.10_

- [x] 8.1 Write property test for registration payload structure
  - **Property 5: Registration Payload Structure Completeness**
  - **Validates: Requirements 3.1, 3.4, 3.5, 3.9, 3.10**
  - Generate random registration data (100 iterations)
  - Verify all required fields present
  - Verify correct JSON types
  - Verify capabilities structure (sensors array, features object with booleans)
  - _Requirements: 3.1, 3.4, 3.5, 3.9, 3.10_

- [x] 8.2 Write property test for conditional friendly name inclusion
  - **Property 6: Conditional Friendly Name Inclusion in Registration**
  - **Validates: Requirements 3.2**
  - Generate payloads with and without friendly_name (100 iterations)
  - Verify friendly_name included when configured
  - Verify friendly_name omitted when not configured
  - _Requirements: 3.2, 3.3_

- [x] 9. Integrate registration flow in main application
  - Generate Boot_ID once in `setup()` and store in global variable
  - Extract Hardware_ID in `setup()`
  - Check registration status on boot
  - On boot, if not registered: set `_retryPending=true`, `_retryCount=0`, `_nextRetryTime=millis()` so first attempt happens immediately
  - Sensor loop starts regardless of registration outcome
  - The retry task attempts registration in background until success or max retries
  - Create FreeRTOS task for background registration retries (non-blocking approach)
  - Invoke `processRetries()` from task to handle background retries
  - _Requirements: 2.1, 2.2, 2.5, 2.6, 2.7, 3.7, 3.8, 10.10, 13.1_

- [x] 9.1 Write unit test for Boot ID non-persistence
  - Verify Boot_ID is not written to NVS
  - Verify Boot_ID is stored only in RAM
  - _Requirements: 3.8_

- [x] 9.2 Write integration test for registration failure graceful degradation
  - Simulate registration failure
  - Verify sensor readings continue
  - Verify data transmission continues
  - Verify payload still includes hardware_id and boot_id when unregistered
  - _Requirements: 2.7, 10.2, 10.3_

- [x] 10. Enhance sensor data payload format
  - Modify sensor data payload building to include hardware_id (always)
  - Include boot_id (always)
  - Include friendly_name (optional, omit if not configured)
  - Explicitly: Sensor payload does NOT include confirmation_id
  - Field names are: hardware_id, boot_id, friendly_name (no device_id alias)
  - Maintain existing readings array structure for backward compatibility
  - _Requirements: 11.1, 11.2, 11.3, 11.4, 11.5_

- [x] 10.1 Write property test for sensor data payload structure
  - **Property 13: Sensor Data Payload Structure Completeness**
  - **Validates: Requirements 11.1, 11.2, 11.4**
  - Generate random sensor data (100 iterations)
  - Verify hardware_id always present
  - Verify boot_id always present
  - Verify friendly_name present if and only if configured
  - Verify confirmation_id is NOT present in sensor payload
  - _Requirements: 11.1, 11.2, 11.3, 11.4_

- [x] 10.2 Write property test for Boot ID session consistency
  - **Property 14: Boot ID Session Consistency**
  - **Validates: Requirements 11.4**
  - Generate multiple sensor payloads in same session (100 iterations)
  - Verify all payloads contain same boot_id
  - _Requirements: 11.4_

- [x] 10.3 Write unit test for backward compatibility
  - Verify readings array structure unchanged
  - Verify new fields added at root level
  - _Requirements: 11.5_

- [x] 11. Add serial console commands
  - Add "register" command to trigger manual registration
  - Add "hwid" command to display Hardware_ID
  - Add "bootid" command to display current Boot_ID
  - Provide callback from main.cpp to ConfigManager for cross-component commands
  - _Requirements: 7.1, 7.2, 7.3, 8.1, 8.2_

- [x] 11.1 Write integration tests for serial console commands
  - Test "register" command triggers registration
  - Test "register" command updates NVS on success
  - Test "hwid" command displays correct MAC address
  - Test "bootid" command displays current Boot_ID
  - _Requirements: 7.2, 7.3_

- [x] 11.2 Write unit test for friendly_name NVS round-trip
  - Test friendly_name write to NVS
  - Test friendly_name read from NVS
  - Verify exact match
  - _Requirements: 3.2, 6.1_

- [x] 12. Implement logging with heap safety
  - Use `Serial.printf()` for production logging to avoid heap fragmentation
  - Log registration attempts, successes, and failures
  - Log retry attempts with backoff delays
  - Log validation errors (invalid MAC, invalid UUID, etc.)
  - Keep LOG_* macros for development only
  - _Requirements: 10.1, 12.3, 12.4, 12.5, 12.6, 12.7, 12.8_

- [ ] 13. Final integration and testing
  - Test complete registration flow from boot to sensor data transmission
  - Test registration with valid backend
  - Test registration with various error responses (4xx, 5xx, timeouts)
  - Test background retry behavior
  - Test graceful degradation when registration fails
  - Verify no blocking of main loop or sensor operations
  - Test across ESP32 variants (ESP32, ESP32-S2, ESP32-S3, ESP32-C3) if available
  - _Requirements: All_

- [ ] 14. Checkpoint - Ensure all tests pass
  - Ensure all tests pass and all tasks completed

## Notes

- Each task references specific requirements for traceability
- Property tests validate universal correctness properties with 100 iterations
- Unit tests validate specific examples and edge cases
- Integration tests verify end-to-end flows
- The recommended approach is to run registration in a FreeRTOS task to avoid blocking
- All NVS keys use "dev." namespace to prevent collisions
- UUID v4 validation enforces strict version and variant bit checks
- Logging uses `Serial.printf()` in production to avoid heap fragmentation
