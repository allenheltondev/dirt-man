# Requirements Document: Serverless Backend API

## Introduction

This specification defines a serverless backend API system for ESP32 environmental monitoring devices. The system consists of two AWS Lambda functions deployed as "lambdaliths" (single binaries with internal routing) written in Rust and triggered by function URLs. The Data Plane API handles high-volume device communication (registration and sensor data ingestion), while the Control Plane API provides administrative functions for managing API keys and devices. Sensor data is published to CloudWatch with appropriate dimensions for monitoring and analysis.

## Glossary

- **Data_Plane_API**: The Lambda function handling device registration and sensor data ingestion
- **Control_Plane_API**: The Lambda function handling administrative operations (API key management, device listing, and device queries)
- **Lambdalith**: A single Lambda function binary with internal routing to handle multiple endpoints
- **Function_URL**: AWS Lambda's built-in HTTP endpoint that triggers Lambda execution
- **Device**: An ESP32-based sensor hardware unit that sends data to the backend
- **Hardware_ID**: The immutable MAC address of the ESP32, formatted as colon-separated hexadecimal
- **Confirmation_ID**: A UUID v4 returned upon successful device registration
- **Boot_ID**: A UUID v4 generated on each device boot to track device sessions
- **API_Key**: Authentication credential used by devices to access the Data Plane API
- **Batch_ID**: Unique identifier for each averaged sensor reading, enabling idempotent retries
- **Timestamp_ms**: Epoch time in milliseconds (UTC), represented as a 64-bit integer
- **DynamoDB**: AWS NoSQL database service for storing device, API key, and sensor reading data
- **Sensor_Reading**: A collection of averaged sensor measurements from a device
- **GSI**: Global Secondary Index in DynamoDB for alternative query patterns
- **TTL**: Time To Live attribute in DynamoDB for automatic record expiration
- **Cursor**: Pagination token (base64-encoded LastEvaluatedKey) for DynamoDB queries

## Requirements

### Requirement 1: Data Plane Lambda Architecture

**User Story:** As a system architect, I want the data plane API to be a single Rust Lambda function with internal routing, so that I can minimize cold start overhead and simplify deployment.

#### Acceptance Criteria

1. THE Data_Plane_API SHALL be implemented as a single Rust Lambda function
2. THE Data_Plane_API SHALL use internal routing to handle multiple endpoints
3. THE Data_Plane_API SHALL be triggered by an AWS Lambda Function URL
4. THE Data_Plane_API SHALL support HTTP POST requests
5. THE Data_Plane_API SHALL parse the request path to determine which handler to invoke
6. THE Data_Plane_API SHALL return appropriate HTTP status codes and JSON responses

### Requirement 2: Device Registration Endpoint

**User Story:** As a device, I want to register with the backend using my hardware ID and capabilities, so that the backend knows about my existence and configuration.

#### Acceptance Criteria

1. THE Data_Plane_API SHALL provide a POST /register endpoint
2. WHEN a device sends a registration request, THE Data_Plane_API SHALL validate the API key
3. WHEN the Hardware_ID is not already registered, THE Data_Plane_API SHALL create a new device record
4. WHEN the Hardware_ID is already registered, THE Data_Plane_API SHALL return the existing Confirmation_ID
5. THE Data_Plane_API SHALL generate a UUID v4 Confirmation_ID for new registrations
6. THE Data_Plane_API SHALL store device information in DynamoDB
7. THE Data_Plane_API SHALL return HTTP 200 with the Confirmation_ID on success
8. THE Data_Plane_API SHALL return HTTP 401 for invalid API keys
9. THE Data_Plane_API SHALL return HTTP 400 for malformed requests

### Requirement 3: Registration Request Validation

**User Story:** As a backend developer, I want registration requests to be validated, so that only well-formed data is accepted.

#### Acceptance Criteria

1. THE Data_Plane_API SHALL require the hardware_id field in registration requests
2. THE Data_Plane_API SHALL require the boot_id field in registration requests
3. THE Data_Plane_API SHALL require the firmware_version field in registration requests
4. THE Data_Plane_API SHALL require the capabilities object in registration requests
5. THE Data_Plane_API SHALL validate that hardware_id matches MAC address format (XX:XX:XX:XX:XX:XX)
6. THE Data_Plane_API SHALL validate that boot_id is a valid UUID v4 format
7. THE Data_Plane_API SHALL accept an optional friendly_name field
8. WHEN validation fails, THE Data_Plane_API SHALL return HTTP 400 with a descriptive error message

### Requirement 4: Device Data Storage

**User Story:** As a backend developer, I want device registration data stored in DynamoDB, so that I can query and manage devices efficiently.

#### Acceptance Criteria

1. THE Data_Plane_API SHALL store device records in a DynamoDB table
2. THE device record SHALL include hardware_id as the partition key
3. THE device record SHALL include confirmation_id (UUID v4)
4. THE device record SHALL include friendly_name (if provided)
5. THE device record SHALL include firmware_version
6. THE device record SHALL include capabilities object
7. THE device record SHALL include first_registered_at timestamp (ISO 8601)
8. THE device record SHALL include last_seen_at timestamp (ISO 8601)
9. THE device record SHALL include last_boot_id
10. WHEN a device re-registers, THE Data_Plane_API SHALL update last_seen_at and last_boot_id

### Requirement 5: Sensor Data Ingestion Endpoint

**User Story:** As a device, I want to send sensor readings to the backend, so that my data can be stored and queried.

#### Acceptance Criteria

1. THE Data_Plane_API SHALL provide a POST /data endpoint
2. WHEN a device sends sensor data, THE Data_Plane_API SHALL validate the API key
3. THE Data_Plane_API SHALL accept both single readings and batch readings (normalized to array internally)
4. THE Data_Plane_API SHALL validate required fields in sensor data payloads
5. THE Data_Plane_API SHALL store sensor readings in DynamoDB
6. THE Data_Plane_API SHALL implement idempotent processing using batch_id
7. THE Data_Plane_API SHALL return HTTP 200 with acknowledged_batch_ids and duplicate_batch_ids on success
8. THE Data_Plane_API SHALL return HTTP 401 for invalid API keys
9. THE Data_Plane_API SHALL return HTTP 400 for malformed requests
10. THE Data_Plane_API SHALL limit batch size to 100 readings per request

### Requirement 6: Sensor Data Validation

**User Story:** As a backend developer, I want sensor data to be validated, so that only well-formed data is processed.

#### Acceptance Criteria

1. THE Data_Plane_API SHALL require the batch_id field in sensor data
2. THE Data_Plane_API SHALL require the hardware_id field in sensor data
3. THE Data_Plane_API SHALL require the timestamp_ms field in sensor data
4. THE Data_Plane_API SHALL validate that timestamp_ms is a 64-bit integer representing epoch milliseconds in UTC
5. THE Data_Plane_API SHALL require the sensors object in sensor data
6. THE Data_Plane_API SHALL require the sensor_status object in sensor data
7. THE Data_Plane_API SHALL validate that batch_id follows the expected format
8. THE Data_Plane_API SHALL accept boot_id and firmware_version fields
9. THE Data_Plane_API SHALL accept optional friendly_name field
10. WHEN validation fails, THE Data_Plane_API SHALL return HTTP 400 with a descriptive error message

### Requirement 7: Sensor Reading Storage

**User Story:** As a backend developer, I want sensor readings stored in DynamoDB with efficient query patterns, so that I can retrieve device history quickly.

#### Acceptance Criteria

1. THE Data_Plane_API SHALL store sensor readings in a DynamoDB table
2. THE reading record SHALL use hardware_id as the partition key
3. THE reading record SHALL use a composite sort key format: "{timestamp_ms}#{batch_id}"
4. THE reading record SHALL include timestamp_ms as a number attribute
5. THE reading record SHALL include batch_id
6. THE reading record SHALL include boot_id
7. THE reading record SHALL include firmware_version
8. THE reading record SHALL include sensors object (map of sensor values)
9. THE reading record SHALL include sensor_status object (map of sensor statuses)
10. THE reading record SHALL optionally include friendly_name as a snapshot
11. THE reading record SHALL support TTL for automatic expiration via an expiration_time attribute
12. THE system SHALL assume typical device write frequency under 1 write per second per device; if higher throughput is required, a sharded partition strategy MAY be introduced

### Requirement 8: Idempotent Data Processing

**User Story:** As a device, I want the backend to handle duplicate submissions gracefully, so that network retries don't create duplicate data.

#### Acceptance Criteria

1. THE Data_Plane_API SHALL track processed batch_ids in DynamoDB
2. THE Data_Plane_API SHALL use a conditional PutItem with attribute_not_exists(batch_id) for idempotency checks
3. WHEN a batch_id has already been processed, THE Data_Plane_API SHALL skip writing the reading
4. THE Data_Plane_API SHALL include duplicate batch_ids in the duplicate_batch_ids response field
5. THE Data_Plane_API SHALL include newly processed batch_ids in the acknowledged_batch_ids response field
6. THE acknowledged_batch_ids field SHALL include only newly processed batch_ids
7. THE duplicate_batch_ids field SHALL include only previously seen batch_ids
8. THE Data_Plane_API SHALL use a DynamoDB table with batch_id as the partition key
9. THE Data_Plane_API SHALL set a TTL on batch_id records to expire after 30 days
10. THE Data_Plane_API SHALL handle batch submissions by checking each batch_id individually
11. THE Data_Plane_API SHALL store hardware_id and received_at timestamp in the processed_batches record

### Requirement 9: API Key Authentication

**User Story:** As a system administrator, I want devices to authenticate using API keys, so that only authorized devices can access the backend.

#### Acceptance Criteria

1. THE Data_Plane_API SHALL require an X-API-Key header in all requests
2. THE Data_Plane_API SHALL validate the API key against a DynamoDB table
3. WHEN the API key is valid, THE Data_Plane_API SHALL process the request
4. WHEN the API key is invalid or missing, THE Data_Plane_API SHALL return HTTP 401
5. THE Data_Plane_API SHALL check if the API key is active (not revoked)
6. THE Data_Plane_API SHALL update the last_used_at timestamp for valid API keys

### Requirement 10: Control Plane Lambda Architecture

**User Story:** As a system architect, I want the control plane API to be a separate Rust Lambda function, so that administrative operations are isolated from high-volume device traffic.

#### Acceptance Criteria

1. THE Control_Plane_API SHALL be implemented as a single Rust Lambda function
2. THE Control_Plane_API SHALL use internal routing to handle multiple endpoints
3. THE Control_Plane_API SHALL be triggered by an AWS Lambda Function URL
4. THE Control_Plane_API SHALL support HTTP GET, POST, PUT, and DELETE requests
5. THE Control_Plane_API SHALL parse the request path and method to determine which handler to invoke

### Requirement 11: API Key Management

**User Story:** As a system administrator, I want to create, list, and revoke API keys, so that I can control device access to the backend.

#### Acceptance Criteria

1. THE Control_Plane_API SHALL provide a POST /api-keys endpoint to create new API keys
2. THE Control_Plane_API SHALL provide a GET /api-keys endpoint to list all API keys
3. THE Control_Plane_API SHALL provide a DELETE /api-keys/{key_id} endpoint to revoke API keys
4. WHEN creating an API key, THE Control_Plane_API SHALL generate a cryptographically secure random key
5. WHEN creating an API key, THE Control_Plane_API SHALL return the raw key value once in the response
6. THE Control_Plane_API SHALL store a hashed representation of the API key value
7. THE Control_Plane_API SHALL NOT store the raw API key after creation
8. THE Control_Plane_API SHALL store API key records in DynamoDB
9. THE API key record SHALL include key_id (UUID v4)
10. THE API key record SHALL include api_key_hash (hashed key value)
11. THE API key record SHALL include created_at timestamp
12. THE API key record SHALL include last_used_at timestamp
13. THE API key record SHALL include is_active boolean flag
14. WHEN revoking an API key, THE Control_Plane_API SHALL set is_active to false

### Requirement 12: Device Listing

**User Story:** As a system administrator, I want to list all registered devices, so that I can monitor device inventory and status.

#### Acceptance Criteria

1. THE Control_Plane_API SHALL provide a GET /devices endpoint
2. THE Control_Plane_API SHALL return a list of all registered devices
3. THE device list SHALL include hardware_id, confirmation_id, friendly_name, firmware_version, first_registered_at, and last_seen_at
4. THE Control_Plane_API SHALL support pagination using cursor parameter (base64-encoded LastEvaluatedKey)
5. THE Control_Plane_API SHALL support limit parameter to control page size
6. THE Control_Plane_API SHALL support filtering by friendly_name (client-side or via GSI)
7. THE Control_Plane_API SHALL return devices sorted by last_seen_at (most recent first) using a GSI
8. THE Control_Plane_API SHALL return a next_cursor field when more results are available

### Requirement 13: Device Detail Retrieval

**User Story:** As a system administrator, I want to retrieve details for a specific device, so that I can view its configuration and status.

#### Acceptance Criteria

1. THE Control_Plane_API SHALL provide a GET /devices/{hardware_id} endpoint
2. THE Control_Plane_API SHALL return the complete device record
3. THE device record SHALL include hardware_id, confirmation_id, friendly_name, firmware_version, capabilities, first_registered_at, last_seen_at, and last_boot_id
4. WHEN the device does not exist, THE Control_Plane_API SHALL return HTTP 404
5. THE Control_Plane_API SHALL return HTTP 200 with the device record on success

### Requirement 14: Device Reading History Query

**User Story:** As a system administrator, I want to query historical sensor readings for a device, so that I can analyze trends and troubleshoot issues.

#### Acceptance Criteria

1. THE Control_Plane_API SHALL provide a GET /devices/{hardware_id}/readings endpoint
2. THE Control_Plane_API SHALL support from parameter (timestamp in milliseconds) for range start
3. THE Control_Plane_API SHALL support to parameter (timestamp in milliseconds) for range end
4. THE Control_Plane_API SHALL support limit parameter to control page size (default 50, max 1000)
5. THE Control_Plane_API SHALL support cursor parameter for pagination
6. THE Control_Plane_API SHALL query DynamoDB using partition key (hardware_id) and sort key range
7. THE Control_Plane_API SHALL return readings sorted by timestamp (newest first by default)
8. THE Control_Plane_API SHALL return a next_cursor field when more results are available
9. THE reading response SHALL include timestamp_ms, batch_id, boot_id, sensors, sensor_status, and firmware_version
10. WHEN the device does not exist, THE Control_Plane_API SHALL return HTTP 404

### Requirement 15: Latest Reading Retrieval

**User Story:** As a system administrator, I want to retrieve the latest reading for a device, so that I can quickly check current status.

#### Acceptance Criteria

1. THE Control_Plane_API SHALL provide a GET /devices/{hardware_id}/latest endpoint
2. THE Control_Plane_API SHALL query DynamoDB with ScanIndexForward=false and Limit=1
3. THE Control_Plane_API SHALL return the most recent reading for the device
4. THE reading response SHALL include timestamp_ms, batch_id, boot_id, sensors, sensor_status, and firmware_version
5. WHEN the device does not exist, THE Control_Plane_API SHALL return HTTP 404 with error_code DEVICE_NOT_FOUND
6. WHEN the device exists but has no readings, THE Control_Plane_API SHALL return HTTP 404 with error_code NO_READINGS
7. THE Control_Plane_API SHALL return HTTP 200 with the reading on success

### Requirement 16: Control Plane Authentication

**User Story:** As a system administrator, I want the control plane API to be secured, so that only authorized administrators can manage the system.

#### Acceptance Criteria

1. THE Control_Plane_API SHALL require an Authorization header in all requests
2. THE Control_Plane_API SHALL support Bearer token authentication
3. WHEN the token is valid, THE Control_Plane_API SHALL process the request
4. WHEN the token is invalid or missing, THE Control_Plane_API SHALL return HTTP 401
5. THE Control_Plane_API SHALL validate tokens against a configured secret or JWT validation

### Requirement 17: Error Handling and Logging

**User Story:** As a developer, I want comprehensive error handling and logging, so that I can diagnose issues quickly.

#### Acceptance Criteria

1. THE Data_Plane_API SHALL log all requests to CloudWatch Logs
2. THE Control_Plane_API SHALL log all requests to CloudWatch Logs
3. WHEN an error occurs, THE APIs SHALL log the error details including request ID
4. THE APIs SHALL return structured JSON error responses
5. THE error response SHALL include an error code and descriptive message
6. THE APIs SHALL use appropriate HTTP status codes for different error types
7. THE APIs SHALL log validation errors with details about which fields failed
8. THE APIs SHALL log authentication failures with the attempted API key or token (masked)

### Requirement 18: DynamoDB Table Design

**User Story:** As a backend developer, I want well-designed DynamoDB tables, so that queries are efficient and costs are optimized.

#### Acceptance Criteria

1. THE system SHALL use a "devices" table with hardware_id as partition key
2. THE "devices" table SHALL have a GSI (GSI1) for listing devices sorted by last_seen_at
3. THE GSI1 SHALL use gsi1pk as partition key and gsi1sk = last_seen_at as sort key
4. THE gsi1pk attribute SHALL be a constant string value "devices" for all device records
5. THE system SHALL use an "api_keys" table with key_id as partition key
6. THE "api_keys" table SHALL have a GSI on api_key_hash for lookup by hashed key value
7. THE system SHALL use a "processed_batches" table with batch_id as partition key
8. THE "processed_batches" table SHALL have TTL enabled on an expiration_time attribute (epoch seconds)
9. THE system SHALL use a "device_readings" table with hardware_id as partition key
10. THE "device_readings" table SHALL use a composite sort key: "{timestamp_ms}#{batch_id}"
11. THE "device_readings" table SHALL support TTL via an expiration_time attribute
12. THE system SHALL use on-demand billing mode for all tables
13. THE system SHALL enable point-in-time recovery for the "devices" and "api_keys" tables

### Requirement 19: Lambda Configuration

**User Story:** As a DevOps engineer, I want Lambda functions properly configured, so that they perform well and handle errors gracefully.

#### Acceptance Criteria

1. THE Data_Plane_API SHALL have a timeout of 30 seconds
2. THE Control_Plane_API SHALL have a timeout of 30 seconds
3. THE Data_Plane_API SHALL have at least 512 MB of memory allocated
4. THE Control_Plane_API SHALL have at least 256 MB of memory allocated
5. THE Lambda functions SHALL use the ARM64 architecture for cost efficiency
6. THE Lambda functions SHALL have appropriate IAM roles with least-privilege permissions
7. THE Lambda functions SHALL have environment variables for DynamoDB table names
8. THE Lambda functions SHALL have environment variables for admin token configuration

### Requirement 20: CORS Support

**User Story:** As a frontend developer, I want the APIs to support CORS, so that I can build web-based management interfaces.

#### Acceptance Criteria

1. THE Control_Plane_API SHALL include CORS headers in all responses
2. THE Control_Plane_API SHALL support preflight OPTIONS requests
3. THE CORS headers SHALL allow specified origins (configurable via environment variable)
4. THE CORS headers SHALL allow GET, POST, PUT, DELETE methods
5. THE CORS headers SHALL allow Authorization and Content-Type headers

### Requirement 21: Health Check Endpoints

**User Story:** As a DevOps engineer, I want health check endpoints, so that I can monitor API availability.

#### Acceptance Criteria

1. THE Data_Plane_API SHALL provide a GET /health endpoint
2. THE Control_Plane_API SHALL provide a GET /health endpoint
3. THE health endpoint SHALL return HTTP 200 with a JSON response
4. THE health response SHALL include a status field with value "healthy"
5. THE health endpoint SHALL not require authentication
6. THE health endpoint SHALL verify DynamoDB connectivity by performing DescribeTable or a single GetItem against a known key
7. WHEN DynamoDB is unreachable, THE health endpoint SHALL return HTTP 503

### Requirement 22: Batch Processing Efficiency

**User Story:** As a backend developer, I want batch sensor data submissions to be processed efficiently, so that devices can sync large amounts of offline data quickly.

#### Acceptance Criteria

1. WHEN a device submits multiple readings, THE Data_Plane_API SHALL process idempotency checks individually
2. THE Data_Plane_API SHALL use conditional PutItem for each batch_id to ensure atomicity
3. THE Data_Plane_API SHALL write readings to device_readings table after idempotency check passes
4. THE Data_Plane_API SHALL limit batch size to 100 readings per request
5. WHEN batch size exceeds 100, THE Data_Plane_API SHALL return HTTP 400 with an error message
6. THE Data_Plane_API SHALL return both acknowledged_batch_ids and duplicate_batch_ids in the response

### Requirement 23: Deployment and Infrastructure as Code

**User Story:** As a DevOps engineer, I want infrastructure defined as code, so that deployments are repeatable and version-controlled.

#### Acceptance Criteria

1. THE system SHALL provide AWS SAM or CDK templates for infrastructure deployment
2. THE templates SHALL define all Lambda functions, DynamoDB tables, and IAM roles
3. THE templates SHALL configure Function URLs for both Lambda functions
4. THE templates SHALL set up CloudWatch log groups with retention policies
5. THE templates SHALL support multiple deployment environments (dev, staging, prod)
6. THE templates SHALL include parameters for configurable values (e.g., admin token, CORS origins)
7. THE templates SHALL define GSI indexes for the devices table
8. THE templates SHALL configure TTL attributes for processed_batches and device_readings tables
