#include "DisplayManager.h"

#include "DataManager.h"
#include "PinConfig.h"
#include <iomanip>
#include <sstream>

#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_RED 0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE 0x001F
#define COLOR_YELLOW 0xFFE0
#define COLOR_ORANGE 0xFD20
#define COLOR_CYAN 0x07FF
#define COLOR_MAGENTA 0xF81F
#define COLOR_GRAY 0x8410
#define COLOR_DARK_GRAY 0x4208
#define DISPLAY_ROTATION 1

DisplayManager::DisplayManager()
    : currentPage(DisplayPage::SUMMARY),
      lastPageChange(0),
      lastActivity(0),
      debugMode(false),
      initialized(false),
      lowPowerMode(false),
      displayEnabled(true),
      backlightBrightness(255),
      screenWidth(0),
      screenHeight(0) {}

DisplayManager::~DisplayManager() {}

bool DisplayManager::initialize() {
#ifndef UNIT_TEST
    tft.init();
    tft.setRotation(DISPLAY_ROTATION);
    screenWidth = tft.width();
    screenHeight = tft.height();
    tft.fillScreen(COLOR_BLACK);
    tft.setTextColor(COLOR_WHITE);
    tft.setTextSize(1);
    initialized = true;
    lastActivity = millis();
    return true;
#else
    screenWidth = 320;
    screenHeight = 240;
    initialized = true;
    lastActivity = 0;
    return true;
#endif
}

void DisplayManager::showStartupScreen(const std::string& firmwareVersion) {
    if (!initialized) {
        return;
    }
#ifndef UNIT_TEST
    tft.fillScreen(COLOR_BLACK);
    drawCenteredText(screenHeight / 2 - 40, "ESP32 Sensor Firmware", COLOR_CYAN, 2);
    drawCenteredText(screenHeight / 2 - 10, firmwareVersion, COLOR_WHITE, 2);
    drawCenteredText(screenHeight / 2 + 20, "Initializing...", COLOR_YELLOW, 1);
    for (int i = 0; i < 3; i++) {
        tft.fillCircle(screenWidth / 2 - 20 + (i * 20), screenHeight / 2 + 50, 3, COLOR_GREEN);
    }
#endif
    lastActivity = millis();
}

void DisplayManager::showCriticalError(const std::string& title, const std::string& message) {
    if (!initialized) {
        return;
    }
#ifndef UNIT_TEST
    tft.fillScreen(COLOR_BLACK);

    // Draw red border
    tft.drawRect(5, 5, screenWidth - 10, screenHeight - 10, COLOR_RED);
    tft.drawRect(6, 6, screenWidth - 12, screenHeight - 12, COLOR_RED);

    // Draw error icon (red X)
    int16_t centerX = screenWidth / 2;
    int16_t iconY = 40;
    tft.fillCircle(centerX, iconY, 20, COLOR_RED);
    tft.drawLine(centerX - 10, iconY - 10, centerX + 10, iconY + 10, COLOR_WHITE);
    tft.drawLine(centerX - 10, iconY + 10, centerX + 10, iconY - 10, COLOR_WHITE);

    // Draw title
    drawCenteredText(iconY + 40, title, COLOR_RED, 2);

    // Draw message (handle multi-line)
    int16_t messageY = iconY + 70;
    std::string line;
    std::istringstream stream(message);
    while (std::getline(stream, line, '\n')) {
        drawCenteredText(messageY, line, COLOR_WHITE, 1);
        messageY += 15;
    }

    // Draw instruction
    drawCenteredText(screenHeight - 30, "System will continue...", COLOR_YELLOW, 1);
#endif
    lastActivity = millis();
}

void DisplayManager::update(const SensorReadings& current, const SystemStatus& status,
                            DataManager* dataManager, const Config* config) {
    if (!initialized) {
        return;
    }
    switch (currentPage) {
        case DisplayPage::SUMMARY:
            renderSummaryPage(current, status, config);
            break;
        case DisplayPage::GRAPH_BME280_TEMP:
            if (dataManager) {
                uint16_t count = 0;
                const DisplayPoint* data =
                    dataManager->getDisplayData(SensorType::BME280_TEMP, count, 120);
                std::vector<DisplayPoint> dataVec(data, data + count);
                renderGraphPage(SensorType::BME280_TEMP, dataVec);
            }
            break;
        case DisplayPage::GRAPH_DS18B20_TEMP:
            if (dataManager) {
                uint16_t count = 0;
                const DisplayPoint* data =
                    dataManager->getDisplayData(SensorType::DS18B20_TEMP, count, 120);
                std::vector<DisplayPoint> dataVec(data, data + count);
                renderGraphPage(SensorType::DS18B20_TEMP, dataVec);
            }
            break;
        case DisplayPage::GRAPH_HUMIDITY:
            if (dataManager) {
                uint16_t count = 0;
                const DisplayPoint* data =
                    dataManager->getDisplayData(SensorType::HUMIDITY, count, 120);
                std::vector<DisplayPoint> dataVec(data, data + count);
                renderGraphPage(SensorType::HUMIDITY, dataVec);
            }
            break;
        case DisplayPage::GRAPH_PRESSURE:
            if (dataManager) {
                uint16_t count = 0;
                const DisplayPoint* data =
                    dataManager->getDisplayData(SensorType::PRESSURE, count, 120);
                std::vector<DisplayPoint> dataVec(data, data + count);
                renderGraphPage(SensorType::PRESSURE, dataVec);
            }
            break;
        case DisplayPage::GRAPH_SOIL_MOISTURE:
            if (dataManager) {
                uint16_t count = 0;
                const DisplayPoint* data =
                    dataManager->getDisplayData(SensorType::SOIL_MOISTURE, count, 120);
                std::vector<DisplayPoint> dataVec(data, data + count);
                renderGraphPage(SensorType::SOIL_MOISTURE, dataVec);
            }
            break;
        case DisplayPage::SYSTEM_HEALTH:
            if (debugMode) {
                renderSystemHealthPage(status);
            }
            break;
    }
    lastActivity = millis();
}

void DisplayManager::cyclePage() {
    switch (currentPage) {
        case DisplayPage::SUMMARY:
            currentPage = DisplayPage::GRAPH_BME280_TEMP;
            break;
        case DisplayPage::GRAPH_BME280_TEMP:
            currentPage = DisplayPage::GRAPH_DS18B20_TEMP;
            break;
        case DisplayPage::GRAPH_DS18B20_TEMP:
            currentPage = DisplayPage::GRAPH_HUMIDITY;
            break;
        case DisplayPage::GRAPH_HUMIDITY:
            currentPage = DisplayPage::GRAPH_PRESSURE;
            break;
        case DisplayPage::GRAPH_PRESSURE:
            currentPage = DisplayPage::GRAPH_SOIL_MOISTURE;
            break;
        case DisplayPage::GRAPH_SOIL_MOISTURE:
            if (debugMode) {
                currentPage = DisplayPage::SYSTEM_HEALTH;
            } else {
                currentPage = DisplayPage::SUMMARY;
            }
            break;
        case DisplayPage::SYSTEM_HEALTH:
            currentPage = DisplayPage::SUMMARY;
            break;
    }
    lastPageChange = millis();
}

void DisplayManager::checkAndCyclePage(uint16_t intervalMs) {
    if (!initialized) {
        return;
    }

    unsigned long now = millis();
    if (now - lastPageChange >= intervalMs) {
        cyclePage();
    }
}

void DisplayManager::setDebugMode(bool enabled) {
    debugMode = enabled;
}

void DisplayManager::checkBurnInProtection() {
#ifndef UNIT_TEST
    unsigned long now = millis();
    const unsigned long BURN_IN_TIMEOUT = 30 * 60 * 1000;  // 30 minutes
    const uint8_t DIM_BRIGHTNESS = 64;                     // 25% brightness (0-255 scale)
    const uint8_t FULL_BRIGHTNESS = 255;                   // 100% brightness

    if (now - lastActivity > BURN_IN_TIMEOUT) {
        // Dim the backlight to prevent burn-in
        analogWrite(TFT_BL_PIN, DIM_BRIGHTNESS);
    } else {
        // Keep backlight at full brightness
        analogWrite(TFT_BL_PIN, FULL_BRIGHTNESS);
    }
#endif
}

void DisplayManager::renderSummaryPage(const SensorReadings& current, const SystemStatus& status,
                                       const Config* config) {
#ifndef UNIT_TEST
    tft.fillScreen(COLOR_BLACK);

    // Header
    drawText(5, 5, "Sensor Summary", COLOR_CYAN, 2);

    // WiFi and Queue indicators in top right
    drawWiFiIndicator(screenWidth - 40, 5, status.wifiRssi);
    drawQueueDepth(screenWidth - 40, 25, status.queueDepth);
    drawTransmissionIndicator(screenWidth - 40, 45, status.lastTransmissionMs);

    int16_t yPos = 35;
    int16_t lineHeight = 28;
    const int16_t labelX = 10;
    const int16_t valueX = screenWidth - 80;

    // BME280 Temperature
    SensorHealth bme280Health =
        (current.sensorStatus & (1 << static_cast<uint8_t>(SensorType::BME280_TEMP)))
            ? SensorHealth::GREEN
            : SensorHealth::RED;
    drawText(labelX, yPos, "BME280 Temp:", COLOR_WHITE, 1);
    drawRightAlignedText(valueX, yPos, formatFloat(current.bme280Temp, 1) + " C", COLOR_WHITE, 1);
    drawHealthBadge(screenWidth - 20, yPos, bme280Health);
    yPos += 12;
    drawMinMax(labelX + 10, yPos, status.minValues.bme280Temp, status.maxValues.bme280Temp);
    yPos += lineHeight - 12;

    // DS18B20 Temperature
    SensorHealth ds18b20Health =
        (current.sensorStatus & (1 << static_cast<uint8_t>(SensorType::DS18B20_TEMP)))
            ? SensorHealth::GREEN
            : SensorHealth::RED;
    drawText(labelX, yPos, "DS18B20 Temp:", COLOR_WHITE, 1);
    drawRightAlignedText(valueX, yPos, formatFloat(current.ds18b20Temp, 1) + " C", COLOR_WHITE, 1);
    drawHealthBadge(screenWidth - 20, yPos, ds18b20Health);
    yPos += 12;
    drawMinMax(labelX + 10, yPos, status.minValues.ds18b20Temp, status.maxValues.ds18b20Temp);
    yPos += lineHeight - 12;

    // Humidity
    SensorHealth humidityHealth =
        (current.sensorStatus & (1 << static_cast<uint8_t>(SensorType::HUMIDITY)))
            ? SensorHealth::GREEN
            : SensorHealth::RED;
    drawText(labelX, yPos, "Humidity:", COLOR_WHITE, 1);
    drawRightAlignedText(valueX, yPos, formatFloat(current.humidity, 1) + " %", COLOR_WHITE, 1);
    drawHealthBadge(screenWidth - 20, yPos, humidityHealth);
    yPos += 12;
    drawMinMax(labelX + 10, yPos, status.minValues.humidity, status.maxValues.humidity);
    yPos += lineHeight - 12;

    // Pressure
    SensorHealth pressureHealth =
        (current.sensorStatus & (1 << static_cast<uint8_t>(SensorType::PRESSURE)))
            ? SensorHealth::GREEN
            : SensorHealth::RED;
    drawText(labelX, yPos, "Pressure:", COLOR_WHITE, 1);
    drawRightAlignedText(valueX, yPos, formatFloat(current.pressure, 1) + " hPa", COLOR_WHITE, 1);
    drawHealthBadge(screenWidth - 20, yPos, pressureHealth);
    yPos += 12;
    drawMinMax(labelX + 10, yPos, status.minValues.pressure, status.maxValues.pressure);
    yPos += lineHeight - 12;

    // Soil Moisture
    SensorHealth soilHealth =
        (current.sensorStatus & (1 << static_cast<uint8_t>(SensorType::SOIL_MOISTURE)))
            ? SensorHealth::GREEN
            : SensorHealth::RED;
    drawText(labelX, yPos, "Soil Moisture:", COLOR_WHITE, 1);
    drawRightAlignedText(valueX, yPos, formatFloat(current.soilMoisture, 1) + " %", COLOR_WHITE, 1);
    drawHealthBadge(screenWidth - 20, yPos, soilHealth);
    yPos += 12;
    drawMinMax(labelX + 10, yPos, status.minValues.soilMoisture, status.maxValues.soilMoisture);

    // Draw threshold indicator if config is provided
    if (config) {
        yPos += 12;
        drawThresholdIndicator(labelX + 10, yPos, current.soilMoisture,
                               static_cast<float>(config->soilMoistureThresholdLow),
                               static_cast<float>(config->soilMoistureThresholdHigh));
    }

    // Footer with system info
    yPos = screenHeight - 40;
    drawText(5, yPos, "Uptime: " + formatUptime(status.uptimeMs), COLOR_GRAY, 1);
    yPos += 12;
    drawText(5, yPos, "Heap: " + std::to_string(status.freeHeap) + " bytes", COLOR_GRAY, 1);
    yPos += 12;

    // Time since last sensor update
    unsigned long timeSinceUpdate = (millis() - current.monotonicMs) / 1000;
    std::string updateText = "Last update: " + std::to_string(timeSinceUpdate) + "s ago";
    drawText(5, yPos, updateText, COLOR_GRAY, 1);
#else
    (void)current;
    (void)status;
#endif
}

void DisplayManager::renderSystemHealthPage(const SystemStatus& status) {
#ifndef UNIT_TEST
    tft.fillScreen(COLOR_BLACK);
    drawText(5, 5, "System Health", COLOR_CYAN, 2);
    int16_t yPos = 35;
    int16_t lineHeight = 15;
    drawText(10, yPos, "Uptime: " + formatUptime(status.uptimeMs), COLOR_WHITE, 1);
    yPos += lineHeight;
    drawText(10, yPos, "Free Heap: " + std::to_string(status.freeHeap) + " bytes", COLOR_WHITE, 1);
    yPos += lineHeight;
    drawText(10, yPos, "WiFi RSSI: " + std::to_string(status.wifiRssi) + " dBm", COLOR_WHITE, 1);
    yPos += lineHeight;
    drawText(10, yPos, "Queue Depth: " + std::to_string(status.queueDepth), COLOR_WHITE, 1);
    yPos += lineHeight;
    drawText(10, yPos, "Boot Count: " + std::to_string(status.bootCount), COLOR_WHITE, 1);
    yPos += lineHeight + 5;
    drawText(10, yPos, "Error Counters:", COLOR_YELLOW, 1);
    yPos += lineHeight;
    drawText(15, yPos, "Sensor: " + std::to_string(status.errors.sensorReadFailures), COLOR_WHITE,
             1);
    yPos += lineHeight;
    drawText(15, yPos, "Network: " + std::to_string(status.errors.networkFailures), COLOR_WHITE, 1);
    yPos += lineHeight;
    drawText(15, yPos, "Buffer: " + std::to_string(status.errors.bufferOverflows), COLOR_WHITE, 1);
#else
    (void)status;
#endif
}

void DisplayManager::renderGraphPage(SensorType type, const std::vector<DisplayPoint>& data) {
#ifndef UNIT_TEST
    tft.fillScreen(COLOR_BLACK);

    // Draw title based on sensor type
    std::string title;
    std::string unit;
    switch (type) {
        case SensorType::BME280_TEMP:
            title = "BME280 Temperature";
            unit = "C";
            break;
        case SensorType::DS18B20_TEMP:
            title = "DS18B20 Temperature";
            unit = "C";
            break;
        case SensorType::HUMIDITY:
            title = "Humidity";
            unit = "%";
            break;
        case SensorType::PRESSURE:
            title = "Pressure";
            unit = "hPa";
            break;
        case SensorType::SOIL_MOISTURE:
            title = "Soil Moisture";
            unit = "%";
            break;
        default:
            title = "Unknown Sensor";
            unit = "";
            break;
    }

    drawText(5, 5, title, COLOR_CYAN, 2);

    if (data.empty()) {
        drawCenteredText(screenHeight / 2, "No data available", COLOR_GRAY, 1);
        return;
    }

    // Downsample if needed
    std::vector<DisplayPoint> plotData = data;
    if (data.size() > 120) {
        plotData = downsample(data, 120);
    }

    // Find min/max for scaling
    float minVal = plotData[0].value;
    float maxVal = plotData[0].value;
    for (const auto& point : plotData) {
        if (point.value < minVal)
            minVal = point.value;
        if (point.value > maxVal)
            maxVal = point.value;
    }

    // Add some padding to min/max
    float range = maxVal - minVal;
    if (range < 0.1f)
        range = 0.1f;  // Avoid division by zero
    minVal -= range * 0.1f;
    maxVal += range * 0.1f;

    // Draw axes
    drawAxes(minVal, maxVal, unit);

    // Draw line graph
    drawLineGraph(plotData, 120);
#else
    (void)type;
    (void)data;
#endif
}

void DisplayManager::drawSensorValue(int16_t x, int16_t y, const std::string& label, float value,
                                     const std::string& unit, SensorHealth health) {
#ifndef UNIT_TEST
    drawText(x, y, label, COLOR_WHITE, 1);
    std::string valueStr = formatFloat(value, 1) + " " + unit;
    drawRightAlignedText(screenWidth - 50, y, valueStr, COLOR_WHITE, 1);
    drawHealthBadge(screenWidth - 35, y, health);
#else
    (void)x;
    (void)y;
    (void)label;
    (void)value;
    (void)unit;
    (void)health;
#endif
}

void DisplayManager::drawHealthBadge(int16_t x, int16_t y, SensorHealth health) {
#ifndef UNIT_TEST
    uint16_t color = getHealthColor(health);
    tft.fillCircle(x, y + 4, 4, color);
#else
    (void)x;
    (void)y;
    (void)health;
#endif
}

void DisplayManager::drawMinMax(int16_t x, int16_t y, float minVal, float maxVal) {
#ifndef UNIT_TEST
    std::string text = "Min: " + formatFloat(minVal, 1) + " Max: " + formatFloat(maxVal, 1);
    drawText(x, y, text, COLOR_GRAY, 1);
#else
    (void)x;
    (void)y;
    (void)minVal;
    (void)maxVal;
#endif
}

void DisplayManager::drawWiFiIndicator(int16_t x, int16_t y, int8_t rssi) {
#ifndef UNIT_TEST
    uint16_t color = getRSSIColor(rssi);
    for (int i = 0; i < 3; i++) {
        int barHeight = (i + 1) * 3;
        if (rssi > -70 || i == 0) {
            tft.fillRect(x + (i * 4), y + (9 - barHeight), 3, barHeight, color);
        } else {
            tft.drawRect(x + (i * 4), y + (9 - barHeight), 3, barHeight, COLOR_GRAY);
        }
    }
#else
    (void)x;
    (void)y;
    (void)rssi;
#endif
}

void DisplayManager::drawQueueDepth(int16_t x, int16_t y, uint16_t count) {
#ifndef UNIT_TEST
    std::string text = "Q:" + std::to_string(count);
    drawText(x - 10, y, text, count > 0 ? COLOR_YELLOW : COLOR_GREEN, 1);
#else
    (void)x;
    (void)y;
    (void)count;
#endif
}

void DisplayManager::drawTransmissionIndicator(int16_t x, int16_t y,
                                               unsigned long lastTransmissionMs) {
#ifndef UNIT_TEST
    unsigned long now = millis();
    const unsigned long FLASH_DURATION = 2000;  // Show indicator for 2 seconds after transmission

    if (lastTransmissionMs > 0 && (now - lastTransmissionMs) < FLASH_DURATION) {
        // Draw a green checkmark or success indicator
        tft.fillCircle(x, y + 4, 5, COLOR_GREEN);
        tft.drawLine(x - 2, y + 4, x, y + 6, COLOR_BLACK);
        tft.drawLine(x, y + 6, x + 3, y + 2, COLOR_BLACK);
    } else if (lastTransmissionMs == 0) {
        // Never transmitted - show gray indicator
        tft.fillCircle(x, y + 4, 5, COLOR_GRAY);
    }
#else
    (void)x;
    (void)y;
    (void)lastTransmissionMs;
#endif
}

void DisplayManager::drawThresholdIndicator(int16_t x, int16_t y, float value, float lowThreshold,
                                            float highThreshold) {
#ifndef UNIT_TEST
    // Only draw if thresholds are configured (non-zero)
    if (lowThreshold == 0 && highThreshold == 0) {
        return;
    }

    std::string indicator;
    uint16_t color;

    if (highThreshold > 0 && value >= highThreshold) {
        indicator = "HIGH";
        color = COLOR_RED;
    } else if (lowThreshold > 0 && value <= lowThreshold) {
        indicator = "LOW";
        color = COLOR_YELLOW;
    } else {
        indicator = "OK";
        color = COLOR_GREEN;
    }

    drawText(x, y, indicator, color, 1);
#else
    (void)x;
    (void)y;
    (void)value;
    (void)lowThreshold;
    (void)highThreshold;
#endif
}

void DisplayManager::drawLineGraph(const std::vector<DisplayPoint>& data, uint16_t maxPoints) {
#ifndef UNIT_TEST
    if (data.empty())
        return;

    // Define graph area
    const int16_t GRAPH_X_START = 40;
    const int16_t GRAPH_X_END = screenWidth - 10;
    const int16_t GRAPH_Y_START = 40;
    const int16_t GRAPH_Y_END = screenHeight - 30;

    // Find min/max for scaling
    float minVal = data[0].value;
    float maxVal = data[0].value;
    for (const auto& point : data) {
        if (point.value < minVal)
            minVal = point.value;
        if (point.value > maxVal)
            maxVal = point.value;
    }

    // Add padding
    float range = maxVal - minVal;
    if (range < 0.1f)
        range = 0.1f;
    minVal -= range * 0.1f;
    maxVal += range * 0.1f;

    // Draw line graph
    for (size_t i = 1; i < data.size() && i < maxPoints; i++) {
        // Map data points to screen coordinates
        int16_t x1 = GRAPH_X_START + ((i - 1) * (GRAPH_X_END - GRAPH_X_START)) / (data.size() - 1);
        int16_t y1 = GRAPH_Y_END - ((data[i - 1].value - minVal) * (GRAPH_Y_END - GRAPH_Y_START)) /
                                       (maxVal - minVal);

        int16_t x2 = GRAPH_X_START + (i * (GRAPH_X_END - GRAPH_X_START)) / (data.size() - 1);
        int16_t y2 = GRAPH_Y_END -
                     ((data[i].value - minVal) * (GRAPH_Y_END - GRAPH_Y_START)) / (maxVal - minVal);

        // Clamp to graph area
        if (y1 < GRAPH_Y_START)
            y1 = GRAPH_Y_START;
        if (y1 > GRAPH_Y_END)
            y1 = GRAPH_Y_END;
        if (y2 < GRAPH_Y_START)
            y2 = GRAPH_Y_START;
        if (y2 > GRAPH_Y_END)
            y2 = GRAPH_Y_END;

        tft.drawLine(x1, y1, x2, y2, COLOR_CYAN);
    }
#else
    (void)data;
    (void)maxPoints;
#endif
}

void DisplayManager::drawAxes(float minVal, float maxVal, const std::string& unit) {
#ifndef UNIT_TEST
    const int16_t GRAPH_X_START = 40;
    const int16_t GRAPH_X_END = screenWidth - 10;
    const int16_t GRAPH_Y_START = 40;
    const int16_t GRAPH_Y_END = screenHeight - 30;

    // Draw Y axis
    tft.drawLine(GRAPH_X_START, GRAPH_Y_START, GRAPH_X_START, GRAPH_Y_END, COLOR_WHITE);

    // Draw X axis
    tft.drawLine(GRAPH_X_START, GRAPH_Y_END, GRAPH_X_END, GRAPH_Y_END, COLOR_WHITE);

    // Draw min label
    std::string minLabel = formatFloat(minVal, 1);
    drawText(5, GRAPH_Y_END - 4, minLabel, COLOR_GRAY, 1);

    // Draw max label
    std::string maxLabel = formatFloat(maxVal, 1);
    drawText(5, GRAPH_Y_START - 4, maxLabel, COLOR_GRAY, 1);

    // Draw unit label
    if (!unit.empty()) {
        drawText(5, GRAPH_Y_START + ((GRAPH_Y_END - GRAPH_Y_START) / 2), unit, COLOR_GRAY, 1);
    }

    // Draw grid lines (optional)
    for (int i = 1; i < 4; i++) {
        int16_t y = GRAPH_Y_START + (i * (GRAPH_Y_END - GRAPH_Y_START) / 4);
        for (int16_t x = GRAPH_X_START; x < GRAPH_X_END; x += 4) {
            tft.drawPixel(x, y, COLOR_DARK_GRAY);
        }
    }
#else
    (void)minVal;
    (void)maxVal;
    (void)unit;
#endif
}

void DisplayManager::drawText(int16_t x, int16_t y, const std::string& text, uint16_t color,
                              uint8_t size) {
#ifndef UNIT_TEST
    tft.setTextColor(color);
    tft.setTextSize(size);
    tft.setCursor(x, y);
    tft.print(text.c_str());
#else
    (void)x;
    (void)y;
    (void)text;
    (void)color;
    (void)size;
#endif
}

void DisplayManager::drawRightAlignedText(int16_t x, int16_t y, const std::string& text,
                                          uint16_t color, uint8_t size) {
#ifndef UNIT_TEST
    tft.setTextColor(color);
    tft.setTextSize(size);
    int16_t textWidth = text.length() * 6 * size;
    tft.setCursor(x - textWidth, y);
    tft.print(text.c_str());
#else
    (void)x;
    (void)y;
    (void)text;
    (void)color;
    (void)size;
#endif
}

void DisplayManager::drawCenteredText(int16_t y, const std::string& text, uint16_t color,
                                      uint8_t size) {
#ifndef UNIT_TEST
    tft.setTextColor(color);
    tft.setTextSize(size);
    int16_t textWidth = text.length() * 6 * size;
    int16_t x = (screenWidth - textWidth) / 2;
    tft.setCursor(x, y);
    tft.print(text.c_str());
#else
    (void)y;
    (void)text;
    (void)color;
    (void)size;
#endif
}

uint16_t DisplayManager::getHealthColor(SensorHealth health) {
    switch (health) {
        case SensorHealth::GREEN:
            return COLOR_GREEN;
        case SensorHealth::YELLOW:
            return COLOR_YELLOW;
        case SensorHealth::RED:
            return COLOR_RED;
        default:
            return COLOR_GRAY;
    }
}

uint16_t DisplayManager::getRSSIColor(int8_t rssi) {
    if (rssi > -50) {
        return COLOR_GREEN;
    } else if (rssi > -70) {
        return COLOR_YELLOW;
    } else {
        return COLOR_RED;
    }
}

std::string DisplayManager::formatUptime(unsigned long uptimeMs) {
    unsigned long seconds = uptimeMs / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;
    if (days > 0) {
        return std::to_string(days) + "d " + std::to_string(hours % 24) + "h";
    } else if (hours > 0) {
        return std::to_string(hours) + "h " + std::to_string(minutes % 60) + "m";
    } else {
        return std::to_string(minutes) + "m " + std::to_string(seconds % 60) + "s";
    }
}

std::string DisplayManager::formatFloat(float value, uint8_t decimals) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << value;
    return oss.str();
}

std::vector<DisplayPoint> DisplayManager::downsample(const std::vector<DisplayPoint>& data,
                                                     uint16_t targetPoints) {
    if (data.size() <= targetPoints) {
        return data;
    }

    std::vector<DisplayPoint> result;
    result.reserve(targetPoints);

    // Always include first point
    result.push_back(data[0]);

    // Calculate step size
    float step = static_cast<float>(data.size() - 1) / (targetPoints - 1);

    // Sample intermediate points
    for (uint16_t i = 1; i < targetPoints - 1; i++) {
        size_t index = static_cast<size_t>(i * step);
        if (index < data.size()) {
            result.push_back(data[index]);
        }
    }

    // Always include last point
    result.push_back(data[data.size() - 1]);

    return result;
}

void DisplayManager::scrollGraphLeft() {
#ifndef UNIT_TEST
    // This method would be used for smooth scrolling animation
    // by shifting the graph area left by one pixel
    // For now, we'll implement full redraw in renderGraphPage
    // Future optimization: use sprite or partial screen updates
#endif
}

void DisplayManager::setLowPowerMode(bool enabled) {
    lowPowerMode = enabled;

    if (lowPowerMode) {
        // In low power mode, reduce backlight and update frequency
        setBacklightBrightness(64);  // 25% brightness
    } else {
        // Normal mode, full brightness
        setBacklightBrightness(255);  // 100% brightness
    }
}

void DisplayManager::setBacklightBrightness(uint8_t brightness) {
    backlightBrightness = brightness;

#ifndef UNIT_TEST
// Set backlight brightness using PWM
// Note: TFT_eSPI may have built-in backlight control
// This is a generic implementation
#ifdef TFT_BL
    ledcSetup(0, 5000, 8);  // Channel 0, 5kHz, 8-bit resolution
    ledcAttachPin(TFT_BL, 0);
    ledcWrite(0, brightness);
#endif
#endif
}

void DisplayManager::disableDisplay() {
    displayEnabled = false;

#ifndef UNIT_TEST
    // Turn off backlight
    setBacklightBrightness(0);

    // Clear screen to save power
    tft.fillScreen(COLOR_BLACK);

// Put display in sleep mode if supported
#ifdef TFT_BL
    digitalWrite(TFT_BL, LOW);
#endif
#endif
}

void DisplayManager::enableDisplay() {
    displayEnabled = true;

#ifndef UNIT_TEST
// Wake up display
#ifdef TFT_BL
    digitalWrite(TFT_BL, HIGH);
#endif

    // Restore backlight brightness
    if (lowPowerMode) {
        setBacklightBrightness(64);
    } else {
        setBacklightBrightness(255);
    }
#endif
}
