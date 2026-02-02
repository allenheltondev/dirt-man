#ifndef SENSOR_HEALTH_H
#define SENSOR_HEALTH_H

#include <cstdint>

enum class SensorHealth : uint8_t {
    GREEN,   // Normal - recent valid readings (< 5 seconds old)
    YELLOW,  // Stale - readings 5-30 seconds old
    RED      // Error - no readings or > 30 seconds old
};

#endif
