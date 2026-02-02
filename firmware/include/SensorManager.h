#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#ifdef UNIT_TEST
#include "../test/mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

#include "models/SensorReadings.h"
#include "models/SensorType.h"

// Forward declarations for sensor libraries
#ifdef UNIT_TEST
// Mock mode - no actual sensor libraries
class Adafruit_BME280 {};
class DallasTemperature {};
class OneWire {};
#else
// Real hardware mode
#include <Adafruit_BME280.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#endif

/**
 * SensorManager handles initialization and reading from all sensors:
 * - BME280: Temperature, humidity, pressure via I2C
 * - DS18B20: Temperature via OneWire
 * - Soil Moisture: Analog reading via ADC with calibration
 */
class SensorManager {
   public:
    SensorManager();
    ~SensorManager();

    /**
     * Initialize all sensors with retry logic.
     * Attempts to initialize each sensor up to 3 times.
     * Sets sensor status bits for available sensors.
     */
    void initialize();

    /**
     * Read all sensors and return readings structure.
     * Applies calibration to soil moisture.
     * Validates readings against physical ranges.
     * Sets sensor status bits for successful reads.
     * @return SensorReadings structure with all sensor data
     */
    SensorReadings readSensors();

    /**
     * Check if a specific sensor is available.
     * @param type The sensor type to check
     * @return true if sensor is available and responding
     */
    bool isSensorAvailable(SensorType type);

    /**
     * Set calibration values for soil moisture sensor.
     * @param dryAdc ADC value when soil is dry (0% moisture)
     * @param wetAdc ADC value when soil is wet (100% moisture)
     */
    void calibrateSoilMoisture(uint16_t dryAdc, uint16_t wetAdc);

   private:
    // Sensor library instances
    Adafruit_BME280 bme280;
    OneWire* oneWire;
    DallasTemperature* ds18b20;

    // Calibration values for soil moisture
    uint16_t soilDryAdc;
    uint16_t soilWetAdc;

    // Sensor status tracking (bitmask: 1 = available, 0 = unavailable)
    uint8_t sensorStatus;

    // Private helper methods for BME280
    float readBME280Temperature();
    float readBME280Humidity();
    float readBME280Pressure();

    // Private helper methods for DS18B20
    float readDS18B20Temperature();

    // Private helper methods for soil moisture
    float readSoilMoisture();
    uint16_t readSoilMoistureRaw();
    float convertSoilMoistureToPercent(uint16_t rawAdc);

    // Validation helper
    bool validateReading(SensorType type, float value);
};

#endif  // SENSOR_MANAGER_H
