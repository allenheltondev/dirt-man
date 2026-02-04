# DynamoDB Table Schemas

This document locks the DynamoDB table schemas for the Serverless Backend API. These schemas are frozen and should not be modified without careful consideration and migration planning.

**Schema Version:** 1.0.0
**Last Updated:** 2026-02-03
**Status:** LOCKED

## Timestamp Conventions

### Metadata Timestamps (RFC3339 Strings)
All metadata timestamps (device registration, API key creation, batch processing) use **RFC3339 format with UTC timezone** (e.g., `2024-01-15T10:30:00Z`).

**Rationale:** RFC3339 strings are:
- Lexicographically sortable (critical for GSI sort keys)
- Human-readable in logs and debugging
- Standard format with timezone information

**Fields using RFC3339:**
- `first_registered_at`
- `last_seen_at`
- `created_at`
- `last_used_at`
- `received_at`

### Reading Timestamps (Epoch Milliseconds)
Sensor reading timestamps use **64-bit integer epoch milliseconds** (e.g., `1704067800000`).

**Rationale:** Epoch milliseconds are:
- Native device format (no conversion needed)
- Compact storage
- Efficient for time-range queries when zero-padded in sort keys

**Fields using epoch milliseconds:**
- `timestamp_ms` (Number attribute)
- Part of `ts_batch` composite sort key (zero-padded 13 digits)

### TTL Timestamps (Epoch Seconds)
All TTL expiration times use **epoch seconds** (e.g., `1707235320`).

**Rationale:** DynamoDB TTL requires epoch seconds (not milliseconds).

**Fields using epoch seconds:**
- `expiration_time` (Number attribute for TTL)

## Table 1: devices

**Purpose:** Store device registration information and track device activity.

### Primary Key
- **Partition Key:** `hardware_id` (String)

### Attributes

| Attribute | Type | Required | Description | Example |
|-----------|------|----------|-------------|---------|
| `hardware_id` | String | Yes | MAC address (XX:XX:XX:XX:XX:XX) | `"AA:BB:CC:DD:EE:FF"` |
| `confirmation_id` | String | Yes | UUID v4 assigned at registration | `"550e8400-e29b-41d4-a716-446655440000"` |
| `capabilities` | Map | Yes | Device capabilities object | See below |
| `first_registered_at` | String | Yes | RFC3339 timestamp of first registration | `"2024-01-15T10:30:00Z"` |
| `last_seen_at` | String | Yes | RFC3339 timestamp of last activity | `"2024-01-15T14:22:00Z"` |
| `last_boot_id` | String | Yes | UUID v4 from most recent boot | `"7c9e6679-7425-40de-944b-e07fc1f90ae7"` |
| `firmware_version` | String | Yes | Firmware version string | `"1.0.16"` |
| `friendly_name` | String | No | User-assigned device name | `"greenhouse-sensor-01"` |
| `gsi1pk` | String | Yes | Constant value "devices" for GSI | `"devices"` |
| `gsi1sk` | String | Yes | Copy of `last_seen_at` for sorting | `"2024-01-15T14:22:00Z"` |

### Capabilities Object Structure
```json
{
  "sensors": ["bme280", "ds18b20", "soil_moisture"],
  "features": {
    "tft_display": true,
    "offline_buffering": true
  }
}
```

### Global Secondary Indexes

#### GSI1: Device Listing by Activity
- **Index Name:** `gsi1`
- **Partition Key:** `gsi1pk` (String)
- **Sort Key:** `gsi1sk` (String)
- **Projection:** ALL
- **Purpose:** List all devices sorted by last activity (most recent first)

**Query Pattern:**
```
KeyConditionExpression: gsi1pk = "devices"
ScanIndexForward: false (descending by last_seen_at)
```

### Example Record
```json
{
  "hardware_id": "AA:BB:CC:DD:EE:FF",
  "confirmation_id": "550e8400-e29b-41d4-a716-446655440000",
  "friendly_name": "greenhouse-sensor-01",
  "firmware_version": "1.0.16",
  "capabilities": {
    "sensor
`
- **Friendly Name Update:** UpdateItem to set `friendly_name` (optional enhancement)

---

## Table 2: device_readings

**Purpose:** Store time-series sensor data for querying and analysis.

### Primary Key
- **Partition Key:** `hardware_id` (String)
- **Sort Key:** `ts_batch` (String)

### Sort Key Format
The `ts_batch` sort key is a composite string with format:
```
{timestamp_ms:013}#{batch_id}
```

**Example:** `"1704067800000#AA:BB:CC:DD:EE:FF_7c9e6679-7425-40de-944b-e07fc1f90ae7_1704067200000_1704067800000"`

**Rationale:**
- Zero-padded 13 digits ensure lexicographic sorting matches numeric sorting
- `#` separator allows range queries by timestamp prefix
- Includes `batch_id` for uniqueness within the same millisecond

### Attributes

| Attribute | Type | Required | Description | Example |
|-----------|------|----------|-------------|---------|
| `hardware_id` | String | Yes | MAC address (partition key) | `"AA:BB:CC:DD:EE:FF"` |
| `ts_batch` | String | Yes | Composite sort key (see above) | `"1704067800000#..."` |
| `timestamp_ms` | Number | Yes | Epoch milliseconds (i64) | `1704067800000` |
| `batch_id` | String | Yes | Unique batch identifier | See batch_id format below |
| `boot_id` | String | Yes | UUID v4 from device boot | `"7c9e6679-7425-40de-944b-e07fc1f90ae7"` |
| `firmware_version` | String | Yes | Firmware version at time of reading | `"1.0.16"` |
| `friendly_name` | String | No | Snapshot of friendly_name at ingestion | `"greenhouse-sensor-01"` |
| `sensors` | Map | Yes | Sensor values object | See below |
| `sensor_status` | Map | Yes | Sensor status object | See below |
| `expiration_time` | Number | No | Epoch seconds for TTL | `1707235320` |

### Batch ID Format
Generated by device firmware:
```
{hardware_id}_{boot_id}_{window_start_ms}_{window_end_ms}
```

**Example:** `"AA:BB:CC:DD:EE:FF_7c9e6679-7425-40de-944b-e07fc1f90ae7_1704067200000_1704067800000"`

**Validation:** Max 256 characters, safe ASCII charset (treat as opaque on server side)

### Sensors Object Structure
```json
{
  "bme280_temp_c": 22.5,
  "ds18b20_temp_c": 21.8,
  "humidity_pct": 45.2,
  "pressure_hpa": 1013.25,
  "soil_moisture_pct": 62.3
}
```

**Note:** All sensor fields are optional (device may not have all sensors).

### Sensor Status Object Structure
```json
{
  "bme280": "ok",
  "ds18b20": "ok",
  "soil_moisture": "ok"
}
```

**Valid Status Values:** `"ok"`, `"error"`

### Example Record
```json
{
  "hardware_id": "AA:BB:CC:DD:EE:FF",
  "ts_batch": "1704067800000#AA:BB:CC:DD:EE:FF_7c9e6679-7425-40de-944b-e07fc1f90ae7_1704067200000_1704067800000",
  "timestamp_ms": 1704067800000,
  "batch_id": "AA:BB:CC:DD:EE:FF_7c9e6679-7425-40de-944b-e07fc1f90ae7_1704067200000_1704067800000",
  "boot_id": "7c9e6679-7425-40de-944b-e07fc1f90ae7",
  "firmware_version": "1.0.16",
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
  },
  "expiration_time": 1707235320
}
```

### Query Patterns

#### Latest Reading
```
KeyConditionExpression: hardware_id = :hw_id
ScanIndexForward: false
Limit: 1
```

#### Time Range Query
```
KeyConditionExpression: hardware_id = :hw_id AND ts_batch BETWEEN :from AND :to
```

**Range Key Format:**
- From: `"{from_ms:013}#"` (e.g., `"1704067200000#"`)
- To: `"{to_ms:013}#\uffff"` (e.g., `"1704067800000#\uffff"`)

The `\uffff` suffix ensures all batch_ids within the timestamp are included.

### TTL Configuration
- **Attribute:** `expiration_time`
- **Format:** Epoch seconds (not milliseconds)
- **Retention:** Configurable (e.g., 90 days)
- **Calculation:** `timestamp_ms / 1000 + retention_seconds`

---

## Table 3: processed_batches

**Purpose:** Track processed batch IDs for idempotent data ingestion.

### Primary Key
- **Partition Key:** `batch_id` (String)

### Attributes

| Attribute | Type | Required | Description | Example |
|-----------|------|----------|-------------|---------|
| `batch_id` | String | Yes | Unique batch identifier (partition key) | See format below |
| `hardware_id` | String | Yes | MAC address of device | `"AA:BB:CC:DD:EE:FF"` |
| `received_at` | String | Yes | RFC3339 timestamp of first receipt | `"2024-01-15T14:22:00Z"` |
| `expiration_time` | Number | Yes | Epoch seconds for TTL (30 days) | `1707235320` |

### Batch ID Format
Same as in `device_readings` table:
```
{hardware_id}_{boot_id}_{window_start_ms}_{window_end_ms}
```

### Example Record
```json
{
  "batch_id": "AA:BB:CC:DD:EE:FF_7c9e6679-7425-40de-944b-e07fc1f90ae7_1704067200000_1704067800000",
  "hardware_id": "AA:BB:CC:DD:EE:FF",
  "received_at": "2024-01-15T14:22:00Z",
  "expiration_time": 1707235320
}
```

### Idempotency Pattern
```
TransactWriteItems:
  1. PutItem to processed_batches with condition: attribute_not_exists(batch_id)
  2. PutItem to device_readings (only if #1 succeeds)
```

**Result:**
- Success: Both writes committed, batch_id acknowledged
- ConditionalCheckFailedException: Duplicate detected, no writes committed

### TTL Configuration
- **Attribute:** `expiration_time`
- **Format:** Epoch seconds
- **Retention:** 30 days
- **Calculation:** `current_epoch_seconds + (30 * 24 * 3600)`

---

## Table 4: api_keys

**Purpose:** Store API keys for device authentication.

### Primary Key
- **Partition Key:** `key_id` (String)

### Attributes

| Attribute | Type | Required | Description | Example |
|-----------|------|----------|-------------|---------|
| `key_id` | String | Yes | UUID v4 identifier (partition key) | `"a1b2c3d4-e5f6-7890-abcd-ef1234567890"` |
| `api_key_hash` | String | Yes | SHA-256 hash of raw key with pepper | `"5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8"` |
| `created_at` | String | Yes | RFC3339 timestamp of creation | `"2024-01-10T08:00:00Z"` |
| `last_used_at` | String | No | RFC3339 timestamp of last use | `"2024-01-15T14:22:00Z"` |
| `is_active` | Boolean | Yes | Whether key is active (not revoked) | `true` |
| `description` | String | No | Admin-provided description | `"Production devices - greenhouse cluster"` |
| `gsi1pk` | String | Yes | Constant value "api_keys" for GSI | `"api_keys"` |
| `gsi1sk` | String | Yes | Copy of `created_at` for sorting | `"2024-01-10T08:00:00Z"` |

### API Key Hashing
```
hash = SHA256(API_KEY_PEPPER + raw_api_key)
```

**Security Notes:**
- Raw API key is 64-character hex string (32 bytes random)
- Pepper is stored in environment variable (not in database)
- Raw key only shown once at creation
- Hash stored in database for validation

### Global Secondary Indexes

#### GSI: api_key_hash_index (Lookup by Hash)
- **Index Name:** `api_key_hash_index`
- **Partition Key:** `api_key_hash` (String)
- **Projection:** ALL
- **Purpose:** Validate API keys by looking up hash

**Query Pattern:**
```
KeyConditionExpression: api_key_hash = :hash
```

#### GSI1: API Key Listing by Creation Date
- **Index Name:** `gsi1`
- **Partition Key:** `gsi1pk` (String)
- **Sort Key:** `gsi1sk` (String)
- **Projection:** ALL
- **Purpose:** List all API keys sorted by creation date (newest first)

**Query Pattern:**
```
KeyConditionExpression: gsi1pk = "api_keys"
ScanIndexForward: false (descending by created_at)
```

### Example Record
```json
{
  "key_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "api_key_hash": "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8",
  "created_at": "2024-01-10T08:00:00Z",
  "last_used_at": "2024-01-15T14:22:00Z",
  "is_active": true,
  "description": "Production devices - greenhouse cluster",
  "gsi1pk": "api_keys",
  "gsi1sk": "2024-01-10T08:00:00Z"
}
```

### Update Patterns
- **Create:** PutItem with all fields, `is_active=true`
- **Update Last Used:** UpdateItem to set `last_used_at` (throttled to 5-minute intervals)
- **Revoke:** UpdateItem to set `is_active=false`

---

## Schema Migration Guidelines

This schema is **LOCKED** and should not be modified without following these guidelines:

### Adding New Attributes
✅ **Safe:** Adding optional attributes to existing tables
- No migration required
- Update application code to handle new fields
- Document in this file

### Modifying Existing Attributes
⚠️ **Requires Migration:** Changing attribute types or meanings
- Create migration plan
- Version schema document
- Deploy backward-compatible code first
- Migrate data
- Deploy new code
- Update this document

### Changing Primary Keys or GSI Keys
❌ **Not Supported:** DynamoDB does not support key changes
- Requires creating new table
- Migrate all data
- Update application code
- Switch traffic
- Delete old table

### Adding New GSIs
✅ **Safe:** Adding new GSIs for new query patterns
- No data migration required
- DynamoDB backfills automatically
- Update application code
- Document in this file

### Removing GSIs
✅ **Safe:** Removing unused GSIs
- Verify no code uses the GSI
- Delete GSI via CloudFormation
- Update this document

---

## Validation Rules

### hardware_id (MAC Address)
- **Format:** `XX:XX:XX:XX:XX:XX`
- **Regex:** `^[0-9A-F]{2}(:[0-9A-F]{2}){5}$`
- **Example:** `"AA:BB:CC:DD:EE:FF"`

### UUID v4 Fields (confirmation_id, boot_id, key_id)
- **Format:** `8-4-4-4-12` hexadecimal
- **Version Bits:** Byte 6 bits 4-7 = 0100 (version 4)
- **Variant Bits:** Byte 8 bits 6-7 = 10 (RFC 4122)
- **Example:** `"550e8400-e29b-41d4-a716-446655440000"`

### timestamp_ms (Epoch Milliseconds)
- **Type:** i64 (64-bit signed integer)
- **Range:** Non-negative, reasonable bounds (e.g., 2000-01-01 to 2100-01-01)
- **Example:** `1704067800000`

### batch_id (Opaque String)
- **Max Length:** 256 characters
- **Charset:** Safe ASCII (alphanumeric, underscore, hyphen)
- **Validation:** Treat as opaque on server side
- **Example:** `"AA:BB:CC:DD:EE:FF_7c9e6679-7425-40de-944b-e07fc1f90ae7_1704067200000_1704067800000"`

### RFC3339 Timestamps
- **Format:** `YYYY-MM-DDTHH:MM:SSZ`
- **Timezone:** Always UTC (Z suffix)
- **Example:** `"2024-01-15T10:30:00Z"`

---

## Performance Considerations

### Write Patterns
- **Devices:** Low write frequency (registration/re-registration)
- **API Keys:** Very low write frequency (creation/revocation)
- **Processed Batches:** High write frequency (per reading)
- **Device Readings:** High write frequency (per reading)

### Read Patterns
- **Devices:** Moderate read frequency (listing, detail queries)
- **API Keys:** High read frequency (every data plane request)
- **Processed Batches:** High read frequency (idempotency checks)
- **Device Readings:** Moderate read frequency (time-range queries)

### Scaling Assumptions
- **Device Count:** 1,000 - 10,000 devices
- **Reading Frequency:** 1 reading per 5 minutes per device
- **Batch Size:** 1-100 readings per request
- **Expected RPS:** ~50-100 requests/second peak

### Hot Partition Mitigation
- **API Keys:** Hash-based lookup distributes load
- **Device Readings:** Partitioned by hardware_id (natural distribution)
- **Processed Batches:** Partitioned by batch_id (natural distribution)

If a single device exceeds 1 write/second sustained, consider sharding strategy.

---

## Change Log

### Version 1.0.0 (2026-02-03)
- Initial schema lock
- Defined all four tables with complete attribute specifications
- Documented timestamp conventions (RFC3339 vs epoch milliseconds vs epoch seconds)
- Documented TTL configuration (30 days for processed_batches)
- Documented GSI structures for devices and api_keys tables
- Documented composite sort key format for device_readings
- Documented validation rules for all key fields
- Documented query patterns and performance considerations
