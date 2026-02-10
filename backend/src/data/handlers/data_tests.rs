#[cfg(test)]
mod tests {
    //! Unit tests for sensor data ingestion handler
    //!
    //! These tests verify:
    //! - Request parsing and deserialization
    //! - Response structure and serialization
    //! - Authentication header extraction
    //! - Batch size limit enforcement (after auth)
    //! - Data structure validation
    //!
    //! Note: Tests that require DynamoDB interaction (API key validation,
    //! idempotency checks, actual data storage) are covered in integration tests.
    //! These unit tests focus on the handler logic that can be tested without
    //! external dependencies.

    use super::super::data::{handle_data, DataRequest, DataResponse};
    use crate::config::Config;
    use crate::error::{ApiError, AuthError, ValidationError};
    use esp32_backend::domain::{Reading, SensorStatus, SensorValues};
    use esp32_backend::FixedClock;
    use lambda_http::{Body, Request};

    // ============================================================================
    // Test Helpers
    // ============================================================================

    /// Create a test config with mock DynamoDB client
    /// Note: For unit tests, we'll test the handler logic without actual DynamoDB calls
    /// Integration tests will use DynamoDB Local
    async fn create_test_config() -> Config {
        Config::for_test(
            "http://localhost:8000",
            "test-devices".to_string(),
            "test-api-keys".to_string(),
            "test-processed-batches".to_string(),
            "test-device-readings".to_string(),
        )
        .await
    }

    /// Create a valid test reading
    fn create_test_reading(batch_id: &str, timestamp_ms: i64) -> Reading {
        Reading {
            batch_id: batch_id.to_string(),
            hardware_id: "AA:BB:CC:DD:EE:FF".to_string(),
            timestamp_ms,
            boot_id: "550e8400-e29b-41d4-a716-446655440000".to_string(),
            firmware_version: "1.0.16".to_string(),
            friendly_name: Some("test-sensor".to_string()),
            sensors: SensorValues {
                bme280_temp_c: Some(22.5),
                ds18b20_temp_c: Some(21.8),
                humidity_pct: Some(45.2),
                pressure_hpa: Some(1013.25),
                soil_moisture_pct: Some(62.3),
            },
            sensor_status: SensorStatus {
                bme280: "ok".to_string(),
                ds18b20: "ok".to_string(),
                soil_moisture: "ok".to_string(),
            },
        }
    }

    /// Create a test request with the given readings
    fn create_test_request(readings: Vec<Reading>, api_key: Option<&str>) -> Request {
        let data_request = DataRequest { readings };
        let body_json = serde_json::to_string(&data_request).unwrap();

        let mut req = lambda_http::http::Request::builder()
            .method("POST")
            .uri("/data");

        // Add API key header if provided
        if let Some(key) = api_key {
            req = req.header("x-api-key", key);
        }

        req.body(Body::from(body_json)).unwrap().into()
    }

    // ============================================================================
    // Authentication Tests
    // ============================================================================

    #[tokio::test]
    async fn test_data_missing_api_key_header() {
        let config = create_test_config().await;
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        let reading = create_test_reading("batch1", 1704067800000);
        let request = create_test_request(vec![reading], None); // No API key

        let result = handle_data(request, "test-request-id", &config, &clock).await;

        // Should return authentication error
        assert!(result.is_err());
        match result {
            Err(ApiError::Auth(AuthError::MissingKey)) => {
                // Expected error
            }
            _ => panic!("Expected AuthError::MissingKey"),
        }
    }

    // Note: Testing invalid API key requires mocking DynamoDB or integration tests
    // Unit tests focus on request parsing, validation, and response structure

    // ============================================================================
    // Request Parsing Tests
    // ============================================================================

    #[tokio::test]
    async fn test_data_empty_body() {
        let config = create_test_config().await;
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        let request = lambda_http::http::Request::builder()
            .method("POST")
            .uri("/data")
            .header("x-api-key", "test-key-123")
            .body(Body::Empty)
            .unwrap()
            .into();

        let result = handle_data(request, "test-request-id", &config, &clock).await;

        // Should return validation error for empty body
        assert!(result.is_err());
        match result {
            Err(ApiError::Validation(ValidationError::InvalidBody(_))) => {
                // Expected error
            }
            _ => panic!("Expected ValidationError::InvalidBody"),
        }
    }

    #[tokio::test]
    async fn test_data_malformed_json() {
        let config = create_test_config().await;
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        let request = lambda_http::http::Request::builder()
            .method("POST")
            .uri("/data")
            .header("x-api-key", "test-key-123")
            .body(Body::from("{invalid json"))
            .unwrap()
            .into();

        let result = handle_data(request, "test-request-id", &config, &clock).await;

        // Should return validation error for malformed JSON
        assert!(result.is_err());
        match result {
            Err(ApiError::Validation(ValidationError::InvalidBody(_))) => {
                // Expected error
            }
            _ => panic!("Expected ValidationError::InvalidBody"),
        }
    }

    // ============================================================================
    // Batch Size Limit Tests
    // ============================================================================

    #[tokio::test]
    async fn test_data_batch_size_limit_exceeded() {
        let config = create_test_config().await;
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        // Create 101 readings (exceeds limit of 100)
        let mut readings = Vec::new();
        for i in 0..101 {
            let reading = create_test_reading(&format!("batch{}", i), 1704067800000 + i);
            readings.push(reading);
        }

        let request = create_test_request(readings, Some("test-key-123"));

        let result = handle_data(request, "test-request-id", &config, &clock).await;

        // Should return validation error for batch size exceeded
        assert!(result.is_err());
        match result {
            Err(ApiError::Validation(ValidationError::BatchSizeExceeded)) => {
                // Expected error
            }
            _ => panic!("Expected ValidationError::BatchSizeExceeded"),
        }
    }

    #[tokio::test]
    async fn test_data_batch_size_limit_exactly_100() {
        let config = create_test_config().await;
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        // Create exactly 100 readings (at the limit)
        let mut readings = Vec::new();
        for i in 0..100 {
            let reading = create_test_reading(&format!("batch{}", i), 1704067800000 + i);
            readings.push(reading);
        }

        let request = create_test_request(readings, Some("test-key-123"));

        // This will fail at API key validation since we don't have a real DynamoDB
        // But it should NOT fail at batch size validation
        let result = handle_data(request, "test-request-id", &config, &clock).await;

        // Should fail at auth, not batch size
        assert!(result.is_err());
        match result {
            Err(ApiError::Validation(ValidationError::BatchSizeExceeded)) => {
                panic!("Should not fail batch size validation for exactly 100 readings");
            }
            Err(ApiError::Auth(_)) => {
                // Expected - fails at auth step, which means batch size validation passed
            }
            _ => {
                // Other errors are acceptable (e.g., database errors)
            }
        }
    }

    // ============================================================================
    // Validation Tests
    // ============================================================================

    #[tokio::test]
    async fn test_data_invalid_hardware_id() {
        let config = create_test_config().await;
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        let mut reading = create_test_reading("batch1", 1704067800000);
        reading.hardware_id = "invalid-mac".to_string(); // Invalid MAC format

        let request = create_test_request(vec![reading], Some("test-key-123"));

        let result = handle_data(request, "test-request-id", &config, &clock).await;

        // Should return validation error for invalid hardware_id
        assert!(result.is_err());
        match result {
            Err(ApiError::Validation(ValidationError::InvalidFormat(msg))) => {
                assert!(msg.contains("hardware_id"));
            }
            _ => panic!("Expected ValidationError::InvalidFormat for hardware_id"),
        }
    }

    #[tokio::test]
    async fn test_data_invalid_hardware_id_lowercase() {
        let config = create_test_config().await;
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        let mut reading = create_test_reading("batch1", 1704067800000);
        reading.hardware_id = "aa:bb:cc:dd:ee:ff".to_string(); // Lowercase (invalid)

        let request = create_test_request(vec![reading], Some("test-key-123"));

        let result = handle_data(request, "test-request-id", &config, &clock).await;

        // Should return validation error
        assert!(result.is_err());
        match result {
            Err(ApiError::Validation(ValidationError::InvalidFormat(msg))) => {
                assert!(msg.contains("hardware_id"));
            }
            _ => panic!("Expected ValidationError::InvalidFormat for hardware_id"),
        }
    }

    #[tokio::test]
    async fn test_data_invalid_timestamp_negative() {
        let config = create_test_config().await;
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        let reading = create_test_reading("batch1", -1); // Negative timestamp

        let request = create_test_request(vec![reading], Some("test-key-123"));

        let result = handle_data(request, "test-request-id", &config, &clock).await;

        // Should return validation error for invalid timestamp
        assert!(result.is_err());
        match result {
            Err(ApiError::Validation(ValidationError::InvalidFormat(msg))) => {
                assert!(msg.contains("timestamp_ms"));
            }
            _ => panic!("Expected ValidationError::InvalidFormat for timestamp_ms"),
        }
    }

    #[tokio::test]
    async fn test_data_invalid_timestamp_too_old() {
        let config = create_test_config().await;
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        // Timestamp before year 2000
        let reading = create_test_reading("batch1", 946684799999);

        let request = create_test_request(vec![reading], Some("test-key-123"));

        let result = handle_data(request, "test-request-id", &config, &clock).await;

        // Should return validation error
        assert!(result.is_err());
        match result {
            Err(ApiError::Validation(ValidationError::InvalidFormat(msg))) => {
                assert!(msg.contains("timestamp_ms"));
            }
            _ => panic!("Expected ValidationError::InvalidFormat for timestamp_ms"),
        }
    }

    #[tokio::test]
    async fn test_data_invalid_timestamp_too_new() {
        let config = create_test_config().await;
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        // Timestamp after year 2100
        let reading = create_test_reading("batch1", 4102444800001);

        let request = create_test_request(vec![reading], Some("test-key-123"));

        let result = handle_data(request, "test-request-id", &config, &clock).await;

        // Should return validation error
        assert!(result.is_err());
        match result {
            Err(ApiError::Validation(ValidationError::InvalidFormat(msg))) => {
                assert!(msg.contains("timestamp_ms"));
            }
            _ => panic!("Expected ValidationError::InvalidFormat for timestamp_ms"),
        }
    }

    #[tokio::test]
    async fn test_data_invalid_batch_id_empty() {
        let config = create_test_config().await;
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        let reading = create_test_reading("", 1704067800000); // Empty batch_id

        let request = create_test_request(vec![reading], Some("test-key-123"));

        let result = handle_data(request, "test-request-id", &config, &clock).await;

        // Should return validation error
        assert!(result.is_err());
        match result {
            Err(ApiError::Validation(ValidationError::InvalidFormat(msg))) => {
                assert!(msg.contains("batch_id"));
            }
            _ => panic!("Expected ValidationError::InvalidFormat for batch_id"),
        }
    }

    #[tokio::test]
    async fn test_data_invalid_batch_id_too_long() {
        let config = create_test_config().await;
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        // Batch ID longer than 256 characters
        let long_batch_id = "a".repeat(257);
        let reading = create_test_reading(&long_batch_id, 1704067800000);

        let request = create_test_request(vec![reading], Some("test-key-123"));

        let result = handle_data(request, "test-request-id", &config, &clock).await;

        // Should return validation error
        assert!(result.is_err());
        match result {
            Err(ApiError::Validation(ValidationError::InvalidFormat(msg))) => {
                assert!(msg.contains("batch_id"));
            }
            _ => panic!("Expected ValidationError::InvalidFormat for batch_id"),
        }
    }

    #[tokio::test]
    async fn test_data_invalid_batch_id_control_characters() {
        let config = create_test_config().await;
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        // Batch ID with control characters
        let reading = create_test_reading("batch\nid", 1704067800000);

        let request = create_test_request(vec![reading], Some("test-key-123"));

        let result = handle_data(request, "test-request-id", &config, &clock).await;

        // Should return validation error
        assert!(result.is_err());
        match result {
            Err(ApiError::Validation(ValidationError::InvalidFormat(msg))) => {
                assert!(msg.contains("batch_id"));
            }
            _ => panic!("Expected ValidationError::InvalidFormat for batch_id"),
        }
    }

    // ============================================================================
    // Response Structure Tests
    // ============================================================================

    #[test]
    fn test_data_response_structure() {
        let response = DataResponse {
            acknowledged_batch_ids: vec!["batch1".to_string(), "batch2".to_string()],
            duplicate_batch_ids: vec!["batch3".to_string()],
        };

        // Verify structure
        assert_eq!(response.acknowledged_batch_ids.len(), 2);
        assert_eq!(response.duplicate_batch_ids.len(), 1);
        assert_eq!(response.acknowledged_batch_ids[0], "batch1");
        assert_eq!(response.acknowledged_batch_ids[1], "batch2");
        assert_eq!(response.duplicate_batch_ids[0], "batch3");
    }

    #[test]
    fn test_data_response_serialization() {
        let response = DataResponse {
            acknowledged_batch_ids: vec!["batch1".to_string()],
            duplicate_batch_ids: vec!["batch2".to_string()],
        };

        let json = serde_json::to_string(&response).unwrap();
        assert!(json.contains("acknowledged_batch_ids"));
        assert!(json.contains("duplicate_batch_ids"));
        assert!(json.contains("batch1"));
        assert!(json.contains("batch2"));

        // Verify deserialization
        let parsed: DataResponse = serde_json::from_str(&json).unwrap();
        assert_eq!(
            parsed.acknowledged_batch_ids,
            response.acknowledged_batch_ids
        );
        assert_eq!(parsed.duplicate_batch_ids, response.duplicate_batch_ids);
    }

    #[test]
    fn test_data_response_empty_lists() {
        let response = DataResponse {
            acknowledged_batch_ids: vec![],
            duplicate_batch_ids: vec![],
        };

        let json = serde_json::to_string(&response).unwrap();
        let parsed: DataResponse = serde_json::from_str(&json).unwrap();

        assert_eq!(parsed.acknowledged_batch_ids.len(), 0);
        assert_eq!(parsed.duplicate_batch_ids.len(), 0);
    }

    #[test]
    fn test_data_response_no_overlap() {
        // Verify that acknowledged and duplicate lists should not overlap
        let response = DataResponse {
            acknowledged_batch_ids: vec!["batch1".to_string(), "batch2".to_string()],
            duplicate_batch_ids: vec!["batch3".to_string(), "batch4".to_string()],
        };

        // Convert to sets to check for overlap
        let acknowledged_set: std::collections::HashSet<_> =
            response.acknowledged_batch_ids.iter().collect();
        let duplicate_set: std::collections::HashSet<_> =
            response.duplicate_batch_ids.iter().collect();

        // No overlap should exist
        let intersection: Vec<_> = acknowledged_set.intersection(&duplicate_set).collect();
        assert_eq!(intersection.len(), 0);
    }

    // ============================================================================
    // Multiple Readings Tests
    // ============================================================================

    #[test]
    fn test_data_request_multiple_readings() {
        let readings = vec![
            create_test_reading("batch1", 1704067800000),
            create_test_reading("batch2", 1704067900000),
            create_test_reading("batch3", 1704068000000),
        ];

        let request = DataRequest {
            readings: readings.clone(),
        };

        assert_eq!(request.readings.len(), 3);
        assert_eq!(request.readings[0].batch_id, "batch1");
        assert_eq!(request.readings[1].batch_id, "batch2");
        assert_eq!(request.readings[2].batch_id, "batch3");
    }

    #[test]
    fn test_data_request_single_reading() {
        let reading = create_test_reading("batch1", 1704067800000);
        let request = DataRequest {
            readings: vec![reading],
        };

        assert_eq!(request.readings.len(), 1);
        assert_eq!(request.readings[0].batch_id, "batch1");
    }

    #[test]
    fn test_data_request_serialization_deserialization() {
        let readings = vec![
            create_test_reading("batch1", 1704067800000),
            create_test_reading("batch2", 1704067900000),
        ];

        let request = DataRequest {
            readings: readings.clone(),
        };

        // Serialize
        let json = serde_json::to_string(&request).unwrap();

        // Deserialize
        let parsed: DataRequest = serde_json::from_str(&json).unwrap();

        assert_eq!(parsed.readings.len(), 2);
        assert_eq!(parsed.readings[0].batch_id, "batch1");
        assert_eq!(parsed.readings[1].batch_id, "batch2");
        assert_eq!(parsed.readings[0].hardware_id, "AA:BB:CC:DD:EE:FF");
    }

    // ============================================================================
    // Edge Case Tests
    // ============================================================================

    #[test]
    fn test_reading_with_optional_fields_present() {
        let reading = create_test_reading("batch1", 1704067800000);

        assert!(reading.friendly_name.is_some());
        assert_eq!(reading.friendly_name.unwrap(), "test-sensor");
        assert!(reading.sensors.bme280_temp_c.is_some());
        assert!(reading.sensors.ds18b20_temp_c.is_some());
    }

    #[test]
    fn test_reading_with_optional_fields_absent() {
        let reading = Reading {
            batch_id: "batch1".to_string(),
            hardware_id: "AA:BB:CC:DD:EE:FF".to_string(),
            timestamp_ms: 1704067800000,
            boot_id: "550e8400-e29b-41d4-a716-446655440000".to_string(),
            firmware_version: "1.0.16".to_string(),
            friendly_name: None, // No friendly name
            sensors: SensorValues {
                bme280_temp_c: Some(22.5),
                ds18b20_temp_c: None,    // No DS18B20 reading
                humidity_pct: None,      // No humidity reading
                pressure_hpa: None,      // No pressure reading
                soil_moisture_pct: None, // No soil moisture reading
            },
            sensor_status: SensorStatus {
                bme280: "ok".to_string(),
                ds18b20: "error".to_string(),
                soil_moisture: "error".to_string(),
            },
        };

        assert!(reading.friendly_name.is_none());
        assert!(reading.sensors.bme280_temp_c.is_some());
        assert!(reading.sensors.ds18b20_temp_c.is_none());
        assert_eq!(reading.sensor_status.ds18b20, "error");
    }

    #[test]
    fn test_reading_with_all_sensor_errors() {
        let reading = Reading {
            batch_id: "batch1".to_string(),
            hardware_id: "AA:BB:CC:DD:EE:FF".to_string(),
            timestamp_ms: 1704067800000,
            boot_id: "550e8400-e29b-41d4-a716-446655440000".to_string(),
            firmware_version: "1.0.16".to_string(),
            friendly_name: Some("failing-sensor".to_string()),
            sensors: SensorValues {
                bme280_temp_c: None,
                ds18b20_temp_c: None,
                humidity_pct: None,
                pressure_hpa: None,
                soil_moisture_pct: None,
            },
            sensor_status: SensorStatus {
                bme280: "error".to_string(),
                ds18b20: "error".to_string(),
                soil_moisture: "error".to_string(),
            },
        };

        // All sensors should be in error state
        assert_eq!(reading.sensor_status.bme280, "error");
        assert_eq!(reading.sensor_status.ds18b20, "error");
        assert_eq!(reading.sensor_status.soil_moisture, "error");

        // All sensor values should be None
        assert!(reading.sensors.bme280_temp_c.is_none());
        assert!(reading.sensors.ds18b20_temp_c.is_none());
        assert!(reading.sensors.humidity_pct.is_none());
        assert!(reading.sensors.pressure_hpa.is_none());
        assert!(reading.sensors.soil_moisture_pct.is_none());
    }

    #[test]
    fn test_reading_with_valid_timestamp_boundaries() {
        // Test minimum valid timestamp (year 2000)
        let reading_min = create_test_reading("batch1", 946684800000);
        assert_eq!(reading_min.timestamp_ms, 946684800000);

        // Test maximum valid timestamp (year 2100)
        let reading_max = create_test_reading("batch2", 4102444800000);
        assert_eq!(reading_max.timestamp_ms, 4102444800000);

        // Test typical timestamp (2024)
        let reading_typical = create_test_reading("batch3", 1704067800000);
        assert_eq!(reading_typical.timestamp_ms, 1704067800000);
    }

    #[test]
    fn test_reading_with_valid_batch_id_formats() {
        // Test various valid batch ID formats
        let max_length_batch_id = "a".repeat(256);
        let batch_ids = vec![
            "simple-batch-id",
            "AA:BB:CC:DD:EE:FF_boot123_1704067200000_1704067800000",
            "batch_with_underscores",
            "batch-with-dashes",
            "batch.with.dots",
            "batch123",
            "a",                  // Single character
            &max_length_batch_id, // Maximum length
        ];

        for batch_id in batch_ids {
            let reading = create_test_reading(batch_id, 1704067800000);
            assert_eq!(reading.batch_id, batch_id);
        }
    }

    #[test]
    fn test_data_request_empty_readings_array() {
        let request = DataRequest { readings: vec![] };

        assert_eq!(request.readings.len(), 0);

        // Verify serialization/deserialization works with empty array
        let json = serde_json::to_string(&request).unwrap();
        let parsed: DataRequest = serde_json::from_str(&json).unwrap();
        assert_eq!(parsed.readings.len(), 0);
    }

    // ============================================================================
    // Validation Order Tests
    // ============================================================================

    #[tokio::test]
    async fn test_validation_order_auth_before_batch_size() {
        let config = create_test_config().await;
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        // Create 101 readings (exceeds batch size limit)
        let mut readings = Vec::new();
        for i in 0..101 {
            readings.push(create_test_reading(
                &format!("batch{}", i),
                1704067800000 + i,
            ));
        }

        // No API key provided
        let request = create_test_request(readings, None);

        let result = handle_data(request, "test-request-id", &config, &clock).await;

        // Should fail at auth first, not batch size validation
        assert!(result.is_err());
        match result {
            Err(ApiError::Auth(AuthError::MissingKey)) => {
                // Expected - auth happens before batch size check
            }
            Err(ApiError::Validation(ValidationError::BatchSizeExceeded)) => {
                panic!("Batch size should be checked AFTER authentication");
            }
            _ => panic!("Expected AuthError::MissingKey"),
        }
    }
}
