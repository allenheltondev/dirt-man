#include "Arduino.h"

#include <ctime>

// Mock millis value (can be overridden by tests)
unsigned long mockMillis = 0;

// Mock implementation of millis()
unsigned long millis() {
    // If mockMillis is set (non-zero), use it; otherwise use clock()
    if (mockMillis > 0) {
        return mockMillis;
    }
    return static_cast<unsigned long>(clock() * 1000 / CLOCKS_PER_SEC);
}

// Mock implementation of delay()
void delay(unsigned long ms) {
    // No-op for native testing
    (void)ms;
}

// Mock implementation of analogRead()
uint16_t analogRead(uint8_t pin) {
    // Return a default value for testing
    (void)pin;
    return 2048;  // Mid-range ADC value
}

// Mock implementation of analogReadResolution()
void analogReadResolution(uint8_t bits) {
    // No-op for native testing
    (void)bits;
}

// Mock implementation of analogSetAttenuation()
void analogSetAttenuation(adc_attenuation_t attenuation) {
    // No-op for native testing
    (void)attenuation;
}

// Mock implementation of analogSetWidth()
void analogSetWidth(adc_bits_width_t bits) {
    // No-op for native testing
    (void)bits;
}
