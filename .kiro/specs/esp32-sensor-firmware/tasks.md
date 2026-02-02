# Implementation Tasks

## Task 1: Project Setup and Pin Configuration

**Description:** Set up the PlatformIO project structure with required libraries, pin definitions, and mock mode support.

**Requirements:** All requirements (foundation for implementation)

**Details:**
- Create platformio.ini with ESP32 board configuration
- Add library dependencies: Adafruit_BME280, DallasTemperature, OneWire, TFT_eSPI, ArduinoJson, HTTPClient, WiFiClientSecure
- Configure build flags and partition scheme for NVS
- Create src/ directory structure with managers/ and models/ subdirectories
- Set up serial monitor configuration (115200 baud)
- Create PinConfig.h with all hardware pin definitions (I2C, OneWire, SPI, ADC)
- Add UNIT_TEST compile flag for mock sensor mode

**Subtasks:**
- [ ] 1.1 Create platformio.ini with ESP32 board and libraries
- [x] 1.2 Create directory structure (src/, include/, test/)
- [ ] 1.3 Create PinConfig.h with I2C, OneWire, SPI, and ADC pin definitions
- [x] 1.4 Add UNIT_TEST build flag for mock sensor mode
- [x] 1.5 Set up native test environment (PlatformIO native) for host-based unit tests
- [x] 1.6 Create empty skeleton files for manager classes
- [x] 1.7 Create header files for data structures

---

## Task 2: Configuration Manager with Validation

**Description:** Implement the ConfigManager class with NVS storage, comprehensive validation, and serial console interface.

**Requirements:** Requirement 6 (Configuration Management)

**Details:**
- Implement Config structure with all required fields
- Implement NVS read/write operations using Preferences library
- Implement default configuration values
- Implement comprehensive validation: WiFi credentials, API endpoint URL, intervals, calibration bounds
- Implement serial console interface for configuration updates
- Sanitize sensitive data from serial output (passwords, tokens)
- Handle configuration errors gracefully

**Subtasks:**
- [x] 2.1 Implement Config structure in models/Config.h
- [x] 2.2 Implement ConfigManager class with NVS operations
- [x] 2.3 Implement loadConfig() and saveConfig() methods
- [x] 2.4 Implement setDefaults() with sensible defaults
- [x] 2.5 Implement validateConfig() with WiFi credential validation
- [x] 2.6 Implement API endpoint URL validation
- [x] 2.7 Implement interval range validation (positive, reasonable bounds)
- [x] 2.8 Implement calibration value validation (ADC range 0-4095)
- [x] 2.9 Implement handleSerialConfig() for serial console interface
- [x] 2.10 Implement sensitive data sanitization for serial output
- [ ] 2.11 Write unit tests for config validation edge cases
- [ ] 2.12 Write property test for config round-trip (Property 3)
- [ ] 2.13 Write property test for configuration validation rejection (Property 14)

---

## Task 3: Display Manager - Initialization and Startup Screen

**Description:** Initialize TFT display early to catch SPI/pin issues and show basic startup screen.

**Requirements:** Requirement 7.1 (Display Initialization)

**Details:**
- Initialize TFT_eSPI display with correct SPI pins and rotation
- Render startup screen showing firmware version and "Initializing..."
- Test display rendering early to catch hardware issues
- Implement basic text rendering utilities

**Subtasks:**
- [x] 3.1 Implement DisplayManager class skeleton
- [x] 3.2 Implement initialize() with TFT setup and pin configuration
- [x] 3.3 Implement showStartupScreen() with firmware version
- [x] 3.4 Test display on hardware to verify SPI pins and rotation
- [x] 3.5 Implement basic text rendering helper methods
- [x] 3.6 Configure TFT_eSPI User_Setup or User_Setup_Select with pin definitions

---

## Task 4: Sensor Manager - Basic Reading with Noise Reduction

**Description:** Implement SensorManager to initialize and read from BME280, DS18B20, and soil moisture sensors with ADC averaging.

**Requirements:** Requirement 1 (Sensor Data Collection)

**Details:**
- Implement SensorReadings structure
- Initialize BME280 via I2C
- Initialize DS18B20 via OneWire
- Configure ADC for soil moisture with appropriate attenuation
- Implement multi-sample ADC averaging (10-20 samples) to reduce noise
- Implement sensor read methods with error handling
- Implement sensor status bitmask tracking
- Implement retry logic for sensor initialization (3 attempts)
- Support mock mode for unit testing

**Subtasks:**
- [x] 4.1 Implement SensorReadings structure in models/SensorReadings.h
- [x] 4.2 Implement SensorManager class skeleton
- [x] 4.3 Implement BME280 initialization and read methods
- [x] 4.4 Implement DS18B20 initialization and read methods
- [-] 4.5 Implement soil moisture ADC read with multi-sample averaging (10-20 samples)
- [-] 4.6 Implement sensor status bitmask logic
- [x] 4.7 Implement sensor initialization retry logic (3 attempts)
- [x] 4.8 Implement mock mode for unit testing (UNIT_TEST flag)
- [x] 4.9 Write unit tests for sensor initialization
- [x] 4.10 Write property test for sensor reading completeness (Property 1)
- [x] 4.11 Write property test for ADC averaging noise reduction (Property 23)

---

## Task 5: Vertical Slice - Read and Display

**Description:** Create early vertical slice: read sensors, display on TFT, print to serial. Proves hardware wiring before building complex features.

**Requirements:** Requirements 1, 7 (Early Integration)

**Details:**
- Implement minimal main loop that reads BME280 and soil moisture
- Display current readings on TFT summary screen
- Print readings to serial console
- Verify all hardware connections work
- Catch 80% of pin and wiring issues early

**Subtasks:**
- [x] 5.1 Implement minimal setup() with ConfigManager, DisplayManager, SensorManager init
- [x] 5.2 Implement minimal loop() that reads sensors every 5 seconds
- [x] 5.3 Implement basic summary display showing sensor values
- [x] 5.4 Print sensor readings to serial console
- [x] 5.5 Test on hardware to verify all sensors and display work
- [ ] 5.6 Debug and fix any pin or wiring issues

---

## Task 6: Sensor Manager - Calibration and Validation

**Description:** Implement soil moisture calibration and physical range validation for all sensors.

**Requirements:** Requirement 2 (Soil Moisture Calibration), Requirement 1.7 (Physical Range Validation)

**Details:**
- Implement two-point calibration for soil moisture
- Load calibration values from ConfigManager
- Implement calibration formula: percentage = (rawAdc - soilDryAdc) / (soilWetAdc - soilDryAdc) * 100
- Implement physical range validation for each sensor type (BME280: -40 to 85°C, etc.)
- Clamp soil moisture percentage to 0-100 range

**Subtasks:**
- [x] 6.1 Implement calibrateSoilMoisture() method
- [x] 6.2 Implement convertSoilMoistureToPercent() with calibration formula
- [x] 6.3 Implement validateReading() for each sensor type
- [x] 6.4 Define physical range constants for each sensor
- [x] 6.5 Write unit tests for calibration edge cases
- [ ] 6.6 Write property test for calibration formula (Property 2)
- [ ] 6.7 Write property test for physical range validation (Property 24)

---

## Task 7: TimeManager - Time Synchronization

**Description:** Implement TimeManager to handle NTP sync and provide consistent time interface for all components.

**Requirements:** Requirement 5.3-5.4 (Timestamp Management)

**Details:**
- Create TimeManager component to centralize time handling
- Implement NTP client to sync with internet time servers
- Provide methods: monotonicMs(), timeSynced(), epochMsOrZero(), uptimeMs()
- Attempt NTP sync when WiFi connects
- Use Unix epoch milliseconds for all timestamps
- Fall back to uptime-based timestamps when not synced

**Subtasks:**
- [x] 7.1 Implement TimeManager class
- [x] 7.2 Implement monotonicMs() using millis()
- [x] 7.3 Implement uptimeMs() tracking
- [x] 7.4 Implement NTP client initialization
- [x] 7.5 Implement tryNtpSync() method
- [x] 7.6 Implement onWiFiConnected() hook for NTP sync
- [x] 7.7 Implement timeSynced() flag management
- [x] 7.8 Implement epochMsOrZero() with sync awareness
- [x] 7.9 Write unit tests for timestamp handling
- [x] 7.10 Write property test for timestamp format consistency (Property 25)

---

## Task 8: SystemStatus Manager

**Description:** Create SystemStatus manager to aggregate system state and metrics for display, networking, and diagnostics.

**Requirements:** Requirement 8 (Error Handling and Diagnostics)

**Details:**
- Implement SystemStatus structure with uptime, heap, RSSI, queue depth, error counters
- Track last successful sensor read time
- Track last successful transmission time
- Track last error string (non-sensitive)
- Provide centralized access to system state for all components
- Implement ErrorCounters structure

**Subtasks:**
- [x] 8.1 Implement SystemStatus structure in models/SystemStatus.h
- [x] 8.2 Implement ErrorCounters structure
- [x] 8.3 Implement SystemStatusManager class
- [x] 8.4 Implement uptime tracking
- [x] 8.5 Implement heap memory monitoring
- [x] 8.6 Implement error counter tracking (sensor, network, buffer overflow)
- [x] 8.7 Implement last successful read/transmit timestamps
- [x] 8.8 Implement last error string tracking
- [x] 8.9 Write property test for error counter increments (Property 21)

---

## Task 9: Data Manager - Averaging Buffer with Fixed Size

**Description:** Implement fixed-size averaging buffer to accumulate sensor readings and calculate averages without heap fragmentation.

**Requirements:** Requirement 3.1-3.4 (Data Averaging)

**Details:**
- Implement Averaging_Buffer as compile-time fixed-size array (not std::vector) to avoid heap churn
- Define MAX_PUBLISH_SAMPLES = 120 (compile-time constant)
- Config validation enforces publishIntervalSamples <= MAX_PUBLISH_SAMPLES
- Use runtime "effective length" from config within fixed array
- Implement addReading() to add readings to buffer
- Implement shouldPublish() to check if Publish_Interval samples reached
- Implement calculateAverages() to compute mean values
- Implement clearAveragingBuffer() to reset after publishing
- Implement AveragedData structure with all required fields
- Implement batch ID generation with clock source encoding: device_e_epochStart_epochEnd (synced) or device_u_uptimeStart_uptimeEnd (unsynced)

**Subtasks:**
- [x] 9.1 Implement AveragedData structure in models/AveragedData.h
- [x] 9.2 Define MAX_PUBLISH_SAMPLES constant (120)
- [x] 9.3 Implement DataManager class with fixed-size averaging buffer array
- [x] 9.4 Add config validation in ConfigManager: publishIntervalSamples <= MAX_PUBLISH_SAMPLES
- [x] 9.5 Implement addReading() method with effective length tracking
- [x] 9.6 Implement shouldPublish() method
- [-] 9.7 Implement calculateAverages() with arithmetic mean
- [-] 9.8 Implement generateBatchId() with clock source encoding (e_ or u_ prefix)
- [-] 9.9 Implement clearAveragingBuffer() method
- [x] 9.10 Write unit tests for averaging buffer lifecycle
- [x] 9.11 Write property test for averaging buffer lifecycle (Property 4)
- [x] 9.12 Write property test for average calculation correctness (Property 5)
- [x] 9.13 Write property test for batch ID uniqueness (Property 10)

---

## Task 10: Data Manager - Transmission Ring Buffer

**Description:** Implement fixed-size ring buffer for storing averaged readings when network is unavailable.

**Requirements:** Requirement 3.5-3.6 (Data Storage and Buffering)

**Details:**
- Implement Data_Buffer as fixed-size ring buffer (not std::deque) for stability
- Use fixed array with head/tail pointers
- Implement bufferForTransmission() to add averaged data
- Implement ring buffer behavior: discard oldest when 90% full
- Implement getBufferedData() to retrieve all buffered readings
- Implement clearAcknowledgedData() to remove acknowledged entries by batch_id
- Track buffer overflow counter in SystemStatus

**Subtasks:**
- [x] 10.1 Implement Data_Buffer as fixed-size ring buffer
- [x] 10.2 Implement bufferForTransmission() method
- [x] 10.3 Implement ring buffer overflow logic (90% threshold)
- [x] 10.4 Implement getBufferedData() method
- [x] 10.5 Implement clearAcknowledgedData() with batch_id matching
- [x] 10.6 Increment buffer overflow counter in SystemStatus
- [x] 10.7 Write unit tests for buffer overflow scenarios
- [x] 10.8 Write property test for data buffer ring behavior (Property 6)
- [x] 10.9 Write property test for idempotent acknowledgment (Property 12)

---

## Task 11: Data Manager - Display Ring Buffer

**Description:** Implement fixed-size ring buffers for storing historical data points for graph rendering.

**Requirements:** Requirement 3.7, 7.9-7.11 (Display Buffer)

**Details:**
- Implement Display_Buffer as fixed-size ring buffers per sensor (not map + deque)
- Use fixed 2D array: DisplayPoint displayBuf[NUM_SENSORS][240]
- Implement addToDisplayBuffer() with 1-minute fixed interval
- Maintain 240 data points per sensor (4 hours of history)
- Implement getDisplayData() with optional downsampling
- Track last display update timestamp

**Subtasks:**
- [x] 11.1 Implement DisplayPoint structure in models/DisplayPoint.h
- [x] 11.2 Implement Display_Buffer as fixed-size 2D array
- [x] 11.3 Implement addToDisplayBuffer() with 1-minute interval check
- [x] 11.4 Implement 240-point capacity limit per sensor
- [x] 11.5 Implement getDisplayData() with downsampling support
- [x] 11.6 Write unit tests for display buffer capacity
- [x] 11.7 Write property test for display buffer fixed interval (Property 7)
- [x] 11.8 Write property test for display buffer capacity limit (Property 8)

---

## Task 12: Network Manager - WiFi Connection

**Description:** Implement WiFi connection management with automatic reconnection and exponential backoff.

**Requirements:** Requirement 4 (WiFi Connectivity Management)

**Details:**
- Implement WiFi connection using stored credentials
- Implement exponential backoff for connection retries (1s, 2s, 4s, 8s, 16s)
- Implement internet connectivity verification via HTTP GET
- Implement automatic reconnection on connection loss
- Track reconnection attempts and last attempt timestamp
- Update SystemStatus with WiFi RSSI

**Subtasks:**
- [x] 12.1 Implement NetworkManager class skeleton
- [x] 12.2 Implement connectWiFi() with retry logic
- [x] 12.3 Implement calculateBackoffDelay() for exponential backoff
- [x] 12.4 Implement optional verifyInternetConnectivity() with HTTP GET (WiFi connected is sufficient; POST decides)
- [x] 12.5 Implement checkConnection() for periodic connection monitoring
- [x] 12.6 Implement isConnected() status check
- [x] 12.7 Update SystemStatus with WiFi RSSI
- [x] 12.8 Write unit tests for WiFi connection scenarios
- [-] 12.9 Write property test for exponential backoff calculation (Property 9)

---

## Task 13: Network Manager - JSON Payload Formatting

**Description:** Implement JSON payload formatting for sensor data transmission (unit testable, no network dependency).

**Requirements:** Requirement 5.1-5.2, 5.12 (JSON Payload)

**Details:**
- Implement formatJsonPayload() for single and batch readings
- Include all required fields: batch_id, device_id, timestamps (both epoch and uptime), sample_count, time_synced, sensors, sensor_status, health
- Include both epoch timestamps (nullable/0 when unsynced) and uptime timestamps (always present)
- Fields: sample_start_epoch_ms, sample_start_uptime_ms, sample_end_epoch_ms, sample_end_uptime_ms
- Handle null values for unavailable sensors
- Include SystemStatus metrics in health section
- Make unit testable without network calls (use native test environment)

**Subtasks:**
- [x] 13.1 Implement formatJsonPayload() for single reading
- [x] 13.2 Implement batch payload formatting for multiple readings
- [x] 13.3 Include both epoch (nullable) and uptime (always) timestamps
- [x] 13.4 Implement null value handling for unavailable sensors
- [x] 13.5 Include SystemStatus metrics in health section
- [x] 13.6 Write unit tests for JSON formatting (native environment)
- [ ] 13.7 Write property test for JSON payload completeness (Property 11)
- [ ] 13.8 Write property test for null sensor representation (Property 13)

---

## Task 14: Network Manager - HTTP Transmission (Plain)

**Description:** Implement HTTP POST transmission over plain WiFi first (before adding TLS complexity).

**Requirements:** Requirement 5 (Data Transmission - HTTP only)

**Details:**
- Implement HTTP POST to API endpoint
- Include API authentication token in request headers
- Parse server response for acknowledged batch_ids
- Implement retry logic with exponential backoff
- Test with local HTTP endpoint first
- Update SystemStatus with last successful transmission time

**Subtasks:**
- [x] 14.1 Implement sendData() with HTTP POST (no TLS yet)
- [x] 14.2 Implement API token header inclusion
- [x] 14.3 Implement parseAcknowledgedBatchIds() from server response
- [x] 14.4 Implement retry logic with exponential backoff
- [x] 14.5 Update SystemStatus with transmission timestamps
- [x] 14.6 Test with local HTTP endpoint
- [x] 14.7 Write unit tests for response parsing

---

## Task 15: Network Manager - HTTPS with TLS

**Description:** Add HTTPS support with certificate validation (staged after HTTP works).

**Requirements:** Requirement 5.6 (HTTPS with Certificate Validation)

**Details:**
- Implement HTTPS POST using WiFiClientSecure
- Configure certificate validation
- Handle TLS handshake errors gracefully
- HTTP fallback: disabled by default, only enabled when tlsValidationMode == INSECURE_DEV_ONLY or allowHttpFallback == true
- Test with real HTTPS endpoint

**Subtasks:**
- [x] 15.1 Implement HTTPS POST using WiFiClientSecure
- [x] 15.2 Configure certificate validation
- [x] 15.3 Implement TLS error handling
- [x] 15.4 Add HTTP fallback (configurable, default false, only when tlsValidationMode == INSECURE_DEV_ONLY)
- [x] 15.5 Test with real HTTPS endpoint

---

## Task 16: Display Manager - Summary Page with Layout Stability

**Description:** Implement summary page showing current sensor values, status indicators, and system information with stable layout.

**Requirements:** Requirement 7.4-7.6, 7.12-7.17 (Summary Page Display)

**Details:**
- Display current values for all sensors with units
- Show colored status indicators (green/yellow/red) based on data staleness
- Display min/max values since boot
- Show WiFi RSSI indicator
- Show transmission status and queue depth
- Display system uptime in hours and minutes
- Show time since last sensor update
- Use fixed-width right-aligned fields to prevent UI shifting

**Subtasks:**
- [x] 16.1 Implement renderSummaryPage() method
- [x] 16.2 Implement drawSensorValue() with unit labels and fixed-width layout
- [x] 16.3 Implement drawHealthBadge() with color coding (green/yellow/red)
- [x] 16.4 Implement drawMinMax() for each sensor
- [x] 16.5 Implement drawWiFiIndicator() with RSSI
- [x] 16.6 Implement drawQueueDepth() display
- [x] 16.7 Implement uptime formatting and display
- [x] 16.8 Implement min/max tracking in DataManager
- [ ] 16.9 Write property test for min/max tracking (Property 16)
- [ ] 16.10 Write property test for sensor health status mapping (Property 18)
- [ ] 16.11 Write property test for uptime formatting (Property 20)

---

## Task 17: Display Manager - Graph Page Rendering

**Description:** Implement graph page rendering with line graphs and smooth scrolling.

**Requirements:** Requirement 7.7-7.8, 7.11 (Graph Page Display)

**Details:**
- Render line graphs for each sensor type
- Implement smooth scrolling by shifting pixels left
- Implement downsampling when data points exceed 120
- Draw axes with min/max labels
- Cycle through different sensor graphs

**Subtasks:**
- [x] 17.1 Implement DisplayPage enum
- [x] 17.2 Implement renderGraphPage() method
- [x] 17.3 Implement drawLineGraph() with line drawing
- [x] 17.4 Implement scrollGraphLeft() for smooth scrolling
- [x] 17.5 Implement downsample() method for data reduction
- [x] 17.6 Implement drawAxes() with labels
- [x] 17.7 Implement graph scaling based on min/max values
- [x] 17.8 Write property test for graph downsampling (Property 17)

---

## Task 18: Display Manager - System Health and Additional Features

**Description:** Implement system health page, threshold indicators, burn-in protection, and page cycling.

**Requirements:** Requirement 7.14, 7.18, 7.20-7.21 (Display Features)

**Details:**
- Implement system health page showing diagnostic info (debug mode only)
- Implement transmission success indicator (brief flash)
- Implement threshold detection for soil moisture
- Implement backlight dimming after 30 minutes
- Implement sensor error indicators
- Implement page cycling with configurable interval

**Subtasks:**
- [x] 18.1 Implement renderSystemHealthPage() method (debug mode)
- [x] 18.2 Implement setDebugMode() method
- [x] 18.3 Implement transmission status indicator
- [x] 18.4 Implement threshold detection logic
- [x] 18.5 Implement checkBurnInProtection() with backlight dimming
- [x] 18.6 Implement sensor error indicator display
- [x] 18.7 Implement cyclePage() with interval check
- [x] 18.8 Write property test for threshold detection (Property 19)

---

## Task 19: Sensitive Data Protection

**Description:** Ensure sensitive configuration data is not exposed via display or logs.

**Requirements:** Requirement 6.7, 7.7 (Sensitive Data Protection)

**Details:**
- Implement checks to prevent WiFi password display
- Implement checks to prevent API token display
- Verify serial console output is sanitized
- Ensure display data structures don't contain sensitive fields

**Subtasks:**
- [x] 19.1 Implement sensitive data filtering in display rendering
- [x] 19.2 Verify serial console output sanitization (from Task 2)
- [x] 19.3 Review all display and log code for sensitive data exposure
- [x] 19.4 Write property test for sensitive data exclusion (Property 15)

---

## Task 20: Main Loop Integration and Coordination

**Description:** Integrate all components in the main loop with proper timing and task coordination.

**Requirements:** All requirements (System Integration)

**Details:**
- Expand main loop from Task 5 vertical slice
- Implement sensor reading at Reading_Interval
- Trigger data averaging and transmission at Publish_Interval
- Update display in every loop iteration
- Handle serial console input for configuration
- Monitor WiFi connection status
- Coordinate timing between components
- Integrate TimeManager for all timestamps

**Subtasks:**
- [x] 20.1 Expand setup() with all manager initializations
- [x] 20.2 Implement main loop() with proper timing logic
- [x] 20.3 Implement sensor reading timer using TimeManager
- [x] 20.4 Implement publish interval checking
- [x] 20.5 Implement display update coordination
- [x] 20.6 Implement serial console monitoring
- [x] 20.7 Implement WiFi reconnection monitoring
- [x] 20.8 Integrate TimeManager for all timestamp operations

---

## Task 21: Error Handling, Diagnostics, and Watchdog

**Description:** Implement comprehensive error handling, logging, diagnostic features, and watchdog protection.

**Requirements:** Requirement 8 (Error Handling and Diagnostics)

**Details:**
- Implement error logging to serial console
- Implement sensor initialization retry (3 attempts)
- Handle critical error states
- Handle display initialization failure gracefully
- Track progress indicators (last read, last transmit, last error)
- Enable watchdog timer to prevent system hangs
- Ensure long WiFi connect and HTTP send loops yield and feed watchdog
- Implement field diagnostics mode via serial console

**Subtasks:**
- [x] 21.1 Implement error logging functions
- [x] 21.2 Verify sensor initialization retry from Task 4
- [x] 21.3 Implement critical error state handling
- [x] 21.4 Implement display failure fallback
- [x] 21.5 Verify progress indicators in SystemStatus (from Task 8)
- [x] 21.6 Enable watchdog timer
- [x] 21.7 Add watchdog feed calls in WiFi connect and HTTP send loops
- [x] 21.8 Ensure no delay(backoff) loops without watchdog feed
- [x] 21.9 Implement "diag" serial command showing sensor flags, queue depth, last HTTP status, heap low watermark
- [x] 21.10 Write property test for sensor initialization retry (Property 22)

---

## Task 22: Integration Testing

**Description:** Implement integration tests to verify end-to-end flows and component interactions.

**Requirements:** All requirements (System Integration Testing)

**Details:**
- Test sensor to display flow
- Test sensor to network flow
- Test offline buffering flow
- Test configuration persistence flow
- Test display page cycling

**Subtasks:**
- [ ] 22.1 Write integration test for sensor to display flow
- [ ] 22.2 Write integration test for sensor to network flow
- [ ] 22.3 Write integration test for offline buffering
- [ ] 22.4 Write integration test for config persistence
- [ ] 22.5 Write integration test for display page cycling

---

## Task 23: Hardware Testing and Validation

**Description:** Perform hardware testing on actual ESP32 with connected sensors and display.

**Requirements:** All requirements (Hardware Validation)

**Details:**
- Test BME280 I2C communication
- Test DS18B20 OneWire communication
- Test soil moisture ADC readings
- Test TFT display rendering
- Test WiFi connectivity with real router
- Test HTTPS POST to real API endpoint
- Measure power consumption

**Subtasks:**
- [ ] 23.1 Test BME280 sensor on hardware
- [ ] 23.2 Test DS18B20 sensor on hardware
- [ ] 23.3 Test soil moisture sensor on hardware
- [ ] 23.4 Test TFT display rendering on hardware
- [ ] 23.5 Test WiFi connectivity on hardware
- [ ] 23.6 Test HTTPS transmission to real API
- [ ] 23.7 Measure and document power consumption

---

## Task 24: Power Management (Optional)

**Description:** Implement power management features for battery operation.

**Requirements:** Requirement 9 (Power Management - Future)

**Details:**
- Implement light sleep mode between sensor readings
- Implement deep sleep mode with state persistence
- Implement battery voltage monitoring
- Implement adaptive Reading_Interval based on battery level
- Implement display power management

**Subtasks:**
- [x] 24.1* Implement light sleep mode
- [x] 24.2* Implement deep sleep with NVS state persistence
- [x] 24.3* Implement battery voltage monitoring
- [x] 24.4* Implement adaptive interval adjustment
- [x] 24.5* Implement display power management

---

## Task 25: Documentation and Deployment

**Description:** Create user documentation and deployment guide.

**Requirements:** All requirements (Documentation)

**Details:**
- Document hardware wiring and pin connections
- Document configuration via serial console
- Document API endpoint requirements
- Document calibration procedure
- Create deployment checklist

**Subtasks:**
- [x] 25.1 Create hardware wiring diagram
- [x] 25.2 Document serial console commands
- [x] 25.3 Document API endpoint specification
- [x] 25.4 Document calibration procedure
- [x] 25.5 Create deployment checklist
