#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <cstdint>

#ifdef ARDUINO
#include <Arduino.h>

#include <driver/adc.h>
#include <esp_sleep.h>
#else
// Mock for unit testing
#define ADC1_CHANNEL_0 0
#define ADC_ATTEN_DB_11 0
#endif

class PowerManager {
   public:
    PowerManager();

    void initialize();

    // Light sleep mode - sleep between sensor readings
    void enterLightSleep(uint32_t durationMs);

    // Deep sleep mode - sleep with state persistence
    void enterDeepSleep(uint32_t durationMs);

    // Battery voltage monitoring
    float readBatteryVoltage();
    bool isBatteryLow();
    uint8_t getBatteryPercentage();

    // Adaptive interval adjustment based on battery level
    uint32_t getAdaptiveReadingInterval(uint32_t baseIntervalMs);

    // Display power management
    void setDisplayLowPowerMode(bool enabled);
    bool isDisplayLowPowerMode() const;

    // Check if power management is enabled
    bool isPowerManagementEnabled() const;
    void setPowerManagementEnabled(bool enabled);

   private:
    bool powerManagementEnabled;
    bool displayLowPowerMode;

    // Battery monitoring
    static constexpr float BATTERY_LOW_THRESHOLD = 3.3f;       // Volts
    static constexpr float BATTERY_CRITICAL_THRESHOLD = 3.0f;  // Volts
    static constexpr float BATTERY_FULL_VOLTAGE = 4.2f;        // Volts (Li-ion)
    static constexpr float BATTERY_EMPTY_VOLTAGE = 3.0f;       // Volts

    // ADC configuration for battery monitoring
    static constexpr int BATTERY_ADC_PIN = 35;          // GPIO35 (ADC1_CH7)
    static constexpr float ADC_VOLTAGE_DIVIDER = 2.0f;  // Voltage divider ratio

    // Adaptive interval multipliers based on battery level
    static constexpr float INTERVAL_MULTIPLIER_NORMAL = 1.0f;
    static constexpr float INTERVAL_MULTIPLIER_LOW = 2.0f;
    static constexpr float INTERVAL_MULTIPLIER_CRITICAL = 4.0f;

    float calculateVoltageFromADC(uint16_t adcValue);
};

#endif  // POWER_MANAGER_H
