#include "NetworkManager.h"

#include "BootId.h"
#include "models/SensorType.h"

#ifndef UNIT_TEST
#include <esp_task_wdt.h>
#endif

NetworkManager::NetworkManager(ConfigManager& configMgr, TimeManager& timeMgr,
                               SystemStatusManager& statusMgr)
    : config(configMgr),
      timeManager(timeMgr),
      statusManager(statusMgr),
      reconnectAttempts(0),
      lastReconnectAttempt(0) {}

void NetworkManager::initialize() {
    // WiFi initialization will be done in connectWiFi()
    Serial.println("[NetworkManager] Initialized");
}

bool NetworkManager::connectWiFi() {
    Config cfg = config.getConfig();

    // Check if already connected
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[NetworkManager] Already connected to WiFi");
        return true;
    }

    // Validate WiFi credentials
    if (strlen(cfg.wifiSsid) == 0) {
        Serial.println("[NetworkManager] ERROR: WiFi SSID not configured");
        return false;
    }

    Serial.print("[NetworkManager] Connecting to WiFi: ");
    Serial.println(cfg.wifiSsid);

    // Set WiFi mode to station
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifiSsid, cfg.wifiPassword);

    // Retry with exponential backoff (up to 5 attempts)
    const uint8_t MAX_ATTEMPTS = 5;
    reconnectAttempts = 0;

    while (reconnectAttempts < MAX_ATTEMPTS) {
        // Feed watchdog before connection attempt
#ifndef UNIT_TEST
        esp_task_wdt_reset();
#endif

        // Wait for connection with timeout
        unsigned long startAttempt = millis();
        const unsigned long CONNECT_TIMEOUT = 10000;  // 10 seconds per attempt

        while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < CONNECT_TIMEOUT) {
            delay(100);
            // Feed watchdog during connection wait
#ifndef UNIT_TEST
            if ((millis() - startAttempt) % 1000 == 0) {
                esp_task_wdt_reset();
            }
#endif
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println();
            Serial.print("[NetworkManager] Connected! IP: ");
            Serial.println(WiFi.localIP());
            Serial.print("[NetworkManager] RSSI: ");
            Serial.print(WiFi.RSSI());
            Serial.println(" dBm");

            // Update system status with WiFi RSSI
            statusManager.setWiFiRSSI(WiFi.RSSI());

            // Trigger NTP sync when WiFi connects
            timeManager.onWiFiConnected();

            reconnectAttempts = 0;
            return true;
        }

        // Connection failed, increment attempt counter
        reconnectAttempts++;
        Serial.println();
        Serial.print("[NetworkManager] Connection attempt ");
        Serial.print(reconnectAttempts);
        Serial.print(" failed. Status: ");
        Serial.println(WiFi.status());

        if (reconnectAttempts < MAX_ATTEMPTS) {
            unsigned long backoffDelay = calculateBackoffDelay(reconnectAttempts - 1);
            Serial.print("[NetworkManager] Retrying in ");
            Serial.print(backoffDelay / 1000);
            Serial.println(" seconds...");

            lastReconnectAttempt = millis();

            // Feed watchdog during backoff delay
            unsigned long backoffStart = millis();
            while (millis() - backoffStart < backoffDelay) {
                delay(100);
#ifndef UNIT_TEST
                if ((millis() - backoffStart) % 1000 == 0) {
                    esp_task_wdt_reset();
                }
#endif
            }
        }
    }

    Serial.println("[NetworkManager] Failed to connect after maximum attempts");
    statusManager.incrementNetworkFailures();
    return false;
}

bool NetworkManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

void NetworkManager::checkConnection() {
    // Check if WiFi is still connected
    if (WiFi.status() == WL_CONNECTED) {
        // Update RSSI in system status
        statusManager.setWiFiRSSI(WiFi.RSSI());
        return;
    }

    // WiFi disconnected - attempt reconnection with backoff
    unsigned long now = millis();
    unsigned long backoffDelay = calculateBackoffDelay(reconnectAttempts);

    // Check if enough time has passed since last reconnect attempt
    if (lastReconnectAttempt > 0 && (now - lastReconnectAttempt) < backoffDelay) {
        return;  // Still in backoff period
    }

    Serial.println("[NetworkManager] WiFi disconnected, attempting reconnection...");
    lastReconnectAttempt = now;

    // Try to reconnect (single attempt, not full retry loop)
    Config cfg = config.getConfig();
    WiFi.begin(cfg.wifiSsid, cfg.wifiPassword);

    // Wait briefly for connection
    unsigned long startAttempt = millis();
    const unsigned long CONNECT_TIMEOUT = 10000;  // 10 seconds

    while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < CONNECT_TIMEOUT) {
        delay(100);
        // Feed watchdog during connection wait
#ifndef UNIT_TEST
        if ((millis() - startAttempt) % 1000 == 0) {
            esp_task_wdt_reset();
        }
#endif
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[NetworkManager] Reconnected successfully");
        Serial.print("[NetworkManager] IP: ");
        Serial.println(WiFi.localIP());

        // Update system status
        statusManager.setWiFiRSSI(WiFi.RSSI());

        // Trigger NTP sync
        timeManager.onWiFiConnected();

        // Reset reconnect attempts on success
        reconnectAttempts = 0;
    } else {
        reconnectAttempts++;
        Serial.print("[NetworkManager] Reconnection failed. Attempt ");
        Serial.println(reconnectAttempts);

        // Cap reconnect attempts to prevent overflow
        if (reconnectAttempts > 10) {
            reconnectAttempts = 5;  // Reset to moderate backoff
        }

        statusManager.incrementNetworkFailures();
    }
}

bool NetworkManager::sendData(const std::vector<AveragedData>& dataList) {
    if (dataList.empty()) {
        Serial.println("[NetworkManager] No data to send");
        return true;  // Not an error, just nothing to do
    }

    if (!isConnected()) {
        Serial.println("[NetworkManager] Cannot send data: WiFi not connected");
        statusManager.incrementNetworkFailures();
        return false;
    }

    Config cfg = config.getConfig();

    // Validate API endpoint
    if (strlen(cfg.apiEndpoint) == 0) {
        Serial.println("[NetworkManager] ERROR: API endpoint not configured");
        statusManager.incrementNetworkFailures();
        return false;
    }

    Serial.print("[NetworkManager] Sending ");
    Serial.print(dataList.size());
    Serial.println(" reading(s) to API...");

    // Format JSON payload
    String payload = formatJsonPayload(dataList);

    Serial.print("[NetworkManager] Payload size: ");
    Serial.print(payload.length());
    Serial.println(" bytes");

    // Determine if endpoint is HTTPS
    bool useHttps = String(cfg.apiEndpoint).startsWith("https://");

    if (useHttps) {
        Serial.println("[NetworkManager] Using HTTPS with TLS");
    } else {
        Serial.println("[NetworkManager] Using plain HTTP");
    }

    // Retry with exponential backoff (up to 5 attempts)
    const uint8_t MAX_ATTEMPTS = 5;
    uint8_t attempt = 0;
    bool success = false;

    while (attempt < MAX_ATTEMPTS && !success) {
        // Feed watchdog before HTTP attempt
#ifndef UNIT_TEST
        esp_task_wdt_reset();
#endif

        if (attempt > 0) {
            unsigned long backoffDelay = calculateBackoffDelay(attempt - 1);
            Serial.print("[NetworkManager] Retrying in ");
            Serial.print(backoffDelay / 1000);
            Serial.println(" seconds...");

            // Feed watchdog during backoff delay
            unsigned long backoffStart = millis();
            while (millis() - backoffStart < backoffDelay) {
                delay(100);
#ifndef UNIT_TEST
                if ((millis() - backoffStart) % 1000 == 0) {
                    esp_task_wdt_reset();
                }
#endif
            }
        }

        Serial.print("[NetworkManager] Attempt ");
        Serial.print(attempt + 1);
        Serial.print("/");
        Serial.println(MAX_ATTEMPTS);

        // Initialize HTTP client with appropriate transport
        if (useHttps) {
            // Configure WiFiClientSecure for HTTPS
            if (cfg.tlsValidateServer) {
                // Use certificate validation with root CA bundle
                // ESP32 Arduino core includes a root CA bundle
                Serial.println("[NetworkManager] TLS: Certificate validation enabled");
                wifiClient.setCACert(NULL);  // Use built-in root CA bundle
            } else {
                // Skip certificate validation (insecure, for development only)
                Serial.println("[NetworkManager] TLS: Certificate validation DISABLED (insecure)");
                wifiClient.setInsecure();
            }

            httpClient.begin(wifiClient, cfg.apiEndpoint);
        } else {
            // Use plain HTTP
            httpClient.begin(cfg.apiEndpoint);
        }

        // Set headers
        httpClient.addHeader("Content-Type", "application/json");

        // Add API token if configured
        if (strlen(cfg.apiToken) > 0) {
            httpClient.addHeader("Authorization", "Bearer " + String(cfg.apiToken));
        }

        // Set timeout
        httpClient.setTimeout(10000);  // 10 seconds

        // Send POST request
        int httpCode = httpClient.POST(payload);

        // Check response
        if (httpCode > 0) {
            Serial.print("[NetworkManager] HTTP Response code: ");
            Serial.println(httpCode);

            if (httpCode == 200 || httpCode == 201 || httpCode == 204) {
                // Success
                String response = httpClient.getString();
                Serial.println("[NetworkManager] Transmission successful");

                // Parse acknowledged batch IDs from response
                std::vector<String> acknowledgedIds = parseAcknowledgedBatchIds(response);

                if (!acknowledgedIds.empty()) {
                    Serial.print("[NetworkManager] Server acknowledged ");
                    Serial.print(acknowledgedIds.size());
                    Serial.println(" batch(es)");
                }

                // Update system status
                statusManager.setLastTransmitTime(timeManager.monotonicMs());

                success = true;
            } else if (httpCode >= 400 && httpCode < 500) {
                // Client error (4xx) - don't retry, log and fail
                Serial.print("[NetworkManager] Client error (4xx): ");
                Serial.println(httpCode);
                String response = httpClient.getString();
                Serial.print("[NetworkManager] Response: ");
                Serial.println(response);

                statusManager.incrementNetworkFailures();
                statusManager.setLastError("HTTP " + String(httpCode));

                httpClient.end();
                return false;  // Don't retry client errors
            } else if (httpCode >= 500) {
                // Server error (5xx) - retry with backoff
                Serial.print("[NetworkManager] Server error (5xx): ");
                Serial.println(httpCode);

                statusManager.incrementNetworkFailures();
                statusManager.setLastError("HTTP " + String(httpCode));

                attempt++;
            } else {
                // Other HTTP codes - retry
                Serial.print("[NetworkManager] Unexpected HTTP code: ");
                Serial.println(httpCode);

                statusManager.incrementNetworkFailures();
                attempt++;
            }
        } else {
            // HTTP request failed
            String errorMsg = httpClient.errorToString(httpCode);
            Serial.print("[NetworkManager] HTTP request failed: ");
            Serial.println(errorMsg);

            // Check for TLS-specific errors
            if (useHttps) {
                if (httpCode == HTTPC_ERROR_CONNECTION_FAILED) {
                    Serial.println("[NetworkManager] TLS: Connection failed - possible causes:");
                    Serial.println("  - Certificate validation failed");
                    Serial.println("  - Server unreachable");
                    Serial.println("  - TLS handshake timeout");

                    if (cfg.tlsValidateServer) {
                        Serial.println(
                            "[NetworkManager] TLS: Try setting tlsValidateServer=false for "
                            "testing");
                    }

                    statusManager.setLastError("TLS connection failed");
                } else if (httpCode == HTTPC_ERROR_READ_TIMEOUT) {
                    Serial.println("[NetworkManager] TLS: Read timeout during handshake");
                    statusManager.setLastError("TLS timeout");
                } else {
                    statusManager.setLastError("TLS error: " + errorMsg);
                }
            } else {
                statusManager.setLastError("HTTP error: " + errorMsg);
            }

            statusManager.incrementNetworkFailures();
            attempt++;
        }

        httpClient.end();
    }

    if (!success) {
        // If HTTPS failed and HTTP fallback is allowed, try once with HTTP
        if (useHttps && cfg.allowHttpFallback) {
            Serial.println("[NetworkManager] HTTPS failed all attempts");
            Serial.println(
                "[NetworkManager] Attempting HTTP fallback (INSECURE - development only)");

            // Convert https:// to http://
            String httpEndpoint = cfg.apiEndpoint;
            httpEndpoint.replace("https://", "http://");

            // Try HTTP once
            httpClient.begin(httpEndpoint);
            httpClient.addHeader("Content-Type", "application/json");

            if (strlen(cfg.apiToken) > 0) {
                httpClient.addHeader("Authorization", "Bearer " + String(cfg.apiToken));
            }

            httpClient.setTimeout(10000);
            int httpCode = httpClient.POST(payload);

            if (httpCode == 200 || httpCode == 201 || httpCode == 204) {
                Serial.println("[NetworkManager] HTTP fallback successful");
                String response = httpClient.getString();

                std::vector<String> acknowledgedIds = parseAcknowledgedBatchIds(response);
                if (!acknowledgedIds.empty()) {
                    Serial.print("[NetworkManager] Server acknowledged ");
                    Serial.print(acknowledgedIds.size());
                    Serial.println(" batch(es)");
                }

                statusManager.setLastTransmitTime(timeManager.monotonicMs());
                httpClient.end();
                return true;
            } else {
                Serial.print("[NetworkManager] HTTP fallback also failed: ");
                Serial.println(httpCode);
                httpClient.end();
            }
        }

        Serial.println("[NetworkManager] Failed to send data after maximum attempts");
        return false;
    }

    return true;
}

bool NetworkManager::verifyInternetConnectivity() {
    // Optional verification - WiFi connected is sufficient
    // This method can be used for additional verification if needed

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    Serial.println("[NetworkManager] Verifying internet connectivity...");

    HTTPClient http;
    http.begin("http://clients3.google.com/generate_204");  // Google connectivity check endpoint
    http.setTimeout(5000);                                  // 5 second timeout

    int httpCode = http.GET();
    http.end();

    if (httpCode == 204 || httpCode == 200) {
        Serial.println("[NetworkManager] Internet connectivity verified");
        return true;
    } else {
        Serial.print("[NetworkManager] Internet connectivity check failed. HTTP code: ");
        Serial.println(httpCode);
        return false;
    }
}

String NetworkManager::formatJsonPayload(const std::vector<AveragedData>& dataList) {
    if (dataList.empty()) {
        return "{}";
    }

    Config cfg = config.getConfig();
    SystemStatus status = statusManager.getStatus();

    String json = "{";
    json += "\"device_id\":\"" + String(cfg.deviceId) + "\",";
    json += "\"readings\":[";

    for (size_t i = 0; i < dataList.size(); i++) {
        const AveragedData& data = dataList[i];

        if (i > 0)
            json += ",";
        json += "{";

        // Batch ID
        json += "\"batch_id\":\"" + String(data.batchId) + "\",";

        // Device ID
        json += "\"device_id\":\"" + String(cfg.deviceId) + "\",";

        // Epoch timestamps (nullable when not synced)
        if (data.timeSynced && data.sampleStartEpochMs > 0) {
            json +=
                "\"sample_start_epoch_ms\":" + String((unsigned long long)data.sampleStartEpochMs) +
                ",";
            json += "\"sample_end_epoch_ms\":" + String((unsigned long long)data.sampleEndEpochMs) +
                    ",";
            json +=
                "\"device_boot_epoch_ms\":" + String((unsigned long long)data.deviceBootEpochMs) +
                ",";
        } else {
            json += "\"sample_start_epoch_ms\":0,";
            json += "\"sample_end_epoch_ms\":0,";
            json += "\"device_boot_epoch_ms\":0,";
        }

        // Uptime timestamps (always present)
        json += "\"sample_start_uptime_ms\":" + String(data.sampleStartUptimeMs) + ",";
        json += "\"sample_end_uptime_ms\":" + String(data.sampleEndUptimeMs) + ",";
        json += "\"uptime_ms\":" + String(data.uptimeMs) + ",";

        // Metadata
        json += "\"sample_count\":" + String(data.sampleCount) + ",";
        json += "\"time_synced\":" + String(data.timeSynced ? "true" : "false") + ",";

        // Sensor readings (null if sensor unavailable)
        json += "\"sensors\":{";

        // BME280 temperature
        if (data.sensorStatus & (1 << static_cast<uint8_t>(SensorType::BME280_TEMP))) {
            json += "\"bme280_temp_c\":" + String(data.avgBme280Temp, 2);
        } else {
            json += "\"bme280_temp_c\":null";
        }
        json += ",";

        // DS18B20 temperature
        if (data.sensorStatus & (1 << static_cast<uint8_t>(SensorType::DS18B20_TEMP))) {
            json += "\"ds18b20_temp_c\":" + String(data.avgDs18b20Temp, 2);
        } else {
            json += "\"ds18b20_temp_c\":null";
        }
        json += ",";

        // Humidity
        if (data.sensorStatus & (1 << static_cast<uint8_t>(SensorType::HUMIDITY))) {
            json += "\"humidity_pct\":" + String(data.avgHumidity, 2);
        } else {
            json += "\"humidity_pct\":null";
        }
        json += ",";

        // Pressure
        if (data.sensorStatus & (1 << static_cast<uint8_t>(SensorType::PRESSURE))) {
            json += "\"pressure_hpa\":" + String(data.avgPressure, 2);
        } else {
            json += "\"pressure_hpa\":null";
        }
        json += ",";

        // Soil moisture
        if (data.sensorStatus & (1 << static_cast<uint8_t>(SensorType::SOIL_MOISTURE))) {
            json += "\"soil_moisture_pct\":" + String(data.avgSoilMoisture, 2);
        } else {
            json += "\"soil_moisture_pct\":null";
        }

        json += "},";  // End sensors

        // Sensor status flags
        json += "\"sensor_status\":{";
        json += "\"bme280\":\"" +
                String((data.sensorStatus & (1 << static_cast<uint8_t>(SensorType::BME280_TEMP)))
                           ? "ok"
                           : "unavailable") +
                "\",";
        json += "\"ds18b20\":\"" +
                String((data.sensorStatus & (1 << static_cast<uint8_t>(SensorType::DS18B20_TEMP)))
                           ? "ok"
                           : "unavailable") +
                "\",";
        json += "\"soil_moisture\":\"" +
                String((data.sensorStatus & (1 << static_cast<uint8_t>(SensorType::SOIL_MOISTURE)))
                           ? "ok"
                           : "unavailable") +
                "\"";
        json += "},";  // End sensor_status

        // Health metrics (only include in last reading to avoid duplication)
        if (i == dataList.size() - 1) {
            json += "\"health\":{";
            json += "\"uptime_ms\":" + String(status.uptimeMs) + ",";
            json += "\"free_heap_bytes\":" + String(status.freeHeap) + ",";
            json += "\"wifi_rssi_dbm\":" + String(status.wifiRssi) + ",";
            json += "\"error_counters\":{";
            json += "\"sensor_read_failures\":" + String(status.errors.sensorReadFailures) + ",";
            json += "\"network_failures\":" + String(status.errors.networkFailures) + ",";
            json += "\"buffer_overflows\":" + String(status.errors.bufferOverflows);
            json += "}";  // End error_counters
            json += "}";  // End health
        } else {
            // For non-last readings, include minimal health info
            json += "\"health\":{";
            json += "\"uptime_ms\":" + String(data.uptimeMs);
            json += "}";
        }

        json += "}";  // End reading
    }

    json += "]";  // End readings
    json += "}";  // End root

    return json;
}

std::vector<String> NetworkManager::parseAcknowledgedBatchIds(const String& response) {
    std::vector<String> batchIds;

    if (response.length() == 0) {
        return batchIds;
    }

    // Look for "acknowledged_batch_ids" array in JSON response
    // Simple parsing without full JSON library to save memory
    int arrayStart = response.indexOf("\"acknowledged_batch_ids\"");
    if (arrayStart == -1) {
        // No acknowledged_batch_ids field - assume all sent data was acknowledged
        Serial.println("[NetworkManager] No acknowledged_batch_ids in response, assuming success");
        return batchIds;
    }

    // Find the opening bracket of the array
    int bracketStart = response.indexOf('[', arrayStart);
    if (bracketStart == -1) {
        Serial.println("[NetworkManager] Malformed acknowledged_batch_ids array");
        return batchIds;
    }

    // Find the closing bracket of the array
    int bracketEnd = response.indexOf(']', bracketStart);
    if (bracketEnd == -1) {
        Serial.println(
            "[NetworkManager] Malformed acknowledged_batch_ids array (no closing bracket)");
        return batchIds;
    }

    // Extract the array content
    String arrayContent = response.substring(bracketStart + 1, bracketEnd);
    arrayContent.trim();

    if (arrayContent.length() == 0) {
        // Empty array
        return batchIds;
    }

    // Parse individual batch IDs (quoted strings separated by commas)
    int pos = 0;
    while (pos < arrayContent.length()) {
        // Find opening quote
        int quoteStart = arrayContent.indexOf('"', pos);
        if (quoteStart == -1)
            break;

        // Find closing quote
        int quoteEnd = arrayContent.indexOf('"', quoteStart + 1);
        if (quoteEnd == -1)
            break;

        // Extract batch ID
        String batchId = arrayContent.substring(quoteStart + 1, quoteEnd);
        batchIds.push_back(batchId);

        // Move to next potential batch ID
        pos = quoteEnd + 1;
    }

    return batchIds;
}

unsigned long NetworkManager::calculateBackoffDelay(uint8_t attempt) {
    // Exponential backoff: 1s, 2s, 4s, 8s, 16s (max 5 attempts)
    if (attempt == 0)
        return 1000;
    if (attempt >= 5)
        return 16000;
    return 1000 * (1 << attempt);  // 2^attempt seconds in milliseconds
}

String NetworkManager::getRegistrationEndpoint() {
    Config cfg = config.getConfig();
    return deriveEndpoint(String(cfg.apiEndpoint));
}

String NetworkManager::deriveEndpoint(const String& dataEndpoint) {
    String endpoint = dataEndpoint;

    // Strip trailing slashes
    while (endpoint.endsWith("/")) {
        endpoint = endpoint.substring(0, endpoint.length() - 1);
    }

    // Remove query string and fragment at earliest occurrence
    int queryPos = endpoint.indexOf('?');
    int fragmentPos = endpoint.indexOf('#');
    int cutPos = -1;

    if (queryPos != -1 && fragmentPos != -1) {
        cutPos = min(queryPos, fragmentPos);
    } else if (queryPos != -1) {
        cutPos = queryPos;
    } else if (fragmentPos != -1) {
        cutPos = fragmentPos;
    }

    if (cutPos != -1) {
        endpoint = endpoint.substring(0, cutPos);
    }

    // Find the last '/' in the path
    int lastSlash = endpoint.lastIndexOf('/');

    if (lastSlash == -1) {
        // No path, append /register
        return endpoint + "/register";
    }

    // Check if last slash is part of protocol (http://)
    if (lastSlash < 8) {
        // No path after domain, append /register
        return endpoint + "/register";
    }

    // Replace last segment with "register"
    return endpoint.substring(0, lastSlash + 1) + "register";
}

RegistrationResult NetworkManager::registerDevice(const String& payload) {
    RegistrationResult result;
    result.statusCode = 0;
    result.confirmationId = "";
    result.shouldRetry = false;

    if (!isConnected()) {
        Serial.printf("[NetworkManager] Cannot register: WiFi not connected\n");
        result.shouldRetry = true;  // Network error - should retry
        return result;
    }

    Config cfg = config.getConfig();

    // Validate API endpoint
    if (strlen(cfg.apiEndpoint) == 0) {
        Serial.printf("[NetworkManager] ERROR: API endpoint not configured\n");
        return result;  // Configuration error - don't retry
    }

    // Get registration endpoint
    String registrationEndpoint = getRegistrationEndpoint();

    Serial.printf("[NetworkManager] Registering device at: %s\n", registrationEndpoint.c_str());
    Serial.printf("[NetworkManager] Payload size: %d bytes\n", payload.length());

    // Determine if endpoint is HTTPS
    bool useHttps = registrationEndpoint.startsWith("https://");

    if (useHttps) {
        Serial.printf("[NetworkManager] Using HTTPS with TLS\n");
    } else {
        Serial.printf("[NetworkManager] Using plain HTTP\n");
    }

    // Feed watchdog before HTTP attempt
#ifndef UNIT_TEST
    esp_task_wdt_reset();
#endif

    // Initialize HTTP client with appropriate transport
    if (useHttps) {
        // Configure WiFiClientSecure for HTTPS
        if (cfg.tlsValidateServer) {
            Serial.printf("[NetworkManager] TLS: Certificate validation enabled\n");
            wifiClient.setCACert(NULL);  // Use built-in root CA bundle
        } else {
            Serial.printf("[NetworkManager] TLS: Certificate validation DISABLED (insecure)\n");
            wifiClient.setInsecure();
        }

        httpClient.begin(wifiClient, registrationEndpoint);
    } else {
        // Use plain HTTP
        httpClient.begin(registrationEndpoint);
    }

    // Set headers
    httpClient.addHeader("Content-Type", "application/json");

    // Add X-API-Key header if configured
    if (strlen(cfg.apiToken) > 0) {
        httpClient.addHeader("X-API-Key", String(cfg.apiToken));
    }

    // Set timeout to 10 seconds for registration
    httpClient.setTimeout(10000);

    // Send POST request
    int httpCode = httpClient.POST(payload);

    result.statusCode = httpCode;

    // Check response
    if (httpCode > 0) {
        Serial.printf("[NetworkManager] Registration HTTP Response code: %d\n", httpCode);

        // Determine if we should retry based on status code BEFORE parsing response
        // Retry on: network errors, 408, 429, or 5xx
        if (httpCode == 408 || httpCode == 429 || (httpCode >= 500 && httpCode <= 599)) {
            result.shouldRetry = true;
        }

        if (httpCode == 200 || httpCode == 201) {
            // Success - try to parse response
            String response = httpClient.getString();

            if (parseRegistrationResponse(response, result.confirmationId)) {
                Serial.printf("[NetworkManager] Registration successful. Confirmation ID: %s\n",
                              result.confirmationId.c_str());
            } else {
                Serial.printf("[NetworkManager] Registration response parsing failed\n");
                // Don't retry if we got 200 but couldn't parse - likely a server issue
            }
        } else if (httpCode >= 400 && httpCode < 500) {
            // Client error (4xx) - log and potentially retry based on shouldRetry flag
            Serial.printf("[NetworkManager] Registration client error (4xx): %d\n", httpCode);
            String response = httpClient.getString();
            Serial.printf("[NetworkManager] Response: %s\n", response.c_str());

            statusManager.incrementNetworkFailures();
            statusManager.setLastError("Registration HTTP " + String(httpCode));
        } else if (httpCode >= 500) {
            // Server error (5xx) - retry with backoff
            Serial.printf("[NetworkManager] Registration server error (5xx): %d\n", httpCode);

            statusManager.incrementNetworkFailures();
            statusManager.setLastError("Registration HTTP " + String(httpCode));
        }
    } else {
        // HTTP request failed - network error, should retry
        result.shouldRetry = true;

        String errorMsg = httpClient.errorToString(httpCode);
        Serial.printf("[NetworkManager] Registration HTTP request failed: %s\n", errorMsg.c_str());

        // Check for TLS-specific errors
        if (useHttps) {
            if (httpCode == HTTPC_ERROR_CONNECTION_FAILED) {
                Serial.printf("[NetworkManager] TLS: Connection failed during registration\n");
                statusManager.setLastError("Registration TLS connection failed");
            } else if (httpCode == HTTPC_ERROR_READ_TIMEOUT) {
                Serial.printf("[NetworkManager] TLS: Read timeout during registration\n");
                statusManager.setLastError("Registration TLS timeout");
            } else {
                statusManager.setLastError("Registration TLS error: " + errorMsg);
            }
        } else {
            statusManager.setLastError("Registration HTTP error: " + errorMsg);
        }

        statusManager.incrementNetworkFailures();
    }

    httpClient.end();

    return result;
}

bool NetworkManager::parseRegistrationResponse(const String& response, String& confirmationId) {
    confirmationId = "";

    if (response.length() == 0) {
        Serial.printf("[NetworkManager] Empty registration response\n");
        return false;
    }

    // Look for "confirmation_id" field in JSON response
    int fieldStart = response.indexOf("\"confirmation_id\"");
    if (fieldStart == -1) {
        Serial.printf("[NetworkManager] Missing confirmation_id field in response\n");
        return false;
    }

    // Find the colon after the field name
    int colonPos = response.indexOf(':', fieldStart);
    if (colonPos == -1) {
        Serial.printf("[NetworkManager] Malformed JSON: missing colon after confirmation_id\n");
        return false;
    }

    // Find the opening quote of the value
    int quoteStart = response.indexOf('"', colonPos);
    if (quoteStart == -1) {
        Serial.printf(
            "[NetworkManager] Malformed JSON: missing opening quote for confirmation_id value\n");
        return false;
    }

    // Find the closing quote of the value
    int quoteEnd = response.indexOf('"', quoteStart + 1);
    if (quoteEnd == -1) {
        Serial.printf(
            "[NetworkManager] Malformed JSON: missing closing quote for confirmation_id value\n");
        return false;
    }

    // Extract confirmation_id
    String extractedId = response.substring(quoteStart + 1, quoteEnd);

    // Validate as UUID v4 using BootId utility
    if (!BootId::isValidUuid(extractedId)) {
        Serial.printf("[NetworkManager] Invalid confirmation_id format (not UUID v4): %s\n",
                      extractedId.c_str());
        return false;
    }

    confirmationId = extractedId;
    return true;
}
