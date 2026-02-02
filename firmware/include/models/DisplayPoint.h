#ifndef DISPLAY_POINT_H
#define DISPLAY_POINT_H

#include <stdint.h>

/**
 * @brief Structure representing a single data point for display graphing
 *
 * Used in the Display_Buffer to store historical sensor readings
 * for rendering line graphs on the TFT display.
 */
struct DisplayPoint {
    float value;         // Sensor value (temperature, humidity, etc.)
    uint32_t timestamp;  // Timestamp in milliseconds (monotonic)
};

#endif  // DISPLAY_POINT_H
