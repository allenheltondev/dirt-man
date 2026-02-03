#ifndef LOGGING_MACROS_H
#define LOGGING_MACROS_H

// ============================================================================
// LOGGING MACROS - DEVELOPMENT ONLY
// ============================================================================
//
// These macros are provided for DEVELOPMENT and DEBUGGING purposes only.
// They use String concatenation which can cause heap fragmentation on ESP32.
//
// PRODUCTION CODE SHOULD USE Serial.printf() DIRECTLY
//
// Example production logging:
//   Serial.printf("[INFO] Registration successful, confirmation_id: %s\n", id.c_str());
//   Serial.printf("[ERROR] Invalid MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
//                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
//   Serial.printf("[WARN] Registration failed with status %d, will retry\n", statusCode);
//
// Benefits of Serial.printf() over String concatenation:
// - No heap allocation for temporary String objects
// - Reduced heap fragmentation
// - More predictable memory usage
// - Better performance
//
// Only use these macros during development/debugging when you need quick
// logging without worrying about format strings.
// ============================================================================

#ifdef DEVELOPMENT_MODE

#include <Arduino.h>

// Log levels for development
#define LOG_ERROR(msg) Serial.println("[ERROR] " + String(msg))
#define LOG_WARN(msg) Serial.println("[WARN] " + String(msg))
#define LOG_INFO(msg) Serial.println("[INFO] " + String(msg))
#define LOG_DEBUG(msg) Serial.println("[DEBUG] " + String(msg))

#else

// In production, these macros do nothing
// Use Serial.printf() directly instead
#define LOG_ERROR(msg) ((void)0)
#define LOG_WARN(msg) ((void)0)
#define LOG_INFO(msg) ((void)0)
#define LOG_DEBUG(msg) ((void)0)

#endif

#endif  // LOGGING_MACROS_H
