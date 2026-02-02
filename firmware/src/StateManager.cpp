#ifndef UNIT_TEST
#include "StateManager.h"

#include "ErrorLogger.h"

StateManager::StateManager() {}

void StateManager::initialize() {
    // NVS will be opened when needed
}

bool StateManager::persistState(const AveragedData* dataBuffer, uint16_t dataBufferCount,
                                const DisplayPoint* displayBuffer, uint16_t displayBufferCount) {
    if (!nvs.begin(NVS_NAMESPACE, false)) {
        ErrorLogger::error(ErrorType::SYSTEM, "Failed to open NVS for state persistence",
                           "StateManager::persistState");
        return false;
    }

    // Validate buffer sizes
    if (dataBufferCount > MAX_DATA_BUFFER_SIZE) {
        ErrorLogger::warning(ErrorType::SYSTEM, "Data buffer count exceeds maximum, truncating",
                             "StateManager::persistState");
        dataBufferCount = MAX_DATA_BUFFER_SIZE;
    }

    if (displayBufferCount > MAX_DISPLAY_BUFFER_SIZE) {
        ErrorLogger::warning(ErrorType::SYSTEM, "Display buffer count exceeds maximum, truncating",
                             "StateManager::persistState");
        displayBufferCount = MAX_DISPLAY_BUFFER_SIZE;
    }

    // Persist data buffer
    if (dataBufferCount > 0 && dataBuffer != nullptr) {
        size_t dataSize = dataBufferCount * sizeof(AveragedData);
        if (!nvs.putBytes(KEY_DATA_BUFFER, dataBuffer, dataSize)) {
            ErrorLogger::error(ErrorType::SYSTEM, "Failed to persist data buffer",
                               "StateManager::persistState");
            nvs.end();
            return false;
        }
        nvs.putUShort(KEY_DATA_COUNT, dataBufferCount);
    } else {
        nvs.putUShort(KEY_DATA_COUNT, 0);
    }

    // Persist display buffer
    if (displayBufferCount > 0 && displayBuffer != nullptr) {
        size_t displaySize = displayBufferCount * sizeof(DisplayPoint);
        if (!nvs.putBytes(KEY_DISPLAY_BUFFER, displayBuffer, displaySize)) {
            ErrorLogger::error(ErrorType::SYSTEM, "Failed to persist display buffer",
                               "StateManager::persistState");
            nvs.end();
            return false;
        }
        nvs.putUShort(KEY_DISPLAY_COUNT, displayBufferCount);
    } else {
        nvs.putUShort(KEY_DISPLAY_COUNT, 0);
    }

    // Mark state as valid
    nvs.putUShort(KEY_STATE_VALID, 1);

    nvs.end();

    ErrorLogger::info(ErrorType::SYSTEM, "State persisted successfully",
                      "StateManager::persistState");
    return true;
}

bool StateManager::restoreState(AveragedData* dataBuffer, uint16_t& dataBufferCount,
                                DisplayPoint* displayBuffer, uint16_t& displayBufferCount) {
    if (!nvs.begin(NVS_NAMESPACE, true)) {  // Read-only mode
        ErrorLogger::error(ErrorType::SYSTEM, "Failed to open NVS for state restore",
                           "StateManager::restoreState");
        return false;
    }

    // Check if state is valid
    uint16_t stateValid = nvs.getUShort(KEY_STATE_VALID, 0);
    if (stateValid != 1) {
        ErrorLogger::info(ErrorType::SYSTEM, "No valid persisted state found",
                          "StateManager::restoreState");
        nvs.end();
        return false;
    }

    // Restore data buffer
    dataBufferCount = nvs.getUShort(KEY_DATA_COUNT, 0);
    if (dataBufferCount > 0 && dataBuffer != nullptr) {
        if (dataBufferCount > MAX_DATA_BUFFER_SIZE) {
            ErrorLogger::warning(ErrorType::SYSTEM, "Persisted data buffer count exceeds maximum",
                                 "StateManager::restoreState");
            dataBufferCount = MAX_DATA_BUFFER_SIZE;
        }

        size_t dataSize = dataBufferCount * sizeof(AveragedData);
        size_t bytesRead = nvs.getBytes(KEY_DATA_BUFFER, dataBuffer, dataSize);
        if (bytesRead != dataSize) {
            ErrorLogger::error(ErrorType::SYSTEM, "Failed to restore data buffer",
                               "StateManager::restoreState");
            nvs.end();
            return false;
        }
    }

    // Restore display buffer
    displayBufferCount = nvs.getUShort(KEY_DISPLAY_COUNT, 0);
    if (displayBufferCount > 0 && displayBuffer != nullptr) {
        if (displayBufferCount > MAX_DISPLAY_BUFFER_SIZE) {
            ErrorLogger::warning(ErrorType::SYSTEM,
                                 "Persisted display buffer count exceeds maximum",
                                 "StateManager::restoreState");
            displayBufferCount = MAX_DISPLAY_BUFFER_SIZE;
        }

        size_t displaySize = displayBufferCount * sizeof(DisplayPoint);
        size_t bytesRead = nvs.getBytes(KEY_DISPLAY_BUFFER, displayBuffer, displaySize);
        if (bytesRead != displaySize) {
            ErrorLogger::error(ErrorType::SYSTEM, "Failed to restore display buffer",
                               "StateManager::restoreState");
            nvs.end();
            return false;
        }
    }

    nvs.end();

    ErrorLogger::info(ErrorType::SYSTEM, "State restored successfully",
                      "StateManager::restoreState");
    return true;
}

bool StateManager::hasPersistedState() {
    if (!nvs.begin(NVS_NAMESPACE, true)) {  // Read-only mode
        return false;
    }

    uint16_t stateValid = nvs.getUShort(KEY_STATE_VALID, 0);
    nvs.end();

    return (stateValid == 1);
}

void StateManager::clearPersistedState() {
    if (!nvs.begin(NVS_NAMESPACE, false)) {
        ErrorLogger::error(ErrorType::SYSTEM, "Failed to open NVS to clear state",
                           "StateManager::clearPersistedState");
        return;
    }

    nvs.clear();
    nvs.end();

    ErrorLogger::info(ErrorType::SYSTEM, "Persisted state cleared",
                      "StateManager::clearPersistedState");
}

#endif  // UNIT_TEST
