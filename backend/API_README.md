# API Documentation

This directory contains comprehensive API documentation for the Serverless Backend API.

## Documentation Files

### 1. API_DOCUMENTATION.md
Comprehensive human-readable API documentation including:
- Overview and authentication methods
- Detailed endpoint documentation with request/response examples
- Error codes reference
- Rate limits and constraints
- Data types and formats
- CORS configuration
- Example workflows for common use cases

**Use this for:** Understanding the API, integration planning, and reference during development.

### 2. openapi.yaml
OpenAPI 3.0 specification for the API including:
- Complete endpoint definitions
- Request/response schemas
- Authentication schemes
- Error response examples
- Data type definitions

**Use this for:**
- Generating client SDKs
- API testing tools (Swagger UI, Postman)
- Contract testing
- Documentation generation

**View in Swagger UI:**
```bash
# Using Docker
docker run -p 8080:8080 -e SWAGGER_JSON=/api/openapi.yaml -v $(pwd):/api swaggerapi/swagger-ui

# Or upload to https://editor.swagger.io/
```

### 3. postman_collection.json
Postman collection with pre-configured requests for all endpoints including:
- Data Plane endpoints (register, data submission)
- Control Plane endpoints (API keys, devices, readings)
- Environment variables for easy configuration
- Example request bodies

**Use this for:**
- Manual API testing
- Exploring the API
- Sharing with team members

**Import into Postman:**
1. Open Postman
2. Click "Import" button
3. Select `postman_collection.json`
4. Configure environment variables:
   - `data_plane_url`: Your Data Plane Function URL
   - `control_plane_url`: Your Control Plane Function URL
   - `api_key`: A valid 64-character hex API key
   - `admin_token`: Your admin bearer token


## Quick Start

### 1. Set Up Environment Variables

For Postman collection:
```json
{
  "data_plane_url": "https://abc123.lambda-url.us-east-1.on.aws",
  "control_plane_url": "https://def456.lambda-url.us-east-1.on.aws",
  "api_key": "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8",
  "admin_token": "my-secret-admin-token"
}
```

### 2. Create Your First API Key

```bash
curl -X POST https://your-control-plane-url/api-keys \
  -H "Authorization: Bearer your-admin-token" \
  -H "Content-Type: application/json" \
  -d '{"description": "My first API key"}'
```

Save the returned `api_key` value - it will not be shown again!

### 3. Register a Device

```bash
curl -X POST https://your-data-plane-url/register \
  -H "X-API-Key: your-api-key" \
  -H "Content-Type: application/json" \
  -d '{
    "hardware_id": "AA:BB:CC:DD:EE:FF",
    "boot_id": "550e8400-e29b-41d4-a716-446655440000",
    "firmware_version": "1.0.0",
    "capabilities": {
      "sensors": ["bme280"],
      "features": {}
    }
  }'
```

### 4. Submit Sensor Data

```bash
curl -X POST https://your-data-plane-url/data \
  -H "X-API-Key: your-api-key" \
  -H "Content-Type: application/json" \
  -d '{
    "readings": [{
      "batch_id": "AA:BB:CC:DD:EE:FF_550e8400-e29b-41d4-a716-446655440000_1704067200000_1704067800000",
      "hardware_id": "AA:BB:CC:DD:EE:FF",
      "boot_id": "550e8400-e29b-41d4-a716-446655440000",
      "firmware_version": "1.0.0",
      "timestamp_ms": 1704067800000,
      "sensors": {"bme280_temp_c": 22.5},
      "sensor_status": {"bme280": "ok"}
    }]
  }'
```

## API Overview

### Data Plane API
**Purpose:** High-volume device operations
**Authentication:** X-API-Key header
**Endpoints:**
- `POST /register` - Register or update device
- `POST /data` - Submit sensor readings (max 100 per request)
- `GET /health` - Health check

### Control Plane API
**Purpose:** Administrative operations
**Authentication:** Bearer token
**Endpoints:**
- `POST /api-keys` - Create API key
- `GET /api-keys` - List API keys
- `DELETE /api-keys/{key_id}` - Revoke API key
- `GET /devices` - List devices
- `GET /devices/{hardware_id}` - Get device details
- `PUT /devices/{hardware_id}` - Update device friendly_name
- `GET /devices/{hardware_id}/readings` - Query readings
- `GET /devices/{hardware_id}/latest` - Get latest reading
- `GET /health` - Health check

## Key Concepts

### Idempotency
The API implements idempotent processing using `batch_id`:
- Each reading has a unique `batch_id`
- Duplicate submissions are detected and skipped
- Response partitions batch_ids into `acknowledged_batch_ids` (new) and `duplicate_batch_ids` (seen before)
- Safe to retry failed requests

### Pagination
List endpoints support cursor-based pagination:
- Use `limit` query parameter to control page size
- Use `cursor` query parameter from `next_cursor` in response
- Cursors are opaque base64-encoded tokens

### Timestamps
Two timestamp formats are used:
- **Epoch milliseconds** (`timestamp_ms`): For sensor readings (64-bit integer)
- **ISO 8601** (RFC3339): For metadata timestamps (string with Z suffix)

### MAC Address Format
Hardware IDs must be in format: `XX:XX:XX:XX:XX:XX`
- Six groups of two uppercase hexadecimal digits
- Separated by colons
- Example: `AA:BB:CC:DD:EE:FF`

### Batch ID Format
Format: `{hardware_id}_{boot_id}_{window_start_ms}_{window_end_ms}`
- Opaque string, maximum 256 characters
- Safe ASCII characters only
- Generated by device firmware
- Ensures idempotent retries

## Error Handling

All errors follow a consistent structure:
```json
{
  "error": "ERROR_CODE",
  "message": "Human-readable description"
}
```

Common error codes:
- `MISSING_API_KEY` (401) - X-API-Key header missing
- `INVALID_API_KEY` (401) - API key invalid or not found
- `KEY_REVOKED` (401) - API key has been revoked
- `MISSING_TOKEN` (401) - Authorization header missing
- `INVALID_TOKEN` (401) - Bearer token invalid
- `MISSING_FIELD` (400) - Required field missing
- `INVALID_FORMAT` (400) - Field format invalid
- `BATCH_SIZE_EXCEEDED` (400) - More than 100 readings
- `DEVICE_NOT_FOUND` (404) - Device doesn't exist
- `NO_READINGS` (404) - Device has no readings
- `DATABASE_ERROR` (500) - DynamoDB operation failed

See `API_DOCUMENTATION.md` for complete error code reference.

## Rate Limits and Constraints

- **Batch size:** Maximum 100 readings per POST /data request
- **Pagination:** Default 50 items, max 100 (devices/keys) or 1000 (readings)
- **batch_id length:** Maximum 256 characters
- **API key format:** 64-character hexadecimal string
- **last_used_at updates:** Throttled to once per 5 minutes per key

## CORS Support

The Control Plane API supports CORS for web-based admin interfaces:
- Configured via `CORS_ALLOWED_ORIGIN` environment variable
- Supports preflight OPTIONS requests
- Allows GET, POST, PUT, DELETE methods
- Allows Content-Type, Authorization, X-API-Key headers

The Data Plane API does not include CORS headers (device-to-server only).

## Additional Resources

- **Design Document:** `design.md` - Architecture and implementation details
- **Requirements Document:** `requirements.md` - Functional requirements
- **Tasks Document:** `tasks.md` - Implementation task list
- **SAM Template:** `template.yaml` - Infrastructure as code

## Support

For questions or issues:
1. Check the API documentation for endpoint details
2. Review error codes in the error reference
3. Test with Postman collection to isolate issues
4. Check CloudWatch logs for detailed error messages
