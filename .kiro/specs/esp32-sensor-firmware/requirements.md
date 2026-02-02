# Requirements Document

## Introduction

This document specifies the requirements for an ESP32-based sensor data collection and transmission system. The firmware will collect environmental data from multiple sensors (BME280, DS18B20, and soil moisture sensor), average readings over time, and transmit the data to a remote API endpoint via WiFi using HTTP POST. The system must handle network connectivity issues gracefully, display real-time data and trends on a TFT screen, and provide a foundation for future battery operation.

## Glossary

- **Firmware**: The embedded software running on the ESP32 microcontroller
- **BME280_Sensor**: Bosch BME280 environmental sensor providing ambient temperature near enclosure, humidity, and pressure readings
- **DS18B20_Sensor**: Dallas DS18B20 digital temperature sensor for probe temperature measurement
- **Soil_Moisture_Sensor**: Capacitive analog soil moisture sensor with ADC-based readings
- **Data_Buffer**: RAM-based ring buffer for averaged sensor readings when network is unavailable
- **API_Endpoint**: Remote server endpoint that receives sensor data via HTTPS
- **Reading_Interval**: Time period between consecutive sensor measurements
- **Publish_Interval**: Time period or sample count between HTTP POST transmissions
- **Averaging_Buffer**: RAM storage for accumulating sensor readings to calculate averages
- **WiFi_Manager**: Component responsible for WiFi connection and reconnection
- **HTTP_Client**: Component for HTTPS POST communication
- **Configuration_Store**: Non-volatile storage in NVS for system settings
- **TFT_Display**: Color display screen for visualizing sensor data
- **Display_Buffer**: RAM storage for historical data points used in graph rendering at fixed intervals
- **Graph_Renderer**: Component responsible for drawing line graphs on the TFT_Display
- **Calibration_Data**: Two-point calibration values stored in NVS for soil moisture conversion
- **Batch_ID**: Unique identifier for each averaged payload to enable idempotent retries

## Requirements

### Requirement 1: Sensor Data Collection

**User Story:** As a system operator, I want the firmware to collect data from all connected sensors with proper calibration, so that I can monitor environmental conditions accurately.

#### Acceptance Criteria

1. WHEN the system initializes, THE Firmware SHALL detect and initialize the BME280_Sensor, DS18B20_Sensor, and Soil_Moisture_Sensor
2. WHEN a Reading_Interval expires, THE Firmware SHALL read temperature, humidity, and pressure from the BME280_Sensor
3. WHEN a Reading_Interval expires, THE Firmware SHALL read temperature from the DS18B20_Sensor
4. WHEN a Reading_Interval expires, THE Firmware SHALL read moisture level from the Soil_Moisture_Sensor using configured ADC attenuation
5. WHEN reading from the Soil_Moisture_Sensor, THE Firmware SHALL apply averaging across multiple ADC samples to reduce noise
6. IF a sensor fails to respond, THEN THE Firmware SHALL log the error and continue operating with remaining sensors
7. WHEN sensor data is collected, THE Firmware SHALL validate that readings are within physically possible ranges for each sensor type

### Requirement 2: Soil Moisture Calibration

**User Story:** As a system operator, I want to calibrate soil moisture readings, so that the sensor provides meaningful percentage values rather than raw ADC counts.

#### Acceptance Criteria

1. THE Configuration_Store SHALL persist two-point calibration values: soilDryAdc and soilWetAdc
2. WHEN soil moisture is read, THE Firmware SHALL convert raw ADC value to percentage using the formula: percentage = (rawAdc - soilDryAdc) / (soilWetAdc - soilDryAdc) * 100
3. WHEN calibration values are not set, THE Firmware SHALL use default values and indicate uncalibrated status
4. THE Firmware SHALL provide a calibration interface via serial console to set soilDryAdc and soilWetAdc values
5. WHERE debug mode is enabled, THE TFT_Display SHALL show both raw ADC value and calibrated percentage for soil moisture

### Requirement 3: Data Averaging and Storage

**User Story:** As a system operator, I want the firmware to average sensor readings over multiple samples, so that I can reduce noise and minimize network transmission frequency.

#### Acceptance Criteria

1. WHEN new sensor data is collected, THE Firmware SHALL store it in the Averaging_Buffer in RAM
2. WHEN the Averaging_Buffer contains readings equal to Publish_Interval sample count, THE Firmware SHALL calculate the average value for each sensor
3. WHEN calculating averages, THE Firmware SHALL compute the mean temperature, humidity, pressure, and soil moisture across all buffered readings
4. WHEN averages are calculated, THE Firmware SHALL clear the Averaging_Buffer and begin collecting the next set of readings
5. WHEN network transmission fails, THE Firmware SHALL store averaged readings in the Data_Buffer RAM ring buffer for later transmission
6. WHEN the Data_Buffer reaches 90 percent capacity, THE Firmware SHALL discard the oldest averaged readings to make room for new data
7. WHEN sensor data is collected, THE Firmware SHALL also store readings in the Display_Buffer at a fixed graph interval independent of Reading_Interval

### Requirement 4: WiFi Connectivity Management

**User Story:** As a system operator, I want the firmware to manage WiFi connectivity automatically, so that the system maintains network communication without manual intervention.

#### Acceptance Criteria

1. WHEN the system starts, THE WiFi_Manager SHALL attempt to connect using stored credentials from Configuration_Store
2. WHEN WiFi connection is established, THE WiFi_Manager SHALL verify internet connectivity by performing an HTTP GET request to a known endpoint
3. IF WiFi connection fails, THEN THE WiFi_Manager SHALL retry connection with exponential backoff up to 5 attempts
4. WHEN WiFi connection is lost during operation, THE WiFi_Manager SHALL attempt automatic reconnection
5. WHILE WiFi is disconnected, THE Firmware SHALL continue collecting sensor data and storing averaged readings in the Data_Buffer
6. WHEN WiFi reconnects after disconnection, THE Firmware SHALL transmit buffered data from the Data_Buffer

### Requirement 5: Data Transmission via HTTP

**User Story:** As a system operator, I want the firmware to send averaged sensor data via HTTPS POST with idempotent retry support, so that I can receive consolidated data reliably using standard web APIs.

#### Acceptance Criteria

1. WHEN Publish_Interval sample count is reached and averages are calculated, THE HTTP_Client SHALL format the averaged data as JSON
2. WHEN the HTTP_Client sends data, THE JSON payload SHALL include: averaged sensor readings, sample_start_ts, sample_end_ts, device_boot_ts, uptime_ms, device_identifier, sample_count, batch_id, and time_synced flag
3. THE Firmware SHALL use Unix epoch milliseconds for all timestamps
4. WHEN time is not synchronized via NTP, THE Firmware SHALL set time_synced to false and use uptime-based timestamps
5. WHEN the HTTP_Client sends data, THE Firmware SHALL generate a unique batch_id for each payload using device_id and timestamp range
6. WHERE the API_Endpoint supports HTTPS, THE HTTP_Client SHALL use HTTPS with certificate validation
7. THE HTTP_Client SHALL include an API key or authentication token from Configuration_Store in request headers
8. IF the HTTP POST request fails, THEN THE Firmware SHALL store the averaged data with its batch_id in the Data_Buffer for retry
9. WHEN the HTTP_Client receives a successful response, THE Firmware SHALL parse the response for acknowledged batch_ids and clear only those entries from the Data_Buffer
10. WHEN the HTTP_Client receives an error response, THE Firmware SHALL log the error code and retry with exponential backoff up to 5 attempts
11. WHEN multiple averaged readings are buffered, THE HTTP_Client SHALL transmit them in a single batch request
12. WHEN a sensor is unavailable, THE Firmware SHALL send null for that sensor's value and include sensor_status flags in the payload

### Requirement 6: Configuration Management

**User Story:** As a system operator, I want to configure system parameters without recompiling firmware, so that I can adapt the system to different deployment scenarios.

#### Acceptance Criteria

1. WHEN the system first boots, THE Firmware SHALL check the Configuration_Store in NVS for existing settings
2. IF no configuration exists, THEN THE Firmware SHALL use default values and store them in the Configuration_Store
3. THE Configuration_Store SHALL persist: WiFi credentials, API_Endpoint URL, API authentication token, Reading_Interval, Publish_Interval, device_identifier, and Calibration_Data
4. WHEN configuration is updated, THE Firmware SHALL validate new settings before storing them in the Configuration_Store
5. WHEN invalid configuration is received, THE Firmware SHALL reject the update and maintain current settings
6. THE Firmware SHALL provide a configuration interface via serial console for initial setup and calibration
7. THE TFT_Display SHALL NOT display sensitive configuration values such as WiFi passwords or API tokens

### Requirement 7: Display Visualization

**User Story:** As a user, I want to see current sensor readings, historical trends, and system status on a display with clear visual indicators, so that I can monitor environmental conditions and system health at a glance without accessing remote systems.

#### Acceptance Criteria

1. WHEN the system initializes, THE Firmware SHALL initialize the TFT_Display and render a startup screen showing firmware version
2. THE TFT_Display SHALL have multiple display pages: Summary page, Graph pages, and System Health page
3. THE TFT_Display SHALL automatically cycle between pages every 10 seconds
4. WHEN in Summary mode, THE TFT_Display SHALL show current values for all sensors with units, WiFi connection indicator, transmission status, and time since last sensor update
5. WHEN displaying sensor values, THE TFT_Display SHALL show a colored status indicator for each sensor: green for normal, yellow for stale data, red for error or out-of-range
6. WHEN displaying sensor values, THE TFT_Display SHALL show minimum and maximum values since boot for each metric
7. WHEN in Graph mode, THE TFT_Display SHALL show one sensor's line graph at a time and cycle through sensors
8. WHEN rendering graphs, THE Graph_Renderer SHALL use smooth scrolling by shifting existing pixels left and drawing new points on the right edge
9. WHEN sensor data is collected, THE Firmware SHALL store readings in the Display_Buffer at a fixed graph interval of 1 minute
10. THE Display_Buffer SHALL maintain up to 240 data points representing 4 hours of history
11. WHEN the Display_Buffer is updated, THE Graph_Renderer SHALL draw a line graph with a maximum of 120 plotted points using downsampling if necessary
12. WHEN WiFi is connected, THE TFT_Display SHALL show WiFi RSSI signal strength indicator
13. WHEN WiFi is disconnected, THE TFT_Display SHALL show a WiFi disconnected indicator icon
14. WHEN data transmission succeeds, THE TFT_Display SHALL briefly show a transmission success indicator
15. WHEN the Data_Buffer contains unsent readings, THE TFT_Display SHALL show the queue depth count
16. WHEN sensor errors occur, THE TFT_Display SHALL show an error indicator for the affected sensor
17. THE TFT_Display SHALL show system uptime in hours and minutes
18. WHERE configured thresholds exist for soil moisture, THE TFT_Display SHALL show visual indicators when values cross thresholds
19. THE Configuration_Store SHALL persist display preferences including temperature units and page cycle interval
20. WHEN the display has been active for more than 30 minutes, THE Firmware SHALL dim the backlight to prevent burn-in
21. WHERE debug mode is enabled via serial console, THE TFT_Display SHALL show a System Health page with raw ADC values, free heap memory, WiFi RSSI in dBm, and boot counter

### Requirement 8: Error Handling and Diagnostics

**User Story:** As a system operator, I want comprehensive error handling and diagnostic information, so that I can troubleshoot issues and ensure system reliability.

#### Acceptance Criteria

1. WHEN any error occurs, THE Firmware SHALL output error information to serial console with error type, timestamp, and context
2. THE Firmware SHALL maintain in-memory rolling counters for error types and include them in transmitted health metrics
3. WHEN sensor initialization fails, THE Firmware SHALL retry initialization up to 3 times before marking the sensor as unavailable
4. IF all sensors fail to initialize, THEN THE Firmware SHALL enter error state and display error message on TFT_Display
5. WHEN network transmission fails repeatedly, THE Firmware SHALL increment failure counter and log the last error message
6. WHEN the Data_Buffer reaches capacity, THE Firmware SHALL increment buffer overflow counter
7. THE Firmware SHALL include system health metrics in transmitted data: uptime_ms, free_heap_bytes, wifi_rssi_dbm, error_counters, and sensor_status flags
8. WHEN critical errors occur, THE Firmware SHALL provide diagnostic information via serial output
9. WHEN the TFT_Display fails to initialize, THE Firmware SHALL continue operation and log the display error to serial console

### Requirement 9: Power Management (Future)

**User Story:** As a system operator, I want the firmware to support efficient power management, so that the system can operate on battery power in future deployments.

#### Acceptance Criteria

1. WHERE power_mode is set to battery in Configuration_Store, THE Firmware SHALL enable power management features
2. WHILE waiting between sensor readings and power_mode is battery, THE Firmware SHALL enter light sleep mode to reduce power consumption
3. WHEN entering deep sleep mode, THE Firmware SHALL persist critical state to NVS including Data_Buffer contents and Display_Buffer
4. WHEN waking from deep sleep, THE Firmware SHALL restore WiFi connection and load persisted state from NVS
5. WHERE battery operation is detected, THE Firmware SHALL reduce display refresh rate or disable display updates
6. WHEN battery voltage drops below a configured threshold, THE Firmware SHALL increase the Reading_Interval to extend operation time
7. THE power management requirements SHALL be implemented only when power_mode is set to battery and display is in low-refresh mode
