#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "ConfigManager.h"
#include "SystemStatusManager.h"
#include "TimeManager.h"
#include "models/AveragedData.h"
#include <vector>

// Registration result structure
struct RegistrationResult {
    int statusCode;
    String confirmationId;
    bool shouldRetry;
};

class NetworkManager {
   public:
    NetworkManager(ConfigManager& configMgr, TimeManager& timeMgr, SystemStatusManager& statusMgr);

    void initialize();
    bool connectWiFi();
    bool isConnected();
    void checkConnection();
    bool sendData(const std::vector<AveragedData>& dataList);

    // Public for unit testing
    String formatJsonPayload(const std::vector<AveragedData>& dataList);

    // Registration endpoint derivation
    String getRegistrationEndpoint();

    // Device registration
    RegistrationResult registerDevice(const String& payload);

   private:
    ConfigManager& config;
    TimeManager& timeManager;
    SystemStatusManager& statusManager;

    WiFiClientSecure wifiClient;
    HTTPClient httpClient;

    uint8_t reconnectAttempts;
    unsigned long lastReconnectAttempt;

    bool verifyInternetConnectivity();
    std::vector<String> parseAcknowledgedBatchIds(const String& response);
    unsigned long calculateBackoffDelay(uint8_t attempt);
    bool sendDataWithProtocol(const String& endpoint, const String& payload, bool useHttps);

    // Derive registration endpoint from configured API endpoint
    String deriveEndpoint(const String& dataEndpoint);

    // Parse registration response and extract confirmation_id
    bool parseRegistrationResponse(const String& response, String& confirmationId);
};

#endif  // NETWORK_MANAGER_H
