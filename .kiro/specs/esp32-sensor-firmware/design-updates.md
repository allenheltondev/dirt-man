# Critical Design Updates

## 1. Time Model Consistency

**Problem:** Mixing millis() with Unix epoch milliseconds causes timeline inconsistencies.

**Solution:** Support two separate clocks:
- **Monotonic Clock:** `millis()` - always available, never jumps, used for intervals
- **Epoch Clock:** Unix epoch ms - only valid after NTP sync, used for server timestamps

**Updated Structures:**
```cpp
struct SensorReadings {
    float bme280Temp;
    float ds18b20Temp;
    float humidity;
    float pressure;
    float soilMoisture;        // Clamped 0-100%
    uint16_t soilMoistureRaw;
    uint8_t sensorStatus;
    uint32_t monotonicMs;      // millis() - monotonic clock
};

struct AveragedData {
    char batchId[64];          // Fixed-size to avoid String heap churn
    float avgBme280Temp;
    float avgDs18b20Temp;
    float avgHumidity;
    float avgPressure;

**Problem:** Calling `bme280.begin()` in hot path (readSensors()) is expensive.

**Solution:** Initialize once, set availability flags, check flags before reading.

```cpp
class SensorManager {
private:
    bool bme280Available;
    bool ds18b20Available;
    bool soilSensorAvailable;

    void initializeSensors() {
        // BME280
        bme280Available = bme280.begin(0x76);
        if (!bme280Available) {
            bme280Available = bme280.begin(0x77); // Try alternate address
        }

        // DS18B20
        ds18b20.begin();
        ds18b20Available = (ds18b20.getDeviceCount() > 0);

        // Soil moisture
        analogSetAttenuation(ADC_11db);
        analogSetWidth(12);
        soilSensorAvailable = true; // Always available (analog pin)
    }

public:
    SensorReadings readSensors() {
        SensorReadings readings = {};
        readings.monotonicMs = millis();
        readings.sensorStatus = 0;

        // Only read if sensor was successfully initialized
        if (bme280Available) {
            readings.bme280Temp = bme280.readTemperature();
            readings.humidity = bme280.readHumidity();
            readings.pressure = bme280.readPressure() / 100.0F;
            if (validateReading(SENSOR_BME280_TEMP, readings.bme280Temp)) {
                readings.sensorStatus |= (1 << SENSOR_BME280);
            }
        }

        if (ds18b20Available) {
            ds18b20.requestTemperatures();
            readings.ds18b20Temp = ds18b20.getTempCByIndex(0);
            if (readings.ds18b20Temp != DEVICE_DISCONNECTED_C) {
                readings.sensorStatus |= (1 << SENSOR_DS18B20);
            }
        }

        if (soilSensorAvailable) {
            readings.soilMoistureRaw = readSoilMoistureRaw();
            readings.soilMoisture = convertSoilMoistureToPercent(readings.soilMoistureRaw);
            // Clamp to 0-100%
            readings.soilMoisture = constrain(readings.soilMoisture, 0.0f, 100.0f);
            readings.sensorStatus |= (1 << SENSOR_SOIL);
        }

        return readings;
    }
};
```

## 3. Error Handling Without Exceptions

**Problem:** ESP32 typically runs with exceptions disabled.

**Solution:** Use error codes and availability flags instead of try-catch.

```cpp
class DisplayManager {
private:
    bool displayAvailable;

public:
    bool initialize() {
        tft.init();
        if (!tft.getRotation()) {  // Simple check if display responds
            displayAvailable = false;
            Serial.println("ERROR: TFT initialization failed");
            return false;
        }
        displayAvailable = true;
        return true;
    }

    void update() {
        if (!displayAvailable) return;  // Skip rendering if display unavailable

        // Rendering code with bounds checking
        // No exceptions needed
    }
};
```

## 4. ADC Configuration Details

**Problem:** ESP32 ADC needs specific configuration for stability.

**Solution:** Use ADC1 pins, specify attenuation and resolution explicitly.

```cpp
// ADC Configuration
static const uint8_t SOIL_MOISTURE_PIN = 32;  // ADC1_CH4 (GPIO32)
// Alternative ADC1 pins: 33, 34, 35, 36, 39
// Avoid ADC2 pins (GPIO 0, 2, 4, 12-15, 25-27) - conflict with WiFi

static const adc_attenuation_t ADC_ATTEN = ADC_11db;  // 0-3.3V range
static const adc_bits_width_t ADC_WIDTH = ADC_WIDTH_BIT_12;  // 0-4095

uint16_t SensorManager::readSoilMoistureRaw() {
    // Discard first reading after wake/boot
    static bool firstRead = true;
    if (firstRead) {
        analogRead(SOIL_MOISTURE_PIN);
        delay(10);
        firstRead = false;
    }

    // Average multiple readings to reduce noise
    const uint8_t NUM_SAMPLES = 10;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < NUM_SAMPLES; i++) {
        sum += analogRead(SOIL_MOISTURE_PIN);
        delay(10);
    }
    return sum / NUM_SAMPLES;
}
```

## 5. WiFi Connectivity Check Strategy

**Problem:** External dependency on Google endpoint can fail for unrelated reasons.

**Solution:** Make connectivity check optional, treat WiFi connected as sufficient.

```cpp
class NetworkManager {
private:
    bool verifyInternetConnectivity() {
        // Optional connectivity check - can be disabled in config
        if (!config.enableConnectivityCheck) {
            return true;  // Trust WiFi connection
        }

        // Try connectivity check with timeout
        HTTPClient http;
        http.setTimeout(5000);  // 5 second timeout
        http.begin("http://clients3.google.com/generate_204");
        int httpCode = http.GET();
        http.end();

        // If check fails, still consider connected (let POST attempts decide)
        return (httpCode == 204 || httpCode == -1);
    }
};
```

## 6. HTTPS Certificate Validation Strategy

**Problem:** No mention of certificate validation approach.

**Solution:** Support multiple strategies with configuration.

```cpp
enum class TLSValidationMode {
    ROOT_CA,           // Validate against root CA (recommended)
    FINGERPRINT,       // Pin certificate fingerprint
    INSECURE_DEV_ONLY  // Skip validation (dev/LAN only)
};

class NetworkManager {
private:
    WiFiClientSecure wifiClient;

    void configureTLS() {
        switch (config.tlsValidationMode) {
            case TLSValidationMode::ROOT_CA:
                wifiClient.setCACert(config.rootCACert);
                break;

            case TLSValidationMode::FINGERPRINT:
                wifiClient.setFingerprint(config.certFingerprint);
                break;

            case TLSValidationMode::INSECURE_DEV_ONLY:
                wifiClient.setInsecure();  // Skip validation
                Serial.println("WARNING: TLS validation disabled");
                break;
        }
    }
};

// Add to Config structure:
struct Config {
    // ... existing fields ...

    // TLS Configuration
    TLSValidationMode tlsValidationMode;
    char rootCACert[2048];      // PEM format root CA
    char certFingerprint[60];   // SHA-1 fingerprint
    bool enableConnectivityCheck;
};
```

## 7. Memory Optimization - Avoid String Heap Churn

**Problem:** Using String and std::vector<String> causes heap fragmentation.

**Solution:** Use fixed-size char arrays and generate batch_id on-the-fly.

```cpp
// Generate batch ID at send time, don't store it
void generateBatchId(const AveragedData& data, char* output, size_t maxLen) {
    snprintf(output, maxLen, "%s_%llu_%llu",
             config.deviceId,
             data.sampleStartUptimeMs,
             data.sampleEndUptimeMs);
}

// Use fixed-size arrays in data structures
struct AveragedData {
    char batchId[64];  // Fixed size, no heap allocation
    // ... other fields ...
};

// Avoid std::vector of String - use fixed-size arrays or circular buffers
class DataManager {
private:
    static const uint8_t MAX_BUFFERED_READINGS = 50;
    AveragedData dataBuffer[MAX_BUFFERED_READINGS];
    uint8_t bufferHead;
    uint8_t bufferTail;
    uint8_t bufferCount;
};
```

## 8. Display Update Performance

**Problem:** Full-screen redraws can cause flicker.

**Solution:** Use dirty rectangles and appropriate update rates.

```cpp
class DisplayManager {
private:
    static const uint16_t SUMMARY_UPDATE_MS = 500;   // 2 Hz
    static const uint16_t GRAPH_UPDATE_MS = 1000;    // 1 Hz

    uint32_t lastSummaryUpdate;
    uint32_t lastGraphUpdate;

    void renderSummaryPage(const SensorReadings& current, const SystemStatus& status) {
        uint32_t now = millis();
        if (now - lastSummaryUpdate < SUMMARY_UPDATE_MS) return;
        lastSummaryUpdate = now;

        // Only redraw changed regions (dirty rectangles)
        updateSensorValue(10, 20, "Temp", current.bme280Temp, "C");
        updateSensorValue(10, 40, "Hum", current.humidity, "%");
        // ... update only changed values, not entire screen
    }

    void updateSensorValue(int16_t x, int16_t y, const char* label, float value, const char* unit) {
        // Clear only the value region
        tft.fillRect(x + 50, y, 60, 16, TFT_BLACK);
        // Draw new value
        tft.setCursor(x + 50, y);
        tft.printf("%.1f%s", value, unit);
    }
};
```

## 9. Staleness Thresholds Based on Reading Interval

**Problem:** Fixed staleness thresholds don't adapt to sampling rate.

**Solution:** Derive thresholds from Reading_Interval.

```cpp
enum class SensorHealth {
    GREEN,   // Fresh data
    YELLOW,  // Stale data
    RED      // Error or very stale
};

SensorHealth calculateSensorHealth(uint32_t lastReadingMs, uint32_t readingIntervalMs) {
    uint32_t age = millis() - lastReadingMs;

    if (age < readingIntervalMs * 1.5) {
        return SensorHealth::GREEN;
    } else if (age < readingIntervalMs * 3) {
        return SensorHealth::YELLOW;
    } else {
        return SensorHealth::RED;
    }
}
```

## 10. Graph Auto-Scale with Minimum Range

**Problem:** Auto-scale can cause jitter on flat data.

**Solution:** Use minimum range to prevent flatline jitter.

```cpp
void DisplayManager::drawLineGraph(const std::vector<DisplayPoint>& data, uint16_t maxPoints) {
    if (data.empty()) return;

    // Find min/max
    float minVal = data[0].value;
    float maxVal = data[0].value;
    for (const auto& point : data) {
        minVal = min(minVal, point.value);
        maxVal = max(maxVal, point.value);
    }

    // Apply minimum range to prevent jitter
    float range = maxVal - minVal;
    const float MIN_RANGE = 2.0;  // Minimum 2 units range
    if (range < MIN_RANGE) {
        float center = (minVal + maxVal) / 2.0;
        minVal = center - MIN_RANGE / 2.0;
        maxVal = center + MIN_RANGE / 2.0;
    }

    // Draw scale values on graph
    tft.setTextSize(1);
    tft.setCursor(GRAPH_X_START - 30, GRAPH_Y_START);
    tft.printf("%.1f", maxVal);
    tft.setCursor(GRAPH_X_START - 30, GRAPH_Y_END);
    tft.printf("%.1f", minVal);

    // Draw graph...
}
```

## 11. Alert Banner for Critical Conditions

**Problem:** Threshold warnings can get lost in UI.

**Solution:** Add prominent alert banner at top of display.

```cpp
void DisplayManager::renderAlertBanner() {
    const char* alert = nullptr;
    uint16_t alertColor = TFT_RED;

    // Check for alerts in priority order
    if (systemStatus.queueDepth > 40) {
        alert = "QUEUE FULL";
    } else if (systemStatus.sensorStatus == 0) {
        alert = "ALL SENSORS OFFLINE";
    } else if (currentReadings.soilMoisture < config.soilMoistureThresholdLow) {
        alert = "DRY SOIL";
        alertColor = TFT_ORANGE;
    } else if (currentReadings.soilMoisture > config.soilMoistureThresholdHigh) {
        alert = "TOO WET";
        alertColor = TFT_BLUE;
    } else if (!networkManager.isConnected()) {
        alert = "WIFI OFFLINE";
        alertColor = TFT_YELLOW;
    }

    if (alert) {
        tft.fillRect(0, 0, tft.width(), 16, alertColor);
        tft.setTextColor(TFT_BLACK);
        tft.setCursor(5, 4);
        tft.print(alert);
    }
}
```

## 12. Firmware Version in Payload

**Problem:** No version tracking in transmitted data.

**Solution:** Include firmware version and build info.

```cpp
#define FIRMWARE_VERSION "1.0.0"
#define BUILD_TIMESTAMP __DATE__ " " __TIME__

// Add to AveragedData structure
struct AveragedData {
    // ... existing fields ...
    char firmwareVersion[16];
    char buildTimestamp[32];
};

// Include in JSON payload
{
    "firmware_version": "1.0.0",
    "build_timestamp": "Jan 15 2025 14:30:00",
    // ... rest of payload ...
}
```

## 13. Factory Reset Command

**Problem:** No way to recover from bad configuration.

**Solution:** Add serial command to clear NVS and reboot.

```cpp
void ConfigManager::handleSerialConfig() {
    if (!Serial.available()) return;

    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "factory-reset") {
        Serial.println("WARNING: This will erase all configuration!");
        Serial.println("Type 'confirm' to proceed:");
        delay(5000);

        if (Serial.available()) {
            String confirm = Serial.readStringUntil('\n');
            confirm.trim();
            if (confirm == "confirm") {
                nvs.clear();
                Serial.println("Configuration erased. Rebooting...");
                delay(1000);
                ESP.restart();
            }
        }
    }
    // ... other commands ...
}
```

## 14. Watchdog and Timeout Strategy

**Problem:** Blocking calls can hang the system.

**Solution:** Add timeouts to all network operations and feed watchdog.

```cpp
void setup() {
    // Enable hardware watchdog (60 second timeout)
    esp_task_wdt_init(60, true);
    esp_task_wdt_add(NULL);
}

void loop() {
    // Feed watchdog in main loop
    esp_task_wdt_reset();

    // ... main loop code ...
}

class NetworkManager {
private:
    bool connectWiFi() {
        WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());

        uint8_t attempt = 0;
        const uint32_t CONNECT_TIMEOUT_MS = 30000;  // 30 second total timeout
        uint32_t startTime = millis();

        while (WiFi.status() != WL_CONNECTED && attempt < 5) {
            if (millis() - startTime > CONNECT_TIMEOUT_MS) {
                Serial.println("WiFi connect timeout");
                return false;
            }

            esp_task_wdt_reset();  // Feed watchdog during long operation
            unsigned long backoff = calculateBackoffDelay(attempt);
            delay(backoff);
            attempt++;
        }

        return (WiFi.status() == WL_CONNECTED);
    }

    bool sendData(const std::vector<AveragedData>& dataList) {
        HTTPClient http;
        http.setTimeout(15000);  // 15 second timeout for HTTP operations
        http.setConnectTimeout(10000);  // 10 second connect timeout

        // ... rest of send logic ...
    }
};
```

## Summary of Critical Changes

1. **Time Model:** Separate monotonic (millis) and epoch (NTP) clocks
2. **Sensor Init:** Initialize once, check availability flags before reading
3. **Error Handling:** Use error codes/flags, not exceptions
4. **ADC:** Use ADC1 pins (GPIO 32-39), specify attenuation/resolution
5. **WiFi Check:** Make connectivity verification optional
6. **HTTPS:** Support ROOT_CA, fingerprint, or insecure modes
7. **Memory:** Use fixed-size char arrays, avoid String heap churn
8. **Display:** Dirty rectangles, appropriate update rates (0.5-2 Hz)
9. **Staleness:** Derive thresholds from Reading_Interval
10. **Graph Scale:** Minimum range to prevent jitter, show scale values
11. **Alerts:** Prominent banner for critical conditions
12. **Version:** Include firmware version in payload and display
13. **Factory Reset:** Serial command to clear NVS
14. **Watchdog:** Timeouts on all network ops, feed watchdog in long operations

## 15. Millis() Overflow Awareness

**Problem:** millis() overflows after ~49 days (32-bit unsigned).

**Solution:** Always use subtraction for time comparisons, never direct comparison.

```cpp
// CORRECT - handles overflow
if (millis() - lastUpdate > interval) {
    // Do something
}

// WRONG - breaks on overflow
if (millis() > lastUpdate + interval) {
    // Don't do this!
}
```

**Design Rule:** All time comparisons SHALL use subtraction (now - then) and never direct > comparisons to handle millis() overflow correctly.

**Correctness Property 26:** Monotonic Clock Invariant
*For any* two successive SensorReadings, the second reading's monotonicMs SHALL be greater than or equal to the first reading's monotonicMs (accounting for overflow using unsigned arithmetic).

## 16. Soil Sensor Diagnostics

**Problem:** Assuming soil sensor is "always available" misses wiring issues.

**Solution:** Detect stuck readings and wiring problems.

```cpp
class SensorManager {
private:
    uint16_t lastSoilReading;
    uint8_t unchangedCount;

    bool validateSo
RNING: Soil sensor stuck - possible wiring issue");
                return false;  // Mark as degraded
            }
        } else {
            unchangedCount = 0;
        }

        lastSoilReading = rawAdc;
        return true;
    }
};
```

## 17. TLS Certificate Storage Optimization

**Problem:** Storing 2KB root CA in NVS causes fragmentation and slow writes.

**Solution:** Use compile-time CA in PROGMEM, store only mode/fingerprint in NVS.

```cpp
// Option A: Compile-time CA (recommended)
const char* ROOT_CA_CERT PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ
...
-----END CERTIFICATE-----
)EOF";

struct Config {
    // ... existing fields ...

    // TLS Configuration (optimized)
    TLSValidationMode tlsValidationMode;
    char certFingerprint[60];   // Only 60 bytes instead of 2KB
    bool useBuiltInCA;          // Use PROGMEM CA
};

void NetworkManager::configureTLS() {
    switch (config.tlsValidationMode) {
        case TLSValidationMode::ROOT_CA:
            if (config.useBuiltInCA) {
                wifiClient.setCACert(ROOT_CA_CERT);  // From PROGMEM
            } else {
                // Load from external source if needed
            }
            break;

        case TLSValidationMode::FINGERPRINT:
            wifiClient.setFingerprint(config.certFingerprint);
            break;

        case TLSValidationMode::INSECURE_DEV_ONLY:
            wifiClient.setInsecure();
            break;
    }
}
```

## 18. Display Layout Stability

**Problem:** Variable-width numbers cause UI elements to shift.

**Solution:** Use fixed-width, right-aligned fields for sensor values.

```cpp
void DisplayManager::drawSensorValue(int16_t x, int16_t y, const char* label,
                                      float value, const char* unit) {
    // Label (left-aligned)
    tft.setCursor(x, y);
    tft.print(label);
    tft.print(":");

    // Value (right-aligned in fixed-width field)
    const uint8_t VALUE_WIDTH = 60;  // Fixed width in pixels
    char valueStr[16];
    snprintf(valueStr, sizeof(valueStr), "%6.1f%s", value, unit);

    // Clear value region
    tft.fillRect(x + 50, y, VALUE_WIDTH, 16, TFT_BLACK);

    // Draw right-aligned
    tft.setCursor(x + 50, y);
    tft.print(valueStr);
}
```

**Design Rule:** Sensor value fields SHALL be fixed-width and right-aligned to prevent UI shifting when values change.

## 19. Watchdog Progress Requirement

**Problem:** Feeding watchdog in infinite retry loops hides deadlocks.

**Solution:** Only feed watchdog in known good progress paths.

```cpp
bool NetworkManager::connectWiFi() {
    uint8_t attempt = 0;
    const uint32_t CONNECT_TIMEOUT_MS = 30000;
    uint32_t startTime = millis();

    while (WiFi.status() != WL_CONNECTED && attempt < 5) {
        if (millis() - startTime > CONNECT_TIMEOUT_MS) {
            return false;  // Timeout - don't feed watchdog, let it reset
        }

        // Only feed watchdog when making progress (new attempt)
        if (attempt > 0) {
            esp_task_wdt_reset();
        }

        unsigned long backoff = calculateBackoffDelay(attempt);
        delay(backoff);
        attempt++;
    }

    return (WiFi.status() == WL_CONNECTED);
}
```

**Design Rule:** Watchdog SHALL NOT be reset inside error retry loops without demonstrable progress to prevent hidden infinite loops.

## 20. Sensor-Specific Graph Ranges

**Problem:** Single MIN_RANGE doesn't work well across different sensor scales.

**Solution:** Define minimum ranges per sensor type.

```cpp
class DisplayManager {
private:
    struct GraphConfig {
        float minRange;
        const char* unit;
        uint16_t color;
    };

    static const GraphConfig GRAPH_CONFIGS[];

    void drawLineGraph(SensorType type, const std::vector<DisplayPoint>& data) {
        const GraphConfig& cfg = GRAPH_CONFIGS[type];

        // Find min/max
        float minVal = data[0].value;
        float maxVal = data[0].value;
        for (const auto& point : data) {
            minVal = min(minVal, point.value);
            maxVal = max(maxVal, point.value);
        }

        // Apply sensor-specific minimum range
        float range = maxVal - minVal;
        if (range < cfg.minRange) {
            float center = (minVal + maxVal) / 2.0;
            minVal = center - cfg.minRange / 2.0;
            maxVal = center + cfg.minRange / 2.0;
        }

        // Draw graph with sensor-specific color
        // ...
    }
};

// Sensor-specific configurations
const DisplayManager::GraphConfig DisplayManager::GRAPH_CONFIGS[] = {
    {1.0,  "C",   TFT_RED},      // BME280 temp
    {1.0,  "C",   TFT_ORANGE},   // DS18B20 temp
    {5.0,  "%",   TFT_CYAN},     // Humidity
    {10.0, "hPa", TFT_GREEN},    // Pressure
    {5.0,  "%",   TFT_BLUE}      // Soil moisture
};
```

## 21. Timeout Policy for All Blocking I/O

**Problem:** Need consistent timeout policy across all operations.

**Solution:** Document explicit timeout requirement.

**Design Rule:** All blocking I/O operations SHALL have explicit timeouts ≤ 15 seconds to prevent system hangs.

```cpp
// Network operations
HTTPClient http;
http.setTimeout(15000);           // 15 second timeout
http.setConnectTimeout(10000);    // 10 second connect timeout

// WiFi operations
const uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;  // 30 seconds total

// Sensor operations
const uint32_t SENSOR_READ_TIMEOUT_MS = 5000;    // 5 seconds per sensor

// Display operations (non-blocking, but with frame skip)
const uint32_t MAX_RENDER_TIME_MS = 100;         // Skip frame if > 100ms
```

## 22. Alert Banner Priority with Battery

**Problem:** Battery alerts should outrank queue depth warnings.

**Solution:** Update alert priority order to include battery status.

```cpp
void DisplayManager::renderAlertBanner() {
    const char* alert = nullptr;
    uint16_t alertColor = TFT_RED;

    // Check for alerts in priority order (highest to lowest)
    if (systemStatus.batteryVoltage > 0 && systemStatus.batteryVoltage < 3.3) {
        alert = "LOW BATTERY";
        alertColor = TFT_RED;
    } else if (systemStatus.sensorStatus == 0) {
        alert = "ALL SENSORS OFFLINE";
        alertColor = TFT_RED;
    } else if (systemStatus.queueDepth > 40) {
        alert = "QUEUE FULL";
        alertColor = TFT_ORANGE;
    } else if (currentReadings.soilMoisture < config.soilMoistureThresholdLow) {
        alert = "DRY SOIL";
        alertColor = TFT_ORANGE;
    } else if (currentReadings.soilMoisture > config.soilMoistureThresholdHigh) {
        alert = "TOO WET";
        alertColor = TFT_BLUE;
    } else if (!networkManager.isConnected()) {
        alert = "WIFI OFFLINE";
        alertColor = TFT_YELLOW;
    }

    if (alert) {
        tft.fillRect(0, 0, tft.width(), 16, alertColor);
        tft.setTextColor(TFT_BLACK);
        tft.setTextSize(1);
        tft.setCursor(5, 4);
        tft.print(alert);
    }
}
```

**Alert Priority Order:**
1. Low Battery (critical)
2. All Sensors Offline (critical)
3. Queue Full (warning)
4. Soil Moisture Thresholds (info)
5. WiFi Offline (info)

## Summary of Additional Refinements

15. **Millis Overflow:** Always use subtraction for time comparisons
16. **Soil Diagnostics:** Detect stuck readings and wiring issues
17. **TLS Storage:** Use PROGMEM for CA, store only mode/fingerprint in NVS
18. **Layout Stability:** Fixed-width, right-aligned sensor value fields
19. **Watchdog Progress:** Only feed watchdog in known good progress paths
20. **Graph Ranges:** Sensor-specific minimum ranges (1°C, 5%, 10hPa, 5%)
21. **Timeout Policy:** All blocking I/O ≤ 15 seconds
22. **Alert Priority:** Battery > All Sensors > Queue > Thresholds > WiFi

**New Correctness Property:**

**Property 26: Monotonic Clock Invariant**
*For any* two successive SensorReadings, the second reading's monotonicMs SHALL be greater than or equal to the first reading's monotonicMs when compared using unsigned arithmetic (handles overflow correctly).
**Validates: Time model consistency**
