#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include "models/AveragedData.h"
#include "models/DisplayPoint.h"
#include <cstdint>

#ifdef ARDUINO
#include <Preferences.h>
#else
// Mock for unit testing
class Preferences {
   public:
    bool begin(const char*, bool) { return true; }
    void end() {}
    bool putBytes(const char*, const void*, size_t) { return true; }
    size_t getBytes(const char*, void*, size_t) { return 0; }
    bool putUShort(const char*, uint16_t) { return true; }
    uint16_t getUShort(const char*, uint16_t) { return 0; }
    bool clear() { return true; }
};
#endif

/**
 * StateManager handles persistence of critical system state to NVS
 * for recovery after deep sleep or unexpected resets.
 */
class StateManager {
   public:
    StateManager();

    void initialize();

    // Persist critical state before deep sleep
    bool persistState(const AveragedData* dataBuffer, uint16_t dataBufferCount,
                      const DisplayPoint* displayBuffer, uint16_t displayBufferCount);

    // Restore state after wakeup from deep sleep
    bool restoreState(AveragedData* dataBuffer, uint16_t& dataBufferCount,
                      DisplayPoint* displayBuffer, uint16_t& displayBufferCount);

    // Check if persisted state exists
    bool hasPersistedState();

    // Clear persisted state (after successful restore)
    void clearPersistedState();

   private:
    Preferences nvs;

    // NVS namespace for state persistence
    static constexpr const char* NVS_NAMESPACE = "state";

    // NVS keys
    static constexpr const char* KEY_DATA_BUFFER = "data_buf";
    static constexpr const char* KEY_DATA_COUNT = "data_cnt";
    static constexpr const char* KEY_DISPLAY_BUFFER = "disp_buf";
    static constexpr const char* KEY_DISPLAY_COUNT = "disp_cnt";
    static constexpr const char* KEY_STATE_VALID = "state_valid";

    // Maximum sizes for NVS storage
    static constexpr uint16_t MAX_DATA_BUFFER_SIZE = 50;
    static constexpr uint16_t MAX_DISPLAY_BUFFER_SIZE = 240;
};

#endif  // STATE_MANAGER_H
