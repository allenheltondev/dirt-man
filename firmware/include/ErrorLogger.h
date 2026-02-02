#ifndef ERROR_LOGGER_H
#define ERROR_LOGGER_H

#ifdef UNIT_TEST
#include "../test/mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

/**
 * Error severity levels for logging
 */
enum class ErrorLevel {
    INFO,     // Informational message
    WARNING,  // Warning that doesn't prevent operation
    ERROR,    // Error that affects functionality
    CRITICAL  // Critical error that may prevent operation
};

/**
 * Error types for categorization
 */
enum class ErrorType {
    SENSOR,         // Sensor-related errors
    NETWORK,        // Network/WiFi errors
    STORAGE,        // NVS/storage errors
    DISPLAY,        // Display-related errors
    MEMORY,         // Memory/heap errors
    CONFIGURATION,  // Configuration errors
    SYSTEM          // General system errors
};

/**
 * ErrorLogger provides centralized error logging with timestamps,
 * error types, and severity levels.
 *
 * All error messages are output to serial console with context information.
 * Sensitive data (passwords, tokens) should never be logged.
 */
class ErrorLogger {
   public:
    /**
     * Log an error message with context.
     * @param level Error severity level
     * @param type Error type/category
     * @param message Error message (non-sensitive)
     * @param context Optional context string (e.g., function name, sensor type)
     */
    static void log(ErrorLevel level, ErrorType type, const char* message,
                    const char* context = nullptr);

    /**
     * Log an informational message.
     * @param type Error type/category
     * @param message Message text
     * @param context Optional context
     */
    static void info(ErrorType type, const char* message, const char* context = nullptr);

    /**
     * Log a warning message.
     * @param type Error type/category
     * @param message Warning text
     * @param context Optional context
     */
    static void warning(ErrorType type, const char* message, const char* context = nullptr);

    /**
     * Log an error message.
     * @param type Error type/category
     * @param message Error text
     * @param context Optional context
     */
    static void error(ErrorType type, const char* message, const char* context = nullptr);

    /**
     * Log a critical error message.
     * @param type Error type/category
     * @param message Critical error text
     * @param context Optional context
     */
    static void critical(ErrorType type, const char* message, const char* context = nullptr);

   private:
    static const char* levelToString(ErrorLevel level);
    static const char* typeToString(ErrorType type);
    static void printTimestamp();
};

#endif  // ERROR_LOGGER_H
