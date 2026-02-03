#ifndef HARDWARE_ID_H
#define HARDWARE_ID_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <string>
typedef std::string String;
#endif

class HardwareId {
   public:
    // Get the formatted hardware ID (MAC address as AA:BB:CC:DD:EE:FF)
    static String getHardwareId();

    // Validate that a MAC address is non-zero
    static bool isValidMac(const uint8_t* mac);

    // Format MAC address bytes to colon-separated hex string (public for testing)
    static String formatMac(const uint8_t* mac);
};

#endif  // HARDWARE_ID_H
