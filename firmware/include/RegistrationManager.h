#ifndef REGISTRATION_MANAGER_H
#define REGISTRATION_MANAGER_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <string>
typedef std::string String;
#endif

#include "ConfigManager.h"
#include "NetworkManager.h"

class RegistrationManager {
   public:
    RegistrationManager(NetworkManager* network, ConfigManager* config);

    // Check if device is registered (valid confirmation_id in NVS)
    bool isRegistered();

    // Get stored confirmation ID
    String getConfirmationId();

    // Build registration payload and cache it for retries
    String buildRegistrationPayload(const String& hardwareId, const String& bootId,
                                    const String& friendlyName, const String& firmwareVersion);

    // Attempt registration (synchronous, with timeout)
    bool registerDevice(const String& hardwareId, const String& bootId, const String& friendlyName,
                        const String& firmwareVersion);

    // Background retry task (called from main loop or separate FreeRTOS task)
    void processRetries();

    // Calculate next retry delay with exponential backoff and jitter (public for testing)
    unsigned long calculateBackoff(int attemptIndex);

   private:
    NetworkManager* _network;
    ConfigManager* _config;

    // Retry state
    int _retryCount;
    unsigned long _nextRetryTime;
    bool _retryPending;

    // Cached registration payload for retries
    String _cachedRegistrationPayload;

    // Validate confirmation ID format (UUID v4)
    bool isValidConfirmationId(const String& confirmationId);

    // Queue a retry attempt
    void queueRetry();
};

#endif  // REGISTRATION_MANAGER_H
