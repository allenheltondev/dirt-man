# ESP32 Sensor Firmware - API Endpoint Specification

## Overview

This document specifies the API endpoint requirement
ken configured in firmware via serial console
- Example: `Authorization: Bearer your-api-token-here`

### URL Format
```
https://your-domain.com/path/to/endpoint
```

**Examples:**
- `https://api.example.com/sensor-data`
- `https://iot.myserver.com/v1/readings`
- `http://192.168.1.100:8080/data` (development only)

## Request Format

### Headers

```http
POST /sensor-data HTTP/1.1
Host: api.example.com
Content-Type: application/json
Authorization: Bearer your-api-token-here
User-Agent: ESP32-Sensor-Firmware/1.0
Content-Length: 1234
```

### JSON Payload Structure

#### Single Reading

```json
{
  "batch_id": "esp32-sensor-001_e_1704067200000_1704067800000",
  "device_id": "esp32-sensor-001",
  "sample_start_epoch_ms": 1704067200000,
  "sample_start_uptime_ms": 7200000,
  "sample_end_epoch_ms": 1704067800000,
  "sample_end_uptime_ms": 7800000,
  "device_boot_epoch_ms": 1704060000000,
  "uptime_ms": 7800000,
  "sample_count": 20,
  "time_synced": true,
  "sensors": {
    "bme280_temp_c": 22.5,
    "ds18b20_temp_c": 21.8,
    "humidity_pct": 45.2,
    "pressure_hpa": 1013.25,
    "soil_moisture_pct": 62.3
  },
  "sensor_status": {
    "bme280": "ok",
    "ds18b20": "ok",
    "soil_moisture": "ok"
  },
  "health": {
    "uptime_ms": 7800000,
    "free_heap_bytes": 245760,
    "wifi_rssi_dbm": -65,
    "error_counters": {
      "sensor_read_failures": 0,
      "network_failures": 0,
      "buffer_overflows": 0
    }
  }
}
```

#### Batch Transmission (Multiple Readings)

When network is restored after offline period, firmware sends multiple readings:

```json
{
  "device_id": "esp32-sensor-001",
  "readings": [
    {
      "batch_id": "esp32-sensor-001_e_1704067200000_1704067800000",
      "sample_start_epoch_ms": 1704067200000,
      "sample_start_uptime_ms": 7200000,
      "sample_end_epoch_ms": 1704067800000,
      "sample_end_uptime_ms": 7800000,
      "sample_count": 20,
      "time_synced": true,
      "sensors": {
        "bme280_temp_c": 22.5,
        "ds18b20_temp_c": 21.8,
        "humidity_pct": 45.2,
        "pressure_hpa": 1013.25,
        "soil_moisture_pct": 62.3
      },
      "sensor_status": {
        "bme280": "ok",
        "ds18b20": "ok",
        "soil_moisture": "ok"
      },
      "health": {
        "uptime_ms": 7800000,
        "free_heap_bytes": 245760,
        "wifi_rssi_dbm": -65,
        "error_counters": {
          "sensor_read_failures": 0,
          "network_failures": 0,
          "buffer_overflows": 0
        }
      }
    },
    {
      "batch_id": "esp32-sensor-001_e_1704067800000_1704068400000",
      "sample_start_epoch_ms": 1704067800000,
      "sample_start_uptime_ms": 7800000,
      "sample_end_epoch_ms": 1704068400000,
      "sample_end_uptime_ms": 8400000,
      "sample_count": 20,
      "time_synced": true,
      "sensors": {
        "bme280_temp_c": 22.8,
        "ds18b20_temp_c": 22.1,
        "humidity_pct": 44.8,
        "pressure_hpa": 1013.50,
        "soil_moisture_pct": 61.5
      },
      "sensor_status": {
        "bme280": "ok",
        "ds18b20": "ok",
        "soil_moisture": "ok"
      },
      "health": {
        "uptime_ms": 8400000,
        "free_heap_bytes": 243520,
        "wifi_rssi_dbm": -67,
        "error_counters": {
          "sensor_read_failures": 0,
          "network_failures": 1,
          "buffer_overflows": 0
        }
      }
    }
  ]
}
```

## Field Specifications

### Top-Level Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `batch_id` | string | Yes | Unique identifier for this averaged reading |
| `device_id` | string | Yes | Device identifier (configured in firmware) |
| `sample_start_epoch_ms` | number | Yes | Unix epoch milliseconds when sampling started (0 if not synced) |
| `sample_start_uptime_ms` | number | Yes | Device uptime in milliseconds when sampling started |
| `sample_end_epoch_ms` | number | Yes | Unix epoch milliseconds when sampling ended (0 if not synced) |
| `sample_end_uptime_ms` | number | Yes | Device uptime in milliseconds when sampling ended |
| `device_boot_epoch_ms` | number | Yes | Unix epoch milliseconds when device booted (0 if not synced) |
| `uptime_ms` | number | Yes | Current device uptime in milliseconds |
| `sample_count` | number | Yes | Number of readings averaged |
| `time_synced` | boolean | Yes | Whether device time is synchronized via NTP |
| `sensors` | object | Yes | Averaged sensor readings |
| `sensor_status` | object | Yes | Status of each sensor |
| `health` | object | Yes | System health metrics |

### Batch ID Format

The `batch_id` uniquely identifies each averaged reading and enables idempotent retries.

**Format:** `{device_id}_{clock_source}_{start_timestamp}_{end_timestamp}`

**Clock Source Indicators:**
- `e_` - Epoch time (NTP synchronized)
- `u_` - Uptime time (not synchronized)

**Examples:**
- `esp32-sensor-001_e_1704067200000_1704067800000` (synced)
- `esp32-sensor-001_u_7200000_7800000` (unsynced)

**Properties:**
- Unique across all readings from same device
- Deterministic (same reading generates same ID)
- Enables server-side deduplication
- Indicates time synchronization status

### Timestamp Fields

The firmware provides **dual timestamps** for each reading:

1. **Epoch Timestamps** (`*_epoch_ms`)
   - Unix epoch milliseconds (milliseconds since 1970-01-01 00:00:00 UTC)
   - Set to `0` when NTP not synchronized
   - Accurate wall-clock time when synced
   - Use for time-series databases and analysis

2. **Uptime Timestamps** (`*_uptime_ms`)
   - Milliseconds since device boot
   - Always available (never 0)
   - Monotonic (never goes backward)
   - Use for relative timing and duration calculations

**Why Dual Timestamps?**
- Device may boot without internet connectivity
- NTP sync may fail or be delayed
- Uptime provides relative timing even without NTP
- Server can reconstruct timeline using both timestamp types

**Example Scenarios:**

**Scenario 1: NTP Synchronized**
```json
{
  "time_synced": true,
  "sample_start_epoch_ms": 1704067200000,
  "sample_start_uptime_ms": 7200000,
  "sample_end_epoch_ms": 1704067800000,
  "sample_end_uptime_ms": 7800000
}
```
Use epoch timestamps for absolute time.

**Scenario 2: NTP Not Synchronized**
```json
{
  "time_synced": false,
  "sample_start_epoch_ms": 0,
  "sample_start_uptime_ms": 7200000,
  "sample_end_epoch_ms": 0,
  "sample_end_uptime_ms": 7800000
}
```
Use uptime timestamps for relative timing. Server can estimate absolute time using device_boot_epoch_ms from later synced readings.

### Sensors Object

| Field | Type | Unit | Range | Null |
|-------|------|------|-------|------|
| `bme280_temp_c` | number | Celsius | -40 to 85 | Yes |
| `ds18b20_temp_c` | number | Celsius | -55 to 125 | Yes |
| `humidity_pct` | number | Percent | 0 to 100 | Yes |
| `pressure_hpa` | number | Hectopascals | 300 to 1100 | Yes |
| `soil_moisture_pct` | number | Percent | 0 to 100 | Yes |

**Null Values:**
- Sensor value is `null` when sensor is unavailable or failed
- Check `sensor_status` for reason
- Server should handle null values gracefully

**Example with Unavailable Sensor:**
```json
{
  "sensors": {
    "bme280_temp_c": 22.5,
    "ds18b20_temp_c": null,
    "humidity_pct": 45.2,
    "pressure_hpa": 1013.25,
    "soil_moisture_pct": 62.3
  },
  "sensor_status": {
    "bme280": "ok",
    "ds18b20": "error",
    "soil_moisture": "ok"
  }
}
```

### Sensor Status Object

| Field | Type | Values | Description |
|-------|------|--------|-------------|
| `bme280` | string | "ok", "error" | BME280 sensor status |
| `ds18b20` | string | "ok", "error" | DS18B20 sensor status |
| `soil_moisture` | string | "ok", "error" | Soil moisture sensor status |

**Status Values:**
- `"ok"` - Sensor functioning normally
- `"error"` - Sensor failed or unavailable

### Health Object

System health and diagnostic information:

| Field | Type | Description |
|-------|------|-------------|
| `uptime_ms` | number | Device uptime in milliseconds |
| `free_heap_bytes` | number | Free heap memory in bytes |
| `wifi_rssi_dbm` | number | WiFi signal strength in dBm |
| `error_counters` | object | Cumulative error counts |

**Error Counters:**
```json
{
  "sensor_read_failures": 0,
  "network_failures": 1,
  "buffer_overflows": 0
}
```

- `sensor_read_failures` - Total sensor read errors since boot
- `network_failures` - Total network transmission failures since boot
- `buffer_overflows` - Total buffer overflow events since boot

**Health Monitoring:**
- `free_heap_bytes` < 100000 indicates memory pressure
- `wifi_rssi_dbm` < -80 indicates weak signal
- Increasing error counters indicate recurring issues

## Response Format

### Success Response

**HTTP Status:** 200 OK

**Headers:**
```http
HTTP/1.1 200 OK
Content-Type: application/json
```

**Body:**
```json
{
  "status": "success",
  "acknowledged_batch_ids": [
    "esp32-sensor-001_e_1704067200000_1704067800000",
    "esp32-sensor-001_e_1704067800000_1704068400000"
  ],
  "message": "Data received successfully"
}
```

**Required Fields:**
- `status` - Must be "success"
- `acknowledged_batch_ids` - Array of batch_ids successfully processed
- `message` - Optional human-readable message

**Important:**
- Firmware uses `acknowledged_batch_ids` to clear buffered data
- Only acknowledged readings are removed from buffer
- Unacknowledged readings will be retried
- Empty array is valid (no readings acknowledged)

### Error Responses

#### 400 Bad Request

Invalid JSON or missing required fields:

```json
{
  "status": "error",
  "error": "Invalid JSON payload",
  "message": "Missing required field: device_id"
}
```

#### 401 Unauthorized

Invalid or missing authentication token:

```json
{
  "status": "error",
  "error": "Unauthorized",
  "message": "Invalid or missing API token"
}
```

#### 500 Internal Server Error

Server-side error:

```json
{
  "status": "error",
  "error": "Internal server error",
  "message": "Database connection failed"
}
```

**Firmware Behavior:**
- 4xx errors: Log and discard (client error, won't succeed on retry)
- 5xx errors: Retry with exponential backoff (server error, may be temporary)
- Network errors: Retry with exponential backoff

## Idempotent Retry Mechanism

The firmware implements idempotent retries using `batch_id`:

### How It Works

1. **Firmware generates unique batch_id** for each averaged reading
2. **Firmware sends reading** with batch_id to server
3. **Server processes reading** and stores batch_id
4. **Server responds** with acknowledged_batch_ids
5. **Firmware clears acknowledged readings** from buffer

### Retry Scenarios

**Scenario 1: Network Timeout**
- Firmware sends reading, no response received
- Reading remains in buffer
- Firmware retries with same batch_id
- Server detects duplicate batch_id and returns success without reprocessing

**Scenario 2: Partial Batch Acknowledgment**
- Firmware sends 3 readings
- Server successfully processes 2, fails on 1
- Server responds with 2 acknowledged batch_ids
- Firmware clears 2 readings, retries 1 failed reading

**Scenario 3: Complete Failure**
- Firmware sends reading, receives 500 error
- Reading remains in buffer
- Firmware retries after exponential backoff
- Server eventually succeeds and acknowledges

### Server Implementation Requirements

To support idempotent retries, server must:

1. **Store batch_ids** of processed readings
2. **Check for duplicates** before processing
3. **Return success** for duplicate batch_ids without reprocessing
4. **Include acknowledged_batch_ids** in all success responses

**Example Server Logic:**
```python
def process_reading(reading):
    batch_id = reading['batch_id']

    # Check if already processed
    if is_duplicate(batch_id):
        return {'status': 'success', 'acknowledged_batch_ids': [batch_id]}

    # Process reading
    store_reading(reading)
    mark_processed(batch_id)

    return {'status': 'success', 'acknowledged_batch_ids': [batch_id]}
```

## Example API Implementations

### Python Flask Example

```python
from flask import Flask, request, jsonify
import json

app = Flask(__name__)

# In-memory storage (use database in production)
processed_batch_ids = set()
readings_db = []

API_TOKEN = "your-secret-token"

@app.route('/sensor-data', methods=['POST'])
def receive_sensor_data():
    # Verify authentication
    auth_header = request.headers.get('Authorization')
    if not auth_header or not auth_header.startswith('Bearer '):
        return jsonify({
            'status': 'error',
            'error': 'Unauthorized',
            'message': 'Missing or invalid API token'
        }), 401

    token = auth_header.split(' ')[1]
    if token != API_TOKEN:
        return jsonify({
            'status': 'error',
            'error': 'Unauthorized',
            'message': 'Invalid API token'
        }), 401

    # Parse JSON payload
    try:
        data = request.get_json()
    except Exception as e:
        return jsonify({
            'status': 'error',
            'error': 'Invalid JSON',
            'message': str(e)
        }), 400

    # Handle single reading or batch
    if 'readings' in data:
        # Batch transmission
        readings = data['readings']
    else:
        # Single reading
        readings = [data]

    acknowledged = []

    for reading in readings:
        batch_id = reading.get('batch_id')

        if not batch_id:
            continue

        # Check for duplicate
        if batch_id in processed_batch_ids:
            acknowledged.append(batch_id)
            continue

        # Process reading
        try:
            # Store in database
            readings_db.append(reading)
            processed_batch_ids.add(batch_id)
            acknowledged.append(batch_id)

            print(f"Received reading from {reading.get('device_id')}")
            print(f"  Temp: {reading['sensors'].get('bme280_temp_c')}°C")
            print(f"  Humidity: {reading['sensors'].get('humidity_pct')}%")
            print(f"  Soil: {reading['sensors'].get('soil_moisture_pct')}%")

        except Exception as e:
            print(f"Error processing reading {batch_id}: {e}")

    return jsonify({
        'status': 'success',
        'acknowledged_batch_ids': acknowledged,
        'message': f'Processed {len(acknowledged)} readings'
    }), 200

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080, ssl_context='adhoc')
```

### Node.js Express Example

```javascript
const express = require('express');
const app = express();

app.use(express.json());

const API_TOKEN = 'your-secret-token';
const processedBatchIds = new Set();
const readingsDb = [];

app.post('/sensor-data', (req, res) => {
    // Verify authentication
    const authHeader = req.headers.authorization;
    if (!authHeader || !authHeader.startsWith('Bearer ')) {
        return res.status(401).json({
            status: 'error',
            error: 'Unauthorized',
            message: 'Missing or invalid API token'
        });
    }

    const token = authHeader.split(' ')[1];
    if (token !== API_TOKEN) {
        return res.status(401).json({
            status: 'error',
            error: 'Unauthorized',
            message: 'Invalid API token'
        });
    }

    // Handle single reading or batch
    const readings = req.body.readings || [req.body];
    const acknowledged = [];

    for (const reading of readings) {
        const batchId = reading.batch_id;

        if (!batchId) continue;

        // Check for duplicate
        if (processedBatchIds.has(batchId)) {
            acknowledged.push(batchId);
            continue;
        }

        // Process reading
        try {
            readingsDb.push(reading);
            processedBatchIds.add(batchId);
            acknowledged.push(batchId);

            console.log(`Received reading from ${reading.device_id}`);
            console.log(`  Temp: ${reading.sensors.bme280_temp_c}°C`);
            console.log(`  Humidity: ${reading.sensors.humidity_pct}%`);
            console.log(`  Soil: ${reading.sensors.soil_moisture_pct}%`);
        } catch (error) {
            console.error(`Error processing reading ${batchId}:`, error);
        }
    }

    res.json({
        status: 'success',
        acknowledged_batch_ids: acknowledged,
        message: `Processed ${acknowledged.length} readings`
    });
});

const PORT = 8080;
app.listen(PORT, () => {
    console.log(`API server listening on port ${PORT}`);
});
```

## Testing the API

### Using curl

**Single Reading:**
```bash
curl -X POST https://api.example.com/sensor-data \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer your-api-token" \
  -d '{
    "batch_id": "test-device_e_1704067200000_1704067800000",
    "device_id": "test-device",
    "sample_start_epoch_ms": 1704067200000,
    "sample_start_uptime_ms": 7200000,
    "sample_end_epoch_ms": 1704067800000,
    "sample_end_uptime_ms": 7800000,
    "device_boot_epoch_ms": 1704060000000,
    "uptime_ms": 7800000,
    "sample_count": 20,
    "time_synced": true,
    "sensors": {
      "bme280_temp_c": 22.5,
      "ds18b20_temp_c": 21.8,
      "humidity_pct": 45.2,
      "pressure_hpa": 1013.25,
      "soil_moisture_pct": 62.3
    },
    "sensor_status": {
      "bme280": "ok",
      "ds18b20": "ok",
      "soil_moisture": "ok"
    },
    "health": {
      "uptime_ms": 7800000,
      "free_heap_bytes": 245760,
      "wifi_rssi_dbm": -65,
      "error_counters": {
        "sensor_read_failures": 0,
        "network_failures": 0,
        "buffer_overflows": 0
      }
    }
  }'
```

### Using Postman

1. Create new POST request
2. URL: `https://api.example.com/sensor-data`
3. Headers:
   - `Content-Type: application/json`
   - `Authorization: Bearer your-api-token`
4. Body: Raw JSON (see example above)
5. Send request
6. Verify 200 OK response with acknowledged_batch_ids

## Security Considerations

### HTTPS/TLS

**Production Requirements:**
- Use HTTPS with valid TLS certificate
- TLS 1.2 or higher
- Strong cipher suites
- Certificate validation enabled

**Development/Testing:**
- HTTP acceptable for local testing
- Self-signed certificates require firmware configuration
- Disable certificate validation only in development

### Authentication

**API Token Best Practices:**
- Use long, random tokens (32+ characters)
- Rotate tokens periodically
- Use different tokens per device or device group
- Never commit tokens to version control
- Store tokens securely on server

**Example Token Generation:**
```bash
# Generate random token
openssl rand -hex 32
```

### Rate Limiting

Implement rate limiting to prevent abuse:
- Limit requests per device per hour
- Typical: 1 request per 5 minutes = 12 requests/hour
- Return 429 Too Many Requests if exceeded

### Input Validation

Validate all input fields:
- Check required fields present
- Validate data types and ranges
- Sanitize strings to prevent injection
- Limit payload size (< 10KB typical)

## Troubleshooting

### Firmware Cannot Connect

**Check:**
- API endpoint URL is correct
- HTTPS certificate is valid
- Firewall allows outbound HTTPS
- DNS resolution works
- Server is accessible from device network

### Authentication Failures

**Check:**
- API token is correct in firmware configuration
- Authorization header format is correct
- Token hasn't expired or been revoked
- Server token validation logic is correct

### Data Not Appearing

**Check:**
- Server returns 200 OK status
- Server includes acknowledged_batch_ids in response
- Server actually stores data (check database)
- Firmware receives and parses response correctly

### Duplicate Data

**Check:**
- Server implements batch_id deduplication
- Server stores processed batch_ids
- Server checks for duplicates before processing

## Next Steps

1. Implement API endpoint on your server
2. Configure firmware with API endpoint URL and token
3. Test with single reading transmission
4. Verify data appears in your database
5. Test offline buffering and batch transmission
6. Monitor error rates and adjust retry logic if needed

For firmware configuration, see [SERIAL_CONSOLE.md](SERIAL_CONSOLE.md)

For deployment checklist, see [DEPLOYMENT.md](DEPLOYMENT.md)
