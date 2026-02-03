#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <string>
#include <vector>

#ifdef UNIT_TEST
#include "../test/mocks/Arduino.h"
#else
#include <Arduino.h>

#include <TFT_eSPI.h>
#endif

#include "ConfigFileManager.h"
#include "TouchDetector.h"
#include "models/Config.h"
#include "models/DisplayPage.h"
#include "models/DisplayPoint.h"
#include "models/SensorHealth.h"
#include "models/SensorReadings.h"
#include "models/SensorType.h"
#include "models/SystemStatus.h"

// Forward declaration
class DataManager;

class DisplayManager {
   public:
    DisplayManager();
    ~DisplayManager();

    bool initialize();
    void showStartupScreen(const std::string& firmwareVersion);
    void showCriticalError(const std::string& title, const std::string& message);
    void update(const SensorReadings& current, const SystemStatus& status,
                DataManager* dataManager = nullptr, const Config* config = nullptr);
    void cyclePage();
    void checkAndCyclePage(uint16_t intervalMs);
    void setDebugMode(bool enabled);
    void checkBurnInProtection();

    // Display power management
    void setLowPowerMode(bool enabled);
    bool isLowPowerMode() const { return lowPowerMode; }
    void setBacklightBrightness(uint8_t brightness);  // 0-255
    void disableDisplay();
    void enableDisplay();

    // Touch enable/disable
    void setTouchEnabled(bool enabled, TouchControllerType type);

    // Config error display
    void showConfigError(ConfigLoadResult errorType);
    void showConfigValidationError(const std::string& missingFields);

    // Provisioning mode display
    void showProvisioningMode(const std::string& instructions);

    DisplayPage getCurrentPage() const { return currentPage; }
    bool isInitialized() const { return initialized; }

   private:
#ifndef UNIT_TEST
    TFT_eSPI tft;
#endif

    DisplayPage currentPage;
    unsigned long lastPageChange;
    unsigned long lastActivity;
    bool debugMode;
    bool initialized;
    bool lowPowerMode;
    bool displayEnabled;
    uint8_t backlightBrightness;
    uint16_t screenWidth;
    uint16_t screenHeight;
    bool touchEnabled;
    TouchControllerType touchType;

    void renderSummaryPage(const SensorReadings& current, const SystemStatus& status,
                           const Config* config = nullptr);
    void renderGraphPage(SensorType type, const std::vector<DisplayPoint>& data);
    void renderSystemHealthPage(const SystemStatus& status);

    void drawSensorValue(int16_t x, int16_t y, const std::string& label, float value,
                         const std::string& unit, SensorHealth health);
    void drawHealthBadge(int16_t x, int16_t y, SensorHealth health);
    void drawMinMax(int16_t x, int16_t y, float minVal, float maxVal);
    void drawWiFiIndicator(int16_t x, int16_t y, int8_t rssi);
    void drawQueueDepth(int16_t x, int16_t y, uint16_t count);
    void drawTransmissionIndicator(int16_t x, int16_t y, unsigned long lastTransmissionMs);
    void drawLineGraph(const std::vector<DisplayPoint>& data, uint16_t maxPoints);
    void drawAxes(float minVal, float maxVal, const std::string& unit);
    void scrollGraphLeft();
    std::vector<DisplayPoint> downsample(const std::vector<DisplayPoint>& data,
                                         uint16_t targetPoints);

    void drawThresholdIndicator(int16_t x, int16_t y, float value, float lowThreshold,
                                float highThreshold);

    void drawText(int16_t x, int16_t y, const std::string& text, uint16_t color, uint8_t size = 1);
    void drawRightAlignedText(int16_t x, int16_t y, const std::string& text, uint16_t color,
                              uint8_t size = 1);
    void drawCenteredText(int16_t y, const std::string& text, uint16_t color, uint8_t size = 1);

    uint16_t getHealthColor(SensorHealth health);
    uint16_t getRSSIColor(int8_t rssi);

    std::string formatUptime(unsigned long uptimeMs);
    std::string formatFloat(float value, uint8_t decimals);

    // Touch driver initialization (conditional)
    bool initializeTouchDriver();
};

#endif  // DISPLAY_MANAGER_H
