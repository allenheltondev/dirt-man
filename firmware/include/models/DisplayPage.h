#ifndef DISPLAY_PAGE_H
#define DISPLAY_PAGE_H

#include <cstdint>

enum class DisplayPage : uint8_t {
    SUMMARY,
    GRAPH_BME280_TEMP,
    GRAPH_DS18B20_TEMP,
    GRAPH_HUMIDITY,
    GRAPH_PRESSURE,
    GRAPH_SOIL_MOISTURE,
    SYSTEM_HEALTH
};

#endif
