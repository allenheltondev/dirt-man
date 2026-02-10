# Tasks: Serverless Backend API

## API Contract (Frozen)

**Authentication Headers:**
- Data Plane: `X-API-Key: <64-char-hex-string>`
- Control Plane: `Authorization: Bearer <token>`

**Key Field Types:**
- `hardware_id`: String, MAC address format (XX:XX:XX:XX:XX:XX)
- `batch_id`: String (opaque, max 256 chars, safe ASCII) - **unique per reading**
- `timestamp_ms`: i64, epoch milliseconds UTC
- Timestamps for metadata: RFC3339 strings (e.g., "2024-01-15T10:30:00Z")

**Idempotency:**
- Each reading has a unique `batch_id`
- Duplicate `batch_id` submissions are detected and skipped
- Response partitions batch_ids into `acknowledged_batch_ids` (new) and `duplicate_batch_ids` (seen before)
- **Partial failure handling**: If ingestion encounters a non-duplicate storage error (e.g., DynamoDB throttling, internal error), the request returns an error response; some earlier readings in the request may have been committed. Client should retry with all batch_ids; duplicates will be correctly classified on retry.

**Path Normalization:**
- All endpoints treat trailing slashes equivalently (e.g., /register and /register/ are the same)
- Router normalizes paths internally

**Endpoints:**
- Data Plane: POST /register, POST /data
- Control Plane: POST /api-keys, GET /api-keys, DELETE /api-keys/{key_id}, GET /devices, GET /devices/{hardware_id}, GET /devices/{hardware_id}/readings, GET /devices/{hardware_id}/latest

**Request/Response Bodies (Key Shapes):**
- POST /register: `{hardware_id, boot_id, firmware_version, friendly_name?, capabilities: {sensors: [], features: {}}}`
- POST /data: `{readings: [{batch_id, hardware_id, timestamp_ms, boot_id, firmware_version, friendly_name?, sensors: {}, sensor_status: {}}]}`
- GET /devices/{hardware_id}/readings: Query params: `from` (timestamp_ms), `to` (timestamp_ms), `limit`, `cursor`

## 1. Project Setup and Infrastructure

- [x] 1.1 Create project directory structure
  - Create `backend/` directory
  - Create `backend/data/`, `backend/control/`, and `backend/shared/` subdirectories
  - Set up Cargo workspace configuration with three crates

- [x] 1.2 Create shared crate for common code
  - Initialize `backend/shared/` Cargo project
  - Define shared domain types (Device, Reading, ApiKey, Capabilities, SensorValues, SensorStatus)
  - Implement shared validators (MAC address format, UUID v4 format)
  - Implement timestamp validators (RFC3339 string for metadata, i64 epoch millis for readings with sane bounds: non-negative, reasonable range to prevent garbage data)
  - Implement batch_id validator (max length 256, safe ASCII charset only - treat as opaque)
  - Implement cursor encoding/decoding utilities
  - Implement error response payload struct with error (stable machine code), message (human-readable), request_id fields
  - Add serde, uuid, base64, chrono dependencies

- [x] 1.3 Create time and UUID abstraction traits
  - Define Clock trait with `now_rfc3339()` (for metadata timestamps) and `now_epoch_seconds()` (for TTL) methods
  - Define IdGenerator trait with `uuid_v4()` method
  - Implement production implementations using chrono and uuid crates
  - Implement test implementations with fixed/controllable values
  - Add to shared crate
  - Note: Readings use timestamp_ms from device (epoch millis), server timestamps use RFC3339

- [x] 1.4 Initialize data Rust project
  - Initialize data Cargo project in `backend/data/`
  - Add dependency on shared crate
  - Configure Cargo.toml with lambda_runtime, lambda_http, aws-sdk-dynamodb, tokio, sha2, tracing

- [x] 1.5 Initialize control Rust project
  - Initialize control Cargo project in `backend/control/`
  - Add dependency on shared crate
  - Configure Cargo.toml with lambda_runtime, lambda_http, aws-sdk-dynamodb, tokio, sha2, tracing

- [x] 1.6 Decide on integration test infrastructure and mocking strategy
  - Choose DynamoDB Local (recommended) or LocalStack for integration tests
  - Document decision and rationale
  - Create setup scripts for local testing environment
  - Decide on unit test mocking strategy: keep unit tests focused on pure logic (validation, auth parsing, throttle calculation, cursor encoding) and push DynamoDB behaviors to integration tests
  - Optional: Create thin repository trait for easier mocking if needed

- [x] 1.7 Create AWS SAM template
  - Create `backend/template.yaml` with all four DynamoDB table definitions
  - Define Data Plane Lambda function with Function URL (AuthType: NONE)
  - Define Control Plane Lambda function with Function URL (AuthType: NONE)
  - Configure IAM roles with least-privilege policies
  - Set up environment variables (table names, ADMIN_TOKEN, API_KEY_PEPPER, CORS_ALLOWED_ORIGIN)
  - Add parameters for environment, admin token, CORS origin
  - Configure CloudWatch log retention (e.g., 7 days for dev, 30 days for prod)
  - Enable X-Ray tracing for Lambda functions (optional but recommended)

- [x] 1.7.1 Lock DynamoDB table schemas
  - **devices table**: pk=hardware_id (String), attributes: confirmation_id, capabilities (Map), first_registered_at (RFC3339 String), last_seen_at (RFC3339 String), last_boot_id, firmware_version, friendly_name, gsi1pk (constant "devices"), gsi1sk (copy of last_seen_at); GSI1: pk=gsi1pk, sk=gsi1sk for listing sorted by last_seen_at
  - **device_readings table**: pk=hardware_id (String), sk=ts_batch (String, format "{timestamp_ms:013}#{batch_id}" - zero-padded 13 digits for lexicographic sorting), attributes: timestamp_ms (Number), batch_id, boot_id, firmware_version, friendly_name, sensors (Map), sensor_status (Map), expiration_time (Number, epoch seconds for TTL)
  - **processed_batches table**: pk=batch_id (String), attributes: hardware_id, received_at (RFC3339 String), expiration_time (Number, epoch seconds for TTL - 30 days)
  - **api_keys table**: pk=key_id (String), attributes: api_key_hash, created_at (RFC3339 String), last_used_at (RFC3339 String), is_active (Boolean), description, gsi1pk (constant "api_keys"), gsi1sk (copy of created_at); GSI_hash: pk=api_key_hash for lookup; GSI_list: pk=gsi1pk, sk=gsi1sk for listing sorted by created_at
  - Document timestamp convention: RFC3339 strings for metadata timestamps (sortable), i64 epoch millis for reading timestamps
  - Document TTL convention: All expiration_time fields are epoch seconds (not milliseconds)

- [x] 1.8 Create build and deployment scripts
  - Create `backend/build.sh` for building both Lambda functions with cargo-lambda
  - Create `backend/deploy.sh` with SAM deploy commands
  - Document environment-specific configuration
  - Add README with build/deploy instructions

- [ ] 1.9 Deploy and smoke test infrastructure
  - Build Lambda functions
  - Deploy SAM stack to dev environment
  - Verify Function URLs are accessible
  - Hit Function URLs and verify Lambda responds (404 is fine before implementation)
  - Verify DynamoDB tables created
  - Check CloudWatch logs are working
  - **Definition of Done**: Both Function URLs respond, DynamoDB tables exist, logs visible

## 2. Data Plane API - Skeleton and Deploy Smoke Test

- [x] 2.1 Implement error handling
  - Create error types in `error.rs` (ApiError, AuthError, ValidationError, DatabaseError)
  - Implement HTTP status code mapping
  - Add structured JSON error responses using shared error payload struct
  - Ensure error field contains stable machine codes (e.g., INVALID_MAC, UNAUTHORIZED, BATCH_SIZE_EXCEEDED)
  - Ensure message field contains human-readable descriptions
  - Include request_id from Lambda context in all error responses
  - Include request_id in success responses (optional but recommended)

- [x] 2.2 Set up DynamoDB client and configuration
  - Create `config.rs` module
  - Initialize AWS SDK DynamoDB client
  - Load table names from environment variables
  - Configure client with appropriate timeouts
  - Add helper for creating test client

- [x] 2.3 Implement router and Lambda entry point
  - Create `data/src/main.rs` with Lambda handler
  - Implement path-based routing logic in `router.rs`
  - Match on (method, path) tuples
  - Normalize paths (handle trailing slashes consistently)
  - Return 404 for unknown routes
  - Set up request/response handling with proper error conversion

- [ ] 2.4 Deploy and test skeleton
  - Build and deploy data function
  - Hit Function URL and verify Lambda responds (404 for unknown routes is fine)
  - Verify CloudWatch logs working
  - **Definition of Done**: Lambda responds to requests, logs show structured output, errors return proper JSON

## 3. Data Plane API - Authentication

- [x] 3.1 Implement API key hashing utility
  - Create `auth.rs` module
  - Implement `hash_api_key()` function using SHA-256 with pepper
  - Load API_KEY_PEPPER from environment variable
  - Add unit tests for hash consistency

- [x] 3.2 Implement API key repository operations
  - Create `repo/api_keys.rs`
  - Implement `get_api_key_by_hash()` to query GSI_hash (pk=api_key_hash)
  - Implement `update_last_used()` for timestamp updates
  - Handle DynamoDB errors

- [x] 3.3 Implement API key validation logic
  - Implement `validate_api_key()` function in `auth.rs`
  - Hash incoming key and query api_keys table GSI_hash (pk=api_key_hash)
  - Check is_active flag
  - Implement `should_update_last_used()` with 5-minute throttling
  - Call update_last_used if needed
  - Return ApiKey record or AuthError

- [x] 3.4 Write unit tests for authentication
  - Test API key hashing produces consistent output
  - Test valid API key acceptance (mock DynamoDB)
  - Test invalid API key rejection
  - Test revoked API key (is_active=false) rejection
  - Test last_used_at throttling logic (5-minute window)
  - Test missing API key header handling

## 4. Data Plane API - Device Registration

- [x] 4.1 Implement registration request/response types
  - Create `handlers/register.rs` with RegisterRequest and RegisterResponse structs
  - Use shared Device type from shared crate
  - Implement deserialization for request
  - Implement serialization for response

- [x] 4.2 Implement device repository operations
  - Create `repo/devices.rs`
  - Implement `get_device(hardware_id)` using GetItem
  - Implement `create_device(device)` using PutItem with gsi1pk="devices" and gsi1sk=last_seen_at (RFC3339 String)
  - Implement `update_device_timestamps(hardware_id, last_seen_at, last_boot_id)` using UpdateItem, also update gsi1sk
  - Handle DynamoDB errors
  - Note: All timestamp fields use RFC3339 strings for sortability

- [x] 4.3 Implement registration handler logic
  - Create handler function in `handlers/register.rs`
  - Extract and validate API key from X-API-Key header
  - Parse and validate request body using shared validators
  - Check if device exists using get_device()
  - If new: generate confirmation_id using IdGenerator trait, create device record
  - If existing: update timestamps and last_boot_id, return existing confirmation_id
  - Return RegisterResponse with status, confirmation_id, hardware_id, registered_at

- [x] 4.4 Wire up registration handler in router
  - Add POST /register route to router
  - Call registration handler
  - Handle errors and return appropriate HTTP responses

- [x] 4.5 Write unit tests for device registration
  - Test new device creation with mocked DynamoDB
  - Test existing device re-registration returns same confirmation_id
  - Test confirmation_id is valid UUID v4
  - Test last_seen_at and last_boot_id updated on re-registration
  - Test gsi1pk and gsi1sk attributes set correctly
  - Test validation errors (invalid MAC, invalid UUID, missing fields)
  - Test authentication errors (missing/invalid API key)

- [ ] 4.6 Write integration test for registration
  - Set up DynamoDB Local table
  - Create test API key
  - Register new device and verify in DynamoDB
  - Re-register same device and verify confirmation_id unchanged
  - Verify timestamps updated
  - **Definition of Done**: /register returns correct responses for new and existing devices, validation errors return 400, auth errors return 401, integration test passes

## 5. Data Plane API - Sensor Data Ingestion

- [x] 5.1 Implement data ingestion request/response types
  - Create `handlers/data.rs` with DataRequest and DataResponse structs
  - Use shared Reading type from shared crate
  - Support readings array in request
  - Define acknowledged_batch_ids and duplicate_batch_ids in response

- [x] 5.2 Implement ingestion repository with transactional writes
  - Create `repo/ingestion.rs`
  - Implement `transact_write_reading_if_new_batch()` function
  - Use TransactWriteItems to atomically write to both processed_batches and device_readings tables
  - First TransactItem: PutItem to processed_batches with condition attribute_not_exists(batch_id), set received_at (RFC3339 String), expiration_time (epoch seconds = now + 30 days in seconds)
  - Second TransactItem: PutItem to device_readings with composite sort key ts_batch="{timestamp_ms:013}#{batch_id}" (zero-padded 13 digits), timestamp_ms as Number, expiration_time (epoch seconds = timestamp_ms/1000 + retention_seconds if TTL desired)
  - Return Ok(true) if transaction succeeds, Ok(false) if ConditionalCheckFailedException (duplicate), Err for other errors
  - Note: batch_id is unique per reading, not per request

- [x] 5.3 Implement data ingestion handler logic
  - Create handler function in `handlers/data.rs`
  - Extract and validate API key from X-API-Key header
  - Parse request body and validate readings array
  - Enforce batch size limit (100 readings max) after authentication
  - Validate each reading using shared validators (hardware_id, timestamp_ms with sane bounds)
  - Validate batch_id using permissive validator (max length 256, safe ASCII charset - treat as opaque)
  - Loop through readings and call transact_write_reading_if_new_batch() for each
  - Partition batch_ids into acknowledged (new) and duplicates lists
  - Use fail-fast error handling: if a non-duplicate DynamoDB error occurs, return error response immediately
  - Return DataResponse with both lists
  - Note: Per-reading transactions are expensive but correct; optimization is optional (see section 14.5)

- [x] 5.4 Wire up data ingestion handler in router
  - Add POST /data route to router
  - Call data ingestion handler
  - Handle errors and return appropriate HTTP responses

- [x] 5.5 Write unit tests for sensor data ingestion
  - Test single reading processing with mocked DynamoDB
  - Test batch reading processing (multiple readings)
  - Test idempotency: submit same batch_id twice, verify duplicate detected
  - Test response structure with acknowled       ged and duplicate lists
  - Test batch size limit enforcement (101 readings returns 400)
  - Test validation errors (invalid hardware_id, invalid timestamp, missing fields)
  - Test authentication errors

- [ ] 5.6 Write integration test for data ingestion
  - Set up DynamoDB Local tables
  - Create test API key
  - Submit new reading and verify in device_readings table
  - Submit duplicate reading and verify not duplicated
  - Verify batch_id in processed_batches table
  - Submit batch of readings and verify all processed correctly
  - **Definition of Done**: /data returns correct acknowledged/duplicate lists, idempotency works, batch limit enforced, integration test passes

## 6. Control Plane API - Skeleton and Deploy Smoke Test

- [x] 6.1 Implement error handling
  - Create error types in `error.rs` (ApiError, AuthError, ValidationError, NotFoundError, DatabaseError)
  - Implement HTTP status code mapping
  - Add structured JSON error responses using shared error payload struct
  - Ensure error field contains stable machine codes (e.g., DEVICE_NOT_FOUND, INVALID_TOKEN)
  - Ensure message field contains human-readable descriptions
  - Include request_id from Lambda context in all error responses
  - Include request_id in success responses (optional but recommended)

- [x] 6.2 Set up DynamoDB client and configuration
  - Create `config.rs` module
  - Initialize AWS SDK DynamoDB client
  - Load table names from environment variables
  - Configure client with appropriate timeouts

- [x] 6.3 Implement CORS support
  - Create `cors.rs` module
  - Implement `add_cors_headers()` function to add Access-Control-* headers
  - Implement `preflight_response()` for OPTIONS requests
  - Read CORS_ALLOWED_ORIGIN from environment variable

- [x] 6.4 Implement router and Lambda entry point
  - Create `control/src/main.rs` with Lambda handler
  - Implement path-based routing with method matching
  - Handle CORS preflight OPTIONS requests first
  - Match on (method, path) tuples
  - Normalize paths (handle trailing slashes consistently)
  - Apply CORS headers to all responses
  - Return 404 for unknown routes

- [x] 6.5 Write unit tests for CORS
  - Test CORS headers added to responses
  - Test OPTIONS preflight response

- [ ] 6.6 Deploy and test skeleton
  - Build and deploy control function
  - Hit Function URL and verify Lambda responds (404 for unknown routes is fine)
  - Verify CORS headers present in responses
  - Verify CloudWatch logs working
  - **Definition of Done**: Lambda responds to requests, CORS headers work, logs show structured output

## 7. Control Plane API - Authentication

- [x] 7.1 Implement Bearer token validation
  - Create `auth.rs` module
  - Implement `validate_bearer_token()` function
  - Extract Authorization header and parse "Bearer <token>"
  - Compare token against ADMIN_TOKEN environment variable using constant-time comparison
  - Return error if ADMIN_TOKEN not set in environment
  - Return Ok(()) or AuthError
  - Note: For production, consider token rotation mechanism and secure storage

- [x] 7.2 Write unit tests for authentication
  - Test valid Bearer token acceptance
  - Test invalid token rejection
  - Test missing Authorization header
  - Test malformed Authorization header (not "Bearer ")
  - Test ADMIN_TOKEN not set in environment

## 8. Control Plane API - API Key Management

- [x] 8.1 Implement API key generation and hashing utilities
  - Create `crypto.rs` module in control
  - Implement `generate_api_key()` using cryptographically secure random (32 bytes, hex-encoded)
  - Implement `hash_api_key()` using SHA-256 with pepper from environment
  - Load API_KEY_PEPPER from environment variable
  - Add unit tests for generation and hashing consistency

- [x] 8.2 Implement API key repository operations
  - Create `repo/api_keys.rs`
  - Implement `create_api_key(key_id, api_key_hash, created_at, description)` using PutItem with gsi1pk="api_keys" and gsi1sk=created_at
  - Implement `list_api_keys(limit, cursor)` using Query on GSI_list (pk=gsi1pk="api_keys"), sorted by gsi1sk (created_at) descending
  - Implement `revoke_api_key(key_id)` using UpdateItem to set is_active=false
  - Handle pagination with LastEvaluatedKey
  - Handle DynamoDB errors

- [x] 8.3 Implement API key creation handler
  - Create `handlers/api_keys.rs` with CreateApiKeyRequest and CreateApiKeyResponse
  - Validate Bearer token
  - Generate new API key using generate_api_key() from crypto module
  - Hash the key using hash_api_key() from crypto module
  - Generate UUID v4 for key_id using IdGenerator trait
  - Store in DynamoDB with created_at timestamp
  - Return response with key_id, raw api_key (only time shown), created_at, and warning message

- [x] 8.4 Implement API key listing handler
  - Validate Bearer token
  - Parse limit and cursor query parameters
  - Query GSI_list on api_keys table (pk=gsi1pk="api_keys") sorted by gsi1sk (created_at) descending
  - Encode next_cursor if more results available using shared cursor utilities
  - Return array with key_id, created_at, last_used_at, is_active, description
  - Exclude api_key_hash from response

- [x] 8.5 Implement API key revocation handler
  - Validate Bearer token
  - Extract key_id from path (/api-keys/{key_id})
  - Call revoke_api_key() to set is_active=false
  - Return success response

- [x] 8.6 Wire up API key handlers in router
  - Add POST /api-keys route
  - Add GET /api-keys route
  - Add DELETE /api-keys/{key_id} route with path parsing
  - Handle errors and return appropriate HTTP responses

- [x] 8.7 Write unit tests for API key management
  - Test API key generation produces 64-character hex string
  - Test API key hashing produces consistent SHA-256 hash
  - Test key creation with mocked DynamoDB (verify gsi1pk and gsi1sk set)
  - Test key listing with pagination
  - Test key listing returns correct fields
  - Test key revocation sets is_active=false
  - Test authentication required for all endpoints

- [ ] 8.8 Write integration test for API key management
  - Create API key via POST /api-keys
  - Verify gsi1pk="api_keys" and gsi1sk=created_at in DynamoDB
  - List API keys via GET /api-keys and verify new key present
  - Test pagination with limit parameter
  - Revoke API key via DELETE
  - Verify key marked inactive
  - **Definition of Done**: API key CRUD works, raw key only shown once, hashing works, GSI listing works with pagination, integration test passes

## 9. Control Plane API - Device Management

- [x] 9.1 Implement device repository operations
  - Create `repo/devices.rs`
  - Implement `list_devices(limit, cursor)` with GSI query on gsi1pk="devices", sorted by gsi1sk (RFC3339 String) descending
  - Verify gsi1sk format preserves sortability (RFC3339 strings sort correctly lexicographically)
  - Implement `get_device(hardware_id)` using GetItem
  - Handle pagination with LastEvaluatedKey
  - Handle DynamoDB errors

- [x] 9.2 Implement device listing handler
  - Create `handlers/devices.rs`
  - Validate Bearer token
  - Parse limit and cursor query parameters
  - Query devices GSI sorted by last_seen_at descending
  - Encode next_cursor if more results available using shared cursor utilities
  - Return device list with hardware_id, confirmation_id, friendly_name, firmware_version, first_registered_at, last_seen_at

- [x] 9.3 Implement device detail handler
  - Validate Bearer token
  - Extract hardware_id from path (/devices/{hardware_id})
  - Query device by partition key
  - Return 404 with DEVICE_NOT_FOUND if not found
  - Return complete device record including capabilities

- [x] 9.4 Wire up device handlers in router
  - Add GET /devices route
  - Add GET /devices/{hardware_id} route with path parsing
  - Handle errors and return appropriate HTTP responses

- [x] 9.5 Write unit tests for device management
  - Test device listing with mocked DynamoDB
  - Test pagination with cursor encoding/decoding
  - Test device detail retrieval
  - Test device not found returns 404
  - Test authentication required

- [ ] 9.6 Write integration test for device management
  - Create multiple devices via data-plane
  - List devices and verify sorted by last_seen_at
  - Test pagination with limit parameter
  - Get device detail and verify complete record
  - **Definition of Done**: Device listing works with pagination, device detail returns complete record, integration test passes

## 10. Control Plane API - Reading Queries

- [x] 10.1 Implement readings repository operations
  - Create `repo/readings.rs`
  - Implement `query_readings(hardware_id, from_ms, to_ms, limit, cursor)` with sort key range
  - Validate from_ms <= to_ms and both are within sane bounds (non-negative, reasonable range)
  - Use KeyConditionExpression with ts_batch BETWEEN "{from_ms:013}#" AND "{to_ms:013}#\uffff" (zero-padded 13 digits)
  - Implement `get_latest_reading(hardware_id)` with ScanIndexForward=false and Limit=1
  - Handle pagination with LastEvaluatedKey
  - Handle DynamoDB errors

- [x] 10.2 Implement readings query handler
  - Create `handlers/readings.rs`
  - Validate Bearer token
  - Extract hardware_id from path
  - Parse from, to, limit, cursor query parameters
  - Query device_readings with sort key range between "{from_ms:013}#" and "{to_ms:013}#\uffff" (zero-padded 13 digits)
  - Return readings sorted by timestamp descending
  - Encode next_cursor if more results available
  - Return 404 with DEVICE_NOT_FOUND if device doesn't exist

- [x] 10.3 Implement latest reading handler
  - Validate Bearer token
  - Extract hardware_id from path (/devices/{hardware_id}/latest)
  - Query with ScanIndexForward=false and Limit=1
  - Return 404 with DEVICE_NOT_FOUND if device doesn't exist
  - Return 404 with NO_READINGS if device has no readings
  - Return most recent reading with timestamp_ms, batch_id, boot_id, sensors, sensor_status, firmware_version

- [x] 10.4 Wire up reading handlers in router
  - Add GET /devices/{hardware_id}/readings route
  - Add GET /devices/{hardware_id}/latest route
  - Handle path parsing for hardware_id
  - Handle errors and return appropriate HTTP responses

- [x] 10.5 Write unit tests for reading queries
  - Test time range queries with mocked DynamoDB
  - Test pagination
  - Test latest reading retrieval
  - Test device not found handling
  - Test no readings handling
  - Test authentication required

- [ ] 10.6 Write integration test for reading queries
  - Create device and submit multiple readings via data-plane
  - Query readings with time range and verify correct subset returned
  - Test pagination with limit parameter
  - Get latest reading and verify most recent returned
  - **Definition of Done**: Reading queries work with time ranges, pagination works, latest reading returns correct record, integration test passes

## 11. Property-Based Testing

- [x] 11.1 Set up property testing framework
  - Add proptest dependency to both Lambda projects
  - Configure test iterations (minimum 100)
  - Create test utilities module in shared crate

- [x] 11.2 Implement property test generators
  - MAC address generator (valid format XX:XX:XX:XX:XX:XX)
  - UUID v4 generator (correct version and variant bits)
  - Timestamp generator (valid i64 epoch milliseconds)
  - Batch ID generator (opaque safe ASCII strings up to 256 chars - no structured format)
  - Reading generator (complete Reading struct with valid fields)
  - API key generator (64-character hex strings)

- [ ] 11.3 Write property test: API Key Validation (Property 1)
  - Generate API keys that exist in test table vs do not exist
  - Verify keys in table with is_active=true are accepted
  - Verify keys not in table return 401
  - Verify keys with is_active=false return 401

- [ ] 11.4 Write property test: New Device Creation (Property 2)
  - Generate random valid hardware_ids
  - Register devices and verify each gets unique UUID v4 confirmation_id
  - Verify confirmation_id format is valid UUID v4

- [ ] 11.5 Write property test: Registration Idempotency (Property 3)
  - Generate random devices
  - Register same device twice
  - Verify confirmation_id unchanged between registrations

- [x] 11.6 Write property test: MAC Address Format Validation (Property 4)
  - Generate valid MAC addresses (XX:XX:XX:XX:XX:XX format)
  - Generate invalid MAC addresses (wrong format, wrong length, invalid chars)
  - Verify valid MACs accepted, invalid MACs return 400

- [x] 11.7 Write property test: UUID v4 Format Validation (Property 5)
  - Generate valid UUID v4 strings
  - Generate invalid UUID strings (wrong version, wrong format)
  - Verify valid UUIDs accepted, invalid UUIDs return 400

- [ ] 11.8 Write property test: Re-registration Updates Timestamps (Property 6)
  - Generate devices and register
  - Wait or advance clock
  - Re-register with new boot_id
  - Verify last_seen_at updated and last_boot_id changed

- [ ] 11.9 Write property test: Batch ID Idempotency (Property 7)
  - Generate readings with same batch_id
  - Submit first reading, verify acknowledged
  - Submit duplicate reading, verify marked as duplicate
  - Verify only one reading in device_readings table

- [x] 11.10 Write property test: Timestamp Type Validation (Property 8)
  - Generate valid i64 timestamps
  - Generate invalid timestamps (strings, floats, out of range)
  - Verify valid timestamps accepted, invalid return 400

- [ ] 11.11 Write property test: Response Batch ID Partitioning (Property 9)
  - Generate mix of new and duplicate batch_ids
  - Submit batch
  - Verify acknowledged_batch_ids and duplicate_batch_ids have no overlap
  - Verify union of both lists equals submitted batch_ids

- [ ] 11.12 Write property test: API Key Usage Tracking (Property 10)
  - Generate API keys and use them
  - Verify last_used_at updated periodically (respecting 5-minute throttle)
  - Verify not updated on every request

- [x] 11.13 Write property test: API Key Hashing (Property 11)
  - Generate random API keys
  - Hash keys and verify SHA-256 format
  - Verify same key produces same hash
  - Verify different keys produce different hashes
  - Verify raw key not stored in database after creation

- [ ] 11.14 Write property test: Device List Sort Order (Property 12)
  - Create devices with various last_seen_at timestamps
  - List devices
  - Verify returned in descending order by last_seen_at

- [ ] 11.15 Write property test: Reading List Sort Order (Property 13)
  - Create readings with various timestamps
  - Query readings
  - Verify returned in descending order by timestamp_ms

- [ ] 11.16 Write property test: GSI Partition Key Consistency (Property 14)
  - Create and update devices
  - Verify gsi1pk always set to constant "devices"
  - Verify gsi1sk always equals last_seen_at

- [ ] 11.17 Write property test: Batch Size Limit Enforcement (Property 15)
  - Generate batches of various sizes (1-150 readings)
  - Submit batches
  - Verify batches ≤100 accepted
  - Verify batches >100 rejected with 400 after authentication
  - Verify error message indicates batch size limit

- [x] 11.18 Run all property tests and verify passing
  - Execute full property test suite
  - Verify all 15 properties pass with 100+ iterations each
  - Document any failures and fix
  - **Definition of Done**: All 15 property tests pass consistently

## 12. End-to-End Integration Testing

- [ ] 12.1 Set up integration test environment
  - Configure DynamoDB Local with all four tables
  - Create test fixtures and seed data utilities
  - Set up test API keys and admin tokens
  - Create helper functions for common operations

- [ ] 12.2 Write device registration integration test
  - Start with clean DynamoDB Local
  - Create test API key
  - Register new device via POST /register
  - Verify device in devices table with correct attributes
  - Re-register same device
  - Verify confirmation_id unchanged
  - Verify timestamps updated
  - Test invalid API key rejection

- [ ] 12.3 Write sensor data ingestion integration test
  - Create device and API key
  - Submit single reading via POST /data
  - Verify reading in device_readings table
  - Verify batch_id in processed_batches table
  - Submit duplicate reading
  - Verify not duplicated in device_readings
  - Verify duplicate_batch_ids in response
  - Submit batch of 50 readings
  - Verify all processed correctly

- [ ] 12.4 Write control plane integration test
  - Create admin token
  - Create API key via POST /api-keys
  - Verify raw key returned
  - List API keys via GET /api-keys
  - Verify new key in list
  - Use API key in data plane
  - Verify last_used_at updated
  - Revoke API key via DELETE /api-keys/{key_id}
  - Verify key marked inactive
  - Verify revoked key rejected by data plane

- [ ] 12.5 Write device management integration test
  - Create multiple devices via data plane with different last_seen_at
  - List devices via GET /devices
  - Verify sorted by last_seen_at descending
  - Test pagination with limit=2
  - Get device detail via GET /devices/{hardware_id}
  - Verify complete record returned

- [ ] 12.6 Write reading queries integration test
  - Create device and submit readings at different timestamps via data plane
  - Query readings with time range via GET /devices/{hardware_id}/readings
  - Verify correct subset returned
  - Test pagination
  - Get latest reading via GET /devices/{hardware_id}/latest
  - Verify most recent reading returned
  - Test device not found scenarios

- [ ] 12.7 Run full integration test suite
  - Execute all integration tests
  - Verify all pass
  - Document any failures and fix
  - **Definition of Done**: All integration tests pass, end-to-end flows work correctly

## 13. Deployment and Documentation

- [ ] 13.1 Create deployment documentation
  - Document build process with cargo-lambda
  - Document SAM deployment commands for dev/staging/prod
  - Document environment variables and their purposes
  - Document parameter configuration (admin token, CORS, pepper)
  - Create troubleshooting guide

- [x] 13.2 Create API documentation
  - Document all Data Plane endpoints with request/response examples
  - Document all Control Plane endpoints with request/response examples
  - Document authentication requirements (X-API-Key vs Bearer token)
  - Document error codes and meanings
  - Document rate limits and constraints (batch size, pagination)
  - Create Postman collection or OpenAPI spec

- [ ] 13.3 Deploy to development environment
  - Build Lambda functions with dev configuration
  - Deploy SAM stack to dev
  - Verify Function URLs accessible
  - Test all endpoints manually
  - Verify CloudWatch logs working
  - Verify DynamoDB tables created correctly

- [ ] 13.4 Set up monitoring and alarms
  - Create CloudWatch alarm for Data Plane errors (threshold: 10 in 5 minutes)
  - Create CloudWatch alarm for Control Plane errors (threshold: 5 in 5 minutes)
  - Create CloudWatch alarm for DynamoDB throttling (threshold: 5 in 5 minutes)
  - Create CloudWatch alarm for Lambda duration (threshold: 25 seconds)
  - Set up SNS topic for alarm notifications
  - Create operational dashboard with key metrics

- [ ] 13.5 Create runbook for operations
  - Document how to create first API key
  - Document how to rotate admin token
  - Document how to investigate errors
  - Document how to scale DynamoDB if needed
  - Document backup and recovery procedures
  - Document how to update Lambda functions

- [ ] 13.6* Deploy to production environment
  - Build Lambda functions with production configuration
  - Deploy SAM stack to production
  - Configure production admin token (secure)
  - Configure CORS for production domain
  - Verify all endpoints functional
  - Run smoke tests
  - Monitor for 24 hours
  - **Definition of Done**: Production deployment successful, all endpoints working, monitoring active

## 14. Optional Enhancements

- [x] 14.1* Implement friendly_name update endpoint
  - Add PUT /devices/{hardware_id} endpoint to Control Plane
  - Allow updating friendly_name field
  - Validate new friendly_name
  - Update device record in DynamoDB
  - Document that historical readings preserve old friendly_name snapshots

- [ ] 14.2* Add CloudWatch custom metrics
  - Publish metric for device registrations (count per minute)
  - Publish metric for data ingestion (readings per minute)
  - Publish metric for API key usage (requests per key)
  - Publish metric for duplicate batch_ids (rate)
  - Create dashboard with custom metrics

- [ ] 14.3* Implement data retention policies
  - Configure TTL for device_readings based on age (e.g., 90 days)
  - Add expiration_time attribute to readings on ingestion
  - Document data retention strategy
  - Add monitoring for TTL deletions

- [ ] 14.4* Add request rate limiting
  - Implement per-device rate limiting (e.g., 10 requests per minute)
  - Store rate limit state in DynamoDB or ElastiCache
  - Add X-RateLimit-* headers to responses
  - Return 429 when limit exceeded
  - Document rate limits in API documentation

- [ ] 14.5* Optimize batch write performance
  - Benchmark current per-reading transaction approach (100 readings = 100 transactions)
  - Evaluate alternative: single transaction per request with batch marker + BatchWriteItem for readings
  - Note: This changes idempotency from per-reading to per-batch
  - Document trade-offs: cost/speed vs granular idempotency
  - Consider accepting small orphan risk with repair logic (if willing to add recovery)
  - Implement optimization only if trade-offs acceptable
