#ifndef ERROR_COUNTERS_H
#define ERROR_COUNTERS_H

#include <cstdint>

struct ErrorCounters {
    uint16_t sensorReadFailures;
    uint16_t networkFailures;
    uint16_t bufferOverflows;
};

#endif
