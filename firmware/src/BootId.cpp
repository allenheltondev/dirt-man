#include "BootId.h"

#ifdef ARDUINO
#include <esp_random.h>
#endif

#ifndef ARDUINO
#include <cstdio>
#include <cstdlib>
#include <ctime>
#endif

String BootId::generate() {
#ifndef ARDUINO
    // Seed random for host testing
    static bool seeded = false;
    if (!seeded) {
        srand(time(NULL));
        seeded = true;
    }
#endif

    // Generate UUID v4: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    // Where:
    // - 4 indicates version 4
    // - y is one of 8, 9, A, or B (variant bits)

    String uuid = "";

    // First group: 8 hex characters
    uuid += randomHex(8);
    uuid += "-";

    // Second group: 4 hex characters
    uuid += randomHex(4);
    uuid += "-";

    // Third group: 4 hex characters with version bits
    // First character must be '4' for UUID v4
    uuid += "4";
    uuid += randomHex(3);
    uuid += "-";

    // Fourth group: 4 hex characters with variant bits
    // First character must be 8, 9, A, or B
    // Generate random value 0-3 and map to 8, 9, A, B
#ifdef ARDUINO
    uint32_t variantValue = esp_random() % 4;
#else
    uint32_t variantValue = rand() % 4;
#endif
    char variantChars[] = {'8', '9', 'A', 'B'};
    uuid += variantChars[variantValue];
    uuid += randomHex(3);
    uuid += "-";

    // Fifth group: 12 hex characters
    uuid += randomHex(12);

#ifdef ARDUINO
    Serial.printf("[INFO] Generated Boot ID: %s\n", uuid.c_str());
#endif

    return uuid;
}

bool BootId::isValidUuid(const String& uuid) {
    // Check length: 8-4-4-4-12 = 36 characters including hyphens
    if (uuid.length() != 36) {
#ifdef ARDUINO
        Serial.printf("[ERROR] Invalid UUID length: %d (expected 36)\n", uuid.length());
#endif
        return false;
    }

    // Check hyphen positions
    if (uuid[8] != '-' || uuid[13] != '-' || uuid[18] != '-' || uuid[23] != '-') {
#ifdef ARDUINO
        Serial.printf("[ERROR] Invalid UUID format: missing hyphens at correct positions\n");
#endif
        return false;
    }

    // Check all other characters are hex digits
    for (size_t i = 0; i < uuid.length(); i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            continue;  // Skip hyphens
        }

        char c = uuid[i];
        bool isHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');

        if (!isHex) {
#ifdef ARDUINO
            Serial.printf("[ERROR] Invalid UUID: non-hex character '%c' at position %d\n", c, i);
#endif
            return false;
        }
    }

    // Check version bits: character at position 14 must be '4'
    if (uuid[14] != '4') {
#ifdef ARDUINO
        Serial.printf("[ERROR] Invalid UUID version: expected '4' at position 14, got '%c'\n",
                      uuid[14]);
#endif
        return false;
    }

    // Check variant bits: character at position 19 must be 8, 9, A, B (or lowercase)
    char variantChar = uuid[19];
    bool validVariant = (variantChar == '8' || variantChar == '9' || variantChar == 'A' ||
                         variantChar == 'a' || variantChar == 'B' || variantChar == 'b');

    if (!validVariant) {
#ifdef ARDUINO
        Serial.printf("[ERROR] Invalid UUID variant: expected 8/9/A/B at position 19, got '%c'\n",
                      variantChar);
#endif
        return false;
    }

    return true;
}

String BootId::randomHex(int length) {
    String hex = "";

    for (int i = 0; i < length; i++) {
#ifdef ARDUINO
        uint32_t randomValue = esp_random() % 16;
#else
        uint32_t randomValue = rand() % 16;
#endif

        // Convert to hex character (0-9, A-F)
        if (randomValue < 10) {
            hex += (char)('0' + randomValue);
        } else {
            hex += (char)('A' + (randomValue - 10));
        }
    }

    return hex;
}
