#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include "WString.h"
#include <cstddef>
#include <cstdint>

// Mock Arduino types and functions for native testing

// Basic types
typedef uint8_t byte;
typedef bool boolean;

// Mock millis value for testing (can be set by tests)
extern unsigned long mockMillis;

// Mock ADC constants (must be defined before function declarations)
typedef enum { ADC_0db = 0, ADC_2_5db = 1, ADC_6db = 2, ADC_11db = 3 } adc_attenuation_t;

typedef enum {
    ADC_WIDTH_BIT_9 = 0,
    ADC_WIDTH_BIT_10 = 1,
    ADC_WIDTH_BIT_11 = 2,
    ADC_WIDTH_BIT_12 = 3
} adc_bits_width_t;

// Mock millis() function
unsigned long millis();

// Mock delay function
void delay(unsigned long ms);

// Mock ADC functions
uint16_t analogRead(uint8_t pin);
void analogReadResolution(uint8_t bits);
void analogSetAttenuation(adc_attenuation_t attenuation);
void analogSetWidth(adc_bits_width_t bits);

#endif  // MOCK_ARDUINO_H
