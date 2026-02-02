#ifndef SENSOR_READINGS_H
#define SENSOR_READINGS_H

#include <cstdint>

/**
 * SensorReadings structure holds data from all sensors at a single point in time.
 * This structure is used for collecting raw sensor data before averaging.
 */
struct SensorReadings {
    float bme280Temp;          // Celsius - ambient temperature near enclosure
    float ds18b20Temp;         // Celsius - probe temperature measurement
    float humidity;            // Percent - relative humidity
    float pressure;            // hPa - atmospheric pressure
    float soilMoisture;        // Percent (clamped 0-100) - calibrated soil moisture
    uint16_t soilMoistureRaw;  // ADC value (0-4095) - raw soil moisture reading
    uint8_t sensorStatus;      // Bitmask: bit per sensor (1 = available, 0 = unavailable)
    uint32_t monotonicMs;      // millis() - monotonic clock timestamp
};

#endif
