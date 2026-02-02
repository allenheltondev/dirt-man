# Design Document: ESP32 Sensor Firmware

## Overview

This firmware implements a sensor data collection, averaging, and transmission system for ESP32 microcontrollers. The system reads from three sensors (BME280, DS18B20, soil moisture), displays real-time data and trends on a TFT screen, and transmits averaged readings to a remote API via HTTPS.

The architecture follows a modular design with clear separation between sensor drivers, data processing, network communication, display rendering, and configuration manage
───┐  ┌──────────────┐  ┌──────────────┐     │
│  │ Sensor Task  │  │ Display Task │  │ Network Task │     │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘     │
│         │                  │                  │              │
└─────────┼──────────────────┼──────────────────┼──────────────┘
          │                  │                  │
    ┌─────▼─────┐      ┌────▼────┐       ┌────▼────┐
    │  Sensor   │      │ Display │       │ Network │
    │  Manager  │      │ Manager │       │ Manager │
    └─────┬─────┘      └────┬────┘       └────┬────┘
          │                  │                  │
    ┌─────▼─────────────────▼──────────────────▼─────┐
    │           Data Manager (Buffers)                │
    │  - Averaging Buffer                             │
    │  - Data Buffer (transmission queue)             │
    │  - Display Buffer (graph history)               │
    └─────────────────────┬───────────────────────────┘
                          │
                    ┌─────▼─────┐
                    │   Config  │
                    │  Manager  │
                    │   (NVS)   │
                    └───────────┘
```

### Component Responsibilities

**Sensor Manager**
- Initialize and configure BME280, DS18B20, and soil moisture sensors
- Read sensor values at configured Reading_Interval
- Apply calibration to soil moisture readings
- Validate readings against physical ranges
- Handle sensor errors and retry logic

**Data Manager**
- Maintain three separate buffers: Averaging_Buffer, Data_Buffer, Display_Buffer
- Accumulate readings in Averaging_Buffer
- Calculate averages when Publish_Interval sample count is reached
- Store averaged readings in Data_Buffer when network unavailable
- Maintain Display_Buffer at fixed 1-minute intervals for graphing
- Generate unique batch_id for each averaged payload

**Network Manager**
- Manage WiFi connection and reconnection with exponential backoff
- Verify internet connectivity via HTTP GET
- Format and send HTTPS POST requests with JSON payloads
- Handle idempotent retries using batch_id
- Parse server responses for acknowledged batch_ids
- Implement exponential backoff for failed requests

**Display Manager**
- Initialize TFT display
- Render multiple pages: Summary, Graph (per sensor), System Health
- Auto-cycle between pages at configured interval
- Draw line graphs with smooth scrolling
- Show status indicators: WiFi RSSI, queue depth, sensor health badges
- Display min/max values, uptime, and threshold warnings
- Implement burn-in protection via backlight dimming

**Config Manager**
- Load and save configuration from/to NVS
- Provide serial console interface for configuration
- Validate configuration updates
- Store: WiFi credentials, API endpoint, auth token, intervals, calibration data, display preferences

## Components and Interfaces

### Sensor Manager

**SensorManager Class**
```cpp
class SensorManager {
public:
    void initialize();
    SensorReadings readSensors();
    bool isSensorAvailable(SensorType type);
    void calibrateSoilMoisture(uint16_t dryAdc, uint16_t wetAdc);

private:
    Adafruit_BME280 bme280;
    DallasTemperature ds18b20;
    uint16_t soilDryAdc;
    uint16_t soilWetAdc;
    uint8_t sensorStatus;

    float readBME280Temperature();
    float readBME280Humidity();
    float readBME280Pressure();
    float readDS18B20Temperature();
    float readSoilMoisture();
    uint16_t readSoilMoistureRaw();
    float convertSoilMoistureToPercent(uint16_t rawAdc);
    bool validateReading(SensorType type, float value);
};
```

**SensorReadings Structure**
```cpp
struct SensorReadings {
    float bme280Temp;        // Celsius
    float ds18b20Temp;       // Celsius
    float humidity;          // Percent
    float pressure;          // hPa
    float soilMoisture;      // Percent (clamped 0-100)
    uint16_t soilMoistureRaw; // ADC value
    uint8_t sensorStatus;    // Bitmask: bit per sensor
    uint32_t monotonicMs;    // millis() - monotonic clock
};
```

### Data Manager

**DataManager Class**
```cpp
class DataManager {
public:
    void addReading(const SensorReadings& reading);
    bool shouldPublish();
    AveragedData getAveragedData();
    void clearAveragingBuffer();

    void bufferForTransmission(const AveragedData& data);
    std::vector<AveragedData> getBufferedData();
    void clearAcknowledgedData(const std::vector<String>& batchIds);

    void addToDisplayBuffer(const SensorReadings& reading);
    std::vector<DisplayPoint> getDisplayData(SensorType type, uint16_t maxPoints);

private:
    std::vector<SensorReadings> averagingBuffer;
    std::deque<AveragedData> dataBuffer;
    std::map<SensorType, std::deque<DisplayPoint>> displayBuffers;

    unsigned long lastDisplayUpdate;
    const unsigned long DISPLAY_INTERVAL_MS = 60000; // 1 minute

    String generateBatchId(const AveragedData& data);
    AveragedData calculateAverages();
};
```

**AveragedData Structure**
```cpp
struct AveragedData {
    char batchId[64];        // Fixed-size to avoid String heap churn
    float avgBme280Temp;
    float avgDs18b20Temp;
    float avgHumidity;
    float avgPressure;
    float avgSoilMoisture;
    uint64_t sampleStartEpochMs;  // Unix epoch ms (0 if not synced)
    uint64_t sampleEndEpochMs;    // Unix epoch ms (0 if not synced)
    uint32_t sampleStartUptimeMs; // Always available
    uint32_t sampleEndUptimeMs;   // Always available
    uint64_t deviceBootEpochMs;   // Unix epoch ms (0 if not synced)
    uint32_t uptimeMs;
    uint16_t sampleCount;
    uint8_t sensorStatus;
    bool timeSynced;
};
```

**DisplayPoint Structure**
```cpp
struct DisplayPoint {
    float value;
    unsigned long timestamp;
};
```

### Network Manager

**NetworkManager Class**
```cpp
class NetworkManager {
public:
    void initialize();
    bool connectWiFi();
    bool isConnected();
    void checkConnection();
    bool sendData(const std::vector<AveragedData>& dataList);

private:
    WiFiClientSecure wifiClient;
    HTTPClient httpClient;

    uint8_t reconnectAttempts;
    unsigned long lastReconnectAttempt;

    bool verifyInternetConnectivity();
    String formatJsonPayload(const std::vector<AveragedData>& dataList);
    std::vector<String> parseAcknowledgedBatchIds(const String& response);
    unsigned long calculateBackoffDelay(uint8_t attempt);
};
```

### Display Manager

**DisplayManager Class**
```cpp
class DisplayManager {
public:
    void initialize();
    void update();
    void showStartupScreen(const String& firmwareVersion);
    void cyclePage();
    void setDebugMode(bool enabled);

private:
    TFT_eSPI tft;
    DisplayPage currentPage;
    unsigned long lastPageChange;
    unsigned long lastActivity;
    bool debugMode;

    void renderSummaryPage(const SensorReadings& current, const SystemStatus& status);
    void renderGraphPage(SensorType type, const std::vector<DisplayPoint>& data);
    void renderSystemHealthPage(const SystemStatus& status);

    void drawSensorValue(int16_t x, int16_t y, const String& label, float value, const String& unit, SensorHealth health);
    void drawHealthBadge(int16_t x, int16_t y, SensorHealth health);
    void drawMinMax(int16_t x, int16_t y, float minVal, float maxVal);
    void drawWiFiIndicator(int16_t x, int16_t y, int8_t rssi);
    void drawQueueDepth(int16_t x, int16_t y, uint16_t count);
    void drawLineGraph(const std::vector<DisplayPoint>& data, uint16_t maxPoints);
    void scrollGraphLeft();
    void checkBurnInProtection();
};
```

**DisplayPage Enum**
```cpp
enum class DisplayPage {
    SUMMARY,
    GRAPH_BME280_TEMP,
    GRAPH_DS18B20_TEMP,
    GRAPH_HUMIDITY,
    GRAPH_PRESSURE,
    GRAPH_SOIL_MOISTURE,
    SYSTEM_HEALTH
};
```

**SystemStatus Structure**
```cpp
struct SystemStatus {
    unsigned long uptimeMs;
    uint32_t freeHeap;
    int8_t wifiRssi;
    uint16_t queueDepth;
    uint16_t bootCount;
    ErrorCounters errors;
    SensorReadings minValues;
    SensorReadings maxValues;
};
```

### Config Manager

**ConfigManager Class**
```cpp
class ConfigManager {
public:
    void initialize();
    bool loadConfig();
    bool saveConfig();
    void setDefaults();
    bool validateConfig();

    void handleSerialConfig();

    Config& getConfig();

private:
    Config config;
    Preferences nvs;

    void printConfigMenu();
    void updateWiFiCredentials();
    void updateApiEndpoint();
    void updateIntervals();
    void updateCalibration();
};
```

**Config Structure**
```cpp
struct Config {
    // WiFi
    String wifiSsid;
    String wifiPassword;

    // API
    String apiEndpoint;
    String apiToken;
    String deviceId;

    // Timing
    uint32_t readingIntervalMs;
    uint16_t publishIntervalSamples;
    uint16_t pageCycleIntervalMs;

    // Calibration
    uint16_t soilDryAdc;
    uint16_t soilWetAdc;

    // Display
    bool temperatureInFahrenheit;
    uint16_t soilMoistureThresholdLow;
    uint16_t soilMoistureThresholdHigh;

    // Power
    bool batteryMode;
};
```

## Data Models

### Memory Layout

**RAM Buffers:**
- Averaging_Buffer: ~20 readings × 32 bytes = 640 bytes
- Data_Buffer: ~50 averaged readings × 128 bytes = 6.4 KB (ring buffer)
- Display_Buffer: 240 points × 5 sensors × 8 bytes = 9.6 KB

**NVS Storage:**
- Config structure: ~512 bytes
- Calibration data: 16 bytes
- Boot counter: 4 bytes

**Total RAM Usage Estimate:** ~20 KB for buffers + ~10 KB for libraries = 30 KB (well within ESP32's 520 KB)

### JSON Payload Format

**Single Averaged Reading:**
```json
{
  "batch_id": "device123_1704067200000_1704067800000",
  "device_id": "device123",
  "sample_start_ts": 1704067200000,
  "sample_end_ts": 1704067800000,
  "device_boot_ts": 1704060000000,
  "uptime_ms": 7200000,
  "sample_count": 20,
  "time_synced": false,
  "sensors": {
    "bme280_temp_c": 22.5,
    "ds18b20_temp_c": 21.8,
    "humidity_pct": 45.2,
    "pressure_hpa": 1013.25,
    "soil_moisture_pct": 62.3
  },
  "sensor_status": {
    "bme280": "ok",
    "ds18b20": "ok",
    "soil_moisture": "ok"
  },
  "health": {
    "uptime_ms": 7200000,
    "free_heap_bytes": 245760,
    "wifi_rssi_dbm": -65,
    "error_counters": {
      "sensor_read_failures": 2,
      "network_failures": 0,
      "buffer_overflows": 0
    }
  }
}
```

**Batch Transmission:**
```json
{
  "device_id": "device123",
  "readings": [
    { /* averaged reading 1 */ },
    { /* averaged reading 2 */ },
    { /* averaged reading 3 */ }
  ]
}
```

**Server Response:**
```json
{
  "status": "success",
  "acknowledged_batch_ids": [
    "device123_1704067200000_1704067800000",
    "device123_1704067800000_1704068400000"
  ]
}
```

## Data Flow

### Sensor Reading Flow

```
1. Timer expires (Reading_Interval)
2. SensorManager.readSensors()
   - Read BME280 (temp, humidity, pressure)
   - Read DS18B20 (temp)
   - Read soil moisture ADC with averaging
   - Apply calibration to soil moisture
   - Validate all readings
   - Set sensor status bits
3. DataManager.addReading(readings)
   - Add to Averaging_Buffer
   - Check if should add to Display_Buffer (1-minute interval)
   - Update min/max tracking
4. If DataManager.shouldPublish() (20 samples reached)
   - Calculate averages
   - Generate batch_id
   - If WiFi connected: NetworkManager.sendData()
   - If WiFi disconnected: DataManager.bufferForTransmission()
   - Clear Averaging_Buffer
```

### Network Transmission Flow

```
1. NetworkManager.sendData(dataList)
2. Format JSON payload with all buffered readings
3. Send HTTPS POST to API_Endpoint with auth token
4. If success:
   - Parse acknowledged_batch_ids from response
   - DataManager.clearAcknowledgedData(batchIds)
5. If failure:
   - Increment error counter
   - Calculate exponential backoff delay
   - Keep data in Data_Buffer for retry
6. If Data_Buffer > 90% full:
   - Discard oldest entries
   - Increment buffer overflow counter
```

### Display Update Flow

```
1. DisplayManager.update() called in main loop
2. Check if page cycle interval elapsed
   - If yes: cyclePage()
3. Get current sensor readings and system status
4. Render current page:
   - SUMMARY: Show all sensors, WiFi, queue, uptime
   - GRAPH: Get display data for current sensor, draw line graph
   - SYSTEM_HEALTH: Show debug info (if enabled)
5. Check burn-in protection timer
   - If > 30 minutes: dim backlight
```

## Implementation Details

### Sensor Reading with Error Handling

```cpp
SensorReadings SensorManager::readSensors() {
    SensorReadings readings = {};
    readings.timestamp = millis();
    readings.sensorStatus = 0;

    // BME280
    if (bme280.begin()) {
        readings.bme280Temp = readBME280Temperature();
        readings.humidity = readBME280Humidity();
        readings.pressure = readBME280Pressure();
        if (validateReading(SENSOR_BME280_TEMP, readings.bme280Temp)) {
            readings.sensorStatus |= (1 << SENSOR_BME280);
        }
    }

    // DS18B20
    ds18b20.requestTemperatures();
    readings.ds18b20Temp = ds18b20.getTempCByIndex(0);
    if (readings.ds18b20Temp != DEVICE_DISCONNECTED_C) {
        readings.sensorStatus |= (1 << SENSOR_DS18B20);
    }

    // Soil Moisture
    readings.soilMoistureRaw = readSoilMoistureRaw();
    readings.soilMoisture = convertSoilMoistureToPercent(readings.soilMoistureRaw);
    if (readings.soilMoistureRaw > 0) {
        readings.sensorStatus |= (1 << SENSOR_SOIL);
    }

    return readings;
}
```

### Averaging Calculation

```cpp
AveragedData DataManager::calculateAverages() {
    AveragedData avg = {};

    if (averagingBuffer.empty()) return avg;

    float sumBme280Temp = 0, sumDs18b20Temp = 0;
    float sumHumidity = 0, sumPressure = 0, sumSoilMoisture = 0;
    uint16_t validSamples = 0;

    for (const auto& reading : averagingBuffer) {
        sumBme280Temp += reading.bme280Temp;
        sumDs18b20Temp += reading.ds18b20Temp;
        sumHumidity += reading.humidity;
        sumPressure += reading.pressure;
        sumSoilMoisture += reading.soilMoisture;
        validSamples++;
    }

    avg.avgBme280Temp = sumBme280Temp / validSamples;
    avg.avgDs18b20Temp = sumDs18b20Temp / validSamples;
    avg.avgHumidity = sumHumidity / validSamples;
    avg.avgPressure = sumPressure / validSamples;
    avg.avgSoilMoisture = sumSoilMoisture / validSamples;

    avg.sampleStartTs = averagingBuffer.front().timestamp;
    avg.sampleEndTs = averagingBuffer.back().timestamp;
    avg.deviceBootTs = 0; // Set from RTC or NTP
    avg.uptimeMs = millis();
    avg.sampleCount = validSamples;
    avg.sensorStatus = averagingBuffer.back().sensorStatus;
    avg.timeSynced = false; // Update when NTP implemented

    avg.batchId = generateBatchId(avg);

    return avg;
}
```

### Batch ID Generation

```cpp
String DataManager::generateBatchId(const AveragedData& data) {
    // Format: deviceId_startTs_endTs
    String batchId = ConfigManager::getConfig().deviceId;
    batchId += "_";
    batchId += String(data.sampleStartTs);
    batchId += "_";
    batchId += String(data.sampleEndTs);
    return batchId;
}
```

### WiFi Connection with Exponential Backoff

```cpp
bool NetworkManager::connectWiFi() {
    Config& config = ConfigManager::getConfig();

    WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());

    uint8_t attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 5) {
        unsigned long backoff = calculateBackoffDelay(attempt);
        delay(backoff);
        attempt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        reconnectAttempts = 0;
        return verifyInternetConnectivity();
    }

    reconnectAttempts++;
    return false;
}

unsigned long NetworkManager::calculateBackoffDelay(uint8_t attempt) {
    // Exponential backoff: 1s, 2s, 4s, 8s, 16s
    return 1000 * (1 << attempt);
}

bool NetworkManager::verifyInternetConnectivity() {
    HTTPClient http;
    http.begin("http://clients3.google.com/generate_204");
    int httpCode = http.GET();
    http.end();
    return (httpCode == 204);
}
```

### Graph Rendering with Smooth Scrolling

```cpp
void DisplayManager::drawLineGraph(const std::vector<DisplayPoint>& data, uint16_t maxPoints) {
    if (data.empty()) return;

    // Downsample if needed
    std::vector<DisplayPoint> plotData = data;
    if (data.size() > maxPoints) {
        plotData = downsample(data, maxPoints);
    }

    // Find min/max for scaling
    float minVal = plotData[0].value;
    float maxVal = plotData[0].value;
    for (const auto& point : plotData) {
        minVal = min(minVal, point.value);
        maxVal = max(maxVal, point.value);
    }

    // Draw axes and labels
    drawAxes(minVal, maxVal);

    // Draw line graph with smooth scrolling
    for (size_t i = 1; i < plotData.size(); i++) {
        int16_t x1 = map(i - 1, 0, plotData.size() - 1, GRAPH_X_START, GRAPH_X_END);
        int16_t y1 = map(plotData[i - 1].value, minVal, maxVal, GRAPH_Y_END, GRAPH_Y_START);
        int16_t x2 = map(i, 0, plotData.size() - 1, GRAPH_X_START, GRAPH_X_END);
        int16_t y2 = map(plotData[i].value, minVal, maxVal, GRAPH_Y_END, GRAPH_Y_START);

        tft.drawLine(x1, y1, x2, y2, TFT_CYAN);
    }
}
```

### Configuration via Serial Console

```cpp
void ConfigManager::handleSerialConfig() {
    if (!Serial.available()) return;

    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "config") {
        printConfigMenu();
    } else if (command.startsWith("wifi ")) {
        updateWiFiCredentials();
    } else if (command.startsWith("api ")) {
        updateApiEndpoint();
    } else if (command.startsWith("calibrate ")) {
        updateCalibration();
    } else if (command == "save") {
        if (validateConfig()) {
            saveConfig();
            Serial.println("Configuration saved");
        } else {
            Serial.println("Invalid configuration");
        }
    } else if (command == "debug on") {
        DisplayManager::setDebugMode(true);
    } else if (command == "debug off") {
        DisplayManager::setDebugMode(false);
    }
}
```


## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system—essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Sensor Reading Completeness
*For any* sensor configuration, when readSensors() is called, the returned SensorReadings structure should contain valid data for all available sensors and set appropriate status bits for unavailable sensors.
**Validates: Requirements 1.2, 1.3, 1.4, 1.6**

### Property 2: Soil Moisture Calibration Formula
*For any* raw ADC value between soilDryAdc and soilWetAdc, converting to percentage and back should preserve the relative position within the range (within floating point precision).
**Validates: Requirements 2.2**

### Property 3: Configuration Round Trip
*For any* valid Config structure, saving to NVS and then loading should produce an equivalent Config structure with all fields preserved including WiFi credentials, API settings, intervals, and calibration data.
**Validates: Requirements 2.1, 6.3, 7.19**

### Property 4: Averaging Buffer Lifecycle
*For any* sequence of sensor readings, after adding N readings to the Averaging_Buffer (where N equals Publish_Interval), calling getAveragedData() should return averaged values and clear the buffer to empty state.
**Validates: Requirements 3.1, 3.2, 3.4**

### Property 5: Average Calculation Correctness
*For any* set of sensor readings in the Averaging_Buffer, the calculated average for each sensor should equal the arithmetic mean of all readings for that sensor.
**Validates: Requirements 3.3**

### Property 6: Data Buffer Ring Behavior
*For any* Data_Buffer at 90% capacity, adding a new averaged reading should result in the oldest entry being removed and the new entry being added, maintaining the buffer size constraint.
**Validates: Requirements 3.6**

### Property 7: Display Buffer Fixed Interval
*For any* sequence of sensor readings collected at Reading_Interval, the Display_Buffer should only be updated when at least 1 minute has elapsed since the last display buffer update, independent of Reading_Interval.
**Validates: Requirements 3.7, 7.9**

### Property 8: Display Buffer Capacity Limit
*For any* sequence of display buffer updates, the Display_Buffer should never exceed 240 data points per sensor, removing oldest points when capacity is reached.
**Validates: Requirements 7.10**

### Property 9: Exponential Backoff Calculation
*For any* retry attempt number N (where 0 ≤ N < 5), the backoff delay should equal 1000 × 2^N milliseconds, and no more than 5 attempts should be made.
**Validates: Requirements 4.3, 5.10**

### Property 10: Batch ID Uniqueness
*For any* two different AveragedData structures with different timestamp ranges or device IDs, their generated batch_ids should be unique.
**Validates: Requirements 5.5**

### Property 11: JSON Payload Completeness
*For any* AveragedData structure, the formatted JSON payload should contain all required fields: batch_id, device_id, timestamps (sample_start_ts, sample_end_ts, device_boot_ts, uptime_ms), sample_count, time_synced, sensor readings, sensor_status, and health metrics.
**Validates: Requirements 5.2, 8.7**

### Property 12: Idempotent Acknowledgment
*For any* Data_Buffer containing multiple averaged readings, after receiving a server response with acknowledged batch_ids, only the entries with matching batch_ids should be removed from the buffer, leaving unacknowledged entries intact.
**Validates: Requirements 5.9**

### Property 13: Null Sensor Representation
*For any* sensor marked as unavailable in sensor_status, the JSON payload should contain null for that sensor's value and the sensor_status flags should correctly indicate unavailability.
**Validates: Requirements 5.12**

### Property 14: Configuration Validation Rejection
*For any* invalid configuration update (e.g., negative intervals, empty required fields), calling validateConfig() should return false and the current configuration should remain unchanged.
**Validates: Requirements 6.4, 6.5**

### Property 15: Sensitive Data Exclusion
*For any* display data structure or rendered display content, sensitive configuration values (WiFi password, API token) should not be present in the data or visible on screen.
**Validates: Requirements 6.7**

### Property 16: Min/Max Tracking
*For any* sequence of sensor readings, the tracked minimum value should never be greater than any reading, and the tracked maximum value should never be less than any reading.
**Validates: Requirements 7.6**

### Property 17: Graph Downsampling
*For any* display data with more than 120 points, the downsampled data should contain exactly 120 points and preserve the overall shape of the data (first and last points preserved, intermediate points representative).
**Validates: Requirements 7.11**

### Property 18: Sensor Health Status Mapping
*For any* sensor with recent valid readings (< 5 seconds old), the health status should be green; for stale readings (5-30 seconds), yellow; for errors or no readings (> 30 seconds), red.
**Validates: Requirements 7.5**

### Property 19: Threshold Detection
*For any* soil moisture reading and configured thresholds, when the reading crosses a threshold boundary, the threshold indicator should be set appropriately (low, normal, or high).
**Validates: Requirements 7.18**

### Property 20: Uptime Formatting
*For any* uptime value in milliseconds, the formatted uptime string should correctly represent hours and minutes, where hours = uptime_ms / 3600000 and minutes = (uptime_ms % 3600000) / 60000.
**Validates: Requirements 7.17**

### Property 21: Error Counter Increments
*For any* error event (sensor failure, network failure, buffer overflow), the corresponding error counter should be incremented by exactly 1, and the counter should never decrease.
**Validates: Requirements 8.2, 8.5, 8.6**

### Property 22: Sensor Initialization Retry
*For any* sensor that fails initialization, the initialization should be attempted exactly 3 times before marking the sensor as permanently unavailable for the current boot cycle.
**Validates: Requirements 8.3**

### Property 23: ADC Averaging Noise Reduction
*For any* set of N consecutive ADC readings from the soil moisture sensor, the averaged value should have lower variance than individual readings, demonstrating noise reduction.
**Validates: Requirements 1.5**

### Property 24: Physical Range Validation
*For any* sensor reading, if the value is outside the physically possible range for that sensor type (e.g., BME280 temp < -40°C or > 85°C), the reading should be flagged as invalid in sensor_status.
**Validates: Requirements 1.7**

### Property 25: Timestamp Format Consistency
*For any* timestamp in the system, it should be represented as Unix epoch milliseconds (positive integer), ensuring consistent time representation across all components.
**Validates: Requirements 5.3**

## Error Handling

### Sensor Errors

**Sensor Initialization Failure:**
- Retry up to 3 times with 1-second delay between attempts
- Mark sensor as unavailable in sensor_status bitmask
- Continue operation with remaining sensors
- Display error indicator on TFT for failed sensor

**Sensor Read Failure:**
- Log error to serial console with sensor type and timestamp
- Increment sensor_read_failures counter
- Set sensor_status bit to 0 for failed sensor
- Use null value in JSON payload for unavailable sensor
- Continue reading other sensors

**All Sensors Failed:**
- Enter error state
- Display error message on TFT: "SENSOR ERROR - CHECK CONNECTIONS"
- Continue attempting to read sensors every Reading_Interval
- Allow system recovery if sensors become available

### Network Errors

**WiFi Connection Failure:**
- Retry with exponential backoff: 1s, 2s, 4s, 8s, 16s (max 5 attempts)
- Increment network_failures counter
- Display WiFi disconnected indicator on TFT
- Buffer sensor data in Data_Buffer
- Continue sensor reading and display updates
- Attempt reconnection every 60 seconds

**HTTP POST Failure:**
- Increment network_failures counter
- Store averaged data in Data_Buffer with batch_id
- Retry with exponential backoff up to 5 attempts
- If all retries fail, keep data in buffer for next transmission cycle
- Display transmission failure indicator on TFT

**Server Error Response (4xx/5xx):**
- Log HTTP status code to serial console
- Increment network_failures counter
- For 4xx errors: log and discard (client error, won't succeed on retry)
- For 5xx errors: retry with exponential backoff
- Display error indicator on TFT

### Memory Errors

**Data Buffer Overflow:**
- Increment buffer_overflows counter
- Discard oldest averaged reading from Data_Buffer
- Add new reading to buffer
- Display queue depth warning on TFT if > 80% full
- Log overflow event to serial console

**Heap Memory Low:**
- Monitor free heap in every loop iteration
- If free heap < 10KB, log warning to serial console
- Reduce Display_Buffer size if necessary
- Include free_heap_bytes in health metrics

**NVS Write Failure:**
- Log error to serial console
- Retry write operation once
- If retry fails, continue with in-memory config
- Display warning on TFT: "CONFIG SAVE FAILED"

### Display Errors

**TFT Initialization Failure:**
- Log error to serial console
- Set display_available flag to false
- Continue all other operations (sensors, network)
- Retry display initialization on next boot

**Display Rendering Error:**
- Catch exceptions in rendering code
- Log error to serial console
- Skip current frame and continue with next update cycle
- Increment display_errors counter

## Testing Strategy

### Unit Testing

Unit tests will verify specific examples, edge cases, and error conditions for individual components. Tests will be written using the Arduino testing framework or PlatformIO unit testing.

**Sensor Manager Tests:**
- Test BME280 initialization with valid/invalid I2C address
- Test DS18B20 initialization with valid/invalid OneWire pin
- Test soil moisture calibration with boundary values (0, 4095)
- Test sensor read timeout handling
- Test physical range validation for each sensor type

**Data Manager Tests:**
- Test averaging buffer with exactly 20 readings
- Test averaging buffer with empty state
- Test data buffer overflow with 51 entries (> 90% of 50)
- Test batch ID generation with known timestamps
- Test display buffer with 240 points (boundary)
- Test display buffer with 241 points (overflow)

**Network Manager Tests:**
- Test WiFi connection with valid credentials
- Test WiFi connection with invalid credentials
- Test exponential backoff calculation for attempts 0-4
- Test JSON payload formatting with all sensors available
- Test JSON payload formatting with some sensors unavailable
- Test batch ID acknowledgment parsing

**Display Manager Tests:**
- Test page cycling through all page types
- Test downsampling with 240 points to 120 points
- Test min/max tracking with known sequences
- Test sensor health status for different staleness values
- Test uptime formatting for various millisecond values

**Config Manager Tests:**
- Test NVS save/load with complete config
- Test NVS save/load with missing config (defaults)
- Test config validation with invalid intervals
- Test config validation with empty required fields

### Property-Based Testing

Property tests will verify universal properties across many generated inputs using a property-based testing library (e.g., QuickCheck-style testing adapted for embedded systems, or custom property test harness).

**Configuration:**
- Minimum 100 iterations per property test
- Each test tagged with: **Feature: esp32-sensor-firmware, Property N: [property text]**

**Property Test Implementation:**

**Property 1: Sensor Reading Completeness**
- Generate: Random sensor availability configurations
- Test: readSensors() returns data for available sensors and sets status bits correctly
- Tag: **Feature: esp32-sensor-firmware, Property 1: For any sensor configuration, readSensors() returns data for all available sensors**

**Property 2: Soil Moisture Calibration Formula**
- Generate: Random ADC values between soilDryAdc and soilWetAdc
- Test: Convert to percentage, verify formula correctness
- Tag: **Feature: esp32-sensor-firmware, Property 2: Soil moisture calibration formula preserves relative position**

**Property 3: Configuration Round Trip**
- Generate: Random valid Config structures
- Test: Save to mock NVS, load, compare for equality
- Tag: **Feature: esp32-sensor-firmware, Property 3: Config round trip preserves all fields**

**Property 4: Averaging Buffer Lifecycle**
- Generate: Random sequences of N sensor readings
- Test: Add to buffer, verify shouldPublish() triggers, verify buffer clears after getAveragedData()
- Tag: **Feature: esp32-sensor-firmware, Property 4: Averaging buffer lifecycle maintains correct state**

**Property 5: Average Calculation Correctness**
- Generate: Random sets of sensor readings
- Test: Calculate average, verify equals arithmetic mean
- Tag: **Feature: esp32-sensor-firmware, Property 5: Average calculation equals arithmetic mean**

**Property 6: Data Buffer Ring Behavior**
- Generate: Random sequences of averaged readings
- Test: Fill buffer to 90%, add new reading, verify oldest removed
- Tag: **Feature: esp32-sensor-firmware, Property 6: Data buffer ring behavior maintains capacity**

**Property 7: Display Buffer Fixed Interval**
- Generate: Random sequences of readings at various intervals
- Test: Verify display buffer only updates at 1-minute intervals
- Tag: **Feature: esp32-sensor-firmware, Property 7: Display buffer updates at fixed 1-minute intervals**

**Property 8: Display Buffer Capacity Limit**
- Generate: Random sequences of 250+ display updates
- Test: Verify buffer never exceeds 240 points
- Tag: **Feature: esp32-sensor-firmware, Property 8: Display buffer capacity limited to 240 points**

**Property 9: Exponential Backoff Calculation**
- Generate: Random retry attempt numbers 0-10
- Test: Verify backoff delay formula and max 5 attempts
- Tag: **Feature: esp32-sensor-firmware, Property 9: Exponential backoff follows 2^N formula**

**Property 10: Batch ID Uniqueness**
- Generate: Random pairs of AveragedData with different timestamps
- Test: Verify batch IDs are unique
- Tag: **Feature: esp32-sensor-firmware, Property 10: Batch IDs are unique for different data**

**Property 11: JSON Payload Completeness**
- Generate: Random AveragedData structures
- Test: Format JSON, verify all required fields present
- Tag: **Feature: esp32-sensor-firmware, Property 11: JSON payload contains all required fields**

**Property 12: Idempotent Acknowledgment**
- Generate: Random Data_Buffer with multiple entries and random acknowledgment lists
- Test: Clear acknowledged entries, verify unacknowledged remain
- Tag: **Feature: esp32-sensor-firmware, Property 12: Only acknowledged batch IDs are removed**

**Property 13: Null Sensor Representation**
- Generate: Random sensor availability configurations
- Test: Verify unavailable sensors have null values in JSON
- Tag: **Feature: esp32-sensor-firmware, Property 13: Unavailable sensors represented as null**

**Property 14: Configuration Validation Rejection**
- Generate: Random invalid configurations
- Test: Verify validateConfig() returns false and config unchanged
- Tag: **Feature: esp32-sensor-firmware, Property 14: Invalid config rejected and current config preserved**

**Property 15: Sensitive Data Exclusion**
- Generate: Random Config with sensitive data
- Test: Verify display data structures don't contain passwords or tokens
- Tag: **Feature: esp32-sensor-firmware, Property 15: Sensitive data excluded from display**

**Property 16: Min/Max Tracking**
- Generate: Random sequences of sensor readings
- Test: Verify min never > any reading, max never < any reading
- Tag: **Feature: esp32-sensor-firmware, Property 16: Min/max tracking maintains invariants**

**Property 17: Graph Downsampling**
- Generate: Random display data with 150-300 points
- Test: Verify downsampled data has exactly 120 points and preserves shape
- Tag: **Feature: esp32-sensor-firmware, Property 17: Downsampling produces exactly 120 points**

**Property 18: Sensor Health Status Mapping**
- Generate: Random sensor readings with various staleness values
- Test: Verify health status (green/yellow/red) matches staleness rules
- Tag: **Feature: esp32-sensor-firmware, Property 18: Sensor health status correctly mapped from staleness**

**Property 19: Threshold Detection**
- Generate: Random soil moisture readings and thresholds
- Test: Verify threshold indicators set correctly when crossing boundaries
- Tag: **Feature: esp32-sensor-firmware, Property 19: Threshold detection triggers at boundaries**

**Property 20: Uptime Formatting**
- Generate: Random uptime values in milliseconds
- Test: Verify formatted string matches hours/minutes calculation
- Tag: **Feature: esp32-sensor-firmware, Property 20: Uptime formatting correctly converts milliseconds**

**Property 21: Error Counter Increments**
- Generate: Random sequences of error events
- Test: Verify counters increment by 1 and never decrease
- Tag: **Feature: esp32-sensor-firmware, Property 21: Error counters increment correctly**

**Property 22: Sensor Initialization Retry**
- Generate: Random sensor failure scenarios
- Test: Verify exactly 3 initialization attempts before marking unavailable
- Tag: **Feature: esp32-sensor-firmware, Property 22: Sensor initialization retries exactly 3 times**

**Property 23: ADC Averaging Noise Reduction**
- Generate: Random sets of noisy ADC readings
- Test: Verify averaged value has lower variance than individual readings
- Tag: **Feature: esp32-sensor-firmware, Property 23: ADC averaging reduces variance**

**Property 24: Physical Range Validation**
- Generate: Random sensor readings including out-of-range values
- Test: Verify out-of-range readings flagged as invalid
- Tag: **Feature: esp32-sensor-firmware, Property 24: Out-of-range readings flagged as invalid**

**Property 25: Timestamp Format Consistency**
- Generate: Random timestamp values
- Test: Verify all timestamps are positive Unix epoch milliseconds
- Tag: **Feature: esp32-sensor-firmware, Property 25: Timestamps use Unix epoch milliseconds**

### Integration Testing

Integration tests will verify end-to-end flows and component interactions:

**Sensor to Display Flow:**
- Read sensors → Display updates with current values
- Verify display shows correct sensor data within 1 second

**Sensor to Network Flow:**
- Read 20 sensors → Average → Format JSON → Send HTTP POST
- Verify complete flow with mock HTTP server

**Offline Buffering Flow:**
- Disconnect WiFi → Read sensors → Buffer data → Reconnect → Transmit buffered data
- Verify no data loss during offline period

**Configuration Persistence Flow:**
- Update config via serial → Save to NVS → Reboot → Load config
- Verify config persists across reboot

**Display Page Cycling:**
- Initialize display → Wait for page cycle interval → Verify all pages shown
- Verify smooth transitions between pages

### Hardware Testing

Hardware tests will verify sensor integration and display rendering on actual ESP32 hardware:

**Sensor Hardware Tests:**
- BME280 I2C communication at 400kHz
- DS18B20 OneWire timing and CRC validation
- Soil moisture ADC with multiple attenuation settings
- Sensor read timing under various conditions

**Display Hardware Tests:**
- TFT initialization and rendering performance
- Graph scrolling smoothness at 10 FPS
- Backlight dimming functionality
- Page cycling visual verification

**Network Hardware Tests:**
- WiFi connection to various router types
- HTTPS POST to real API endpoint
- Certificate validation with real certificates
- Network reconnection after router reboot

**Power Consumption Tests:**
- Measure current draw in active mode
- Measure current draw in light sleep mode
- Verify power management reduces consumption
- Battery life estimation with typical usage pattern
