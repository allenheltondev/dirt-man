#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

#ifdef ARDUINO
#include <WString.h>
#else
#include <string>
using String = std::string;
#endif

struct Config {
    String wifiSsid;
    String wifiPassword;
    String apiEndpoint;
    String apiToken;
    String deviceId;
    uint32_t readingIntervalMs;
    uint16_t publishIntervalSamples;
    uint16_t pageCycleIntervalMs;
    uint16_t soilDryAdc;
    uint16_t soilWetAdc;
    bool temperatureInFahrenheit;
    uint16_t soilMoistureThresholdLow;
    uint16_t soilMoistureThresholdHigh;
    bool batteryMode;

    // TLS/HTTPS configuration
    bool tlsValidateServer;  // Enable certificate validation (default: true)
    bool allowHttpFallback;  // Allow HTTP fallback if HTTPS fails (default: false)
};

#endif
