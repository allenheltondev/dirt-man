#include "HardwareId.h"

#ifdef ARDUINO
#include <esp_mac.h>
#endif

#ifndef ARDUINO
#include <cstdio>
#endif

String HardwareId::getHardwareId() {
#ifdef ARDUINO
    uint8_t mac[6];

    // Extract WiFi Station MAC address
    esp_err_t result = esp_read_mac(mac, ESP_MAC_WIFI_STA);

    if (result != ESP_OK) {
        Serial.printf("[ERROR] Failed to read MAC address: %d\n", result);
        return "";
    }

    // Validate MAC is not all zeros
    if (!isValidMac(mac)) {
        Serial.printf("[ERROR] Invalid MAC address: 00:00:00:00:00:00\n");
        return "";
    }

    String formatted = formatMac(mac);
    Serial.printf("[INFO] Hardware ID (MAC): %s\n", formatted.c_str());
    return formatted;
#else
    // For unit testing on host, return a mock MAC
    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    return formatMac(mac);
#endif
}

bool HardwareId::isValidMac(const uint8_t* mac) {
    // Check if MAC is all zeros (00:00:00:00:00:00)
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0) {
            return true;
        }
    }
    return false;
}

String HardwareId::formatMac(const uint8_t* mac) {
    String formatted = "";

    for (int i = 0; i < 6; i++) {
        // Convert byte to uppercase hex
        char hex[3];
        snprintf(hex, sizeof(hex), "%02X", mac[i]);
        formatted += hex;

        // Add colon separator except after last byte
        if (i < 5) {
            formatted += ":";
        }
    }

    return formatted;
}
