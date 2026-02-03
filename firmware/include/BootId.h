#ifndef BOOT_ID_H
#define BOOT_ID_H

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <string>
typedef std::string String;
#endif

class BootId {
   public:
    // Generate a new UUID v4 boot ID
    static String generate();

    // Validate UUID v4 format (8-4-4-4-12 hex characters with strict version/variant checks)
    static bool isValidUuid(const String& uuid);

   private:
    // Generate random hex string of specified length
    static String randomHex(int length);
};

#endif  // BOOT_ID_H
