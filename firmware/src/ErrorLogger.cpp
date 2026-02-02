#include "ErrorLogger.h"

void ErrorLogger::log(ErrorLevel level, ErrorType type, const char* message, const char* context) {
    // Print timestamp
    printTimestamp();

    // Print severity level
    Serial.print("[");
    Serial.print(levelToString(level));
    Serial.print("] ");

    // Print error type
    Serial.print("[");
    Serial.print(typeToString(type));
    Serial.print("] ");

    // Print message
    Serial.print(message);

    // Print context if provided
    if (context != nullptr && context[0] != '\0') {
        Serial.print(" (");
        Serial.print(context);
        Serial.print(")");
    }

    Serial.println();
}

void ErrorLogger::info(ErrorType type, const char* message, const char* context) {
    log(ErrorLevel::INFO, type, message, context);
}

void ErrorLogger::warning(ErrorType type, const char* message, const char* context) {
    log(ErrorLevel::WARNING, type, message, context);
}

void ErrorLogger::error(ErrorType type, const char* message, const char* context) {
    log(ErrorLevel::ERROR, type, message, context);
}

void ErrorLogger::critical(ErrorType type, const char* message, const char* context) {
    log(ErrorLevel::CRITICAL, type, message, context);
}

const char* ErrorLogger::levelToString(ErrorLevel level) {
    switch (level) {
        case ErrorLevel::INFO:
            return "INFO";
        case ErrorLevel::WARNING:
            return "WARN";
        case ErrorLevel::ERROR:
            return "ERROR";
        case ErrorLevel::CRITICAL:
            return "CRIT";
        default:
            return "UNKNOWN";
    }
}

const char* ErrorLogger::typeToString(ErrorType type) {
    switch (type) {
        case ErrorType::SENSOR:
            return "SENSOR";
        case ErrorType::NETWORK:
            return "NETWORK";
        case ErrorType::STORAGE:
            return "STORAGE";
        case ErrorType::DISPLAY:
            return "DISPLAY";
        case ErrorType::MEMORY:
            return "MEMORY";
        case ErrorType::CONFIGURATION:
            return "CONFIG";
        case ErrorType::SYSTEM:
            return "SYSTEM";
        default:
            return "UNKNOWN";
    }
}

void ErrorLogger::printTimestamp() {
    unsigned long ms = millis();
    unsigned long seconds = ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;

    // Format: [HH:MM:SS.mmm]
    Serial.print("[");

    if (hours < 10)
        Serial.print("0");
    Serial.print(hours);
    Serial.print(":");

    if ((minutes % 60) < 10)
        Serial.print("0");
    Serial.print(minutes % 60);
    Serial.print(":");

    if ((seconds % 60) < 10)
        Serial.print("0");
    Serial.print(seconds % 60);
    Serial.print(".");

    unsigned long milliseconds = ms % 1000;
    if (milliseconds < 100)
        Serial.print("0");
    if (milliseconds < 10)
        Serial.print("0");
    Serial.print(milliseconds);

    Serial.print("] ");
}
