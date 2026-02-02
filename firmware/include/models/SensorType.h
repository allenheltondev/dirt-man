#ifndef SENSOR_TYPE_H
#define SENSOR_TYPE_H

#include <cstdint>

enum class SensorType : uint8_t { BME280_TEMP, DS18B20_TEMP, HUMIDITY, PRESSURE, SOIL_MOISTURE };

constexpr uint8_t NUM_SENSORS = 5;

#endif
