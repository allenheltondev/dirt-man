# Configuration File Guide

## Overview

The ESP32 sensor firmware supports configuration via a JSON file stored on the LittleFS filesystem. This provides an alternative to serial console configuration, especially useful for devices without touch screens or for automated deployments.

## File Location

The configuration file must be located at:
```
/config.json
```

This is the root of the LittleFS filesystem partition on the ESP32.

## Basic Example

Here's a minimal configuration file with only required fields:

```json
{
  "schema_version": 1,
  "checksum": "",
  "wifi_ssid": "MyHomeNetwork",
  "wifi_password": "MySecurePassword123",
  "backend_url": "https://api.example.com/sensor-data"
}
```

**Note:** The `checksum` field should be calculated after filling in all other fields (see Checksum Calculation section below).

## Complete Example

Here's a full configuration with all optional fields specified:

```json
{
  "schema_version": 1,
  "checksum": "A1B2C3D4",
  "wifi_ssid": "MyHomeNetwork",
  "wifi_password": "MySecurePassword123",
  "backend_url": "https://api.example.com/sensor-data",
  "friendly_name": "Garden Sensor North",
  "display_brightness": 200,
  "data_upload_interval": 300,
  "sensor_read_interval": 30,
  "enable_deep_sleep": false
}
```

## Field Reference

### Required Fields

These fields MUST be present for the device to operate normally. If any required field is missing, the device enters provisioning mode.

#### `wifi_ssid` (string)
- **Description:** WiFi network name (SSID)
- **Requir
`http://` or `https://`, max 128 characters
- **Example:** `"https://api.example.com/sensor-data"`
- **Local Development:** `"http://192.168.1.100:8080/data"`

### Optional Fields

These fields have default values and can be omitted. If present but invalid, the firmware will use the default value and log a warning.

#### `friendly_name` (string)
- **Description:** Human-readable device name
- **Default:** `"ESP32-Sensor-{hardware_id}"` (e.g., "ESP32-Sensor-A1B2C3D4")
- **Validation:** Max 128 characters
- **Example:** `"Garden Sensor North"`
- **Usage:** Displayed on screen, included in API payloads

#### `display_brightness` (integer)
- **Description:** TFT display backlight brightness level
- **Default:** `128`
- **Validation:** 0-255 (0 = off, 255 = maximum brightness)
- **Example:** `200`
- **Notes:** Higher values consume more power

#### `data_upload_interval` (integer)
- **Description:** Interval between data uploads to backend server (in seconds)
- **Default:** `60`
- **Validation:** 10-86400 seconds (10 seconds to 24 hours)
- **Example:** `300` (5 minutes)
- **Notes:** Longer intervals reduce network traffic and power consumption

#### `sensor_read_interval` (integer)
- **Description:** Interval between sensor readings (in seconds)
- **Default:** `10`
- **Validation:** 1-3600 seconds (1 second to 1 hour)
- **Example:** `30`
- **Notes:** Multiple readings are averaged before upload

#### `enable_deep_sleep` (boolean)
- **Description:** Enable deep sleep power saving mode
- **Default:** `false`
- **Validation:** `true` or `false`
- **Example:** `true`
- **Notes:** When enabled, device enters deep sleep after each upload cycle. Wake-up timer is set based on `data_upload_interval`.

### Schema Metadata

#### `schema_version` (integer)
- **Description:** Configuration file schema version
- **Current Version:** `1`
- **Required:** Yes
- **Validation:** Must be present
- **Forward Compatibility:** Firmware ignores unknown fields in newer schemas
- **Backward Compatibility:** Firmware applies defaults for missing fields in older schemas

#### `checksum` (string)
- **Description:** CRC32 checksum for integrity verification
- **Required:** Yes
- **Validation:** 8-character hexadecimal string
- **Example:** `"A1B2C3D4"`
- **Purpose:** Detects file corruption or incomplete writes

## Checksum Calculation

The checksum ensures configuration file integrity. Here's how to calculate it:

### Manual Calculation Steps

1. **Set checksum to empty string:**
   ```json
   {
     "schema_version": 1,
     "checksum": "",
     "wifi_ssid": "MyNetwork",
     ...
   }
   ```

2. **Serialize to canonical form:**
   - Minified (no whitespace)
   - Fixed field order: `schema_version`, `checksum`, `wifi_ssid`, `wifi_password`, `backend_url`, `friendly_name`, `display_brightness`, `data_upload_interval`, `sensor_read_interval`, `enable_deep_sleep`
   - UTF-8 encoding
   - Numbers without trailing `.0`

   Example canonical form:
   ```
   {"schema_version":1,"checksum":"","wifi_ssid":"MyNetwork","wifi_password":"Pass123","backend_url":"https://api.example.com/data","friendly_name":"Sensor","display_brightness":128,"data_upload_interval":60,"sensor_read_interval":10,"enable_deep_sleep":false}
   ```

3. **Calculate CRC32:**
   - Compute CRC32 over UTF-8 bytes of canonical form
   - Convert to 8-character uppercase hexadecimal string

4. **Update checksum field:**
   ```json
   {
     "schema_version": 1,
     "checksum": "A1B2C3D4",
     "wifi_ssid": "MyNetwork",
     ...
   }
   ```

### Python Script Example

```python
import json
import zlib

def calculate_checksum(config):
    # Create a copy with empty checksum
    config_copy = config.copy()
    config_copy['checksum'] = ''

    # Serialize to canonical form (fixed field order)
    field_order = [
        'schema_version', 'checksum', 'wifi_ssid', 'wifi_password',
        'backend_url', 'friendly_name', 'display_brightness',
        'data_upload_interval', 'sensor_read_interval', 'enable_deep_sleep'
    ]

    # Build ordered dict with only present fields
    ordered = {k: config_copy[k] for k in field_order if k in config_copy}

    # Serialize to minified JSON
    json_str = json.dumps(ordered, separators=(',', ':'), ensure_ascii=False)

    # Calculate CRC32
    crc = zlib.crc32(json_str.encode('utf-8')) & 0xFFFFFFFF

    # Convert to 8-character hex string
    return f"{crc:08X}"

# Example usage
config = {
    "schema_version": 1,
    "wifi_ssid": "MyNetwork",
    "wifi_password": "Pass123",
    "backend_url": "https://api.example.com/data",
    "friendly_name": "Sensor",
    "display_brightness": 128,
    "data_upload_interval": 60,
    "sensor_read_interval": 10,
    "enable_deep_sleep": False
}

checksum = calculate_checksum(config)
config['checksum'] = checksum

print(json.dumps(config, indent=2))
```

### Online CRC32 Calculators

You can also use online CRC32 calculators:
1. Prepare canonical JSON string (minified, fixed order, checksum empty)
2. Calculate CRC32 using online tool (e.g., https://crccalc.com/)
3. Use CRC-32 result, convert to uppercase hex

## Configuration Examples

### Home Network with Defaults

```json
{
  "schema_version": 1,
  "checksum": "12345678",
  "wifi_ssid": "HomeNetwork",
  "wifi_password": "MyPassword123",
  "backend_url": "https://api.myserver.com/sensors"
}
```

### Battery-Powered Outdoor Sensor

```json
{
  "schema_version": 1,
  "checksum": "ABCDEF01",
  "wifi_ssid": "OutdoorNetwork",
  "wifi_password": "SecurePass456",
  "backend_url": "https://api.myserver.com/sensors",
  "friendly_name": "Garden Sensor",
  "display_brightness": 100,
  "data_upload_interval": 600,
  "sensor_read_interval": 60,
  "enable_deep_sleep": true
}
```

**Notes:**
- Longer intervals reduce power consumption
- Deep sleep enabled for maximum battery life
- Lower display brightness saves power

### High-Frequency Monitoring

```json
{
  "schema_version": 1,
  "checksum": "23456789",
  "wifi_ssid": "LabNetwork",
  "wifi_password": "LabPass789",
  "backend_url": "https://api.lab.com/realtime",
  "friendly_name": "Lab Sensor 01",
  "display_brightness": 255,
  "data_upload_interval": 30,
  "sensor_read_interval": 5,
  "enable_deep_sleep": false
}
```

**Notes:**
- Short intervals for real-time monitoring
- Maximum brightness for visibility
- Deep sleep disabled for continuous operation

### Open WiFi Network

```json
{
  "schema_version": 1,
  "checksum": "34567890",
  "wifi_ssid": "PublicNetwork",
  "wifi_password": "",
  "backend_url": "https://api.example.com/sensors",
  "friendly_name": "Public Sensor"
}
```

**Notes:**
- Empty password for open networks
- Not recommended for production (security risk)

### Local Development Server

```json
{
  "schema_version": 1,
  "checksum": "45678901",
  "wifi_ssid": "DevNetwork",
  "wifi_password": "DevPass123",
  "backend_url": "http://192.168.1.100:8080/data",
  "friendly_name": "Dev Sensor",
  "data_upload_interval": 10,
  "sensor_read_interval": 5
}
```

**Notes:**
- HTTP (not HTTPS) for local testing
- Short intervals for rapid development
- Local IP address for development server

## Uploading Configuration File

### Method 1: PlatformIO Filesystem Upload

1. Create `data` folder in firmware directory:
   ```
   firmware/data/config.json
   ```

2. Place your `config.json` file in the `data` folder

3. Upload filesystem:
   ```bash
   pio run -e esp32dev --target uploadfs
   ```

### Method 2: Serial Upload (using esptool)

```bash
# Create filesystem image
mkspiffs -c data -b 4096 -p 256 -s 0x20000 spiffs.bin

# Upload to ESP32
esptool.py --port COM3 write_flash 0x310000 spiffs.bin
```

### Method 3: Web Upload (if web interface available)

Some firmware builds include a web interface for file upload. Check your specific firmware version.

### Method 4: Serial Console Commands (future enhancement)

Future firmware versions may support uploading config via serial console commands.

## Configuration Loading Behavior

### Boot Sequence

1. **Mount LittleFS filesystem**
   - If mount fails: Enter provisioning mode

2. **Check for `/config.json`**
   - If file exists: Parse and validate
   - If file missing: Try NVS storage

3. **Validate configuration**
   - Check JSON syntax
   - Verify checksum
   - Validate field types and ranges
   - Check required fields present

4. **Apply configuration**
   - If valid: Use config file settings
   - If invalid: Try NVS fallback
   - If both fail: Use defaults for optional fields only

5. **Check required fields**
   - If all present: Proceed to normal operation
   - If any missing: Enter provisioning mode

### Error Handling

**File Not Found:**
- Not an error - tries NVS next
- Display: "Config: file missing"

**Parse Error:**
- Invalid JSON syntax
- Display: "Config: Parse error"
- Fallback: Try NVS, then defaults

**Schema Error:**
- Missing `schema_version` field
- Display: "Config: Schema error"
- Fallback: Try NVS, then defaults

**Checksum Error:**
- Checksum mismatch (file corrupted)
- Display: "Config: Checksum error"
- Fallback: Try NVS, then defaults

**Validation Error:**
- Invalid field values
- Optional fields: Use defaults, log warning
- Required fields: Enter provisioning mode

**Missing Required Fields:**
- Display: "Config: missing required"
- Enter provisioning mode
- Requires serial console configuration

### Provisioning Mode

When required fields are missing, the device enters provisioning mode:

1. **Display shows:** "Provisioning Mode - Use Serial Console"
2. **Serial console active:** Configure via serial commands
3. **Configuration saved:** To both NVS and config file
4. **Automatic reboot:** After successful configuration
5. **Next boot:** Loads from config file normally

See [SERIAL_CONSOLE.md](SERIAL_CONSOLE.md) for provisioning commands.

## Configuration Priority

The firmware uses this priority order:

1. **Config File** (`/config.json`)
   - Loaded first if present and valid
   - Becomes source of truth

2. **NVS Storage**
   - Used if config file missing or invalid
   - Automatically migrated to config file on first boot

3. **Defaults**
   - Applied to optional fields only
   - Never used for required fields

**Important:** Required fields (wifi_ssid, wifi_password, backend_url) MUST be satisfied by config file or NVS. Defaults never satisfy required fields.

## Migration from NVS

If you have an existing device configured via serial console (NVS storage):

1. **First boot after firmware update:**
   - Firmware detects no config file
   - Loads configuration from NVS
   - Automatically writes `/config.json`
   - Calculates and stores checksum

2. **Subsequent boots:**
   - Loads from `/config.json`
   - NVS becomes backup only

3. **Manual migration:**
   - Connect to serial console
   - Run `show` command to see current config
   - Create `config.json` with those values
   - Upload to filesystem

## Validation Rules

### String Fields

- **Maximum length:** 128 characters
- **Encoding:** UTF-8
- **No null bytes**
- **Optional fields:** Trimmed to 128 chars if exceeded (with warning)
- **Required fields:** Rejected if exceeded (provisioning mode)

### Integer Fields

- **display_brightness:** 0-255
  - Out of range: Clamped to valid range (with warning)

- **data_upload_interval:** 10-86400 seconds
  - Out of range: Clamped to valid range (with warning)

- **sensor_read_interval:** 1-3600 seconds
  - Out of range: Clamped to valid range (with warning)

### URL Validation

- **backend_url:** Must start with `http://` or `https://`
  - Invalid: Rejected (provisioning mode)
  - Maximum length: 128 characters

### Boolean Fields

- **enable_deep_sleep:** Must be `true` or `false`
  - Invalid: Uses default value (false)

## Security Considerations

### Sensitive Data

The configuration file contains sensitive information:
- WiFi password
- Backend URL (may contain API keys)

**Storage:**
- Stored in plaintext on LittleFS
- Physical access to device = access to credentials

**Mitigation:**
- Use WPA3 for WiFi security
- Rotate credentials regularly
- Use HTTPS for backend communication
- Consider device encryption for high-security applications

### Checksum Security

**Purpose:**
- Detects accidental corruption
- Detects incomplete writes
- NOT cryptographically secure

**Limitations:**
- CRC32 does not prevent malicious tampering
- Not suitable for authentication
- Future enhancement: HMAC-SHA256 for tamper detection

## Troubleshooting

### Config File Not Loading

**Symptoms:**
- Device uses NVS or defaults instead of config file
- Display shows "Config: file missing"

**Solutions:**
1. Verify file exists at `/config.json` (root of LittleFS)
2. Check filesystem uploaded correctly
3. Verify LittleFS partition configured in platformio.ini
4. Check serial output for mount errors

### Checksum Validation Fails

**Symptoms:**
- Display shows "Config: Checksum error"
- Device falls back to NVS or defaults

**Solutions:**
1. Recalculate checksum using canonical form
2. Verify field order matches specification
3. Check for extra whitespace or formatting
4. Use provided Python script to generate checksum
5. Verify UTF-8 encoding (no BOM)

### Required Fields Missing

**Symptoms:**
- Display shows "Config: missing required"
- Device enters provisioning mode

**Solutions:**
1. Verify all required fields present: wifi_ssid, wifi_password, backend_url
2. Check field names match exactly (case-sensitive)
3. Ensure wifi_password present (can be empty string)
4. Configure via serial console in provisioning mode

### Invalid Field Values

**Symptoms:**
- Device uses default values instead of config values
- Warnings in serial output

**Solutions:**
1. Check integer ranges (brightness 0-255, intervals within limits)
2. Verify URL starts with http:// or https://
3. Check string lengths (max 128 characters)
4. Review serial output for specific validation errors

### Parse Errors

**Symptoms:**
- Display shows "Config: Parse error"
- Device falls back to NVS or defaults

**Solutions:**
1. Validate JSON syntax using online validator
2. Check for missing commas or quotes
3. Verify no trailing commas
4. Ensure proper escaping of special characters
5. Use JSON formatter to check structure

## Best Practices

1. **Always calculate checksum:**
   - Use provided Python script
   - Verify checksum after any changes

2. **Test configuration locally:**
   - Validate JSON syntax before upload
   - Test with development server first

3. **Backup configurations:**
   - Keep copies of working configs
   - Document device-specific settings

4. **Use version control:**
   - Store config templates in git
   - Track changes over time

5. **Document deployments:**
   - Record which config used for each device
   - Note any device-specific customizations

6. **Security:**
   - Use HTTPS in production
   - Rotate credentials regularly
   - Protect config files from unauthorized access

7. **Testing:**
   - Test config changes on development device first
   - Verify all fields load correctly
   - Check serial output for warnings

## Related Documentation

- [SERIAL_CONSOLE.md](SERIAL_CONSOLE.md) - Serial console configuration commands
- [DEPLOYMENT.md](DEPLOYMENT.md) - Deployment checklist including config file setup
- [README.md](../README.md) - Main firmware documentation

## Support

For issues with configuration files:
1. Check serial output for specific error messages
2. Validate JSON syntax
3. Verify checksum calculation
4. Review this guide for field requirements
5. Test with minimal configuration first

