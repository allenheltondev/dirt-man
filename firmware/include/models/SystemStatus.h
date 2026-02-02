#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include "ErrorCounters.h"
#include "SensorReadings.h"
#include <cstdint>

struct SystemStatus {
    unsigned long uptimeMs;
    uint32_t freeHeap;
    int8_t wifiRssi;
    uint16_t queueDepth;
    uint16_t bootCount;
    unsigned long lastTransmissionMs;  // Timestamp of last successful transmission
    ErrorCounters errors;
    SensorReadings minValues;
    SensorReadings maxValues;
};

#endif
