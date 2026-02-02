#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#ifdef UNIT_TEST
#include "../test/mocks/Arduino.h"
#else
#include <Arduino.h>
#endif

// ============================================================================
// ESP32 Pin Configuration for Sensor Firmware
// ============================================================================
// This file defines all hardware pin assignments for sensors and display.
// Pin selections follow ESP32 best practices to avoid conflicts with WiFi,
// boot modes, and internal peripherals.
// ============================================================================

// ============================================================================
// I2C Configuration (BME280 Sensor)
// ============================================================================
// BME280 environmental sensor (temperature, humidity, pressure)
// Uses I2C protocol at 400kHz
// Default I2C address: 0x76 (alternate: 0x77)

static const uint8_t I2C_SDA_PIN = 21;         // GPIO21 - I2C Data
static const uint8_t I2C_SCL_PIN = 22;         // GPIO22 - I2C Clock
static const uint32_t I2C_FREQUENCY = 400000;  // 400kHz

// ============================================================================
// OneWire Configuration (DS18B20 Temperature Sensor)
// ============================================================================
// DS18B20 digital temperature sensor for probe measurements
// Uses OneWire protocol (single data line)
// Requires 4.7kΩ pull-up resistor to 3.3V

static const uint8_t ONEWIRE_PIN = 4;  // GPIO4 - OneWire Data

// ============================================================================
// ADC Configuration (Soil Moisture Sensor)
// ============================================================================
// Capacitive soil moisture sensor with analog output
// IMPORTANT: Use ADC1 pins only (GPIO 32-39) to avoid WiFi conflicts
// ADC2 pins (GPIO 0, 2, 4, 12-15, 25-27) cannot be used when WiFi is active
//
// ADC Configuration:
// - Attenuation: ADC_11db (0-3.3V range)
// - Resolution: 12-bit (0-4095)
// - Multi-sample averaging applied to reduce noise

static const uint8_t SOIL_MOISTURE_PIN = 32;  // GPIO32 - ADC1_CH4

// Alternative ADC1 pins if GPIO32 is unavailable:
// GPIO33 (ADC1_CH5), GPIO34 (ADC1_CH6), GPIO35 (ADC1_CH7),
// GPIO36 (ADC1_CH0), GPIO39 (ADC1_CH3)

static const adc_attenuation_t ADC_ATTENUATION = ADC_11db;   // 0-3.3V range
static const adc_bits_width_t ADC_WIDTH = ADC_WIDTH_BIT_12;  // 12-bit resolution (0-4095)

// ============================================================================
// SPI Configuration (TFT Display)
// ============================================================================
// TFT display using SPI interface
// Uses hardware SPI (VSPI on ESP32)
// Display library: TFT_eSPI
//
// Note: TFT_eSPI pin configuration is typically done in User_Setup.h
// or User_Setup_Select.h in the TFT_eSPI library folder.
// These definitions are provided for reference and documentation.

// Hardware SPI pins (VSPI)
static const uint8_t TFT_MOSI_PIN = 23;  // GPIO23 - SPI MOSI (Master Out Slave In)
static const uint8_t TFT_MISO_PIN = 19;  // GPIO19 - SPI MISO (Master In Slave Out)
static const uint8_t TFT_SCLK_PIN = 18;  // GPIO18 - SPI Clock

// TFT Control pins
static const uint8_t TFT_CS_PIN = 5;    // GPIO5  - Chip Select
static const uint8_t TFT_DC_PIN = 2;    // GPIO2  - Data/Command
static const uint8_t TFT_RST_PIN = 15;  // GPIO15 - Reset

// Optional TFT backlight control
static const uint8_t TFT_BL_PIN = 27;  // GPIO27 - Backlight (PWM capable)

// ============================================================================
// Pin Usage Summary
// ============================================================================
// GPIO2  - TFT DC (Data/Command)
// GPIO4  - OneWire (DS18B20)
// GPIO5  - TFT CS (Chip Select)
// GPIO15 - TFT RST (Reset)
// GPIO18 - TFT SCLK (SPI Clock)
// GPIO19 - TFT MISO (SPI)
// GPIO21 - I2C SDA (BME280)
// GPIO22 - I2C SCL (BME280)
// GPIO23 - TFT MOSI (SPI)
// GPIO27 - TFT Backlight
// GPIO32 - Soil Moisture (ADC1_CH4)
//
// Reserved/Avoided Pins:
// GPIO0  - Boot mode (pulled high, avoid)
// GPIO1  - UART TX (serial console)
// GPIO3  - UART RX (serial console)
// GPIO6-11 - Flash memory (do not use)
// GPIO12 - Boot mode (must be low at boot)
// GPIO13-15 - ADC2 (conflicts with WiFi)
// GPIO25-27 - ADC2 (conflicts with WiFi)
//
// Safe for future expansion:
// GPIO13, GPIO14, GPIO16, GPIO17, GPIO25, GPIO26, GPIO33-39
// (Note: GPIO34-39 are input-only, no pull-up/pull-down)

// ============================================================================
// Validation Macros
// ============================================================================
// Compile-time checks to ensure pin assignments are valid
// Skip validation in UNIT_TEST mode (native environment)

#ifndef UNIT_TEST
#if SOIL_MOISTURE_PIN < 32 || SOIL_MOISTURE_PIN > 39
#error "SOIL_MOISTURE_PIN must be an ADC1 pin (GPIO 32-39) to avoid WiFi conflicts"
#endif

// Ensure no pin conflicts
#if I2C_SDA_PIN == I2C_SCL_PIN
#error "I2C SDA and SCL pins must be different"
#endif

#if TFT_CS_PIN == TFT_DC_PIN || TFT_CS_PIN == TFT_RST_PIN || TFT_DC_PIN == TFT_RST_PIN
#error "TFT control pins must be unique"
#endif
#endif  // UNIT_TEST

// ============================================================================
// Configuration Notes
// ============================================================================
//
// 1. TFT_eSPI Library Configuration:
//    The TFT_eSPI library requires pin configuration in its User_Setup.h file.
//    Copy these pin definitions to:
//    <Arduino Libraries>/TFT_eSPI/User_Setup.h
//    Or create a custom setup file and reference it in User_Setup_Select.h
//
// 2. ADC Calibration:
//    The ESP32 ADC is non-linear and may require calibration for accurate
//    voltage measurements. For soil moisture sensing, relative values are
//    sufficient, so two-point calibration (dry/wet) is used instead.
//
// 3. OneWire Pull-up:
//    The DS18B20 requires a 4.7kΩ pull-up resistor between the data line
//    and 3.3V. Some modules include this resistor; verify your hardware.
//
// 4. I2C Pull-ups:
//    I2C requires pull-up resistors (typically 4.7kΩ) on SDA and SCL lines.
//    The ESP32 has weak internal pull-ups, but external resistors are
//    recommended for reliable operation, especially with long wires.
//
// 5. Power Considerations:
//    - BME280: 3.3V, ~3.6μA sleep, ~714μA active
//    - DS18B20: 3.3V, ~1μA standby, ~1.5mA active
//    - TFT Display: 3.3V, varies by model (typically 20-100mA)
//    - Total current draw: ~150mA typical, ~200mA peak
//
// 6. Boot Mode Pins:
//    Avoid using GPIO0, GPIO2, GPIO12, GPIO15 for sensors if possible,
//    as they affect boot mode. If used, ensure proper pull-up/pull-down
//    resistors to maintain correct boot behavior.
//
// ============================================================================

#endif  // PIN_CONFIG_H
