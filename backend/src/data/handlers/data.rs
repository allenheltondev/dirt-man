use lambda_http::{Body, Request, Response};
use serde::{Deserialize, Serialize};

use crate::error::ApiError;
use esp32_backend::domain::Reading;

/// Request payload for POST /data endpoint
///
/// Accepts an array of sensor readings from devices.
/// Each reading includes sensor values, status, and metadata.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DataRequest {
    /// Array of sensor readings to ingest
    pub readings: Vec<Reading>,
}

/// Response payload for POST /data endpoint
///
/// Returns two lists of batch IDs:
/// - acknowledged_batch_ids: Newly processed readings
/// - duplicate_batch_ids: Previously seen readings (idempotent retries)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DataResponse {
    /// Batch IDs that were newly processed and stored
    pub acknowledged_batch_ids: Vec<String>,

    /// Batch IDs that were previously processed (duplicates)
    pub duplicate_batch_ids: Vec<String>,
}

/// Handle POST /data requests for sensor data ingestion
///
/// This handler validates API key authentication, parses and validates
/// the request body, checks batch size limits (max 100 readings), processes
/// each reading with idempotency checks, and returns acknowledged and
/// duplicate batch IDs.
pub async fn handle_data(
    event: Request,
    _request_id: &str,
    config: &crate::config::Config,
    clock: &dyn esp32_backend::Clock,
) -> Result<Response<Body>, ApiError> {
    // Step 1: Extract and validate API key from X-API-Key header
    let api_key = event
        .headers()
        .get("x-api-key")
        .and_then(|v| v.to_str().ok())
        .ok_or(crate::error::AuthError::MissingKey)?;

    // Validate API key against DynamoDB
    crate::auth::validate_api_key(
        &config.dynamodb_client,
        &config.api_keys_table,
        api_key,
        clock,
    )
    .await?;

    // Step 2: Parse request body
    let body_bytes = match event.body() {
        lambda_http::Body::Text(text) => text.as_bytes(),
        lambda_http::Body::Binary(bytes) => bytes.as_slice(),
        lambda_http::Body::Empty => {
            return Err(crate::error::ValidationError::InvalidBody(
                "Request body is empty".to_string(),
            )
            .into());
        }
    };

    let request: DataRequest = serde_json::from_slice(body_bytes).map_err(|e| {
        crate::error::ValidationError::InvalidBody(format!("Failed to parse JSON: {}", e))
    })?;

    // Step 3: Enforce batch size limit (100 readings max) after authentication
    if request.readings.len() > 100 {
        return Err(crate::error::ValidationError::BatchSizeExceeded.into());
    }

    // Step 4: Validate each reading
    for reading in &request.readings {
        // Validate hardware_id (MAC address format)
        esp32_backend::validate_mac_address(&reading.hardware_id).map_err(|e| {
            crate::error::ValidationError::InvalidFormat(format!("hardware_id: {}", e.message))
        })?;

        // Validate timestamp_ms (epoch milliseconds with sane bounds)
        esp32_backend::validate_epoch_millis(reading.timestamp_ms).map_err(|e| {
            crate::error::ValidationError::InvalidFormat(format!("timestamp_ms: {}", e.message))
        })?;

        // Validate batch_id (max length 256, safe ASCII charset - treat as opaque)
        esp32_backend::validate_batch_id(&reading.batch_id).map_err(|e| {
            crate::error::ValidationError::InvalidFormat(format!("batch_id: {}", e.message))
        })?;
    }

    // Step 5: Process each reading with idempotency checks
    let mut acknowledged_batch_ids = Vec::new();
    let mut duplicate_batch_ids = Vec::new();

    for reading in &request.readings {
        // Call transact_write_reading_if_new_batch for each reading
        // This uses DynamoDB transactions to atomically check idempotency and write
        match crate::repo::ingestion::transact_write_reading_if_new_batch(
            &config.dynamodb_client,
            &config.processed_batches_table,
            &config.device_readings_table,
            reading,
            clock,
            None, // No TTL for now (optional enhancement in task 14.3)
        )
        .await
        {
            Ok(true) => {
                // Transaction succeeded, reading was written
                acknowledged_batch_ids.push(reading.batch_id.clone());
            }
            Ok(false) => {
                // Duplicate batch_id detected
                duplicate_batch_ids.push(reading.batch_id.clone());
            }
            Err(e) => {
                // Non-duplicate DynamoDB error occurred
                // Use fail-fast error handling: return error immediately
                return Err(crate::error::ApiError::Database(e));
            }
        }
    }

    // Step 6: Return DataResponse with both lists
    let response = DataResponse {
        acknowledged_batch_ids,
        duplicate_batch_ids,
    };

    let response_body = serde_json::to_string(&response)
        .map_err(|e| ApiError::Internal(format!("Failed to serialize response: {}", e)))?;

    Response::builder()
        .status(200)
        .header("content-type", "application/json")
        .body(Body::from(response_body))
        .map_err(|e| ApiError::Internal(format!("Failed to build response: {}", e)))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_data_request_deserialization() {
        let json = r#"{
            "readings": [
                {
                    "batch_id": "AA:BB:CC:DD:EE:FF_550e8400-e29b-41d4-a716-446655440000_1704067200000_1704067800000",
                    "hardware_id": "AA:BB:CC:DD:EE:FF",
                    "timestamp_ms": 1704067800000,
                    "boot_id": "550e8400-e29b-41d4-a716-446655440000",
                    "firmware_version": "1.0.16",
                    "friendly_name": "test-sensor",
                    "sensors": {
                        "bme280_temp_c": 22.5,
                        "humidity_pct": 45.2
                    },
                    "sensor_status": {
                        "bme280": "ok",
                        "ds18b20": "ok",
                        "soil_moisture": "ok"
                    }
                }
            ]
        }"#;

        let request: DataRequest = serde_json::from_str(json).unwrap();
        assert_eq!(request.readings.len(), 1);
        assert_eq!(request.readings[0].hardware_id, "AA:BB:CC:DD:EE:FF");
        assert_eq!(request.readings[0].timestamp_ms, 1704067800000);
    }

    #[test]
    fn test_data_request_multiple_readings() {
        let json = r#"{
            "readings": [
                {
                    "batch_id": "batch1",
                    "hardware_id": "AA:BB:CC:DD:EE:FF",
                    "timestamp_ms": 1704067800000,
                    "boot_id": "550e8400-e29b-41d4-a716-446655440000",
                    "firmware_version": "1.0.16",
                    "sensors": {},
                    "sensor_status": {
                        "bme280": "ok",
                        "ds18b20": "ok",
                        "soil_moisture": "ok"
                    }
                },
                {
                    "batch_id": "batch2",
                    "hardware_id": "AA:BB:CC:DD:EE:FF",
                    "timestamp_ms": 1704067900000,
                    "boot_id": "550e8400-e29b-41d4-a716-446655440000",
                    "firmware_version": "1.0.16",
                    "sensors": {},
                    "sensor_status": {
                        "bme280": "ok",
                        "ds18b20": "ok",
                        "soil_moisture": "ok"
                    }
                }
            ]
        }"#;

        let request: DataRequest = serde_json::from_str(json).unwrap();
        assert_eq!(request.readings.len(), 2);
        assert_eq!(request.readings[0].batch_id, "batch1");
        assert_eq!(request.readings[1].batch_id, "batch2");
    }

    #[test]
    fn test_data_response_serialization() {
        let response = DataResponse {
            acknowledged_batch_ids: vec!["batch1".to_string(), "batch2".to_string()],
            duplicate_batch_ids: vec!["batch3".to_string()],
        };

        let json = serde_json::to_string(&response).unwrap();
        assert!(json.contains("acknowledged_batch_ids"));
        assert!(json.contains("duplicate_batch_ids"));
        assert!(json.contains("batch1"));
        assert!(json.contains("batch2"));
        assert!(json.contains("batch3"));
    }

    #[test]
    fn test_data_response_empty_lists() {
        let response = DataResponse {
            acknowledged_batch_ids: vec![],
            duplicate_batch_ids: vec![],
        };

        let json = serde_json::to_string(&response).unwrap();
        assert!(json.contains("acknowledged_batch_ids"));
        assert!(json.contains("duplicate_batch_ids"));
    }

    #[test]
    fn test_data_response_only_acknowledged() {
        let response = DataResponse {
            acknowledged_batch_ids: vec!["batch1".to_string()],
            duplicate_batch_ids: vec![],
        };

        let json = serde_json::to_string(&response).unwrap();
        let parsed: DataResponse = serde_json::from_str(&json).unwrap();
        assert_eq!(parsed.acknowledged_batch_ids.len(), 1);
        assert_eq!(parsed.duplicate_batch_ids.len(), 0);
    }

    #[test]
    fn test_data_response_only_duplicates() {
        let response = DataResponse {
            acknowledged_batch_ids: vec![],
            duplicate_batch_ids: vec!["batch1".to_string()],
        };

        let json = serde_json::to_string(&response).unwrap();
        let parsed: DataResponse = serde_json::from_str(&json).unwrap();
        assert_eq!(parsed.acknowledged_batch_ids.len(), 0);
        assert_eq!(parsed.duplicate_batch_ids.len(), 1);
    }

    #[test]
    fn test_reading_with_optional_fields() {
        let json = r#"{
            "readings": [
                {
                    "batch_id": "batch1",
                    "hardware_id": "AA:BB:CC:DD:EE:FF",
                    "timestamp_ms": 1704067800000,
                    "boot_id": "550e8400-e29b-41d4-a716-446655440000",
                    "firmware_version": "1.0.16",
                    "sensors": {
                        "bme280_temp_c": 22.5
                    },
                    "sensor_status": {
                        "bme280": "ok",
                        "ds18b20": "error",
                        "soil_moisture": "ok"
                    }
                }
            ]
        }"#;

        let request: DataRequest = serde_json::from_str(json).unwrap();
        assert_eq!(request.readings.len(), 1);
        assert_eq!(request.readings[0].sensors.bme280_temp_c, Some(22.5));
        assert_eq!(request.readings[0].sensors.ds18b20_temp_c, None);
        assert_eq!(request.readings[0].friendly_name, None);
    }
}
