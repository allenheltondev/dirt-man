#ifndef UNIT_TEST
#include <Arduino.h>

#include <esp_task_wdt.h>

#include "BootId.h"
#include "ConfigManager.h"
#include "DataManager.h"
#include "DisplayManager.h"
#include "ErrorLogger.h"
#include "HardwareId.h"
#include "NetworkManager.h"
#include "PowerManager.h"
#include "SensorManager.h"
#include "StateManager.h"
#include "SystemStatusManager.h"
#include "TimeManager.h"
#include "TouchDetector.h"
#include "Version.h"
#include "models/AveragedData.h"
#include "models/SensorReadings.h"
#include "models/SystemStatus.h"

// Firmware version (auto-generated)
// See scripts/version_increment.py and include/Version.h

// Watchdog timeout (30 seconds)
#define WDT_TIMEOUT 30

// Critical error state flag
bool criticalErrorState = false;

// Global manager instances
ConfigManager configManager;
TimeManager timeManager;
SystemStatusManager systemStatusManager;
DataManager dataManager;
DisplayManager displayManager;
SensorManager sensorManager;
NetworkManager networkManager(configManager, timeManager, systemStatusManager);
PowerManager powerManager;
StateManager stateManager;

// Global Boot ID (generated once in setup())
String g_bootId;

// Registration callback function (to be called from ConfigManager)
void triggerManualRegistration() {
    Serial.println("Manual registration triggered via serial console");
    // TODO: When RegistrationManager is integrated (Task 9), call:
    // registrationManager.registerDevice(hardwareId, g_bootId, friendlyName, firmwareVersion);
    Serial.println("Note: Full registration implementation pending Task 9 integration");
}

// Timing variables
unsigned long lastSensorRead = 0;
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 60000;  // Check WiFi every 60 seconds

// Diagnostic function
void printDiagnostics() {
    Serial.println("\n=== System Diagnostics ===");

    // System uptime
    Serial.print("Uptime: ");
    Serial.print(systemStatusManager.getUptimeMs() / 1000);
    Serial.println(" seconds");

    // Heap memory
    Serial.print("Free Heap: ");
    Serial.print(systemStatusManager.getFreeHeap());
    Serial.println(" bytes");

#ifdef ARDUINO
    Serial.print("Heap Low Watermark: ");
    Serial.print(ESP.getMinFreeHeap());
    Serial.println(" bytes");
#endif

    // WiFi status
    Serial.print("WiFi Status: ");
    if (networkManager.isConnected()) {
        Serial.print("Connected (RSSI: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm)");
    } else {
        Serial.println("Disconnected");
    }

    // Sensor status
    Serial.println("\nSensor Status:");
    Serial.print("  BME280: ");
    Serial.println(sensorManager.isSensorAvailable(SensorType::BME280_TEMP) ? "Available"
                                                                            : "Unavailable");
    Serial.print("  DS18B20: ");
    Serial.println(sensorManager.isSensorAvailable(SensorType::DS18B20_TEMP) ? "Available"
                                                                             : "Unavailable");
    Serial.print("  Soil Moisture: ");
    Serial.println(sensorManager.isSensorAvailable(SensorType::SOIL_MOISTURE) ? "Available"
                                                                              : "Unavailable");

    // Queue depth
    Serial.print("\nTransmission Queue Depth: ");
    Serial.print(dataManager.getBufferedDataCount());
    Serial.println(" readings");

    // Last successful operations
    Serial.print("Last Sensor Read: ");
    unsigned long lastRead = systemStatusManager.getLastSensorReadTime();
    if (lastRead > 0) {
        Serial.print((timeManager.monotonicMs() - lastRead) / 1000);
        Serial.println(" seconds ago");
    } else {
        Serial.println("Never");
    }

    Serial.print("Last Transmission: ");
    unsigned long lastTransmit = systemStatusManager.getLastTransmissionTime();
    if (lastTransmit > 0) {
        Serial.print((timeManager.monotonicMs() - lastTransmit) / 1000);
        Serial.println(" seconds ago");
    } else {
        Serial.println("Never");
    }

    // Error counters
    SystemStatus status = systemStatusManager.getStatus();
    Serial.println("\nError Counters:");
    Serial.print("  Sensor Failures: ");
    Serial.println(status.errors.sensorReadFailures);
    Serial.print("  Network Failures: ");
    Serial.println(status.errors.networkFailures);
    Serial.print("  Buffer Overflows: ");
    Serial.println(status.errors.bufferOverflows);

    // Last error
    const char* lastError = systemStatusManager.getLastError();
    if (lastError && strlen(lastError) > 0) {
        Serial.print("\nLast Error: ");
        Serial.println(lastError);
    }

    Serial.println("==========================\n");
}

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    delay(1000);  // Give serial time to initialize

    Serial.println("\n\n=== ESP32 Sensor Firmware ===");
    Serial.print("Version: ");
    Serial.println(FIRMWARE_VERSION);
    Serial.print("Build: ");
    Serial.print(BUILD_TIMESTAMP);
    Serial.print(" (#");
    Serial.print(BUILD_NUMBER);
    Serial.println(")");
    Serial.println("Initializing...\n");

    // Enable watchdog timer (30 second timeout)
    Serial.println("Enabling watchdog timer...");
    esp_task_wdt_init(WDT_TIMEOUT, true);  // 30 second timeout, panic on timeout
    esp_task_wdt_add(NULL);                // Add current task to watchdog
    ErrorLogger::info(ErrorType::SYSTEM, "Watchdog timer enabled (30s timeout)", "setup");
    Serial.println("Watchdog timer enabled");

    // Initialize ConfigManager first (other components depend on config)
    Serial.println("Initializing ConfigManager...");
    configManager.initialize();

    // Try loading from config file first
    Serial.println("Loading configuration from file...");
    bool configLoaded = false;
    if (configManager.loadFromFile()) {
        Serial.println("Config file loaded successfully");

        // Validate required fields
        if (configManager.hasRequiredFields()) {
            Serial.println("All required fields present");
            configLoaded = true;
        } else {
            Serial.println("ERROR: Required fields missing from config file");
            String missingFields = configManager.getMissingRequiredFields();
            Serial.print("Missing fields: ");
            Serial.println(missingFields);

            // Display error on TFT if available
            if (displayManager.isInitialized()) {
                displayManager.showConfigValidationError(missingFields.c_str());
            }

            // Enter provisioning mode
            Serial.println("Entering provisioning mode...");
            configManager.enterProvisioningMode();
            stateManager.enterProvisioningMode();

            // Show provisioning mode on display
            if (displayManager.isInitialized()) {
                displayManager.showProvisioningMode("Type 'help' for commands");
            }

            ErrorLogger::critical(ErrorType::SYSTEM, "Required config fields missing", "setup");
        }
    } else {
        Serial.println("Config file not found or invalid, trying NVS...");

        // Fall back to NVS
        if (configManager.loadConfig()) {
            Serial.println("Config loaded from NVS");

            // Validate required fields from NVS
            if (configManager.hasRequiredFields()) {
                Serial.println("All required fields present");
                configLoaded = true;
            } else {
                Serial.println("ERROR: Required fields missing from NVS config");
                String missingFields = configManager.getMissingRequiredFields();
                Serial.print("Missing fields: ");
                Serial.println(missingFields);

                // Display error on TFT if available
                if (displayManager.isInitialized()) {
                    displayManager.showConfigValidationError(missingFields.c_str());
                }

                // Enter provisioning mode
                Serial.println("Entering provisioning mode...");
                configManager.enterProvisioningMode();
                stateManager.enterProvisioningMode();

                // Show provisioning mode on display
                if (displayManager.isInitialized()) {
                    displayManager.showProvisioningMode("Type 'help' for commands");
                }

                ErrorLogger::critical(ErrorType::SYSTEM, "Required config fields missing", "setup");
            }
        } else {
            Serial.println("No saved config found, using defaults");
            configManager.setDefaults();

            // Check if defaults satisfy required fields (they shouldn't for required fields)
            if (!configManager.hasRequiredFields()) {
                Serial.println("ERROR: Defaults do not satisfy required fields");
                String missingFields = configManager.getMissingRequiredFields();
                Serial.print("Missing fields: ");
                Serial.println(missingFields);

                // Display error on TFT if available
                if (displayManager.isInitialized()) {
                    displayManager.showConfigValidationError(missingFields.c_str());
                }

                // Enter provisioning mode
                Serial.println("Entering provisioning mode...");
                configManager.enterProvisioningMode();
                stateManager.enterProvisioningMode();

                // Show provisioning mode on display
                if (displayManager.isInitialized()) {
                    displayManager.showProvisioningMode("Type 'help' for commands");
                }

                ErrorLogger::critical(ErrorType::SYSTEM, "Device not configured", "setup");
            }
        }
    }

    if (!configLoaded) {
        Serial.println("WARNING: Device not fully configured, some features may not work");
    }

    // Generate Boot ID once (stored in RAM only, not persisted to NVS)
    g_bootId = BootId::generate();
    Serial.print("Boot ID: ");
    Serial.println(g_bootId);

    // Set up serial console command callbacks
    configManager.setBootIdReference(&g_bootId);
    configManager.setRegistrationCallback(triggerManualRegistration);

    Serial.println("ConfigManager initialized");
    esp_task_wdt_reset();  // Feed watchdog

    // Initialize TimeManager
    Serial.println("Initializing TimeManager...");
    timeManager.initialize();
    Serial.println("TimeManager initialized");
    esp_task_wdt_reset();  // Feed watchdog

    // Initialize SystemStatusManager
    Serial.println("Initializing SystemStatusManager...");
    systemStatusManager.initialize();
    Serial.println("SystemStatusManager initialized");
    esp_task_wdt_reset();  // Feed watchdog

    // Initialize PowerManager
    Serial.println("Initializing PowerManager...");
    powerManager.initialize();
    Config& config = configManager.getConfig();
    powerManager.setPowerManagementEnabled(config.batteryMode);
    if (config.batteryMode) {
        Serial.println("Battery mode enabled - power management active");
        float batteryVoltage = powerManager.readBatteryVoltage();
        Serial.print("Battery voltage: ");
        Serial.print(batteryVoltage, 2);
        Serial.print("V (");
        Serial.print(powerManager.getBatteryPercentage());
        Serial.println("%)");
    }
    Serial.println("PowerManager initialized");
    esp_task_wdt_reset();  // Feed watchdog

    // Initialize StateManager and check for persisted state
    Serial.println("Initializing StateManager...");
    stateManager.initialize();
    Serial.println("StateManager initialized");
    esp_task_wdt_reset();  // Feed watchdog

    // Initialize DisplayManager early to catch SPI/pin issues
    Serial.println("Initializing DisplayManager...");
    if (displayManager.initialize()) {
        Serial.println("DisplayManager initialized");
        displayManager.showStartupScreen(FIRMWARE_VERSION);
        delay(2000);  // Show startup screen for 2 seconds
    } else {
        Serial.println("WARNING: DisplayManager initialization failed");
        Serial.println("Continuing without display...");
        ErrorLogger::warning(ErrorType::DISPLAY, "Display initialization failed", "setup");
    }
    esp_task_wdt_reset();  // Feed watchdog

    // Perform touch detection after display initialization
    Serial.println("Detecting touch controller...");
    TouchDetector touchDetector;
    TouchDetectionResult touchResult = touchDetector.detect();

    if (touchResult.detected) {
        Serial.print("Touch controller detected: ");
        switch (touchResult.type) {
            case TouchControllerType::XPT2046:
                Serial.println("XPT2046 (SPI resistive)");
                break;
            case TouchControllerType::FT6236:
                Serial.println("FT6236 (I2C capacitive)");
                break;
            case TouchControllerType::CST816:
                Serial.println("CST816 (I2C capacitive)");
                break;
            case TouchControllerType::GT911:
                Serial.println("GT911 (I2C capacitive)");
                break;
            default:
                Serial.println("Unknown");
                break;
        }
        Serial.print("Detection time: ");
        Serial.print(touchResult.detectionTimeMs);
        Serial.println(" ms");
        ErrorLogger::info(ErrorType::SYSTEM, "Touch controller detected", "setup");
    } else {
        Serial.println("No touch controller detected");
        Serial.print("Detection time: ");
        Serial.print(touchResult.detectionTimeMs);
        Serial.println(" ms");
        ErrorLogger::info(ErrorType::SYSTEM, "No touch controller detected", "setup");
    }

    // Pass touch detection result to ConfigManager
    configManager.setTouchDetected(touchResult.detected, touchResult.type);

    // Pass touch detection result to DisplayManager
    if (displayManager.isInitialized()) {
        displayManager.setTouchEnabled(touchResult.detected, touchResult.type);
        if (touchResult.detected) {
            Serial.println("Touch-based config page enabled");
        } else {
            Serial.println("Touch-based config page disabled (no touch controller)");
        }
    }

    Serial.println("Touch detection complete");
    esp_task_wdt_reset();  // Feed watchdog

    // Initialize SensorManager with calibration from config
    Serial.println("Initializing SensorManager...");
    sensorManager.initialize();
    Config& config = configManager.getConfig();
    sensorManager.calibrateSoilMoisture(config.soilDryAdc, config.soilWetAdc);

    // Check if all sensors failed to initialize (critical error)
    if (!sensorManager.isSensorAvailable(SensorType::BME280_TEMP) &&
        !sensorManager.isSensorAvailable(SensorType::DS18B20_TEMP) &&
        !sensorManager.isSensorAvailable(SensorType::SOIL_MOISTURE)) {
        ErrorLogger::critical(ErrorType::SENSOR, "All sensors failed to initialize", "setup");
        criticalErrorState = true;

        // Display error on TFT if available
        if (displayManager.isInitialized()) {
            displayManager.showCriticalError("SENSOR ERROR",
                                             "All sensors failed.\nCheck connections.");
        }

        Serial.println("CRITICAL ERROR: All sensors failed to initialize!");
        Serial.println("System will continue attempting to read sensors...");
    } else {
        Serial.println("SensorManager initialized");

        // Log which sensors are available
        if (sensorManager.isSensorAvailable(SensorType::BME280_TEMP)) {
            ErrorLogger::info(ErrorType::SENSOR, "BME280 initialized successfully", "setup");
        } else {
            ErrorLogger::warning(ErrorType::SENSOR, "BME280 not available", "setup");
        }

        if (sensorManager.isSensorAvailable(SensorType::DS18B20_TEMP)) {
            ErrorLogger::info(ErrorType::SENSOR, "DS18B20 initialized successfully", "setup");
        } else {
            ErrorLogger::warning(ErrorType::SENSOR, "DS18B20 not available", "setup");
        }

        if (sensorManager.isSensorAvailable(SensorType::SOIL_MOISTURE)) {
            ErrorLogger::info(ErrorType::SENSOR, "Soil moisture sensor initialized successfully",
                              "setup");
        } else {
            ErrorLogger::warning(ErrorType::SENSOR, "Soil moisture sensor not available", "setup");
        }
    }

    // Initialize DataManager with publish interval from config
    Serial.println("Initializing DataManager...");
    dataManager.setPublishIntervalSamples(config.publishIntervalSamples);

    // Check for persisted state from deep sleep
    if (stateManager.hasPersistedState()) {
        Serial.println("Found persisted state from deep sleep, restoring...");

        // Restore data buffer
        uint16_t dataBufferCount = 0;
        uint16_t displayBufferCount = 0;
        AveragedData tempDataBuffer[50];
        DisplayPoint tempDisplayBuffer[240];

        if (stateManager.restoreState(tempDataBuffer, dataBufferCount, tempDisplayBuffer,
                                      displayBufferCount)) {
            Serial.print("Restored ");
            Serial.print(dataBufferCount);
            Serial.print(" data buffer entries and ");
            Serial.print(displayBufferCount);
            Serial.println(" display buffer entries");

            // TODO: Add methods to DataManager to restore buffers
            // For now, just clear the persisted state
            stateManager.clearPersistedState();
        } else {
            Serial.println("Failed to restore persisted state");
        }
    }

    Serial.println("DataManager initialized");
    esp_task_wdt_reset();  // Feed watchdog

    // Initialize NetworkManager and attempt WiFi connection
    Serial.println("Initializing NetworkManager...");
    networkManager.initialize();
    esp_task_wdt_reset();  // Feed watchdog before WiFi connection attempt

    if (networkManager.connectWiFi()) {
        Serial.println("WiFi connected successfully");
        systemStatusManager.setWiFiRSSI(WiFi.RSSI());
        // Attempt NTP sync when WiFi connects
        timeManager.onWiFiConnected();
    } else {
        Serial.println("WiFi connection failed, will retry later");
        Serial.println("Continuing in offline mode...");
        ErrorLogger::warning(ErrorType::NETWORK, "Initial WiFi connection failed", "setup");
    }
    Serial.println("NetworkManager initialized");
    esp_task_wdt_reset();  // Feed watchdog

    Serial.println("\n=== Initialization Complete ===");
    Serial.print("Reading Interval: ");
    Serial.print(config.readingIntervalMs / 1000);
    Serial.println(" seconds");
    Serial.print("Publish Interval: ");
    Serial.print(config.publishIntervalSamples);
    Serial.println(" samples");
    Serial.println("Starting main loop...\n");
}

void loop() {
    // Feed watchdog at start of loop
    esp_task_wdt_reset();

    unsigned long currentTime = timeManager.monotonicMs();
    Config& config = configManager.getConfig();

    // Update system status every loop iteration
    systemStatusManager.update();

    // Get adaptive reading interval if in battery mode
    uint32_t effectiveReadingInterval = config.readingIntervalMs;
    if (powerManager.isPowerManagementEnabled()) {
        effectiveReadingInterval =
            powerManager.getAdaptiveReadingInterval(config.readingIntervalMs);
    }

    // Read sensors at configured Reading_Interval (or adaptive interval)
    if (currentTime - lastSensorRead >= effectiveReadingInterval) {
        lastSensorRead = currentTime;

        // Read all sensors
        SensorReadings readings = sensorManager.readSensors();

        // Update system status with sensor read time
        systemStatusManager.setLastSensorReadTime(currentTime);
        systemStatusManager.updateMinMax(readings);

        // Add reading to averaging buffer
        dataManager.addReading(readings);

        // Add reading to display buffer (at fixed 1-minute intervals)
        dataManager.addToDisplayBuffer(readings);

        // Print readings to serial console (verbose mode)
        Serial.println("=== Sensor Readings ===");
        Serial.print("Timestamp: ");
        Serial.print(readings.monotonicMs);
        Serial.println(" ms");

        Serial.print("BME280 Temperature: ");
        if (readings.sensorStatus & (1 << 0)) {
            Serial.print(readings.bme280Temp, 2);
            Serial.println(" °C");
        } else {
            Serial.println("N/A");
        }

        Serial.print("DS18B20 Temperature: ");
        if (readings.sensorStatus & (1 << 1)) {
            Serial.print(readings.ds18b20Temp, 2);
            Serial.println(" °C");
        } else {
            Serial.println("N/A");
        }

        Serial.print("Humidity: ");
        if (readings.sensorStatus & (1 << 0)) {
            Serial.print(readings.humidity, 2);
            Serial.println(" %");
        } else {
            Serial.println("N/A");
        }

        Serial.print("Pressure: ");
        if (readings.sensorStatus & (1 << 0)) {
            Serial.print(readings.pressure, 2);
            Serial.println(" hPa");
        } else {
            Serial.println("N/A");
        }

        Serial.print("Soil Moisture: ");
        if (readings.sensorStatus & (1 << 2)) {
            Serial.print(readings.soilMoisture, 1);
            Serial.print(" % (Raw ADC: ");
            Serial.print(readings.soilMoistureRaw);
            Serial.println(")");
        } else {
            Serial.println("N/A");
        }

        Serial.print("Averaging Buffer: ");
        Serial.print(dataManager.getCurrentSampleCount());
        Serial.print(" / ");
        Serial.println(dataManager.getPublishIntervalSamples());

        Serial.print("Free Heap: ");
        Serial.print(systemStatusManager.getFreeHeap());
        Serial.println(" bytes");

        Serial.println("=======================\n");

        // Check if we should publish (Publish_Interval samples reached)
        if (dataManager.shouldPublish()) {
            Serial.println("=== Publishing Averaged Data ===");

            // Calculate averages
            AveragedData avgData = dataManager.calculateAverages();

            // Set timestamps using TimeManager
            avgData.timeSynced = timeManager.timeSynced();
            avgData.sampleStartEpochMs = timeManager.epochMsOrZero();
            avgData.sampleEndEpochMs = timeManager.epochMsOrZero();
            avgData.deviceBootEpochMs = timeManager.deviceBootEpochMs();
            avgData.uptimeMs = timeManager.uptimeMs();

            // Clear averaging buffer
            dataManager.clearAveragingBuffer();

            // Attempt to send data if WiFi is connected
            if (networkManager.isConnected()) {
                Serial.println("WiFi connected, attempting transmission...");

                // Get all buffered data for batch transmission
                uint16_t bufferedCount = 0;
                const AveragedData* bufferedData = dataManager.getBufferedData(bufferedCount);

                // Create vector with current data + buffered data
                std::vector<AveragedData> dataToSend;
                dataToSend.push_back(avgData);
                for (uint16_t i = 0; i < bufferedCount; i++) {
                    dataToSend.push_back(bufferedData[i]);
                }

                Serial.print("Sending ");
                Serial.print(dataToSend.size());
                Serial.println(" reading(s)...");

                // Send data
                if (networkManager.sendData(dataToSend)) {
                    Serial.println("Transmission successful!");
                    systemStatusManager.setLastTransmissionTime(currentTime);

                    // Note: NetworkManager handles clearing acknowledged data from buffer

                    // Check if deep sleep should be triggered after successful upload
                    Config& config = configManager.getConfig();
                    powerManager.checkAndTriggerDeepSleep(
                        config.batteryMode,
                        config.publishIntervalSamples * (config.readingIntervalMs / 1000));
                } else {
                    Serial.println("Transmission failed, buffering data...");
                    dataManager.bufferForTransmission(avgData);
                    systemStatusManager.incrementNetworkFailures();
                }
            } else {
                Serial.println("WiFi not connected, buffering data...");
                dataManager.bufferForTransmission(avgData);
            }

            // Update queue depth in system status
            systemStatusManager.setQueueDepth(dataManager.getBufferedDataCount());

            // Check for buffer overflow warning
            if (dataManager.isBufferNearFull()) {
                Serial.println("WARNING: Transmission buffer > 80% full!");
            }

            Serial.println("================================\n");
        }
    }

    // Check WiFi connection periodically
    if (currentTime - lastWiFiCheck >= WIFI_CHECK_INTERVAL) {
        lastWiFiCheck = currentTime;

        networkManager.checkConnection();

        if (networkManager.isConnected()) {
            systemStatusManager.setWiFiRSSI(WiFi.RSSI());
        } else {
            systemStatusManager.setWiFiRSSI(-100);  // Disconnected indicator
        }
    }

    // Update display in every loop iteration (only if initialized)
    if (displayManager.isInitialized()) {
        // Check if display should be disabled in battery mode
        if (powerManager.isPowerManagementEnabled()) {
            // Enable low power mode for display
            if (!displayManager.isLowPowerMode()) {
                displayManager.setLowPowerMode(true);
                Serial.println("Display low power mode enabled");
            }

            // Optionally disable display completely if battery is very low
            if (powerManager.isBatteryLow()) {
                displayManager.disableDisplay();
            }
        }

        SystemStatus status = systemStatusManager.getStatus();
        SensorReadings currentReadings = sensorManager.readSensors();  // Get latest for display
        displayManager.update(currentReadings, status);
    }

    // Handle serial configuration commands
    configManager.handleSerialConfig();

    // Handle diagnostic command
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        if (command == "diag") {
            printDiagnostics();
        } else if (command == "battery") {
            // Battery status command
            if (powerManager.isPowerManagementEnabled()) {
                Serial.println("\n=== Battery Status ===");
                Serial.print("Voltage: ");
                Serial.print(powerManager.readBatteryVoltage(), 2);
                Serial.println(" V");
                Serial.print("Percentage: ");
                Serial.print(powerManager.getBatteryPercentage());
                Serial.println(" %");
                Serial.print("Status: ");
                if (powerManager.isBatteryLow()) {
                    Serial.println("LOW");
                } else {
                    Serial.println("OK");
                }
                Serial.print("Adaptive Interval: ");
                Serial.print(powerManager.getAdaptiveReadingInterval(config.readingIntervalMs) /
                             1000);
                Serial.println(" seconds");
                Serial.println("======================\n");
            } else {
                Serial.println("Battery mode not enabled");
            }
        }
    }

    // Power management: enter light sleep between sensor readings if in battery mode
    if (powerManager.isPowerManagementEnabled()) {
        // Calculate time until next sensor reading
        unsigned long timeSinceLastRead = currentTime - lastSensorRead;
        uint32_t effectiveInterval =
            powerManager.getAdaptiveReadingInterval(config.readingIntervalMs);

        if (timeSinceLastRead < effectiveInterval) {
            uint32_t sleepDuration = effectiveInterval - timeSinceLastRead;

            // Only sleep if duration is significant (> 1 second)
            if (sleepDuration > 1000) {
                Serial.print("Entering light sleep for ");
                Serial.print(sleepDuration / 1000);
                Serial.println(" seconds...");

                // Enter light sleep (maintains WiFi connection and RAM)
                powerManager.enterLightSleep(sleepDuration);

                // Update time after wakeup
                currentTime = timeManager.monotonicMs();
            }
        }
    } else {
        // Small delay to prevent tight loop when not in battery mode
        delay(10);
    }
}

#endif  // UNIT_TEST
