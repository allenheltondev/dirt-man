#include "SystemStatusManager.h"

#include <cstring>

SystemStatusManager::SystemStatusManager()
    : bootTimeMs(0), lastSensorReadMs(0), lastTransmissionMs(0) {
    memset(&status, 0, sizeof(SystemStatus));
    memset(lastErrorStr, 0, sizeof(lastErrorStr));
}

SystemStatusManager::~SystemStatusManager() {}

void SystemStatusManager::initialize() {
    bootTimeMs = millis();
    status.uptimeMs = 0;
    status.freeHeap = 0;
    status.wifiRssi = -127;  // Invalid RSSI (not connected)
    status.queueDepth = 0;
    status.bootCount = 0;  // TODO: Load from NVS in future

    // Initialize error counters
    status.errors.sensorReadFailures = 0;
    status.errors.networkFailures = 0;
    status.errors.bufferOverflows = 0;

    // Initialize min/max values to invalid ranges
    // Min values start at maximum possible
    status.minValues.bme280Temp = 999.0f;
    status.minValues.ds18b20Temp = 999.0f;
    status.minValues.humidity = 999.0f;
    status.minValues.pressure = 9999.0f;
    status.minValues.soilMoisture = 999.0f;
    status.minValues.soilMoistureRaw = 9999;

    // Max values start at minimum possible
    status.maxValues.bme280Temp = -999.0f;
    status.maxValues.ds18b20Temp = -999.0f;
    status.maxValues.humidity = -999.0f;
    status.maxValues.pressure = -9999.0f;
    status.maxValues.soilMoisture = -999.0f;
    status.maxValues.soilMoistureRaw = 0;

    lastSensorReadMs = 0;
    lastTransmissionMs = 0;
    strcpy(lastErrorStr, "");

    // Initial update
    update();
}

void SystemStatusManager::update() {
    updateUptime();
    updateHeapMemory();
}

SystemStatus SystemStatusManager::getStatus() const {
    return status;
}

void SystemStatusManager::setWiFiRSSI(int8_t rssi) {
    status.wifiRssi = rssi;
}

void SystemStatusManager::setQueueDepth(uint16_t depth) {
    status.queueDepth = depth;
}

void SystemStatusManager::incrementSensorFailures() {
    status.errors.sensorReadFailures++;
}

void SystemStatusManager::incrementNetworkFailures() {
    status.errors.networkFailures++;
}

void SystemStatusManager::incrementBufferOverflows() {
    status.errors.bufferOverflows++;
}

void SystemStatusManager::setLastSensorReadTime(unsigned long timestampMs) {
    lastSensorReadMs = timestampMs;
}

void SystemStatusManager::setLastTransmissionTime(unsigned long timestampMs) {
    lastTransmissionMs = timestampMs;
}

void SystemStatusManager::setLastError(const char* error) {
    if (error == nullptr) {
        lastErrorStr[0] = '\0';
        return;
    }

    // Copy error string with bounds checking
    strncpy(lastErrorStr, error, sizeof(lastErrorStr) - 1);
    lastErrorStr[sizeof(lastErrorStr) - 1] = '\0';  // Ensure null termination
}

void SystemStatusManager::updateMinMax(const SensorReadings& readings) {
    // Update BME280 temperature min/max
    if (readings.bme280Temp < status.minValues.bme280Temp && readings.bme280Temp > -100.0f) {
        status.minValues.bme280Temp = readings.bme280Temp;
    }
    if (readings.bme280Temp > status.maxValues.bme280Temp && readings.bme280Temp < 200.0f) {
        status.maxValues.bme280Temp = readings.bme280Temp;
    }

    // Update DS18B20 temperature min/max
    if (readings.ds18b20Temp < status.minValues.ds18b20Temp && readings.ds18b20Temp > -100.0f) {
        status.minValues.ds18b20Temp = readings.ds18b20Temp;
    }
    if (readings.ds18b20Temp > status.maxValues.ds18b20Temp && readings.ds18b20Temp < 200.0f) {
        status.maxValues.ds18b20Temp = readings.ds18b20Temp;
    }

    // Update humidity min/max
    if (readings.humidity < status.minValues.humidity && readings.humidity >= 0.0f) {
        status.minValues.humidity = readings.humidity;
    }
    if (readings.humidity > status.maxValues.humidity && readings.humidity <= 100.0f) {
        status.maxValues.humidity = readings.humidity;
    }

    // Update pressure min/max
    if (readings.pressure < status.minValues.pressure && readings.pressure > 0.0f) {
        status.minValues.pressure = readings.pressure;
    }
    if (readings.pressure > status.maxValues.pressure && readings.pressure < 2000.0f) {
        status.maxValues.pressure = readings.pressure;
    }

    // Update soil moisture min/max
    if (readings.soilMoisture < status.minValues.soilMoisture && readings.soilMoisture >= 0.0f) {
        status.minValues.soilMoisture = readings.soilMoisture;
    }
    if (readings.soilMoisture > status.maxValues.soilMoisture && readings.soilMoisture <= 100.0f) {
        status.maxValues.soilMoisture = readings.soilMoisture;
    }

    // Update soil moisture raw min/max
    if (readings.soilMoistureRaw < status.minValues.soilMoistureRaw) {
        status.minValues.soilMoistureRaw = readings.soilMoistureRaw;
    }
    if (readings.soilMoistureRaw > status.maxValues.soilMoistureRaw) {
        status.maxValues.soilMoistureRaw = readings.soilMoistureRaw;
    }
}

void SystemStatusManager::resetMinMax(const SensorReadings& readings) {
    status.minValues = readings;
    status.maxValues = readings;
}

unsigned long SystemStatusManager::getUptimeMs() const {
    return status.uptimeMs;
}

uint32_t SystemStatusManager::getFreeHeap() const {
    return status.freeHeap;
}

unsigned long SystemStatusManager::getLastSensorReadTime() const {
    return lastSensorReadMs;
}

unsigned long SystemStatusManager::getLastTransmissionTime() const {
    return lastTransmissionMs;
}

const char* SystemStatusManager::getLastError() const {
    return lastErrorStr;
}

// Private helper methods

void SystemStatusManager::updateUptime() {
    unsigned long currentMs = millis();
    status.uptimeMs = currentMs - bootTimeMs;
}

void SystemStatusManager::updateHeapMemory() {
#ifdef ARDUINO
    status.freeHeap = ESP.getFreeHeap();
#else
    // Mock value for native testing
    status.freeHeap = 200000;  // 200 KB
#endif
}
