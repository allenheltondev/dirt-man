# ESP32 Sensor Firmware - Hardware Wiring Guide

## Overview

This document provides complete wiring instructions for connecting sensors and display to the ESP32 microcontroller. Follow these instructions carefully to ensure proper operation and avoid hardware conflicts.

## Required Components

### Microcontroller
- **ESP32 Development Board** (ESP32-WROOM-32 or compatible)
- USB cable for programming and power
- 5V power supply (optional, for standalone operation)

### Sensors
- **BME280** - Environmental sensor (temperature, humidity, pressure)
  - I2C interface
  - Operating voltage: 3.3V
  - Current: 3.6μA sleep, 714μA active

- **DS18B20** - Digital temperature probe
  - OneWire interface
  - Operating voltage: 3.3V
  - Current: 1μA standby, 1.5mA active
  - **Requires 4.7kΩ pull-up resistor**

- **Capacitive Soil Moisture Sensor**
  - Analog output (0-3.3V)
  - Operating voltage: 3.3V or 5V (check your model)
  - Current: ~5mA typical

### Display
- **TFT LCD Display** (compatible with TFT_eSPI library)
  - SPI interface
  - Recommended: 240x320 or 128x160 resolution
  - Operating voltage: 3.3V
  - Current: 20-100mA (varies by model and backlight)

### Additional Components
- **4.7kΩ resistor** (for DS18B20 pull-up)
- **4.7kΩ resistors x2** (optional, for I2C pull-ups if not on sensor board)
- Breadboard or PCB for connections
- Jumper wires (male-to-male, male-to-female)

## Pin Assignments

### Complete Pin Map

| GPIO | Function | Component | Notes |
|------|----------|-----------|-------|
| GPIO2 | TFT DC | TFT Display | Data/Command select |
| GPIO4 | OneWire Data | DS18B20 | Requires 4.7kΩ pull-up to 3.3V |
| GPIO5 | TFT CS | TFT Display | Chip Select |
| GPIO15 | TFT RST | TFT Display | Reset |
| GPIO18 | SPI SCLK | TFT Display | SPI Clock |
| GPIO19 | SPI MISO | TFT Display | SPI Master In Slave Out |
| GPIO21 | I2C SDA | BME280 | I2C Data (4.7kΩ pull-up recommended) |
| GPIO22 | I2C SCL | BME280 | I2C Clock (4.7kΩ pull-up recommended) |
| GPIO23 | SPI MOSI | TFT Display | SPI Master Out Slave In |
| GPIO27 | TFT Backlight | TFT Display | PWM control (optional) |
| GPIO32 | ADC Input | Soil Moisture | ADC1_CH4 (safe with WiFi) |

### Reserved Pins (Do Not Use)

| GPIO | Reason |
|------|--------|
| GPIO0 | Boot mode selection (pulled high) |
| GPIO1 | UART TX (serial console) |
| GPIO3 | UART RX (serial console) |
| GPIO6-11 | Connected to flash memory |
| GPIO12 | Boot mode (must be low at boot) |

### Pins to Avoid with WiFi Active

**ADC2 Pins** - These conflict with WiFi and should not be used for analog inputs:
- GPIO0, GPIO2, GPIO4, GPIO12, GPIO13, GPIO14, GPIO15, GPIO25, GPIO26, GPIO27

**Note:** We use GPIO32 (ADC1_CH4) for soil moisture to avoid WiFi conflicts.

## Wiring Diagrams

### BME280 Sensor (I2C)

```
BME280          ESP32
------          -----
VCC    -------> 3.3V
GND    -------> GND
SDA    -------> GPIO21 (I2C SDA)
SCL    -------> GPIO22 (I2C SCL)
```

**I2C Address:** 0x76 (default) or 0x77 (alternate)

**Pull-up Resistors:** Most BME280 breakout boards include I2C pull-up resistors. If your board doesn't have them, add 4.7kΩ resistors:
- One between SDA (GPIO21) and 3.3V
- One between SCL (GPIO22) and 3.3V

### DS18B20 Temperature Sensor (OneWire)

```
DS18B20         ESP32
-------         -----
VCC    -------> 3.3V
GND    -------> GND
DATA   -------> GPIO4 (OneWire)
```

**CRITICAL:** Add a 4.7kΩ pull-up resistor between DATA (GPIO4) and 3.3V.

```
        3.3V
         |
        [4.7kΩ]
         |
GPIO4 ---+--- DS18B20 DATA
```

**Note:** Some DS18B20 modules include the pull-up resistor. Check your module's documentation.

### Soil Moisture Sensor (Analog)

```
Soil Moisture   ESP32
-------------   -----
VCC    -------> 3.3V or 5V (check sensor specs)
GND    -------> GND
AOUT   -------> GPIO32 (ADC1_CH4)
```

**Important Notes:**
- Use GPIO32 (ADC1) to avoid WiFi conflicts
- If your sensor requires 5V power, connect VCC to 5V (VIN) pin
- The analog output (AOUT) should be 0-3.3V regardless of power voltage
- Verify your sensor's output voltage range before connecting

**ADC Configuration:**
- Attenuation: 11dB (0-3.3V range)
- Resolution: 12-bit (0-4095)
- Multi-sample averaging applied in firmware

### TFT Display (SPI)

```
TFT Display     ESP32
-----------     -----
VCC    -------> 3.3V
GND    -------> GND
CS     -------> GPIO5
RESET  -------> GPIO15
DC     -------> GPIO2
MOSI   -------> GPIO23
SCK    -------> GPIO18
LED    -------> GPIO27 (backlight, optional)
MISO   -------> GPIO19 (if display has MISO)
```

**Backlight Control:**
- Connect LED pin to GPIO27 for PWM backlight control
- Or connect LED to 3.3V through a current-limiting resistor (typically 100Ω)
- Check your display's backlight current requirements

**Display Models:**
- ILI9341 (240x320)
- ST7735 (128x160)
- ST7789 (240x240)
- Other TFT_eSPI compatible displays

## Complete Wiring Diagram (ASCII Art)

```
                           ESP32
                    ┌─────────
       ─────────┤ GPIO23 (MOSI)│
           ─────────┤ GPIO18 (SCLK)│
           ─────────┤ GPIO19 (MISO)│
           ─────────┤ GPIO27 (BL)  │
                    │              │
    Power  ─────────┤ 3.3V         │
           ─────────┤ GND          │
           ─────────┤ 5V (VIN)     │ (optional, for 5V sensors)
                    │              │
    USB    ─────────┤ USB Port     │ (programming & power)
                    └──────────────┘
```

## Power Considerations

### Current Draw Estimates

| Component | Sleep/Idle | Active | Peak |
|-----------|------------|--------|------|
| ESP32 | 10mA | 80mA | 240mA (WiFi TX) |
| BME280 | 3.6μA | 714μA | 714μA |
| DS18B20 | 1μA | 1.5mA | 1.5mA |
| Soil Moisture | - | 5mA | 5mA |
| TFT Display | - | 50mA | 100mA |
| **Total** | ~10mA | ~140mA | ~350mA |

### Power Supply Recommendations

**USB Power (Development):**
- USB provides 5V at up to 500mA
- Sufficient for development and testing
- Use quality USB cable to minimize voltage drop

**External Power (Deployment):**
- 5V 1A power supply recommended
- Connect to VIN pin (5V) and GND
- Provides stable power for all components
- Allows for future expansion

**Battery Power (Optional):**
- 3.7V LiPo battery with voltage regulator
- Capacity: 2000mAh minimum for 12+ hours operation
- Enable power management features in firmware
- See Task 24 documentation for power optimization

## Assembly Instructions

### Step 1: Prepare Components
1. Gather all components and tools
2. Test ESP32 with USB connection (upload blink sketch)
3. Verify sensor modules are functional

### Step 2: Wire Sensors
1. **Start with power rails:**
   - Connect ESP32 3.3V to breadboard positive rail
   - Connect ESP32 GND to breadboard negative rail

2. **Connect BME280 (I2C):**
   - VCC to 3.3V rail
   - GND to GND rail
   - SDA to GPIO21
   - SCL to GPIO22
   - Add pull-up resistors if needed

3. **Connect DS18B20 (OneWire):**
   - VCC to 3.3V rail
   - GND to GND rail
   - DATA to GPIO4
   - **Add 4.7kΩ pull-up resistor from DATA to 3.3V**

4. **Connect Soil Moisture:**
   - VCC to 3.3V or 5V (check sensor specs)
   - GND to GND rail
   - AOUT to GPIO32

### Step 3: Wire Display
1. **Connect TFT Display (SPI):**
   - VCC to 3.3V rail
   - GND to GND rail
   - CS to GPIO5
   - RESET to GPIO15
   - DC to GPIO2
   - MOSI to GPIO23
   - SCK to GPIO18
   - MISO to GPIO19 (if available)
   - LED to GPIO27 or 3.3V with resistor

### Step 4: Verify Connections
1. Double-check all connections against pin map
2. Verify no short circuits between power and ground
3. Check pull-up resistor on DS18B20
4. Ensure no loose connections

### Step 5: Power On and Test
1. Connect USB cable to ESP32
2. Open serial monitor (115200 baud)
3. Upload firmware
4. Verify startup screen on display
5. Check sensor readings in serial output

## Troubleshooting

### Display Not Working
- **Blank screen:** Check power connections (VCC, GND)
- **White screen:** Verify SPI pins (MOSI, SCK, CS)
- **Garbled display:** Check DC and RESET pins
- **No backlight:** Verify LED pin connection or resistor
- **Wrong colors:** Check TFT_eSPI configuration in User_Setup.h

### BME280 Not Detected
- **I2C address:** Try 0x76 or 0x77 in firmware
- **Pull-ups:** Add 4.7kΩ resistors on SDA and SCL if missing
- **Wiring:** Verify SDA on GPIO21, SCL on GPIO22
- **Power:** Check 3.3V supply is stable
- **Test:** Use I2C scanner sketch to detect address

### DS18B20 Not Reading
- **Pull-up resistor:** Verify 4.7kΩ resistor from DATA to 3.3V
- **Wiring:** Check DATA connected to GPIO4
- **Power:** Ensure VCC and GND connected
- **Multiple sensors:** Each sensor needs unique ROM address
- **Test:** Use OneWire scanner sketch to detect devices

### Soil Moisture Erratic Readings
- **ADC noise:** Firmware applies multi-sample averaging
- **Calibration:** Run calibration procedure (see CALIBRATION.md)
- **Power:** Ensure stable 3.3V or 5V supply
- **Wiring:** Check AOUT connected to GPIO32 (ADC1)
- **WiFi interference:** GPIO32 is ADC1, safe with WiFi

### WiFi Not Connecting
- **Credentials:** Verify SSID and password in configuration
- **Signal:** Check WiFi signal strength (RSSI)
- **Channel:** Some ESP32 boards don't support channels 12-14
- **Power:** Weak power supply can cause WiFi failures
- **Antenna:** Ensure PCB antenna is not obstructed

### General Issues
- **Brown-out reset:** Increase power supply capacity
- **Random crashes:** Check for loose connections
- **Sensor conflicts:** Verify no pin conflicts in PinConfig.h
- **Serial output garbled:** Check baud rate (115200)

## Testing Checklist

After wiring, verify each component:

- [ ] ESP32 powers on (LED indicator)
- [ ] Serial console output visible (115200 baud)
- [ ] Display shows startup screen
- [ ] BME280 detected and reading temperature
- [ ] DS18B20 detected and reading temperature
- [ ] Soil moisture reading ADC values
- [ ] WiFi connects to network
- [ ] Display cycles through pages
- [ ] Sensor data updates on display
- [ ] Serial console shows sensor readings

## Safety Notes

1. **Voltage Levels:** ESP32 GPIO pins are 3.3V tolerant. Do not connect 5V signals directly.
2. **Current Limits:** GPIO pins can source/sink max 12mA. Use transistors for high-current loads.
3. **ESD Protection:** Handle ESP32 with care to avoid electrostatic discharge damage.
4. **Power Supply:** Use regulated power supply to avoid voltage spikes.
5. **Heat:** ESP32 can get warm during WiFi transmission. Ensure adequate ventilation.

## Next Steps

After successful wiring:
1. Configure firmware via serial console (see SERIAL_CONSOLE.md)
2. Calibrate soil moisture sensor (see CALIBRATION.md)
3. Configure API endpoint and WiFi credentials
4. Deploy and monitor system operation

## References

- [ESP32 Pinout Reference](https://randomnerdtutorials.com/esp32-pinout-reference-gpios/)
- [BME280 Datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf)
- [DS18B20 Datasheet](https://datasheets.maximintegrated.com/en/ds/DS18B20.pdf)
- [TFT_eSPI Library](https://github.com/Bodmer/TFT_eSPI)
- [ESP32 ADC Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html)
