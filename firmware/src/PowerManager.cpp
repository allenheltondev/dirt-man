#ifndef UNIT_TEST
#include "PowerManager.h"

PowerManager::PowerManager() : powerManagementEnabled(false), displayLowPowerMode(false) {}

void PowerManager::initialize() {
#ifdef ARDUINO
    // Configure ADC for battery voltage monitoring
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);  // GPIO35
#endif
}

void PowerManager::enterLightSleep(uint32_t durationMs) {
    if (!powerManagementEnabled) {
        // If power management is disabled, just delay
        delay(durationMs);
        return;
    }

#ifdef ARDUINO
    // Configure timer wakeup
    esp_sleep_enable_timer_wakeup(durationMs * 1000ULL);  // Convert ms to microseconds

    // Enter light sleep mode
    // Light sleep maintains RAM and WiFi connection state
    esp_light_sleep_start();
#else
    // Mock implementation for unit testing
    delay(durationMs);
#endif
}

void PowerManager::enterDeepSleep(uint32_t durationMs) {
    if (!powerManagementEnabled) {
        return;
    }

#ifdef ARDUINO
    // Configure timer wakeup
    esp_sleep_enable_timer_wakeup(durationMs * 1000ULL);  // Convert ms to microseconds

    // Note: Before calling this, the caller should persist critical state to NVS
    // Deep sleep will reset the ESP32 and lose all RAM contents

    // Enter deep sleep mode
    esp_deep_sleep_start();

    // This line will never be reached - ESP32 will reset on wakeup
#endif
}

float PowerManager::readBatteryVoltage() {
#ifdef ARDUINO
    // Read ADC value (12-bit: 0-4095)
    uint16_t adcValue = adc1_get_raw(ADC1_CHANNEL_7);

    // Convert ADC value to voltage
    return calculateVoltageFromADC(adcValue);
#else
    // Mock implementation for unit testing
    return 3.7f;  // Return nominal voltage
#endif
}

bool PowerManager::isBatteryLow() {
    float voltage = readBatteryVoltage();
    return voltage < BATTERY_LOW_THRESHOLD;
}

uint8_t PowerManager::getBatteryPercentage() {
    float voltage = readBatteryVoltage();

    // Clamp voltage to valid range
    if (voltage >= BATTERY_FULL_VOLTAGE) {
        return 100;
    }
    if (voltage <= BATTERY_EMPTY_VOLTAGE) {
        return 0;
    }

    // Linear interpolation between empty and full
    float percentage =
        ((voltage - BATTERY_EMPTY_VOLTAGE) / (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE)) *
        100.0f;

    return static_cast<uint8_t>(percentage);
}

uint32_t PowerManager::getAdaptiveReadingInterval(uint32_t baseIntervalMs) {
    if (!powerManagementEnabled) {
        return baseIntervalMs;
    }

    float voltage = readBatteryVoltage();

    // Adjust interval based on battery level
    if (voltage < BATTERY_CRITICAL_THRESHOLD) {
        // Critical battery: 4x interval
        return baseIntervalMs * INTERVAL_MULTIPLIER_CRITICAL;
    } else if (voltage < BATTERY_LOW_THRESHOLD) {
        // Low battery: 2x interval
        return baseIntervalMs * INTERVAL_MULTIPLIER_LOW;
    } else {
        // Normal battery: 1x interval
        return baseIntervalMs * INTERVAL_MULTIPLIER_NORMAL;
    }
}

void PowerManager::setDisplayLowPowerMode(bool enabled) {
    displayLowPowerMode = enabled;
}

bool PowerManager::isDisplayLowPowerMode() const {
    return displayLowPowerMode;
}

bool PowerManager::isPowerManagementEnabled() const {
    return powerManagementEnabled;
}

void PowerManager::setPowerManagementEnabled(bool enabled) {
    powerManagementEnabled = enabled;
}

float PowerManager::calculateVoltageFromADC(uint16_t adcValue) {
    // ESP32 ADC reference voltage is 3.3V with 12-bit resolution (0-4095)
    // With 11dB attenuation, the measurable range is 0-3.3V
    float voltage = (adcValue / 4095.0f) * 3.3f;

    // Apply voltage divider correction
    // If using a voltage divider (e.g., 2:1), multiply by the ratio
    voltage *= ADC_VOLTAGE_DIVIDER;

    return voltage;
}

void PowerManager::checkAndTriggerDeepSleep(bool enableDeepSleep,
                                            uint32_t dataUploadIntervalSeconds) {
    // Only enter deep sleep if the flag is enabled
    if (!enableDeepSleep) {
        return;
    }

    // Convert data upload interval from seconds to milliseconds
    uint32_t sleepDurationMs = dataUploadIntervalSeconds * 1000;

    // Log deep sleep entry
#ifdef ARDUINO
    Serial.print("Deep sleep enabled. Entering deep sleep for ");
    Serial.print(dataUploadIntervalSeconds);
    Serial.println(" seconds...");
    Serial.flush();  // Ensure message is sent before sleep
#endif

    // Enter deep sleep with wake-up timer configured to data upload interval
    // Note: This will reset the ESP32 on wake-up
    enterDeepSleep(sleepDurationMs);
}

#endif  // UNIT_TEST
