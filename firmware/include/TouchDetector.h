#ifndef TOUCH_DETECTOR_H
#define TOUCH_DETECTOR_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <cstdint>
#endif

/**
 * TouchControllerType enum defines supported touch controller types.
 * NONE indicates no touch controller detected.
 */
enum class TouchControllerType {
    NONE,     // No touch controller detected
    XPT2046,  // SPI resistive touch controller
    FT6236,   // I2C capacitive touch controller
    CST816,   // I2C capacitive touch controller
    GT911     // I2C capacitive touch controller
};

/**
 * TouchDetectionResult contains the result of touch detection.
 *
 * Invariant: detected == false if and only if type == TouchControllerType::NONE
 */
struct TouchDetectionResult {
    bool detected;             // True if touch controller detected
    TouchControllerType type;  // Type of detected controller
    uint32_t detectionTimeMs;  // Time taken for detection in milliseconds
};

/**
 * TouchDetector probes for touch controllers at boot time.
 *
 * Detection sequence:
 * 1. Probe SPI controllers (XPT2046) - Priority 1
 * 2. Probe I2C controllers (FT6236, CST816, GT911) - Priority 2
 * 3. Verify stability with 2 consecutive successful reads
 * 4. Total timeout: 500ms maximum
 * 5. Per-probe timeout: 150ms maximum
 */
class TouchDetector {
   public:
    TouchDetector();
    ~TouchDetector();

    /**
     * Perform touch controller detection sequence.
     * Blocking operation with maximum 500ms timeout.
     *
     * @return TouchDetectionResult with detection status and timing
     */
    TouchDetectionResult detect();

    /**
     * Get the last detection result.
     *
     * @return TouchDetectionResult from last detect() call
     */
    TouchDetectionResult getLastResult() const;

   private:
    TouchDetectionResult lastResult;

    /**
     * Probe XPT2046 SPI resistive touch controller.
     * Attempts to read controller ID or generate test touch sample.
     *
     * @param timeoutMs Maximum time to spend probing (150ms recommended)
     * @return true if controller detected and stable
     */
    bool probeXPT2046(uint32_t timeoutMs);

    /**
     * Probe FT6236 I2C capacitive touch controller at address 0x38.
     * Reads known ID register to verify presence.
     *
     * @param timeoutMs Maximum time to spend probing (150ms recommended)
     * @return true if controller detected and stable
     */
    bool probeFT6236(uint32_t timeoutMs);

    /**
     * Probe CST816 I2C capacitive touch controller at address 0x15.
     * Reads known ID register to verify presence.
     *
     * @param timeoutMs Maximum time to spend probing (150ms recommended)
     * @return true if controller detected and stable
     */
    bool probeCST816(uint32_t timeoutMs);

    /**
     * Probe GT911 I2C capacitive touch controller at addresses 0x5D/0x14.
     * Reads known ID register to verify presence.
     *
     * @param timeoutMs Maximum time to spend probing (150ms recommended)
     * @return true if controller detected and stable
     */
    bool probeGT911(uint32_t timeoutMs);

    /**
     * Verify detection stability by requiring 2 consecutive successful reads.
     * Adds 10ms delay between reads.
     *
     * @param type Controller type to verify
     * @return true if both reads succeed
     */
    bool verifyStability(TouchControllerType type);
};

#endif  // TOUCH_DETECTOR_H
