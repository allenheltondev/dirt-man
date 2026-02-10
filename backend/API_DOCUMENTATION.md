# Serverless Backend API Documentation

## Overview

This document provides comprehensive documentation for the Serverless Backend API system for ESP32 environmental monitoring devices. The system consists of two AWS Lambda functions:

- **Data Plane API**: Handles device registration and sensor data ingestion
- **Control Plane API**: Provides administrative functions for managing API keys, devices, and querying sensor data

Both APIs are deployed as AWS Lambda functions with Function URLs (public HTTPS endpoints).

## Base URLs

- **Data Plane API**: `https://<data-plane-function-url>.lambda-url.<region>.on.aws/`
- **Control Plane API**: `https://<control-plane-function-url>.lambda-url.<region>.on.aws/`

## Authentication

### Data Plane Authentication

The Data Plane API uses API key authentication via the `X-API-Key` header.

**Header Format:**
```
X-API-Key: <64-character-hex-string>
```

**Example:**
```
X-API-Key: 5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8
```

**Authentication Flow:**
1. Include `X-API-Key` header in all Data Plane requests
2. API validates key against stored hash in DynamoDB
3. If valid and active, request proceeds
4. If invalid, missing, or revoked, returns 401 Unauthorized

### Control Plane Authentication

The Control Plane API uses Bearer token authentication via the `Authorization` header.

**Header Format:**
```
Authorization: Bearer <admin-token>
```

**Example:**
```
Authorization: Bearer my-secret-admin-token-12345
```

**Authentication Flow:**
1. Include `Authorization` header in all Control Plane requests
2. API validates token against environment variable
3. If valid, request proceeds
4. If invalid or missing, returns 401 Unauthorized


## Data Plane API Endpoints

### POST /register

Register a device with the backend or update an existing device's last seen timestamp.

**Authentication:** Required (X-API-Key)

**Request Body:**
```json
{
  "hardware_id": "AA:BB:CC:DD:EE:FF",
  "boot_id": "550e8400-e29b-41d4-a716-446655440000",
  "firmware_version": "1.0.16",
  "friendly_name": "greenhouse-sensor-01",
  "capabilities": {
    "sensors": ["bme280", "ds18b20", "soil_moisture"],
    "features": {
      "tft_display": true,
      "offline_buffering": true
    }
  }
}
```

**Request Fields:**
- `hardware_id` (string, required): MAC address in format XX:XX:XX:XX:XX:XX (uppercase hex)
- `boot_id` (string, required): UUID v4 generated on device boot
- `firmware_version` (string, required): Device firmware version
- `friendly_name` (string, optional): Human-readable device name
- `capabilities` (object, required): Device capabilities
  - `sensors` (array of strings): List of available sensors
  - `features` (object): Map of feature names to boolean values

**Success Response (200 OK):**
```json
{
  "status": "registered",
  "confirmation_id": "7c9e6679-7425-40de-944b-e07fc1f90ae7",
  "hardware_id": "AA:BB:CC:DD:EE:FF",
  "registered_at": "2024-01-15T10:30:00Z"
}
```

**Response Fields:**
- `status` (string): Always "registered"
- `confirmation_id` (string): UUID v4 assigned to device (stable across re-registrations)
- `hardware_id` (string): Echo of the hardware_id from request
- `registered_at` (string): ISO 8601 timestamp of registration

**Error Responses:**

**401 Unauthorized - Missing API Key:**
```json
{
  "error": "MISSING_API_KEY",
  "message": "X-API-Key header is required"
}
```

**401 Unauthorized - Invalid API Key:**
```json
{
  "error": "INVALID_API_KEY",
  "message": "API key is invalid or not found"
}
```

**400 Bad Request - Invalid MAC Address:**
```json
{
  "error": "INVALID_FORMAT",
  "message": "Invalid format for field: hardware_id"
}
```

**400 Bad Request - Invalid UUID:**
```json
{
  "error": "INVALID_FORMAT",
  "message": "Invalid format for field: boot_id"
}
```

**400 Bad Request - Missing Field:**
```json
{
  "error": "MISSING_FIELD",
  "message": "Required field missing: firmware_version"
}
```


### POST /data

Submit sensor readings from a device. Supports both single readings and batches (up to 100 readings per request).

**Authentication:** Required (X-API-Key)

**Request Body:**
```json
{
  "readings": [
    {
      "batch_id": "AA:BB:CC:DD:EE:FF_7c9e6679-7425-40de-944b-e07fc1f90ae7_1704067200000_1704067800000",
      "hardware_id": "AA:BB:CC:DD:EE:FF",
      "boot_id": "7c9e6679-7425-40de-944b-e07fc1f90ae7",
      "firmware_version": "1.0.16",
      "timestamp_ms": 1704067800000,
      "friendly_name": "greenhouse-sensor-01",
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
      }
    }
  ]
}
```

**Request Fields:**
- `readings` (array, required): Array of sensor readings (max 100)
  - `batch_id` (string, required): Unique identifier for this reading (max 256 chars, safe ASCII)
  - `hardware_id` (string, required): MAC address in format XX:XX:XX:XX:XX:XX
  - `boot_id` (string, required): UUID v4 from device boot
  - `firmware_version` (string, required): Device firmware version
  - `timestamp_ms` (integer, required): Epoch milliseconds UTC (64-bit integer)
  - `friendly_name` (string, optional): Device name snapshot
  - `sensors` (object, required): Map of sensor readings (all values optional)
    - `bme280_temp_c` (number): BME280 temperature in Celsius
    - `ds18b20_temp_c` (number): DS18B20 temperature in Celsius
    - `humidity_pct` (number): Humidity percentage
    - `pressure_hpa` (number): Pressure in hectopascals
    - `soil_moisture_pct` (number): Soil moisture percentage
  - `sensor_status` (object, required): Map of sensor statuses
    - `bme280` (string): "ok" or "error"
    - `ds18b20` (string): "ok" or "error"
    - `soil_moisture` (string): "ok" or "error"

**Success Response (200 OK):**
```json
{
  "acknowledged_batch_ids": [
    "AA:BB:CC:DD:EE:FF_7c9e6679-7425-40de-944b-e07fc1f90ae7_1704067200000_1704067800000"
  ],
  "duplicate_batch_ids": []
}
```

**Response Fields:**
- `acknowledged_batch_ids` (array of strings): Batch IDs that were newly processed
- `duplicate_batch_ids` (array of strings): Batch IDs that were previously seen (duplicates)

**Idempotency Behavior:**
- Each reading has a unique `batch_id`
- Duplicate submissions (same `batch_id`) are detected and skipped
- Response partitions batch_ids into acknowledged (new) and duplicates (seen before)
- If a non-duplicate error occurs during ingestion, the request returns an error; some earlier readings may have been committed
- Client should retry with all batch_ids; duplicates will be correctly classified on retry

**Error Responses:**

**400 Bad Request - Batch Size Exceeded:**
```json
{
  "error": "BATCH_SIZE_EXCEEDED",
  "message": "Batch size exceeds maximum of 100 readings"
}
```

**400 Bad Request - Invalid Timestamp:**
```json
{
  "error": "INVALID_FORMAT",
  "message": "Invalid format for field: timestamp_ms"
}
```

**401 Unauthorized - Invalid API Key:**
```json
{
  "error": "INVALID_API_KEY",
  "message": "API key is invalid or not found"
}
```


### GET /health

Health check endpoint for the Data Plane API. Does not require authentication.

**Authentication:** Not required

**Success Response (200 OK):**
```json
{
  "status": "healthy"
}
```

**Error Response (503 Service Unavailable):**
```json
{
  "status": "unhealthy",
  "error": "DATABASE_ERROR",
  "message": "DynamoDB connectivity check failed"
}
```

---

## Control Plane API Endpoints

### POST /api-keys

Create a new API key for device authentication.

**Authentication:** Required (Bearer token)

**Request Body:**
```json
{
  "description": "Production devices - greenhouse cluster"
}
```

**Request Fields:**
- `description` (string, optional): Human-readable description for the API key

**Success Response (200 OK):**
```json
{
  "key_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "api_key": "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8",
  "created_at": "2024-01-15T14:30:00Z",
  "message": "API key created successfully. Save this key - it will not be shown again."
}
```

**Response Fields:**
- `key_id` (string): UUID v4 identifier for the API key
- `api_key` (string): The raw API key value (64-character hex string) - **only shown once**
- `created_at` (string): ISO 8601 timestamp of creation
- `message` (string): Warning to save the key

**Important:** The raw `api_key` value is only returned in this response. It cannot be retrieved later. Store it securely.

**Error Responses:**

**401 Unauthorized - Missing Token:**
```json
{
  "error": "MISSING_TOKEN",
  "message": "Authorization header is required"
}
```

**401 Unauthorized - Invalid Token:**
```json
{
  "error": "INVALID_TOKEN",
  "message": "Bearer token is invalid"
}
```


### GET /api-keys

List all API keys with pagination support.

**Authentication:** Required (Bearer token)

**Query Parameters:**
- `limit` (integer, optional): Maximum number of keys to return (default: 50, max: 100)
- `cursor` (string, optional): Pagination cursor from previous response

**Example Request:**
```
GET /api-keys?limit=10&cursor=eyJrZXlfaWQiOiJhMWIyYzNkNC1lNWY2LTc4OTAtYWJjZC1lZjEyMzQ1Njc4OTAifQ==
```

**Success Response (200 OK):**
```json
{
  "api_keys": [
    {
      "key_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
      "created_at": "2024-01-15T14:30:00Z",
      "last_used_at": "2024-01-15T16:22:00Z",
      "is_active": true,
      "description": "Production devices - greenhouse cluster"
    },
    {
      "key_id": "b2c3d4e5-f6a7-8901-bcde-f12345678901",
      "created_at": "2024-01-10T08:00:00Z",
      "last_used_at": "2024-01-14T12:15:00Z",
      "is_active": false,
      "description": "Test devices"
    }
  ],
  "next_cursor": "eyJrZXlfaWQiOiJiMmMzZDRlNS1mNmE3LTg5MDEtYmNkZS1mMTIzNDU2Nzg5MDEifQ=="
}
```

**Response Fields:**
- `api_keys` (array): List of API key records
  - `key_id` (string): UUID v4 identifier
  - `created_at` (string): ISO 8601 timestamp of creation
  - `last_used_at` (string): ISO 8601 timestamp of last use (null if never used)
  - `is_active` (boolean): Whether the key is active
  - `description` (string): Human-readable description
- `next_cursor` (string, optional): Cursor for next page (omitted if no more results)

**Note:** The raw API key value is never returned in list responses. Keys are sorted by `created_at` descending (newest first).

**Error Responses:**

**401 Unauthorized:**
```json
{
  "error": "INVALID_TOKEN",
  "message": "Bearer token is invalid"
}
```


### DELETE /api-keys/{key_id}

Revoke an API key by setting its `is_active` flag to false.

**Authentication:** Required (Bearer token)

**Path Parameters:**
- `key_id` (string, required): UUID v4 identifier of the API key to revoke

**Example Request:**
```
DELETE /api-keys/a1b2c3d4-e5f6-7890-abcd-ef1234567890
```

**Success Response (200 OK):**
```json
{
  "status": "revoked",
  "key_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"
}
```

**Response Fields:**
- `status` (string): Always "revoked"
- `key_id` (string): Echo of the revoked key ID

**Error Responses:**

**404 Not Found:**
```json
{
  "error": "API_KEY_NOT_FOUND",
  "message": "API key not found"
}
```

**401 Unauthorized:**
```json
{
  "error": "INVALID_TOKEN",
  "message": "Bearer token is invalid"
}
```


### GET /devices

List all registered devices with pagination support.

**Authentication:** Required (Bearer token)

**Query Parameters:**
- `limit` (integer, optional): Maximum number of devices to return (default: 50, max: 100)
- `cursor` (string, optional): Pagination cursor from previous response

**Example Request:**
```
GET /devices?limit=20&cursor=eyJoYXJkd2FyZV9pZCI6IkFBOkJCOkNDOkREOkVFOkZGIiwiZ3NpMXNrIjoiMjAyNC0wMS0xNVQxNDoyMjowMFoifQ==
```

**Success Response (200 OK):**
```json
{
  "devices": [
    {
      "hardware_id": "AA:BB:CC:DD:EE:FF",
      "confirmation_id": "7c9e6679-7425-40de-944b-e07fc1f90ae7",
      "friendly_name": "greenhouse-sensor-01",
      "firmware_version": "1.0.16",
      "first_registered_at": "2024-01-15T10:30:00Z",
      "last_seen_at": "2024-01-15T14:22:00Z"
    },
    {
      "hardware_id": "BB:CC:DD:EE:FF:00",
      "confirmation_id": "8d0f7780-8536-51ef-b827-f18gd2g01bf8",
      "friendly_name": "barn-sensor-02",
      "firmware_version": "1.0.15",
      "first_registered_at": "2024-01-10T08:00:00Z",
      "last_seen_at": "2024-01-15T12:00:00Z"
    }
  ],
  "next_cursor": "eyJoYXJkd2FyZV9pZCI6IkJCOkNDOkREOkVFOkZGOjAwIiwiZ3NpMXNrIjoiMjAyNC0wMS0xNVQxMjowMDowMFoifQ=="
}
```

**Response Fields:**
- `devices` (array): List of device records
  - `hardware_id` (string): MAC address
  - `confirmation_id` (string): UUID v4 assigned to device
  - `friendly_name` (string): Human-readable device name
  - `firmware_version` (string): Current firmware version
  - `first_registered_at` (string): ISO 8601 timestamp of first registration
  - `last_seen_at` (string): ISO 8601 timestamp of last activity
- `next_cursor` (string, optional): Cursor for next page (omitted if no more results)

**Note:** Devices are sorted by `last_seen_at` descending (most recently active first).

**Error Responses:**

**401 Unauthorized:**
```json
{
  "error": "INVALID_TOKEN",
  "message": "Bearer token is invalid"
}
```


### GET /devices/{hardware_id}

Get detailed information for a specific device.

**Authentication:** Required (Bearer token)

**Path Parameters:**
- `hardware_id` (string, required): MAC address of the device (XX:XX:XX:XX:XX:XX)

**Example Request:**
```
GET /devices/AA:BB:CC:DD:EE:FF
```

**Success Response (200 OK):**
```json
{
  "hardware_id": "AA:BB:CC:DD:EE:FF",
  "confirmation_id": "7c9e6679-7425-40de-944b-e07fc1f90ae7",
  "friendly_name": "greenhouse-sensor-01",
  "firmware_version": "1.0.16",
  "capabilities": {
    "sensors": ["bme280", "ds18b20", "soil_moisture"],
    "features": {
      "tft_display": true,
      "offline_buffering": true
    }
  },
  "first_registered_at": "2024-01-15T10:30:00Z",
  "last_seen_at": "2024-01-15T14:22:00Z",
  "last_boot_id": "7c9e6679-7425-40de-944b-e07fc1f90ae7"
}
```

**Response Fields:**
- `hardware_id` (string): MAC address
- `confirmation_id` (string): UUID v4 assigned to device
- `friendly_name` (string): Human-readable device name
- `firmware_version` (string): Current firmware version
- `capabilities` (object): Device capabilities
  - `sensors` (array of strings): Available sensors
  - `features` (object): Feature flags
- `first_registered_at` (string): ISO 8601 timestamp of first registration
- `last_seen_at` (string): ISO 8601 timestamp of last activity
- `last_boot_id` (string): UUID v4 from most recent boot

**Error Responses:**

**404 Not Found:**
```json
{
  "error": "DEVICE_NOT_FOUND",
  "message": "Device not found"
}
```

**401 Unauthorized:**
```json
{
  "error": "INVALID_TOKEN",
  "message": "Bearer token is invalid"
}
```


### PUT /devices/{hardware_id}

Update the friendly_name field for a device.

**Note:** Historical sensor readings preserve the friendly_name value at the time of ingestion, so changes to the device's friendly_name do not affect previously stored readings.

**Authentication:** Required (Bearer token)

**Path Parameters:**
- `hardware_id` (string, required): MAC address of the device (XX:XX:XX:XX:XX:XX)

**Request Body:**
```json
{
  "friendly_name": "new-device-name"
}
```

**Request Fields:**
- `friendly_name` (string or null, required): New friendly name (max 64 characters, safe ASCII only) or null to remove

**Example Request:**
```
PUT /devices/AA:BB:CC:DD:EE:FF
Content-Type: application/json

{
  "friendly_name": "updated-greenhouse-sensor"
}
```

**Success Response (200 OK):**
```json
{
  "message": "Friendly name updated successfully",
  "hardware_id": "AA:BB:CC:DD:EE:FF",
  "friendly_name": "updated-greenhouse-sensor"
}
```

**Response Fields:**
- `message` (string): Success message
- `hardware_id` (string): MAC address of the device
- `friendly_name` (string or null): Updated friendly name

**Error Responses:**

**400 Bad Request (Invalid friendly_name):**
```json
{
  "error": "INVALID_VALUE",
  "message": "Invalid value for field: friendly_name: Friendly name length 65 exceeds maximum of 64 characters"
}
```

**404 Not Found:**
```json
{
  "error": "DEVICE_NOT_FOUND",
  "message": "Device not found"
}
```

**401 Unauthorized:**
```json
{
  "error": "INVALID_TOKEN",
  "message": "Bearer token is invalid"
}
```


### GET /devices/{hardware_id}/readings

Query historical sensor readings for a device with time range filtering and pagination.

**Authentication:** Required (Bearer token)

**Path Parameters:**
- `hardware_id` (string, required): MAC address of the device

**Query Parameters:**
- `from` (integer, optional): Start of time range in epoch milliseconds
- `to` (integer, optional): End of time range in epoch milliseconds
- `limit` (integer, optional): Maximum number of readings to return (default: 50, max: 1000)
- `cursor` (string, optional): Pagination cursor from previous response

**Example Request:**
```
GET /devices/AA:BB:CC:DD:EE:FF/readings?from=1704067200000&to=1704153600000&limit=100
```

**Success Response (200 OK):**
```json
{
  "readings": [
    {
      "timestamp_ms": 1704067800000,
      "batch_id": "AA:BB:CC:DD:EE:FF_7c9e6679-7425-40de-944b-e07fc1f90ae7_1704067200000_1704067800000",
      "boot_id": "7c9e6679-7425-40de-944b-e07fc1f90ae7",
      "firmware_version": "1.0.16",
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
      }
    }
  ],
  "next_cursor": "eyJoYXJkd2FyZV9pZCI6IkFBOkJCOkNDOkREOkVFOkZGIiwidHNfYmF0Y2giOiIxNzA0MDY3ODAwMDAwI0FBOkJCOkNDOkREOkVFOkZGXzdjOWU2Njc5LTc0MjUtNDBkZS05NDRiLWUwN2ZjMWY5MGFlN18xNzA0MDY3MjAwMDAwXzE3MDQwNjc4MDAwMDAifQ=="
}
```

**Response Fields:**
- `readings` (array): List of sensor readings
  - `timestamp_ms` (integer): Epoch milliseconds UTC
  - `batch_id` (string): Unique identifier for this reading
  - `boot_id` (string): UUID v4 from device boot
  - `firmware_version` (string): Firmware version at time of reading
  - `sensors` (object): Sensor values (all fields optional)
  - `sensor_status` (object): Sensor health status
- `next_cursor` (string, optional): Cursor for next page (omitted if no more results)

**Note:** Readings are sorted by `timestamp_ms` descending (newest first).

**Error Responses:**

**404 Not Found:**
```json
{
  "error": "DEVICE_NOT_FOUND",
  "message": "Device not found"
}
```

**400 Bad Request - Invalid Time Range:**
```json
{
  "error": "INVALID_VALUE",
  "message": "from timestamp must be less than or equal to to timestamp"
}
```

**401 Unauthorized:**
```json
{
  "error": "INVALID_TOKEN",
  "message": "Bearer token is invalid"
}
```


### GET /devices/{hardware_id}/latest

Get the most recent sensor reading for a device.

**Authentication:** Required (Bearer token)

**Path Parameters:**
- `hardware_id` (string, required): MAC address of the device

**Example Request:**
```
GET /devices/AA:BB:CC:DD:EE:FF/latest
```

**Success Response (200 OK):**
```json
{
  "timestamp_ms": 1704067800000,
  "batch_id": "AA:BB:CC:DD:EE:FF_7c9e6679-7425-40de-944b-e07fc1f90ae7_1704067200000_1704067800000",
  "boot_id": "7c9e6679-7425-40de-944b-e07fc1f90ae7",
  "firmware_version": "1.0.16",
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
  }
}
```

**Response Fields:**
- `timestamp_ms` (integer): Epoch milliseconds UTC
- `batch_id` (string): Unique identifier for this reading
- `boot_id` (string): UUID v4 from device boot
- `firmware_version` (string): Firmware version at time of reading
- `sensors` (object): Sensor values
- `sensor_status` (object): Sensor health status

**Error Responses:**

**404 Not Found - Device Not Found:**
```json
{
  "error": "DEVICE_NOT_FOUND",
  "message": "Device not found"
}
```

**404 Not Found - No Readings:**
```json
{
  "error": "NO_READINGS",
  "message": "Device exists but has no readings"
}
```

**401 Unauthorized:**
```json
{
  "error": "INVALID_TOKEN",
  "message": "Bearer token is invalid"
}
```


### GET /health

Health check endpoint for the Control Plane API. Does not require authentication.

**Authentication:** Not required

**Success Response (200 OK):**
```json
{
  "status": "healthy"
}
```

**Error Response (503 Service Unavailable):**
```json
{
  "status": "unhealthy",
  "error": "DATABASE_ERROR",
  "message": "DynamoDB connectivity check failed"
}
```

---

## Error Codes Reference

All error responses follow a consistent structure:

```json
{
  "error": "ERROR_CODE",
  "message": "Human-readable error description"
}
```

### Authentication Errors (401)

| Error Code | Description |
|------------|-------------|
| `MISSING_API_KEY` | X-API-Key header is missing from Data Plane request |
| `INVALID_API_KEY` | API key is invalid or not found in database |
| `KEY_REVOKED` | API key has been revoked (is_active=false) |
| `MISSING_TOKEN` | Authorization header is missing from Control Plane request |
| `INVALID_TOKEN` | Bearer token does not match configured admin token |

### Validation Errors (400)

| Error Code | Description |
|------------|-------------|
| `MISSING_FIELD` | Required field is missing from request body |
| `INVALID_FORMAT` | Field value does not match expected format (MAC, UUID, timestamp, etc.) |
| `INVALID_VALUE` | Field value is invalid (e.g., from > to in time range) |
| `BATCH_SIZE_EXCEEDED` | Readings array contains more than 100 items |

### Not Found Errors (404)

| Error Code | Description |
|------------|-------------|
| `DEVICE_NOT_FOUND` | Device with specified hardware_id does not exist |
| `NO_READINGS` | Device exists but has no sensor readings |
| `API_KEY_NOT_FOUND` | API key with specified key_id does not exist |

### Server Errors (500)

| Error Code | Description |
|------------|-------------|
| `DATABASE_ERROR` | DynamoDB operation failed |
| `INTERNAL_ERROR` | Unexpected internal server error |


## Rate Limits and Constraints

### Batch Size Limits

- **Maximum readings per POST /data request:** 100
- Requests exceeding this limit return 400 Bad Request
- Validation occurs after authentication to avoid leaking behavior to unauthenticated callers

### Pagination Limits

- **Default page size:** 50 items
- **Maximum page size:**
  - Device listings: 100 devices
  - API key listings: 100 keys
  - Reading queries: 1000 readings
- Use `limit` query parameter to control page size
- Use `cursor` query parameter to fetch subsequent pages

### Field Length Limits

- **batch_id:** Maximum 256 characters, safe ASCII only
- **friendly_name:** Recommended maximum 64 characters
- **description:** Recommended maximum 256 characters

### Timestamp Constraints

- **timestamp_ms:** Must be non-negative 64-bit integer
- **timestamp_ms:** Must be within reasonable range (not far future or distant past)
- **Time range queries:** `from` must be ≤ `to`

### API Key Constraints

- **API key format:** 64-character hexadecimal string
- **API key hashing:** SHA-256 with system pepper
- **last_used_at updates:** Throttled to once per 5 minutes per key

### Path Normalization

- All endpoints treat trailing slashes equivalently
- `/register` and `/register/` are the same endpoint
- Router normalizes paths internally


## Data Types and Formats

### MAC Address Format

**Format:** `XX:XX:XX:XX:XX:XX`
- Six groups of two uppercase hexadecimal digits
- Separated by colons
- Example: `AA:BB:CC:DD:EE:FF`

### UUID v4 Format

**Format:** `xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx`
- 8-4-4-4-12 hexadecimal characters
- Version bits: 4 (in third group)
- Variant bits: 10xx (in fourth group)
- Example: `550e8400-e29b-41d4-a716-446655440000`

### Timestamp Formats

**Epoch Milliseconds (timestamp_ms):**
- 64-bit integer representing milliseconds since Unix epoch (UTC)
- Used for sensor reading timestamps
- Example: `1704067800000` (January 1, 2024, 00:30:00 UTC)

**ISO 8601 / RFC3339 (metadata timestamps):**
- String format: `YYYY-MM-DDTHH:MM:SSZ`
- Always UTC with Z suffix
- Used for registration, creation, and last seen timestamps
- Example: `2024-01-15T10:30:00Z`

### Batch ID Format

**Format:** `{hardware_id}_{boot_id}_{window_start_ms}_{window_end_ms}`
- Opaque string, maximum 256 characters
- Safe ASCII characters only
- Generated by device firmware
- Example: `AA:BB:CC:DD:EE:FF_7c9e6679-7425-40de-944b-e07fc1f90ae7_1704067200000_1704067800000`

**Properties:**
- Stable: Uses hardware_id (immutable) not friendly_name (mutable)
- Unique: Includes boot_id to distinguish device restarts
- Deterministic: Same time window produces same batch_id for idempotent retries

### Cursor Format

**Format:** Base64-encoded JSON object
- Opaque pagination token
- Contains DynamoDB LastEvaluatedKey
- Example: `eyJoYXJkd2FyZV9pZCI6IkFBOkJCOkNDOkREOkVFOkZGIn0=`
- Clients should treat as opaque and not decode


## CORS Configuration

The Control Plane API supports Cross-Origin Resource Sharing (CORS) for web-based administration interfaces.

### CORS Headers

All Control Plane responses include:
- `Access-Control-Allow-Origin`: Configured via `CORS_ALLOWED_ORIGIN` environment variable
- `Access-Control-Allow-Methods`: `GET, POST, PUT, DELETE, OPTIONS`
- `Access-Control-Allow-Headers`: `Content-Type, Authorization, X-API-Key`
- `Access-Control-Max-Age`: `3600` (1 hour)

### Preflight Requests

The Control Plane API handles OPTIONS preflight requests:

**Request:**
```
OPTIONS /devices
Origin: https://admin.example.com
Access-Control-Request-Method: GET
Access-Control-Request-Headers: Authorization
```

**Response:**
```
200 OK
Access-Control-Allow-Origin: https://admin.example.com
Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization, X-API-Key
Access-Control-Max-Age: 3600
```

### Configuration

Set the `CORS_ALLOWED_ORIGIN` environment variable:
- Development: `*` (allow all origins)
- Production: `https://admin.example.com` (specific origin)

**Note:** The Data Plane API does not include CORS headers as it is intended for device-to-server communication only.


## Example Workflows

### Workflow 1: Device Registration and First Data Submission

**Step 1: Create API Key (Admin)**
```bash
curl -X POST https://control-plane-url/api-keys \
  -H "Authorization: Bearer admin-token-12345" \
  -H "Content-Type: application/json" \
  -d '{
    "description": "Production greenhouse devices"
  }'
```

**Response:**
```json
{
  "key_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "api_key": "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8",
  "created_at": "2024-01-15T14:30:00Z",
  "message": "API key created successfully. Save this key - it will not be shown again."
}
```

**Step 2: Register Device**
```bash
curl -X POST https://data-plane-url/register \
  -H "X-API-Key: 5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8" \
  -H "Content-Type: application/json" \
  -d '{
    "hardware_id": "AA:BB:CC:DD:EE:FF",
    "boot_id": "550e8400-e29b-41d4-a716-446655440000",
    "firmware_version": "1.0.16",
    "friendly_name": "greenhouse-sensor-01",
    "capabilities": {
      "sensors": ["bme280", "ds18b20"],
      "features": {
        "tft_display": true
      }
    }
  }'
```

**Response:**
```json
{
  "status": "registered",
  "confirmation_id": "7c9e6679-7425-40de-944b-e07fc1f90ae7",
  "hardware_id": "AA:BB:CC:DD:EE:FF",
  "registered_at": "2024-01-15T10:30:00Z"
}
```

**Step 3: Submit Sensor Data**
```bash
curl -X POST https://data-plane-url/data \
  -H "X-API-Key: 5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8" \
  -H "Content-Type: application/json" \
  -d '{
    "readings": [
      {
        "batch_id": "AA:BB:CC:DD:EE:FF_550e8400-e29b-41d4-a716-446655440000_1704067200000_1704067800000",
        "hardware_id": "AA:BB:CC:DD:EE:FF",
        "boot_id": "550e8400-e29b-41d4-a716-446655440000",
        "firmware_version": "1.0.16",
        "timestamp_ms": 1704067800000,
        "sensors": {
          "bme280_temp_c": 22.5,
          "humidity_pct": 45.2
        },
        "sensor_status": {
          "bme280": "ok",
          "ds18b20": "error"
        }
      }
    ]
  }'
```

**Response:**
```json
{
  "acknowledged_batch_ids": [
    "AA:BB:CC:DD:EE:FF_550e8400-e29b-41d4-a716-446655440000_1704067200000_1704067800000"
  ],
  "duplicate_batch_ids": []
}
```


### Workflow 2: Query Device History

**Step 1: List All Devices**
```bash
curl -X GET "https://control-plane-url/devices?limit=10" \
  -H "Authorization: Bearer admin-token-12345"
```

**Response:**
```json
{
  "devices": [
    {
      "hardware_id": "AA:BB:CC:DD:EE:FF",
      "confirmation_id": "7c9e6679-7425-40de-944b-e07fc1f90ae7",
      "friendly_name": "greenhouse-sensor-01",
      "firmware_version": "1.0.16",
      "first_registered_at": "2024-01-15T10:30:00Z",
      "last_seen_at": "2024-01-15T14:22:00Z"
    }
  ],
  "next_cursor": null
}
```

**Step 2: Get Device Details**
```bash
curl -X GET "https://control-plane-url/devices/AA:BB:CC:DD:EE:FF" \
  -H "Authorization: Bearer admin-token-12345"
```

**Response:**
```json
{
  "hardware_id": "AA:BB:CC:DD:EE:FF",
  "confirmation_id": "7c9e6679-7425-40de-944b-e07fc1f90ae7",
  "friendly_name": "greenhouse-sensor-01",
  "firmware_version": "1.0.16",
  "capabilities": {
    "sensors": ["bme280", "ds18b20"],
    "features": {
      "tft_display": true
    }
  },
  "first_registered_at": "2024-01-15T10:30:00Z",
  "last_seen_at": "2024-01-15T14:22:00Z",
  "last_boot_id": "550e8400-e29b-41d4-a716-446655440000"
}
```

**Step 3: Query Recent Readings**
```bash
curl -X GET "https://control-plane-url/devices/AA:BB:CC:DD:EE:FF/readings?limit=50" \
  -H "Authorization: Bearer admin-token-12345"
```

**Response:**
```json
{
  "readings": [
    {
      "timestamp_ms": 1704067800000,
      "batch_id": "AA:BB:CC:DD:EE:FF_550e8400-e29b-41d4-a716-446655440000_1704067200000_1704067800000",
      "boot_id": "550e8400-e29b-41d4-a716-446655440000",
      "firmware_version": "1.0.16",
      "sensors": {
        "bme280_temp_c": 22.5,
        "humidity_pct": 45.2
      },
      "sensor_status": {
        "bme280": "ok",
        "ds18b20": "error"
      }
    }
  ],
  "next_cursor": null
}
```

**Step 4: Get Latest Reading**
```bash
curl -X GET "https://control-plane-url/devices/AA:BB:CC:DD:EE:FF/latest" \
  -H "Authorization: Bearer admin-token-12345"
```

**Response:**
```json
{
  "timestamp_ms": 1704067800000,
  "batch_id": "AA:BB:CC:DD:EE:FF_550e8400-e29b-41d4-a716-446655440000_1704067200000_1704067800000",
  "boot_id": "550e8400-e29b-41d4-a716-446655440000",
  "firmware_version": "1.0.16",
  "sensors": {
    "bme280_temp_c": 22.5,
    "humidity_pct": 45.2
  },
  "sensor_status": {
    "bme280": "ok",
    "ds18b20": "error"
  }
}
```


### Workflow 3: Idempotent Retry with Duplicates

**Step 1: Submit Batch of Readings**
```bash
curl -X POST https://data-plane-url/data \
  -H "X-API-Key: 5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8" \
  -H "Content-Type: application/json" \
  -d '{
    "readings": [
      {
        "batch_id": "batch_001",
        "hardware_id": "AA:BB:CC:DD:EE:FF",
        "boot_id": "550e8400-e29b-41d4-a716-446655440000",
        "firmware_version": "1.0.16",
        "timestamp_ms": 1704067800000,
        "sensors": {"bme280_temp_c": 22.5},
        "sensor_status": {"bme280": "ok"}
      },
      {
        "batch_id": "batch_002",
        "hardware_id": "AA:BB:CC:DD:EE:FF",
        "boot_id": "550e8400-e29b-41d4-a716-446655440000",
        "firmware_version": "1.0.16",
        "timestamp_ms": 1704068100000,
        "sensors": {"bme280_temp_c": 23.0},
        "sensor_status": {"bme280": "ok"}
      }
    ]
  }'
```

**Response:**
```json
{
  "acknowledged_batch_ids": ["batch_001", "batch_002"],
  "duplicate_batch_ids": []
}
```

**Step 2: Retry Same Batch (Network Error Recovery)**
```bash
curl -X POST https://data-plane-url/data \
  -H "X-API-Key: 5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8" \
  -H "Content-Type: application/json" \
  -d '{
    "readings": [
      {
        "batch_id": "batch_001",
        "hardware_id": "AA:BB:CC:DD:EE:FF",
        "boot_id": "550e8400-e29b-41d4-a716-446655440000",
        "firmware_version": "1.0.16",
        "timestamp_ms": 1704067800000,
        "sensors": {"bme280_temp_c": 22.5},
        "sensor_status": {"bme280": "ok"}
      },
      {
        "batch_id": "batch_002",
        "hardware_id": "AA:BB:CC:DD:EE:FF",
        "boot_id": "550e8400-e29b-41d4-a716-446655440000",
        "firmware_version": "1.0.16",
        "timestamp_ms": 1704068100000,
        "sensors": {"bme280_temp_c": 23.0},
        "sensor_status": {"bme280": "ok"}
      }
    ]
  }'
```

**Response:**
```json
{
  "acknowledged_batch_ids": [],
  "duplicate_batch_ids": ["batch_001", "batch_002"]
}
```

**Note:** All batch_ids are now marked as duplicates, confirming idempotent behavior.


### Workflow 4: API Key Management

**Step 1: Create Multiple API Keys**
```bash
# Create key for production devices
curl -X POST https://control-plane-url/api-keys \
  -H "Authorization: Bearer admin-token-12345" \
  -H "Content-Type: application/json" \
  -d '{"description": "Production devices"}'

# Create key for test devices
curl -X POST https://control-plane-url/api-keys \
  -H "Authorization: Bearer admin-token-12345" \
  -H "Content-Type: application/json" \
  -d '{"description": "Test devices"}'
```

**Step 2: List All API Keys**
```bash
curl -X GET "https://control-plane-url/api-keys?limit=10" \
  -H "Authorization: Bearer admin-token-12345"
```

**Response:**
```json
{
  "api_keys": [
    {
      "key_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
      "created_at": "2024-01-15T14:30:00Z",
      "last_used_at": "2024-01-15T16:22:00Z",
      "is_active": true,
      "description": "Production devices"
    },
    {
      "key_id": "b2c3d4e5-f6a7-8901-bcde-f12345678901",
      "created_at": "2024-01-15T14:35:00Z",
      "last_used_at": null,
      "is_active": true,
      "description": "Test devices"
    }
  ],
  "next_cursor": null
}
```

**Step 3: Revoke API Key**
```bash
curl -X DELETE "https://control-plane-url/api-keys/b2c3d4e5-f6a7-8901-bcde-f12345678901" \
  -H "Authorization: Bearer admin-token-12345"
```

**Response:**
```json
{
  "status": "revoked",
  "key_id": "b2c3d4e5-f6a7-8901-bcde-f12345678901"
}
```

**Step 4: Verify Revoked Key Rejected**
```bash
curl -X POST https://data-plane-url/register \
  -H "X-API-Key: <revoked-key>" \
  -H "Content-Type: application/json" \
  -d '{...}'
```

**Response:**
```json
{
  "error": "KEY_REVOKED",
  "message": "API key has been revoked"
}
```

