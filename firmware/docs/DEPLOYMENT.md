# ESP32 Sensor Firmware - Deployment Checklist

## Overview

This comprehensive checklist guides you through deploying the ESP32 sensor firmware from initial setup to production operation. Follow each section in order to ensure a successful deployment.

## Pre-Deployment Preparation

### Hardware Verification

- [ ] **ESP32 board tested and functional**
  - USB connection works
  - Can upload firmware
  - Serial console accessible at 115200 baud

- [ ] **All sensors connected and tested**
  - BME280 (I2C) on GPIO21/22
  - DS18B20 (OneWire) on GPIO4 with 4.7kΩ pull-up
  - Soil moisture sensor (ADC) on GPIO32
  - All sensors reading values

- [ ] **Display connected and functional**
  - TFT display on SPI pins (GPIO5, 15, 2, 23, 18, 19)
  - Startup screen visible
  - All display pages render correctly
  - Backlight control working (GPIO27)

- [ ] **Power supply adequate**
  - 5V 1A minimum for USB power
  - External power supply if using battery
  - Voltage stable under WiFi transmission load

- [ ] **Enclosure prepared (if applicable)**
  - Weatherproof for outdoor deployment
  - Ventilation for BME280 sensor
  - Cable glands for sensor wires
  - Mounting hardware ready

### Firmware Preparation

- [ ] **Latest firmware compiled and tested**
  - No compilation errors
  - Unit tests passing
  - Integration tests passing
  - Version number documented

- [ ] **Firmware uploaded to ESP32**
  - Upload successful
  - Serial output shows startup messages
  - No error messages during boot
  - All managers initialize successfully

- [ ] **Configuration defaults reviewed**
  - Default intervals appropriate
  - Default device ID unique
  - Default API endpoint placeholder present

### Network Preparation

- [ ] **WiFi network accessible**
  - 2.4GHz WiFi (ESP32 doesn't support 5GHz)
  - SSID and password known
  - Signal strength adequate at deployment location (RSSI > -75 dBm)
  - Internet connectivity available

- [ ] **API endpoint ready**
  - Server running and accessible
  - HTTPS certificate valid (if using HTTPS)
  - API token generated
  - Endpoint tested with curl or Postman

- [ ] **Firewall rules configured**
  - Outbound HTTPS (port 443) allowed
  - Or outbound HTTP (port 80) for development
  - DNS resolution working

## Initial Configuration

### Step 1: Connect to Serial Console

- [ ] Connect ESP32 via USB
- [ ] Open serial terminal (115200 baud, 8N1)
- [ ] Verify startup messages appear
- [ ] Type `help` to see configuration menu

### Step 2: Configure WiFi

```
> wifi
Enter WiFi SSID: YourNetworkName
Enter WiFi Password: YourPassword123
WiFi credentials updated
```

- [ ] WiFi SSID entered correctly (case-sensitive)
- [ ] WiFi password entered correctly
- [ ] Validation passed (no error messages)

### Step 3: Configure API Endpoint

```
> api
Enter API Endpoint URL: https://api.example.com/sensor-data
Enter API Token: your-secret-token-12345
API configuration updated
```

- [ ] API endpoint URL correct (starts with http:// or https://)
- [ ] API token correct
- [ ] Validation passed

### Step 4: Set Device Identifier

```
> deviceid
Enter Device ID: greenhouse-sensor-01
Device ID updated
```

- [ ] Device ID unique across all devices
- [ ] Device ID descriptive and meaningful
- [ ] Device ID documented in deployment records

### Step 5: Configure Intervals (Optional)

```
> intervals
Enter Reading Interval (ms, 1000-3600000): 5000
Enter Publish Interval (samples, 1-120): 20
Enter Page Cycle Interval (ms, 1000-60000): 10000
Intervals updated
```

- [ ] Reading interval appropriate for application
- [ ] Publish interval balances frequency vs. network usage
- [ ] Page cycle interval comfortable for viewing

**Default intervals:**
- Reading: 5000 ms (5 seconds)
- Publish: 20 samples (100 seconds with default reading interval)
- Page cycle: 10000 ms (10 seconds)

### Step 6: Save Configuration

```
> save
Configuration saved successfully
```

- [ ] Configuration saved without errors
- [ ] Confirmation message received

### Step 7: Verify Configuration

```
> show
```

- [ ] All settings correct
- [ ] Sensitive data masked (passwords, tokens)
- [ ] No unexpected values

### Step 8: Reboot and Test

- [ ] Press reset button or power cycle
- [ ] Watch serial output during boot
- [ ] Verify WiFi connection established
- [ ] Check IP address assigned
- [ ] Confirm sensors reading values

## Sensor Calibration

### Soil Moisture Calibration

**See [CALIBRATION.md](CALIBRATION.md) for detailed procedures**

- [ ] **Choose calibration method:**
  - [ ] Quick (air and water) - 5 minutes
  - [ ] Soil-based (recommended) - 30 minutes
  - [ ] In-situ (most accurate) - 24+ hours

- [ ] **Perform dry calibration:**
  - [ ] Sensor in dry condition
  - [ ] Reading stabilized
  - [ ] Dry ADC value recorded

- [ ] **Perform wet calibration:**
  - [ ] Sensor in wet condition
  - [ ] Reading stabilized
  - [ ] Wet ADC value recorded

- [ ] **Enter calibration values:**
  ```
  > calibrate
  Enter Dry ADC value (0-4095): 3000
  Enter Wet ADC value (0-4095): 1500
  Calibration updated
  ```

- [ ] **Save calibration:**
  ```
  > save
  Configuration saved successfully
  ```

-

  - [ ] Display shows WiFi indicator

- [ ] **Test WiFi stability:**
  - [ ] Monitor for 10 minutes
  - [ ] No disconnections
  - [ ] RSSI stable

- [ ] **Test WiFi reconnection:**
  - [ ] Disable WiFi router briefly
  - [ ] Verify firmware attempts reconnection
  - [ ] Verify successful reconnection
  - [ ] Check buffered data transmitted after reconnection

### API Transmission Test

- [ ] **Monitor first transmission:**
  - [ ] Wait for publish interval to elapse
  - [ ] Serial output shows "HTTP POST to..."
  - [ ] Response code 200 OK received
  - [ ] Acknowledged batch IDs received

- [ ] **Verify data on server:**
  - [ ] Check server logs for received data
  - [ ] Verify JSON payload structure correct
  - [ ] Confirm sensor values reasonable
  - [ ] Check batch_id format correct

- [ ] **Test transmission retry:**
  - [ ] Temporarily disable API endpoint
  - [ ] Verify firmware buffers data
  - [ ] Re-enable API endpoint
  - [ ] Verify buffered data transmitted
  - [ ] Check no data loss

- [ ] **Test error handling:**
  - [ ] Return 500 error from server
  - [ ] Verify firmware retries with backoff
  - [ ] Check error counters increment
  - [ ] Verify eventual success

## Display Verification

### Display Functionality

- [ ] **Startup screen:**
  - [ ] Firmware version displayed
  - [ ] "Initializing..." message shown
  - [ ] Transitions to summary page

- [ ] **Summary page:**
  - [ ] All sensor values displayed
  - [ ] Units shown correctly (°C, %, hPa)
  - [ ] WiFi indicator present
  - [ ] Queue depth shown
  - [ ] Uptime displayed
  - [ ] Min/max values shown

- [ ] **Graph pages:**
  - [ ] Graphs render for each sensor
  - [ ] Line graphs smooth and readable
  - [ ] Axes labeled with min/max
  - [ ] Data points accumulate over time
  - [ ] Smooth scrolling works

- [ ] **Page cycling:**
  - [ ] Pages cycle automatically
  - [ ] Cycle interval correct (default 10 seconds)
  - [ ] All pages displayed in sequence
  - [ ] No display glitches

- [ ] **Status indicators:**
  - [ ] Sensor health badges (green/yellow/red)
  - [ ] WiFi signal strength indicator
  - [ ] Transmission success indicator
  - [ ] Error indicators when appropriate

- [ ] **Burn-in protection:**
  - [ ] Backlight dims after 30 minutes
  - [ ] Can be re-enabled by activity

## System Diagnostics

### Run Diagnostics Command

```
> diag
```

- [ ] **System status healthy:**
  - [ ] Uptime incrementing
  - [ ] Free heap > 100KB
  - [ ] WiFi connected
  - [ ] WiFi RSSI > -75 dBm

- [ ] **Sensor status OK:**
  - [ ] BME280: OK
  - [ ] DS18B20: OK
  - [ ] Soil Moisture: OK

- [ ] **Recent activity:**
  - [ ] Last sensor read < 10 seconds ago
  - [ ] Last transmission < 5 minutes ago
  - [ ] Queue depth reasonable (< 10)

- [ ] **Error counters low:**
  - [ ] Sensor failures: 0-2
  - [ ] Network failures: 0-2
  - [ ] Buffer overflows: 0

- [ ] **Last error acceptable:**
  - [ ] No critical errors
  - [ ] Transient errors only

### Monitor Serial Output

- [ ] **Sensor readings appearing:**
  - [ ] BME280 temperature, humidity, pressure
  - [ ] DS18B20 temperature
  - [ ] Soil moisture percentage and raw ADC
  - [ ] Values reasonable and stable

- [ ] **Network events normal:**
  - [ ] WiFi connection maintained
  - [ ] Periodic transmissions successful
  - [ ] Response codes 200 OK
  - [ ] Acknowledged batch IDs received

- [ ] **No error messages:**
  - [ ] No sensor read failures
  - [ ] No network timeouts
  - [ ] No buffer overflows
  - [ ] No memory warnings

## Physical Installation

### Sensor Placement

- [ ] **BME280 (ambient sensor):**
  - [ ] Mounted in ventilated enclosure
  - [ ] Protected from direct sunlight
  - [ ] Protected from rain/moisture
  - [ ] Away from heat sources

- [ ] **DS18B20 (probe sensor):**
  - [ ] Probe inserted at desired depth
  - [ ] Good thermal contact
  - [ ] Cable strain relief
  - [ ] Waterproof if needed

- [ ] **Soil moisture sensor:**
  - [ ] Inserted vertically in soil
  - [ ] Depth appropriate for application (typically 5-15 cm)
  - [ ] Good soil contact
  - [ ] Away from irrigation emitters
  - [ ] Representative of monitored area

### Enclosure Installation

- [ ] **ESP32 and display protected:**
  - [ ] Weatherproof enclosure if outdoor
  - [ ] Ventilation for heat dissipation
  - [ ] Display visible and readable
  - [ ] USB port accessible for maintenance

- [ ] **Power supply secured:**
  - [ ] Power cable strain relief
  - [ ] Connections waterproof
  - [ ] Power supply protected from weather
  - [ ] Backup power if critical (battery/UPS)

- [ ] **Mounting secure:**
  - [ ] Enclosure firmly mounted
  - [ ] Won't be disturbed by wind/animals
  - [ ] Accessible for maintenance
  - [ ] Cables protected from damage

## 24-Hour Burn-In Test

### Initial Monitoring Period

- [ ] **Hour 1: Intensive monitoring**
  - [ ] Watch serial output continuously
  - [ ] Verify sensor readings stable
  - [ ] Check first transmission successful
  - [ ] Monitor display updates

- [ ] **Hour 2-4: Periodic checks**
  - [ ] Check every 30 minutes
  - [ ] Verify WiFi maintained
  - [ ] Check transmissions successful
  - [ ] Monitor error counters

- [ ] **Hour 4-24: Occasional checks**
  - [ ] Check every 2-4 hours
  - [ ] Verify system still running
  - [ ] Check data appearing on server
  - [ ] Review error logs

### Burn-In Checklist

- [ ] **System stability:**
  - [ ] No crashes or reboots
  - [ ] Uptime incrementing continuously
  - [ ] Memory not leaking (free heap stable)

- [ ] **Sensor reliability:**
  - [ ] All sensors reading consistently
  - [ ] No sensor failures
  - [ ] Values within expected ranges

- [ ] **Network reliability:**
  - [ ] WiFi connection maintained
  - [ ] All transmissions successful
  - [ ] No excessive retries
  - [ ] Server receiving all data

- [ ] **Display reliability:**
  - [ ] Display updating correctly
  - [ ] No display glitches
  - [ ] Page cycling working
  - [ ] Backlight dimming after 30 minutes

- [ ] **Data quality:**
  - [ ] Sensor readings reasonable
  - [ ] Calibration accurate
  - [ ] Timestamps correct
  - [ ] No data gaps on server

## Production Deployment

### Final Verification

- [ ] **All tests passed:**
  - [ ] Hardware tests complete
  - [ ] Configuration verified
  - [ ] Calibration accurate
  - [ ] Network tests successful
  - [ ] 24-hour burn-in passed

- [ ] **Documentation complete:**
  - [ ] Device ID recorded
  - [ ] Calibration values documented
  - [ ] Installation location noted
  - [ ] Configuration backed up

- [ ] **Monitoring configured:**
  - [ ] Server monitoring active
  - [ ] Alerts configured for failures
  - [ ] Dashboard showing data
  - [ ] Backup/archival configured

### Deployment Sign-Off

- [ ] **Deployment information recorded:**
  - Device ID: ___________________________
  - Location: ___________________________
  - Installation Date: ___________________________
  - Installed By: ___________________________
  - Firmware Version: ___________________________

- [ ] **Configuration documented:**
  - WiFi SSID: ___________________________
  - API Endpoint: ___________________________
  - Reading Interval: ___________________________
  - Publish Interval: ___________________________

- [ ] **Calibration documented:**
  - Calibration Date: ___________________________
  - Dry ADC: ___________________________
  - Wet ADC: ___________________________
  - Next Calibration: ___________________________

- [ ] **Contact information:**
  - Primary Contact: ___________________________
  - Phone: ___________________________
  - Email: ___________________________

## Ongoing Maintenance

### Daily Checks (First Week)

- [ ] Verify data appearing on server
- [ ] Check for error messages
- [ ] Monitor WiFi connection stability
- [ ] Review sensor readings for anomalies

### Weekly Checks (First Month)

- [ ] Review error counters
- [ ] Check free heap memory
- [ ] Verify calibration accuracy
- [ ] Clean display if needed

### Monthly Checks (Ongoing)

- [ ] Review system uptime
- [ ] Check for firmware updates
- [ ] Verify sensor readings reasonable
- [ ] Clean sensors if needed
- [ ] Check enclosure condition

### Semi-Annual Maintenance

- [ ] Recalibrate soil moisture sensor
- [ ] Clean all sensors thoroughly
- [ ] Check all connections
- [ ] Update firmware if available
- [ ] Review and optimize configuration

### Annual Maintenance

- [ ] Full system inspection
- [ ] Replace sensors if degraded
- [ ] Update firmware to latest version
- [ ] Review deployment documentation
- [ ] Plan for next year

## Troubleshooting Quick Reference

### System Won't Boot

1. Check power supply
2. Verify USB cable
3. Re-upload firmware
4. Check serial output for errors

### WiFi Won't Connect

1. Verify SSID and password
2. Check 2.4GHz WiFi available
3. Move closer to router
4. Check router settings

### Sensors Not Reading

1. Check wiring connections
2. Verify pin assignments
3. Run diagnostics command
4. Check sensor power

### Display Not Working

1. Check SPI connections
2. Verify TFT_eSPI configuration
3. Check backlight connection
4. Re-upload firmware

### Data Not Reaching Server

1. Verify API endpoint URL
2. Check API token
3. Test endpoint with curl
4. Review server logs

## Support Resources

### Documentation

- [HARDWARE_WIRING.md](HARDWARE_WIRING.md) - Wiring diagrams and pin assignments
- [SERIAL_CONSOLE.md](SERIAL_CONSOLE.md) - Configuration commands
- [API_SPECIFICATION.md](API_SPECIFICATION.md) - API endpoint requirements
- [CALIBRATION.md](CALIBRATION.md) - Sensor calibration procedures

### Online Resources

- ESP32 Documentation: https://docs.espressif.com/
- TFT_eSPI Library: https://github.com/Bodmer/TFT_eSPI
- BME280 Datasheet: https://www.bosch-sensortec.com/
- DS18B20 Datasheet: https://datasheets.maximintegrated.com/

### Community Support

- ESP32 Forum: https://www.esp32.com/
- Arduino Forum: https://forum.arduino.cc/
- PlatformIO Community: https://community.platformio.org/

## Deployment Success Criteria

Your deployment is successful when:

✓ System runs continuously for 24+ hours without errors

✓ All sensors reading accurate values

✓ WiFi connection stable (RSSI > -75 dBm)

✓ Data transmitting successfully to server

✓ Display showing current information

✓ Error counters remain low (< 5 per 24 hours)

✓ Free heap memory stable (> 100KB)

✓ Calibration accurate and verified

✓ Documentation complete and filed

✓ Monitoring and alerts configured

**Congratulations on your successful deployment!**

## Deployment Record

Use this section to record deployment details:

```
DEPLOYMENT RECORD

Device Information:
  Device ID: _______________________
  Firmware Version: _______________________
  Hardware Revision: _______________________

Installation:
  Location: _______________________
  Installation Date: _______________________
  Installed By: _______________________

Configuration:
  WiFi SSID: _______________________
  API Endpoint: _______________________
  Reading Interval: _______________________
  Publish Interval: _______________________

Calibration:
  Calibration Date: _______________________
  Calibration Method: _______________________
  Dry ADC: _______________________
  Wet ADC: _______________________
  Next Calibration: _______________________

Testing:
  Hardware Test: PASS / FAIL
  Network Test: PASS / FAIL
  Sensor Test: PASS / FAIL
  24-Hour Burn-In: PASS / FAIL

Sign-Off:
  Deployed By: _______________________
  Signature: _______________________
  Date: _______________________

Notes:
  _______________________
  _______________________
  _______________________
```
