#include "TouchDetector.h"

#ifdef ARDUINO
#include <SPI.h>
#include <Wire.h>

#include "LoggingMacros.h"
#include "PinConfig.h"
#else
// Mock implementations for testing
#endif

// XPT2046 SPI Commands
#define XPT2046_CMD_X 0xD0   // Read X position
#define XPT2046_CMD_Y 0x90   // Read Y position
#define XPT2046_CMD_Z1 0xB0  // Read Z1 (pressure)

// I2C Addresses
#define FT6236_I2C_ADDR 0x38
#define CST816_I2C_ADDR 0x15
#define GT911_I2C_ADDR_1 0x5D
#define GT911_I2C_ADDR_2 0x14

// Register addresses for ID verification
#define FT6236_REG_CHIPID 0xA3       // Expected: 0x36 or 0x64
#define CST816_REG_CHIPID 0xA7       // Expected: 0xB4
#define GT911_REG_PRODUCT_ID 0x8140  // Expected: "911"

// Timing constants
#define PROBE_TIMEOUT_MS 150
#define STABILITY_DELAY_MS 10
#define TOTAL_TIMEOUT_MS 500

TouchDetector::TouchDetector() {
    lastResult.detected = false;
    lastResult.type = TouchControllerType::NONE;
    lastResult.detectionTimeMs = 0;
}

TouchDetector::~TouchDetector() {
    // Cleanup if needed
}

TouchDetectionResult TouchDetector::detect() {
#ifdef ARDUINO
    uint32_t startTime = millis();
    Serial.printf("[INFO] TouchDetector: Starting detection sequence\n");

    // Priority 1: Probe SPI controllers
    Serial.printf("[INFO] TouchDetector: Probing SPI controllers\n");
    if (probeXPT2046(PROBE_TIMEOUT_MS)) {
        lastResult.detected = true;
        lastResult.type = TouchControllerType::XPT2046;
        lastResult.detectionTimeMs = millis() - startTime;
        Serial.printf("[INFO] TouchDetector: XPT2046 detected in %u ms\n",
                      lastResult.detectionTimeMs);
        return lastResult;
    }

    // Check total timeout before continuing
    if (millis() - startTime >= TOTAL_TIMEOUT_MS) {
        Serial.printf("[WARN] TouchDetector: Total timeout reached after SPI probes (%u ms)\n",
                      millis() - startTime);
        lastResult.detected = false;
        lastResult.type = TouchControllerType::NONE;
        lastResult.detectionTimeMs = millis() - startTime;
        return lastResult;
    }

    // Priority 2: Probe I2C controllers
    Serial.printf("[INFO] TouchDetector: Probing I2C controllers\n");

    if (probeFT6236(PROBE_TIMEOUT_MS)) {
        lastResult.detected = true;
        lastResult.type = TouchControllerType::FT6236;
        lastResult.detectionTimeMs = millis() - startTime;
        Serial.printf("[INFO] TouchDetector: FT6236 detected in %u ms\n",
                      lastResult.detectionTimeMs);
        return lastResult;
    }

    if (millis() - startTime >= TOTAL_TIMEOUT_MS) {
        Serial.printf("[WARN] TouchDetector: Total timeout reached after FT6236 probe (%u ms)\n",
                      millis() - startTime);
        lastResult.detected = false;
        lastResult.type = TouchControllerType::NONE;
        lastResult.detectionTimeMs = millis() - startTime;
        return lastResult;
    }

    if (probeCST816(PROBE_TIMEOUT_MS)) {
        lastResult.detected = true;
        lastResult.type = TouchControllerType::CST816;
        lastResult.detectionTimeMs = millis() - startTime;
        Serial.printf("[INFO] TouchDetector: CST816 detected in %u ms\n",
                      lastResult.detectionTimeMs);
        return lastResult;
    }

    if (millis() - startTime >= TOTAL_TIMEOUT_MS) {
        Serial.printf("[WARN] TouchDetector: Total timeout reached after CST816 probe (%u ms)\n",
                      millis() - startTime);
        lastResult.detected = false;
        lastResult.type = TouchControllerType::NONE;
        lastResult.detectionTimeMs = millis() - startTime;
        return lastResult;
    }

    if (probeGT911(PROBE_TIMEOUT_MS)) {
        lastResult.detected = true;
        lastResult.type = TouchControllerType::GT911;
        lastResult.detectionTimeMs = millis() - startTime;
        Serial.printf("[INFO] TouchDetector: GT911 detected in %u ms\n",
                      lastResult.detectionTimeMs);
        return lastResult;
    }

    // No touch controller detected
    lastResult.detected = false;
    lastResult.type = TouchControllerType::NONE;
    lastResult.detectionTimeMs = millis() - startTime;
    Serial.printf("[INFO] TouchDetector: No touch controller detected (total time: %u ms)\n",
                  lastResult.detectionTimeMs);
#else
    // Mock implementation for testing
    lastResult.detected = false;
    lastResult.type = TouchControllerType::NONE;
    lastResult.detectionTimeMs = 0;
#endif

    return lastResult;
}

TouchDetectionResult TouchDetector::getLastResult() const {
    return lastResult;
}

bool TouchDetector::probeXPT2046(uint32_t timeoutMs) {
#ifdef ARDUINO
    uint32_t startTime = millis();
    Serial.printf("[INFO] TouchDetector: Probing XPT2046 (SPI) with %u ms timeout\n", timeoutMs);

    // Initialize SPI for touch controller
    // Note: TFT display may already have initialized SPI
    // Use same SPI bus but different CS pin
    SPI.begin(TFT_SCLK_PIN, TFT_MISO_PIN, TFT_MOSI_PIN);

    // XPT2046 typically uses a separate CS pin from the display
    // For now, we'll attempt a basic probe
    // In a real implementation, this would need the actual touch CS pin

    // Attempt to read X position as a test
    // If we get a reasonable response, controller is present
    bool detected = false;

    // Try to read a sample
    // XPT2046 should respond with 12-bit values
    // We'll do a simple communication test

    // Check timeout
    uint32_t probeTime = millis() - startTime;
    if (probeTime >= timeoutMs) {
        Serial.printf("[WARN] TouchDetector: XPT2046 probe timeout (%u ms)\n", probeTime);
        return false;
    }

    // For now, return false as we need proper CS pin configuration
    // This will be implemented when hardware is available
    Serial.printf(
        "[INFO] TouchDetector: XPT2046 not detected (requires CS pin configuration) - probe time: "
        "%u ms\n",
        probeTime);
    return false;
#else
    return false;
#endif
}

bool TouchDetector::probeFT6236(uint32_t timeoutMs) {
#ifdef ARDUINO
    uint32_t startTime = millis();
    Serial.printf("[INFO] TouchDetector: Probing FT6236 at 0x%02X with %u ms timeout\n",
                  FT6236_I2C_ADDR, timeoutMs);

    // Initialize I2C if not already done
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);

    // Try to read chip ID register
    Wire.beginTransmission(FT6236_I2C_ADDR);
    Wire.write(FT6236_REG_CHIPID);
    uint8_t error = Wire.endTransmission(false);

    if (error != 0) {
        uint32_t probeTime = millis() - startTime;
        Serial.printf(
            "[INFO] TouchDetector: FT6236 not responding (I2C error: %u) - probe time: %u ms\n",
            error, probeTime);
        return false;
    }

    // Check timeout
    uint32_t probeTime = millis() - startTime;
    if (probeTime >= timeoutMs) {
        Serial.printf("[WARN] TouchDetector: FT6236 probe timeout (%u ms)\n", probeTime);
        return false;
    }

    // Request 1 byte
    uint8_t bytesRead = Wire.requestFrom(FT6236_I2C_ADDR, (uint8_t)1);
    if (bytesRead != 1) {
        probeTime = millis() - startTime;
        Serial.printf("[INFO] TouchDetector: FT6236 failed to read chip ID - probe time: %u ms\n",
                      probeTime);
        return false;
    }

    uint8_t chipId = Wire.read();
    probeTime = millis() - startTime;
    Serial.printf("[INFO] TouchDetector: FT6236 chip ID: 0x%02X - probe time: %u ms\n", chipId,
                  probeTime);

    // FT6236 chip ID should be 0x36 or 0x64
    if (chipId == 0x36 || chipId == 0x64) {
        // Verify stability
        Serial.printf("[INFO] TouchDetector: FT6236 chip ID valid, verifying stability\n");
        if (verifyStability(TouchControllerType::FT6236)) {
            probeTime = millis() - startTime;
            Serial.printf(
                "[INFO] TouchDetector: FT6236 detected and stable - total probe time: %u ms\n",
                probeTime);
            return true;
        } else {
            probeTime = millis() - startTime;
            Serial.printf(
                "[WARN] TouchDetector: FT6236 stability check failed - probe time: %u ms\n",
                probeTime);
        }
    } else {
        Serial.printf("[INFO] TouchDetector: FT6236 chip ID invalid (expected 0x36 or 0x64)\n");
    }

    probeTime = millis() - startTime;
    Serial.printf("[INFO] TouchDetector: FT6236 not detected - total probe time: %u ms\n",
                  probeTime);
    return false;
#else
    return false;
#endif
}

bool TouchDetector::probeCST816(uint32_t timeoutMs) {
#ifdef ARDUINO
    uint32_t startTime = millis();
    Serial.printf("[INFO] TouchDetector: Probing CST816 at 0x%02X with %u ms timeout\n",
                  CST816_I2C_ADDR, timeoutMs);

    // Initialize I2C if not already done
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);

    // Try to read chip ID register
    Wire.beginTransmission(CST816_I2C_ADDR);
    Wire.write(CST816_REG_CHIPID);
    uint8_t error = Wire.endTransmission(false);

    if (error != 0) {
        uint32_t probeTime = millis() - startTime;
        Serial.printf(
            "[INFO] TouchDetector: CST816 not responding (I2C error: %u) - probe time: %u ms\n",
            error, probeTime);
        return false;
    }

    // Check timeout
    uint32_t probeTime = millis() - startTime;
    if (probeTime >= timeoutMs) {
        Serial.printf("[WARN] TouchDetector: CST816 probe timeout (%u ms)\n", probeTime);
        return false;
    }

    // Request 1 byte
    uint8_t bytesRead = Wire.requestFrom(CST816_I2C_ADDR, (uint8_t)1);
    if (bytesRead != 1) {
        probeTime = millis() - startTime;
        Serial.printf("[INFO] TouchDetector: CST816 failed to read chip ID - probe time: %u ms\n",
                      probeTime);
        return false;
    }

    uint8_t chipId = Wire.read();
    probeTime = millis() - startTime;
    Serial.printf("[INFO] TouchDetector: CST816 chip ID: 0x%02X - probe time: %u ms\n", chipId,
                  probeTime);

    // CST816 chip ID should be 0xB4
    if (chipId == 0xB4) {
        // Verify stability
        Serial.printf("[INFO] TouchDetector: CST816 chip ID valid, verifying stability\n");
        if (verifyStability(TouchControllerType::CST816)) {
            probeTime = millis() - startTime;
            Serial.printf(
                "[INFO] TouchDetector: CST816 detected and stable - total probe time: %u ms\n",
                probeTime);
            return true;
        } else {
            probeTime = millis() - startTime;
            Serial.printf(
                "[WARN] TouchDetector: CST816 stability check failed - probe time: %u ms\n",
                probeTime);
        }
    } else {
        Serial.printf("[INFO] TouchDetector: CST816 chip ID invalid (expected 0xB4)\n");
    }

    probeTime = millis() - startTime;
    Serial.printf("[INFO] TouchDetector: CST816 not detected - total probe time: %u ms\n",
                  probeTime);
    return false;
#else
    return false;
#endif
}

bool TouchDetector::probeGT911(uint32_t timeoutMs) {
#ifdef ARDUINO
    uint32_t startTime = millis();
    Serial.printf("[INFO] TouchDetector: Probing GT911 with %u ms timeout\n", timeoutMs);

    // Initialize I2C if not already done
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);

    // GT911 can be at two addresses: 0x5D or 0x14
    // Try both addresses
    uint8_t addresses[] = {GT911_I2C_ADDR_1, GT911_I2C_ADDR_2};

    for (uint8_t addr : addresses) {
        Serial.printf("[INFO] TouchDetector: Trying GT911 at 0x%02X\n", addr);

        // GT911 uses 16-bit register addresses
        Wire.beginTransmission(addr);
        Wire.write((GT911_REG_PRODUCT_ID >> 8) & 0xFF);  // High byte
        Wire.write(GT911_REG_PRODUCT_ID & 0xFF);         // Low byte
        uint8_t error = Wire.endTransmission(false);

        if (error != 0) {
            uint32_t probeTime = millis() - startTime;
            Serial.printf(
                "[INFO] TouchDetector: GT911 not responding at 0x%02X (I2C error: %u) - probe "
                "time: %u ms\n",
                addr, error, probeTime);
            continue;
        }

        // Check timeout
        uint32_t probeTime = millis() - startTime;
        if (probeTime >= timeoutMs) {
            Serial.printf("[WARN] TouchDetector: GT911 probe timeout (%u ms)\n", probeTime);
            return false;
        }

        // Request 4 bytes (product ID is "911" + null terminator)
        uint8_t bytesRead = Wire.requestFrom(addr, (uint8_t)4);
        if (bytesRead != 4) {
            probeTime = millis() - startTime;
            Serial.printf(
                "[INFO] TouchDetector: GT911 failed to read product ID at 0x%02X - probe time: %u "
                "ms\n",
                addr, probeTime);
            continue;
        }

        char productId[5] = {0};
        for (int i = 0; i < 4; i++) {
            productId[i] = Wire.read();
        }
        probeTime = millis() - startTime;
        Serial.printf("[INFO] TouchDetector: GT911 product ID at 0x%02X: %s - probe time: %u ms\n",
                      addr, productId, probeTime);

        // Check if product ID matches "911"
        if (strncmp(productId, "911", 3) == 0) {
            // Verify stability
            Serial.printf("[INFO] TouchDetector: GT911 product ID valid, verifying stability\n");
            if (verifyStability(TouchControllerType::GT911)) {
                probeTime = millis() - startTime;
                Serial.printf(
                    "[INFO] TouchDetector: GT911 detected and stable at 0x%02X - total probe time: "
                    "%u ms\n",
                    addr, probeTime);
                return true;
            } else {
                probeTime = millis() - startTime;
                Serial.printf(
                    "[WARN] TouchDetector: GT911 stability check failed at 0x%02X - probe time: %u "
                    "ms\n",
                    addr, probeTime);
            }
        } else {
            Serial.printf(
                "[INFO] TouchDetector: GT911 product ID invalid at 0x%02X (expected \"911\")\n",
                addr);
        }
    }

    uint32_t probeTime = millis() - startTime;
    Serial.printf(
        "[INFO] TouchDetector: GT911 not detected at any address - total probe time: %u ms\n",
        probeTime);
    return false;
#else
    return false;
#endif
}

bool TouchDetector::verifyStability(TouchControllerType type) {
#ifdef ARDUINO
    Serial.printf("[INFO] TouchDetector: Verifying stability for controller type %d\n", (int)type);

    // Delay between reads
    delay(STABILITY_DELAY_MS);

    // Perform second read based on controller type
    bool secondReadSuccess = false;

    switch (type) {
        case TouchControllerType::FT6236: {
            Wire.beginTransmission(FT6236_I2C_ADDR);
            Wire.write(FT6236_REG_CHIPID);
            uint8_t error = Wire.endTransmission(false);
            if (error == 0) {
                uint8_t bytesRead = Wire.requestFrom(FT6236_I2C_ADDR, (uint8_t)1);
                if (bytesRead == 1) {
                    uint8_t chipId = Wire.read();
                    secondReadSuccess = (chipId == 0x36 || chipId == 0x64);
                    Serial.printf(
                        "[INFO] TouchDetector: FT6236 stability check - chip ID: 0x%02X, success: "
                        "%d\n",
                        chipId, secondReadSuccess);
                } else {
                    Serial.printf(
                        "[WARN] TouchDetector: FT6236 stability check - failed to read chip ID\n");
                }
            } else {
                Serial.printf("[WARN] TouchDetector: FT6236 stability check - I2C error: %u\n",
                              error);
            }
            break;
        }

        case TouchControllerType::CST816: {
            Wire.beginTransmission(CST816_I2C_ADDR);
            Wire.write(CST816_REG_CHIPID);
            uint8_t error = Wire.endTransmission(false);
            if (error == 0) {
                uint8_t bytesRead = Wire.requestFrom(CST816_I2C_ADDR, (uint8_t)1);
                if (bytesRead == 1) {
                    uint8_t chipId = Wire.read();
                    secondReadSuccess = (chipId == 0xB4);
                    Serial.printf(
                        "[INFO] TouchDetector: CST816 stability check - chip ID: 0x%02X, success: "
                        "%d\n",
                        chipId, secondReadSuccess);
                } else {
                    Serial.printf(
                        "[WARN] TouchDetector: CST816 stability check - failed to read chip ID\n");
                }
            } else {
                Serial.printf("[WARN] TouchDetector: CST816 stability check - I2C error: %u\n",
                              error);
            }
            break;
        }

        case TouchControllerType::GT911: {
            // Try both addresses
            uint8_t addresses[] = {GT911_I2C_ADDR_1, GT911_I2C_ADDR_2};
            for (uint8_t addr : addresses) {
                Wire.beginTransmission(addr);
                Wire.write((GT911_REG_PRODUCT_ID >> 8) & 0xFF);
                Wire.write(GT911_REG_PRODUCT_ID & 0xFF);
                uint8_t error = Wire.endTransmission(false);
                if (error == 0) {
                    uint8_t bytesRead = Wire.requestFrom(addr, (uint8_t)4);
                    if (bytesRead == 4) {
                        char productId[5] = {0};
                        for (int i = 0; i < 4; i++) {
                            productId[i] = Wire.read();
                        }
                        if (strncmp(productId, "911", 3) == 0) {
                            secondReadSuccess = true;
                            Serial.printf(
                                "[INFO] TouchDetector: GT911 stability check at 0x%02X - product "
                                "ID: %s, success: %d\n",
                                addr, productId, secondReadSuccess);
                            break;
                        } else {
                            Serial.printf(
                                "[WARN] TouchDetector: GT911 stability check at 0x%02X - invalid "
                                "product ID: %s\n",
                                addr, productId);
                        }
                    } else {
                        Serial.printf(
                            "[WARN] TouchDetector: GT911 stability check at 0x%02X - failed to "
                            "read product ID\n",
                            addr);
                    }
                } else {
                    Serial.printf(
                        "[WARN] TouchDetector: GT911 stability check at 0x%02X - I2C error: %u\n",
                        addr, error);
                }
            }
            break;
        }

        case TouchControllerType::XPT2046:
            // SPI controller stability check
            // For now, assume stable if first read succeeded
            secondReadSuccess = true;
            Serial.printf("[INFO] TouchDetector: XPT2046 stability check - assumed stable\n");
            break;

        default:
            secondReadSuccess = false;
            Serial.printf("[WARN] TouchDetector: Stability check - unknown controller type\n");
            break;
    }

    if (secondReadSuccess) {
        Serial.printf("[INFO] TouchDetector: Stability verified successfully\n");
        return true;
    } else {
        Serial.printf("[WARN] TouchDetector: Stability check failed\n");
        return false;
    }
#else
    return false;
#endif
}
