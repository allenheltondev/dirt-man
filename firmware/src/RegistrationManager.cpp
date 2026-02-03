#include "RegistrationManager.h"

#ifdef ARDUINO
#include <esp_random.h>

#include "BootId.h"
#include "Version.h"
#else
#include <cstdlib>
#include <ctime>
#endif

RegistrationManager::RegistrationManager(NetworkManager* network, ConfigManager* config)
    : _network(network),
      _config(config),
      _retryCount(0),
      _nextRetryTime(0),
      _retryPending(false),
      _cachedRegistrationPayload("") {}

bool RegistrationManager::isRegistered() {
    return _config->hasValidConfirmationId();
}

String RegistrationManager::getConfirmationId() {
    return _config->getConfirmationId();
}

String RegistrationManager::buildRegistrationPayload(const String& hardwareId, const String& bootId,
                                                     const String& friendlyName,
                                                     const String& firmwareVersion) {
    // Build JSON payload
    String payload = "{";

    // Required fields
    payload += "\"hardware_id\":\"" + hardwareId + "\",";
    payload += "\"boot_id\":\"" + bootId + "\",";
    payload += "\"firmware_version\":\"" + firmwareVersion + "\",";

    // Optional friendly_name field - only include if not empty
    if (friendlyName.length() > 0) {
        payload += "\"friendly_name\":\"" + friendlyName + "\",";
    }

    // Capabilities object
    payload += "\"capabilities\":{";

    // Sensors array - hardcoded for now based on firmware capabilities
    payload += "\"sensors\":[\"bme280\",\"ds18b20\",\"soil_moisture\"],";

    // Features object
    payload += "\"features\":{";
    payload += "\"tft_display\":true,";
    payload += "\"offline_buffering\":true,";
    payload += "\"ntp_sync\":true";
    payload += "}";

    payload += "}";  // Close capabilities
    payload += "}";  // Close root

    // Cache the payload for retries
    _cachedRegistrationPayload = payload;

    return payload;
}

bool RegistrationManager::registerDevice(const String& hardwareId, const String& bootId,
                                         const String& friendlyName,
                                         const String& firmwareVersion) {
    Serial.printf("[INFO] Attempting device registration...\n");

    // Build and cache the registration payload
    String payload = buildRegistrationPayload(hardwareId, bootId, friendlyName, firmwareVersion);

    // Call NetworkManager to send registration request
    RegistrationResult result = _network->registerDevice(payload);

    // Handle the result
    if (result.statusCode >= 200 && result.statusCode < 300) {
        // Success - check if we got a valid confirmation_id
        if (result.confirmationId.length() > 0 && isValidConfirmationId(result.confirmationId)) {
            // Store confirmation_id in NVS
            _config->setConfirmationId(result.confirmationId);

            // Clear retry state
            _retryPending = false;
            _retryCount = 0;

            Serial.printf("[INFO] Registration successful, confirmation_id: %s\n",
                          result.confirmationId.c_str());
            return true;
        } else {
            Serial.printf("[ERROR] Registration response missing or invalid confirmation_id\n");
            return false;
        }
    } else if (result.shouldRetry) {
        // Retryable error - queue retry
        Serial.printf("[WARN] Registration failed with status %d, will retry\n", result.statusCode);
        queueRetry();
        return false;
    } else {
        // Non-retryable error
        Serial.printf("[ERROR] Registration failed with status %d, will not retry\n",
                      result.statusCode);
        return false;
    }
}

void RegistrationManager::processRetries() {
    // Check if we have a pending retry
    if (!_retryPending) {
        return;
    }

    // Check if it's time to retry
#ifdef ARDUINO
    unsigned long currentTime = millis();
#else
    unsigned long currentTime = (unsigned long)(clock() * 1000 / CLOCKS_PER_SEC);
#endif

    if (currentTime < _nextRetryTime) {
        return;  // Not time yet
    }

    // Check if we've exceeded max retries
    if (_retryCount >= 5) {
        Serial.printf("[ERROR] Max registration retries (5) exceeded, giving up\n");
        _retryPending = false;
        return;
    }

    // Attempt retry using cached payload
    Serial.printf("[INFO] Attempting registration retry %d/5\n", _retryCount + 1);

    RegistrationResult result = _network->registerDevice(_cachedRegistrationPayload);

    // Handle the result
    if (result.statusCode >= 200 && result.statusCode < 300) {
        // Success
        if (result.confirmationId.length() > 0 && isValidConfirmationId(result.confirmationId)) {
            _config->setConfirmationId(result.confirmationId);
            _retryPending = false;
            _retryCount = 0;
            Serial.printf("[INFO] Registration retry successful, confirmation_id: %s\n",
                          result.confirmationId.c_str());
        } else {
            Serial.printf(
                "[ERROR] Registration retry response missing or invalid confirmation_id\n");
            _retryPending = false;
        }
    } else if (result.shouldRetry) {
        // Still retryable - queue another retry
        _retryCount++;
        if (_retryCount < 5) {
            queueRetry();
            Serial.printf("[WARN] Registration retry %d failed with status %d, will retry again\n",
                          _retryCount, result.statusCode);
        } else {
            Serial.printf("[ERROR] Max registration retries (5) exceeded after status %d\n",
                          result.statusCode);
            _retryPending = false;
        }
    } else {
        // Non-retryable error
        Serial.printf("[ERROR] Registration retry failed with non-retryable status %d\n",
                      result.statusCode);
        _retryPending = false;
    }
}

unsigned long RegistrationManager::calculateBackoff(int attemptIndex) {
    // Base delay: 1 second
    unsigned long baseDelay = 1000;

    // Exponential: 2^attemptIndex seconds
    // attemptIndex is zero-based: 0→1s, 1→2s, 2→4s, 3→8s, 4→16s
    unsigned long delay = baseDelay * (1UL << attemptIndex);

    // Clamp to maximum 30 seconds to prevent overflow
    if (delay > 30000) {
        delay = 30000;
    }

    // Add random jitter: 0-500ms
#ifdef ARDUINO
    unsigned long jitter = esp_random() % 501;
#else
    unsigned long jitter = rand() % 501;
#endif

    return delay + jitter;
}

bool RegistrationManager::isValidConfirmationId(const String& confirmationId) {
    // Use BootId's UUID v4 validation
    return BootId::isValidUuid(confirmationId);
}

void RegistrationManager::queueRetry() {
    _retryPending = true;

    // Calculate backoff delay
    unsigned long backoffDelay = calculateBackoff(_retryCount);

    // Set next retry time
#ifdef ARDUINO
    _nextRetryTime = millis() + backoffDelay;
#else
    _nextRetryTime = (unsigned long)(clock() * 1000 / CLOCKS_PER_SEC) + backoffDelay;
#endif

    Serial.printf("[INFO] Registration retry queued, will retry in %lu ms\n", backoffDelay);
}
