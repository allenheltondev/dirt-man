# ESP32 Sensor Firmware - Serial Console Guide

## Overview

The ESP32 sensor firmware provides a comprehensive serial console interface for configuration, diagnostics, and monitoring. This guide covers all available commands and their usage.

## Connecting to Serial Console

### Hardware Setup
1. Connect ESP32 to computer via USB cable
2. ESP32 will appear as a COM port (Windows) or /dev/ttyUSB* (Linux) or /dev/cu.* (Mac)

### Serial Terminal Settings
- **Baud Rate:** 115200
- **Data Bits:** 8
- **Parity:** None
- **Stop Bits:** 1
- **Flow Control:** None
- **Line Ending:** Newline (LF or CR+LF)

### Recommended Tools
- **Arduino IDE:** Tools → Serial Monitor
- **PlatformIO:** PlatformIO → Serial Monitor
- **PuTTY** (Windows)
- **screen** (Linux/Mac): `screen /dev/ttyUSB0 115200`
- **minicom** (Linux): `minicom -D /dev/ttyUSB0 -b 115200`

## Startup Output

When the ESP32 boots, you'll see output similar to:

```
===========================================
ESP32 Sensor Firmware v1.0.0
===========================================
[INIT] Initializing ConfigManager...
[INIT] Mounting LittleFS filesystem...
[INIT] LittleFS mounted successfully
[INIT] Loading configuration from file...
[CONFIG] Configuration loaded from /config.json
[INIT] Configuration loaded successfully
[INIT] Initializing DisplayManager...
[INIT] Display initialized successfully
[TOUCH] Starting touch detection...
[TOUCH] Probing XPT2046 (SPI)...
[TOUCH] XPT2046 not detected
[TOUCH] Probing FT6236 (I2C 0x38)...
[TOUCH] FT6236 detected successfully
[TOUCH] Touch detection complete: FT6236 (250ms)
[CONFIG] Touch-based config page enabled
[INIT] Initializing SensorManager...
[INIT] BME280 sensor initialized
[INIT] DS18B20 sensor initialized
[INIT] Soil moisture sensor initialized
[INIT] Initializing NetworkManager...
[INIT] Connecting to WiFi: MyNetwork
[WIFI] Connected! IP: 192.168.1.100
[INIT] Initialization complete
===========================================

Type 'help' for configuration menu
```

**Note:** If configuration file is missing or invalid, you'll see:
```
[CONFIG] Config file not found, trying NVS...
[CONFIG] Configuration loaded from NVS
```

**Note:** If required fields are missing, you'll see:
```
[CONFIG] Required fields missing: wifi_ssid, backend_url
[CONFIG] Entering provisioning mode
===========================================
PROVISIONING MODE
===========================================
Required configuration fields are missing.
Please configure via serial console.

Type 'help' for configuration menu
```

## Configuration Commands

### Main Menu

Type `config` or `help` to display the configuration menu:

```
=== Configuration Menu ===
config    - Show this menu
show      - Display current configuration
wifi      - Update WiFi credentials
api       - Update API endpoint and token
intervals - Update timing intervals
calibrate - Update soil moisture calibration
deviceid  - Update device identifier
save      - Save configuration to NVS
defaults  - Reset to default configuration
diag      - Show system diagnostics
help      - Show this menu
========================
```

### Show Current Configuration

**Command:** `show`

Displays all current configuration values with sensitive data masked:

```
=== Current Configuration ===
WiFi SSID: MyNetwork
WiFi Password: ********rd
API Endpoint
te WiFi SSID and password:

```
=== Update WiFi Credentials ===
Enter WiFi SSID: MyNetwork
Enter WiFi Password: MyPassword123
WiFi credentials updated
```

**Validation Rules:**
- SSID: 1-32 characters
- Password: 0 characters (open network) or 8-63 characters
- Changes take effect after `save` command and reboot

**Example:**
```
> wifi
Enter WiFi SSID: HomeNetwork
Enter WiFi Password: SecurePass123
WiFi credentials updated
> save
Configuration saved successfully
```

### Update API Configuration

**Command:** `api`

Interactive prompt to update API endpoint URL and authentication token:

```
=== Update API Configuration ===
Enter API Endpoint URL: https://api.myserver.com/data
Enter API Token: my-secret-token-12345
API configuration updated
```

**Validation Rules:**
- Endpoint must start with `http://` or `https://`
- Endpoint maximum length: 256 characters
- Token can be any string (no length limit)
- HTTPS recommended for production

**Example:**
```
> api
Enter API Endpoint URL: https://api.example.com/sensor-data
Enter API Token: Bearer abc123xyz789
API configuration updated
> save
Configuration saved successfully
```

### Update Timing Intervals

**Command:** `intervals`

Interactive prompt to update reading, publish, and display intervals:

```
=== Update Intervals ===
Enter Reading Interval (ms, 1000-3600000): 10000
Enter Publish Interval (samples, 1-120): 30
Enter Page Cycle Interval (ms, 1000-60000): 15000
Intervals updated
```

**Parameters:**

1. **Reading Interval** (ms)
   - How often to read sensors
   - Range: 1000-3600000 ms (1 second to 1 hour)
   - Default: 5000 ms (5 seconds)
   - Lower values = more frequent readings, higher power consumption

2. **Publish Interval** (samples)
   - How many readings to average before transmitting
   - Range: 1-120 samples
   - Default: 20 samples
   - Higher values = less frequent transmissions, more averaging

3. **Page Cycle Interval** (ms)
   - How often to cycle display pages
   - Range: 1000-60000 ms (1-60 seconds)
   - Default: 10000 ms (10 seconds)

**Example:**
```
> intervals
Enter Reading Interval (ms, 1000-3600000): 30000
Enter Publish Interval (samples, 1-120): 10
Enter Page Cycle Interval (ms, 1000-60000): 20000
Intervals updated
> save
Configuration saved successfully
```

**Transmission Frequency Calculation:**
```
Transmission Interval = Reading Interval × Publish Interval
Example: 5000 ms × 20 samples = 100 seconds between transmissions
```

### Calibrate Soil Moisture Sensor

**Command:** `calibrate`

Interactive two-point calibration procedure for soil moisture sensor:

```
=== Update Soil Moisture Calibration ===
Place sensor in DRY soil and press Enter
[Press Enter]
Enter Dry ADC value (0-4095): 3000

Place sensor in WET soil and press Enter
[Press Enter]
Enter Wet ADC value (0-4095): 1500

Calibration updated
Dry ADC: 3000
Wet ADC: 1500
```

**Calibration Procedure:**

1. **Dry Calibration:**
   - Remove sensor from soil or place in completely dry soil
   - Wait 10 seconds for reading to stabilize
   - Note the ADC value from sensor readings
   - Enter this value as "Dry ADC"

2. **Wet Calibration:**
   - Place sensor in saturated soil or water
   - Wait 10 seconds for reading to stabilize
   - Note the ADC value from sensor readings
   - Enter this value as "Wet ADC"

**Validation Rules:**
- Both values must be 0-4095 (12-bit ADC range)
- Dry ADC must be greater than Wet ADC
- Typical values: Dry ~2500-3500, Wet ~1000-2000

**Example:**
```
> calibrate
Place sensor in DRY soil and press Enter
[Press Enter]
Enter Dry ADC value (0-4095): 3200
Place sensor in WET soil and press Enter
[Press Enter]
Enter Wet ADC value (0-4095): 1400
Calibration updated
Dry ADC: 3200
Wet ADC: 1400
> save
Configuration saved successfully
```

See [CALIBRATION.md](CALIBRATION.md) for detailed calibration guide.

### Update Device Identifier

**Command:** `deviceid`

Set a unique identifier for this device:

```
=== Update Device ID ===
Enter Device ID: greenhouse-sensor-01
Device ID updated
```

**Usage:**
- Identifies this device in transmitted data
- Used in batch_id generation
- Helps distinguish multiple devices in same system
- Can be any string (alphanumeric recommended)

**Example:**
```
> deviceid
Enter Device ID: garden-north-sensor
Device ID updated
> save
Configuration saved successfully
```

### Save Configuration

**Command:** `save`

Saves current configuration to non-volatile storage (NVS):

```
> save
Configuration saved successfully
```

**Important:**
- Configuration is validated before saving
- Invalid configuration will not be saved
- Changes persist across reboots
- No automatic save - you must explicitly save

**Validation Errors:**
```
> save
Invalid configuration - cannot save
```

If you see this error, use `show` to review configuration and fix invalid values.

### Reset to Defaults

**Command:** `defaults`

Resets all configuration to factory defaults:

```
> defaults
Configuration reset to defaults
```

**Default Values:**
- WiFi SSID: (empty)
- WiFi Password: (empty)
- API Endpoint: https://api.example.com/sensor-data
- API Token: (empty)
- Device ID: esp32-sensor-001
- Reading Interval: 5000 ms
- Publish Interval: 20 samples
- Page Cycle Interval: 10000 ms
- Soil Dry ADC: 3000
- Soil Wet ADC: 1500
- Temperature: Celsius
- Soil Thresholds: 30% (low), 70% (high)
- Battery Mode: Disabled

**Note:** Use `save` after `defaults` to persist the reset.

## Diagnostic Commands

### System Diagnostics

**Command:** `diag`

Displays comprehensive system diagnostics:

```
=== System Diagnostics ===
Uptime: 0h 15m
Free Heap: 245760 bytes
WiFi Status: Connected
WiFi RSSI: -65 dBm
WiFi IP: 192.168.1.100

Sensor Status:
  BME280: OK
  DS18B20: OK
  Soil Moisture: OK

Last Sensor Read: 2 seconds ago
Last Transmission: 45 seconds ago
Queue Depth: 3 readings buffered

Error Counters:
  Sensor Failures: 0
  Network Failures: 1
  Buffer Overflows: 0

Last Error: HTTP POST timeout
Last HTTP Status: 200 OK
============================
```

**Diagnostic Information:**

1. **System Status:**
   - Uptime in hours and minutes
   - Free heap memory (should be > 100KB)
   - WiFi connection status and signal strength

2. **Sensor Status:**
   - Individual sensor health (OK/ERROR)
   - Time since last successful reading

3. **Network Status:**
   - Last successful transmission time
   - Buffered readings count
   - Last HTTP status code

4. **Error Tracking:**
   - Cumulative error counters
   - Last error message
   - Helps identify recurring issues

## Monitoring Output

### Continuous Sensor Readings

The firmware continuously outputs sensor readings to serial console:

```
[SENSOR] BME280 Temp: 22.5°C, Humidity: 45.2%, Pressure: 1013.25 hPa
[SENSOR] DS18B20 Temp: 21.8°C
[SENSOR] Soil Moisture: 62.3% (Raw ADC: 2100)
[DATA] Added reading to averaging buffer (15/20)
```

### Network Events

```
[WIFI] Connecting to WiFi: MyNetwork
[WIFI] Connected! IP: 192.168.1.100
[WIFI] RSSI: -65 dBm

[NET] Preparing to send 3 buffered readings
[NET] HTTP POST to https://api.example.com/sensor-data
[NET] Response: 200 OK
[NET] Acknowledged batch IDs: 3
[DATA] Cleared 3 acknowledged readings from buffer
```

### Error Messages

```
[ERROR] BME280 sensor read failed
[ERROR] WiFi connection lost
[ERROR] HTTP POST failed: timeout
[ERROR] Buffer overflow: discarding oldest reading
```

### Display Updates

```
[DISPLAY] Page changed: SUMMARY → GRAPH_BME280_TEMP
[DISPLAY] Backlight dimmed (burn-in protection)
```

## Configuration Workflow

### Initial Setup (First Boot)

1. Connect to serial console (115200 baud)
2. Wait for startup messages
3. Configure WiFi:
   ```
   > wifi
   Enter WiFi SSID: MyNetwork
   Enter WiFi Password: MyPassword123
   ```
4. Configure API:
   ```
   > api
   Enter API Endpoint URL: https://api.myserver.com/data
   Enter API Token: my-token-12345
   ```
5. Set device ID:
   ```
   > deviceid
   Enter Device ID: sensor-01
   ```
6. Save configuration:
   ```
   > save
   ```
7. Reboot ESP32 (press reset button or power cycle)

### Calibration Workflow

1. Connect to serial console
2. Run calibration:
   ```
   > calibrate
   ```
3. Follow dry/wet calibration steps
4. Save configuration:
   ```
   > save
   ```
5. Verify readings show percentage values

### Troubleshooting Workflow

1. Check current configuration:
   ```
   > show
   ```
2. Run diagnostics:
   ```
   > diag
   ```
3. Review error counters and last error
4. Check sensor status
5. Verify WiFi connection
6. Monitor continuous output for errors

### Provisioning Mode Workflow

Provisioning mode is entered automatically when required configuration fields are missing. This typically occurs on:
- First boot of new device
- After factory reset
- When config file is corrupt or missing required fields
- When NVS storage is empty or invalid

**Entry Conditions:**
- Missing `wifi_ssid`
- Missing `wifi_password` (must be present, can be empty string)
- Missing `backend_url`
- Any combination of the above

**Provisioning Mode Indicators:**
1. **Serial Console:**
   ```
   ===========================================
   PROVISIONING MODE
   ===========================================
   Required configuration fields are missing.
   Please configure via serial console.

   Missing fields: wifi_ssid, backend_url

   Type 'help' for configuration menu
   ```

2. **TFT Display:**
   - Shows "Provisioning Mode" message
   - Lists missing required fields
   - Instructions: "Use Serial Console" or "Use Touch Interface" (if touch detected)

**Provisioning Steps:**

1. **Connect to serial console** (115200 baud)

2. **Configure required fields:**
   ```
   > wifi
   Enter WiFi SSID: MyNetwork
   Enter WiFi Password: MyPassword123
   WiFi credentials updated

   > api
   Enter API Endpoint URL: https://api.example.com/sensors
   Enter API Token: my-token-12345
   API configuration updated
   ```

3. **Optionally configure other settings:**
   ```
   > deviceid
   Enter Device ID: sensor-01
   Device ID updated

   > intervals
   [Configure as needed]
   ```

4. **Save configuration:**
   ```
   > save
   Configuration saved successfully
   Saving to NVS...
   Saving to config file...
   Configuration saved to both NVS and /config.json
   ```

5. **Automatic reboot:**
   ```
   Rebooting in 3 seconds...
   ```
   Device automatically reboots after successful provisioning

6. **Verify normal operation:**
   - Watch boot sequence
   - Confirm "Configuration loaded" message
   - Verify WiFi connection
   - Check sensors reading

**Validation During Provisioning:**

The firmware validates all fields as you enter them:

**WiFi SSID:**
- Must be 1-32 characters
- Cannot be empty

**WiFi Password:**
- Must be 0 characters (open network) or 8-63 characters
- Can be empty string for open networks

**Backend URL:**
- Must start with `http://` or `https://`
- Maximum 128 characters
- Must be valid URL format

**Invalid Input Example:**
```
> api
Enter API Endpoint URL: invalid-url
Error: URL must start with http:// or https://
API configuration NOT updated
```

**Exiting Provisioning Mode:**

Provisioning mode can only be exited by:
1. Providing all required fields
2. Saving configuration successfully
3. Automatic reboot

**Cannot exit provisioning mode by:**
- Manual reboot (will re-enter provisioning mode)
- Power cycle (will re-enter provisioning mode)
- Resetting to defaults without providing required fields

**Provisioning with Config File:**

If you prefer to use a config file instead of serial console:

1. **Create config.json** with required fields:
   ```json
   {
     "schema_version": 1,
     "checksum": "CALCULATE_ME",
     "wifi_ssid": "MyNetwork",
     "wifi_password": "MyPassword123",
     "backend_url": "https://api.example.com/sensors"
   }
   ```

2. **Calculate checksum** (see [CONFIG_FILE_EXAMPLE.md](CONFIG_FILE_EXAMPLE.md))

3. **Upload to filesystem:**
   ```bash
   pio run -e esp32dev --target uploadfs
   ```

4. **Reboot device** - will load from config file and exit provisioning mode

**Provisioning with Touch Interface (if available):**

If touch screen is detected during provisioning mode:

1. **Touch interface enabled** for configuration
2. **On-screen forms** for entering required fields
3. **Validation feedback** shown immediately
4. **Save button** writes to both NVS and config file
5. **Automatic reboot** after successful save

**Troubleshooting Provisioning Mode:**

**Stuck in Provisioning Mode:**

Problem: Device keeps entering provisioning mode after reboot

Solutions:
1. Verify all required fields entered:
   ```
   > show
   ```
2. Check for validation errors in serial output
3. Ensure `save` command executed successfully
4. Verify config file has correct format (if using file method)
5. Check NVS partition not corrupted

**Save Command Fails:**

Problem: `save` command returns error

Solutions:
1. Check field validation errors
2. Verify NVS partition accessible
3. Check LittleFS filesystem mounted
4. Review serial output for specific error
5. Try `defaults` then reconfigure

**Config File Not Loading:**

Problem: Config file present but provisioning mode still entered

Solutions:
1. Verify all required fields in config file
2. Check checksum is valid
3. Verify JSON syntax correct
4. Check file location is `/config.json` (root of LittleFS)
5. Review serial output for parse errors

## Advanced Configuration

### Adjusting for Battery Operation

For battery-powered deployments, increase intervals to reduce power consumption:

```
> intervals
Enter Reading Interval (ms, 1000-3600000): 60000
Enter Publish Interval (samples, 1-120): 10
Enter Page Cycle Interval (ms, 1000-60000): 30000
Intervals updated
> save
```

This configuration:
- Reads sensors every 60 seconds
- Transmits every 10 readings (10 minutes)
- Cycles display every 30 seconds

### High-Frequency Monitoring

For real-time monitoring applications:

```
> intervals
Enter Reading Interval (ms, 1000-3600000): 1000
Enter Publish Interval (samples, 1-120): 5
Enter Page Cycle Interval (ms, 1000-60000): 5000
Intervals updated
> save
```

This configuration:
- Reads sensors every second
- Transmits every 5 readings (5 seconds)
- Cycles display every 5 seconds

**Warning:** High-frequency operation increases power consumption and network traffic.

### Development/Testing Mode

For development with local HTTP server:

```
> api
Enter API Endpoint URL: http://192.168.1.50:8080/data
Enter API Token: test-token
API configuration updated
> save
```

**Note:** HTTP (not HTTPS) is acceptable for local testing but not recommended for production.

## Security Considerations

### Sensitive Data Protection

1. **Serial Output:**
   - Passwords and tokens are masked in `show` command
   - Only last 2 characters visible
   - Full values never displayed

2. **Display:**
   - Sensitive data never shown on TFT display
   - Only device ID and non-sensitive settings visible

3. **Storage:**
   - Configuration stored in encrypted NVS partition
   - Credentials persist across reboots
   - Factory reset clears all stored data

### Best Practices

1. **Change Default Device ID:**
   - Use unique identifier for each device
   - Helps track devices in multi-device deployments

2. **Use HTTPS:**
   - Always use HTTPS endpoints in production
   - Protects data in transit
   - Validates server certificates

3. **Strong WiFi Password:**
   - Use WPA2 with strong password
   - Minimum 8 characters required

4. **Secure API Token:**
   - Use long, random tokens
   - Rotate tokens periodically
   - Never share tokens publicly

## Troubleshooting

### Cannot Connect to Serial Console

**Problem:** No output in serial monitor

**Solutions:**
- Verify correct COM port selected
- Check baud rate is 115200
- Try different USB cable
- Press reset button on ESP32
- Check USB drivers installed

### Configuration Won't Save

**Problem:** `save` command fails

**Solutions:**
- Run `show` to check current configuration
- Verify all values are within valid ranges
- Check WiFi SSID length (1-32 characters)
- Ensure API endpoint starts with http:// or https://
- Verify calibration: Dry ADC > Wet ADC

### WiFi Not Connecting

**Problem:** WiFi connection fails after configuration

**Solutions:**
- Verify SSID is correct (case-sensitive)
- Check password is correct
- Ensure WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
- Check WiFi signal strength
- Try moving ESP32 closer to router

### Sensor Readings Not Appearing

**Problem:** No sensor data in serial output

**Solutions:**
- Check sensor wiring (see HARDWARE_WIRING.md)
- Run `diag` to check sensor status
- Verify power supply is adequate
- Check for loose connections
- Review error messages in serial output

## Command Reference Quick Guide

| Command | Description | Interactive |
|---------|-------------|-------------|
| `config` | Show configuration menu | No |
| `show` | Display current configuration | No |
| `wifi` | Update WiFi credentials | Yes |
| `api` | Update API endpoint and token | Yes |
| `intervals` | Update timing intervals | Yes |
| `calibrate` | Calibrate soil moisture sensor | Yes |
| `deviceid` | Update device identifier | Yes |
| `save` | Save configuration to NVS and config file | No |
| `defaults` | Reset to default configuration | No |
| `diag` | Show system diagnostics | No |
| `help` | Show configuration menu | No |

**Interactive:** Commands that prompt for additional input

**Note:** The `save` command now saves to both NVS storage and `/config.json` file (if LittleFS is mounted). This ensures configuration persists and is available via both methods.

## Configuration Sources

The firmware supports multiple configuration sources with the following priority:

1. **Config File** (`/config.json` on LittleFS)
   - Loaded first if present and valid
   - See [CONFIG_FILE_EXAMPLE.md](CONFIG_FILE_EXAMPLE.md)
   - Recommended for automated deployments

2. **NVS Storage** (Non-Volatile Storage)
   - Used if config file missing or invalid
   - Traditional serial console configuration
   - Automatically migrated to config file on first boot

3. **Defaults**
   - Applied to optional fields only
   - Never used for required fields (wifi_ssid, wifi_password, backend_url)

**Provisioning Mode:** If required fields are missing from all sources, device enters provisioning mode and requires configuration before normal operation.

## Next Steps

After configuration:
1. Verify sensor readings in serial output
2. Check display shows current values
3. Confirm WiFi connection established
4. Monitor first data transmission
5. Review API server receives data correctly

For configuration file format and examples, see [CONFIG_FILE_EXAMPLE.md](CONFIG_FILE_EXAMPLE.md)

For detailed calibration procedures, see [CALIBRATION.md](CALIBRATION.md)

For API endpoint requirements, see [API_SPECIFICATION.md](API_SPECIFICATION.md)

For deployment checklist, see [DEPLOYMENT.md](DEPLOYMENT.md)
