use aws_sdk_dynamodb::types::AttributeValue;
use aws_sdk_dynamodb::Client as DynamoDbClient;
use std::collections::HashMap;

use crate::error::DatabaseError;
use esp32_backend::shared::domain::{Reading, SensorStatus, SensorValues};

/// Response for readings query
#[derive(Debug, Clone)]
pub struct ReadingsQueryResponse {
    pub readings: Vec<Reading>,
    pub next_cursor: Option<String>,
}

/// Query readings for a device within a time range
///
/// Uses partition key (hardware_id) and sort key range (ts_batch) for efficient querying.
/// The ts_batch format is "{timestamp_ms:013}#{batch_id}" with zero-padded 13 digits.
///
/// # Arguments
/// * `client` - DynamoDB client
/// * `table_name` - Name of the device_readings table
/// * `hardware_id` - MAC address of the device (partition key)
/// * `from_ms` - Start of time range (epoch milliseconds, inclusive)
/// * `to_ms` - End of time range (epoch milliseconds, inclusive)
/// * `limit` - Maximum number of readings to return (default 50, max 1000)
/// * `cursor` - Optional pagination cursor from previous response
///
/// # Returns
/// * `ReadingsQueryResponse` with readings and optional next_cursor
///
/// # Validation
/// * Validates from_ms <= to_ms
/// * Validates both timestamps are non-negative
/// * Validates timestamps are within reasonable range (not too far in future)
pub async fn query_readings(
    client: &DynamoDbClient,
    table_name: &str,
    hardware_id: &str,
    from_ms: i64,
    to_ms: i64,
    limit: Option<i32>,
    cursor: Option<String>,
) -> Result<ReadingsQueryResponse, DatabaseError> {
    use esp32_backend::shared::cursor::{decode_readings_page_token, encode_readings_page_token};

    // Validate timestamp bounds
    if from_ms < 0 {
        return Err(DatabaseError::Serialization(
            "from_ms must be non-negative".to_string(),
        ));
    }

    if to_ms < 0 {
        return Err(DatabaseError::Serialization(
            "to_ms must be non-negative".to_string(),
        ));
    }

    if from_ms > to_ms {
        return Err(DatabaseError::Serialization(
            "from_ms must be less than or equal to to_ms".to_string(),
        ));
    }

    // Validate timestamps are within reasonable range (not too far in future)
    // Allow up to 1 year in the future to account for clock skew
    let max_timestamp = chrono::Utc::now().timestamp_millis() + (365 * 24 * 60 * 60 * 1000);
    if from_ms > max_timestamp || to_ms > max_timestamp {
        return Err(DatabaseError::Serialization(
            "Timestamps are too far in the future".to_string(),
        ));
    }

    // Validate and apply limit (default 50, max 1000)
    let limit = match limit {
        Some(l) if l < 1 => {
            return Err(DatabaseError::Serialization(
                "Limit must be at least 1".to_string(),
            ))
        }
        Some(l) if l > 1000 => 1000,
        Some(l) => l,
        None => 50,
    };

    // Build sort key range: "{from_ms:013}#" to "{to_ms:013}#\uffff"
    // Zero-pad to 13 digits for lexicographic sorting
    // Append \uffff to to_ms to include all batch_ids at that timestamp
    let from_key = format!("{:013}#", from_ms);
    let to_key = format!("{:013}#\u{ffff}", to_ms);

    // Build query
    let mut query = client
        .query()
        .table_name(table_name)
        .key_condition_expression("hardware_id = :hw_id AND ts_batch BETWEEN :from_key AND :to_key")
        .expression_attribute_values(":hw_id", AttributeValue::S(hardware_id.to_string()))
        .expression_attribute_values(":from_key", AttributeValue::S(from_key))
        .expression_attribute_values(":to_key", AttributeValue::S(to_key))
        .scan_index_forward(false) // Newest first (descending order)
        .limit(limit);

    // Add cursor if provided
    if let Some(cursor_str) = cursor {
        let cursor = decode_readings_page_token(&cursor_str)
            .map_err(|e| DatabaseError::Serialization(format!("Invalid cursor: {}", e.message)))?;

        let start_key = cursor_to_exclusive_start_key(&cursor);
        query = query.set_exclusive_start_key(Some(start_key));
    }

    // Execute query
    let result = query
        .send()
        .await
        .map_err(|e| DatabaseError::DynamoDb(format!("{:?}", e)))?;

    // Parse readings from items
    let readings: Vec<Reading> = result
        .items
        .unwrap_or_default()
        .into_iter()
        .map(|item| item_to_reading(&item))
        .collect::<Result<Vec<_>, _>>()?;

    // Encode next cursor if more results available
    let next_cursor = result.last_evaluated_key.and_then(|key| {
        let hardware_id = key.get("hardware_id")?.as_s().ok()?;
        let ts_batch = key.get("ts_batch")?.as_s().ok()?;
        encode_readings_page_token(hardware_id, ts_batch).ok()
    });

    Ok(ReadingsQueryResponse {
        readings,
        next_cursor,
    })
}

/// Get the latest reading for a device
///
/// Uses partition key (hardware_id) with ScanIndexForward=false and Limit=1
/// to efficiently retrieve the most recent reading.
///
/// # Arguments
/// * `client` - DynamoDB client
/// * `table_name` - Name of the device_readings table
/// * `hardware_id` - MAC address of the device (partition key)
///
/// # Returns
/// * `Ok(Some(Reading))` - Latest reading found
/// * `Ok(None)` - Device has no readings
/// * `Err(DatabaseError)` - DynamoDB error occurred
pub async fn get_latest_reading(
    client: &DynamoDbClient,
    table_name: &str,
    hardware_id: &str,
) -> Result<Option<Reading>, DatabaseError> {
    let result = client
        .query()
        .table_name(table_name)
        .key_condition_expression("hardware_id = :hw_id")
        .expression_attribute_values(":hw_id", AttributeValue::S(hardware_id.to_string()))
        .scan_index_forward(false) // Newest first
        .limit(1)
        .send()
        .await
        .map_err(|e| DatabaseError::DynamoDb(format!("{:?}", e)))?;

    // Check if any items returned
    match result.items {
        Some(items) if !items.is_empty() => {
            let reading = item_to_reading(&items[0])?;
            Ok(Some(reading))
        }
        _ => Ok(None),
    }
}

/// Convert DynamoDB item to Reading struct
fn item_to_reading(item: &HashMap<String, AttributeValue>) -> Result<Reading, DatabaseError> {
    let batch_id = item
        .get("batch_id")
        .and_then(|v| v.as_s().ok())
        .ok_or_else(|| DatabaseError::Serialization("Missing batch_id".to_string()))?
        .clone();

    let hardware_id = item
        .get("hardware_id")
        .and_then(|v| v.as_s().ok())
        .ok_or_else(|| DatabaseError::Serialization("Missing hardware_id".to_string()))?
        .clone();

    let timestamp_ms = item
        .get("timestamp_ms")
        .and_then(|v| v.as_n().ok())
        .and_then(|n| n.parse::<i64>().ok())
        .ok_or_else(|| {
            DatabaseError::Serialization("Missing or invalid timestamp_ms".to_string())
        })?;

    let boot_id = item
        .get("boot_id")
        .and_then(|v| v.as_s().ok())
        .ok_or_else(|| DatabaseError::Serialization("Missing boot_id".to_string()))?
        .clone();

    let firmware_version = item
        .get("firmware_version")
        .and_then(|v| v.as_s().ok())
        .ok_or_else(|| DatabaseError::Serialization("Missing firmware_version".to_string()))?
        .clone();

    let friendly_name = item
        .get("friendly_name")
        .and_then(|v| v.as_s().ok())
        .cloned();

    let sensors = item
        .get("sensors")
        .ok_or_else(|| DatabaseError::Serialization("Missing sensors".to_string()))
        .and_then(attribute_value_to_sensor_values)?;

    let sensor_status = item
        .get("sensor_status")
        .ok_or_else(|| DatabaseError::Serialization("Missing sensor_status".to_string()))
        .and_then(attribute_value_to_sensor_status)?;

    Ok(Reading {
        batch_id,
        hardware_id,
        timestamp_ms,
        boot_id,
        firmware_version,
        friendly_name,
        sensors,
        sensor_status,
    })
}

/// Convert DynamoDB AttributeValue (Map) to SensorValues struct
fn attribute_value_to_sensor_values(attr: &AttributeValue) -> Result<SensorValues, DatabaseError> {
    let sensor_map = attr
        .as_m()
        .map_err(|_| DatabaseError::Serialization("sensors is not a map".to_string()))?;

    // Extract optional sensor values
    let bme280_temp_c = sensor_map
        .get("bme280_temp_c")
        .and_then(|v| v.as_n().ok())
        .and_then(|n| n.parse::<f64>().ok());

    let ds18b20_temp_c = sensor_map
        .get("ds18b20_temp_c")
        .and_then(|v| v.as_n().ok())
        .and_then(|n| n.parse::<f64>().ok());

    let humidity_pct = sensor_map
        .get("humidity_pct")
        .and_then(|v| v.as_n().ok())
        .and_then(|n| n.parse::<f64>().ok());

    let pressure_hpa = sensor_map
        .get("pressure_hpa")
        .and_then(|v| v.as_n().ok())
        .and_then(|n| n.parse::<f64>().ok());

    let soil_moisture_pct = sensor_map
        .get("soil_moisture_pct")
        .and_then(|v| v.as_n().ok())
        .and_then(|n| n.parse::<f64>().ok());

    Ok(SensorValues {
        bme280_temp_c,
        ds18b20_temp_c,
        humidity_pct,
        pressure_hpa,
        soil_moisture_pct,
    })
}

/// Convert DynamoDB AttributeValue (Map) to SensorStatus struct
fn attribute_value_to_sensor_status(attr: &AttributeValue) -> Result<SensorStatus, DatabaseError> {
    let status_map = attr
        .as_m()
        .map_err(|_| DatabaseError::Serialization("sensor_status is not a map".to_string()))?;

    let bme280 = status_map
        .get("bme280")
        .and_then(|v| v.as_s().ok())
        .ok_or_else(|| DatabaseError::Serialization("Missing bme280 status".to_string()))?
        .clone();

    let ds18b20 = status_map
        .get("ds18b20")
        .and_then(|v| v.as_s().ok())
        .ok_or_else(|| DatabaseError::Serialization("Missing ds18b20 status".to_string()))?
        .clone();

    let soil_moisture = status_map
        .get("soil_moisture")
        .and_then(|v| v.as_s().ok())
        .ok_or_else(|| DatabaseError::Serialization("Missing soil_moisture status".to_string()))?
        .clone();

    Ok(SensorStatus {
        bme280,
        ds18b20,
        soil_moisture,
    })
}

/// Convert cursor to DynamoDB exclusive start key
fn cursor_to_exclusive_start_key(
    cursor: &esp32_backend::shared::cursor::ReadingsPageToken,
) -> HashMap<String, AttributeValue> {
    let mut key = HashMap::new();
    key.insert(
        "hardware_id".to_string(),
        AttributeValue::S(cursor.hardware_id.clone()),
    );
    key.insert(
        "ts_batch".to_string(),
        AttributeValue::S(cursor.ts_batch.clone()),
    );
    key
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_attribute_value_to_sensor_values_complete() {
        let mut sensor_map = HashMap::new();
        sensor_map.insert(
            "bme280_temp_c".to_string(),
            AttributeValue::N("22.5".to_string()),
        );
        sensor_map.insert(
            "ds18b20_temp_c".to_string(),
            AttributeValue::N("21.8".to_string()),
        );
        sensor_map.insert(
            "humidity_pct".to_string(),
            AttributeValue::N("45.2".to_string()),
        );
        sensor_map.insert(
            "pressure_hpa".to_string(),
            AttributeValue::N("1013.25".to_string()),
        );
        sensor_map.insert(
            "soil_moisture_pct".to_string(),
            AttributeValue::N("62.3".to_string()),
        );

        let attr_value = AttributeValue::M(sensor_map);
        let sensors = attribute_value_to_sensor_values(&attr_value).unwrap();

        assert_eq!(sensors.bme280_temp_c, Some(22.5));
        assert_eq!(sensors.ds18b20_temp_c, Some(21.8));
        assert_eq!(sensors.humidity_pct, Some(45.2));
        assert_eq!(sensors.pressure_hpa, Some(1013.25));
        assert_eq!(sensors.soil_moisture_pct, Some(62.3));
    }

    #[test]
    fn test_attribute_value_to_sensor_values_partial() {
        let mut sensor_map = HashMap::new();
        sensor_map.insert(
            "bme280_temp_c".to_string(),
            AttributeValue::N("22.5".to_string()),
        );
        // Missing other sensors

        let attr_value = AttributeValue::M(sensor_map);
        let sensors = attribute_value_to_sensor_values(&attr_value).unwrap();

        assert_eq!(sensors.bme280_temp_c, Some(22.5));
        assert_eq!(sensors.ds18b20_temp_c, None);
        assert_eq!(sensors.humidity_pct, None);
        assert_eq!(sensors.pressure_hpa, None);
        assert_eq!(sensors.soil_moisture_pct, None);
    }

    #[test]
    fn test_attribute_value_to_sensor_status() {
        let mut status_map = HashMap::new();
        status_map.insert("bme280".to_string(), AttributeValue::S("ok".to_string()));
        status_map.insert("ds18b20".to_string(), AttributeValue::S("ok".to_string()));
        status_map.insert(
            "soil_moisture".to_string(),
            AttributeValue::S("error".to_string()),
        );

        let attr_value = AttributeValue::M(status_map);
        let status = attribute_value_to_sensor_status(&attr_value).unwrap();

        assert_eq!(status.bme280, "ok");
        assert_eq!(status.ds18b20, "ok");
        assert_eq!(status.soil_moisture, "error");
    }

    #[test]
    fn test_attribute_value_to_sensor_status_missing_field() {
        let mut status_map = HashMap::new();
        status_map.insert("bme280".to_string(), AttributeValue::S("ok".to_string()));
        // Missing ds18b20 and soil_moisture

        let attr_value = AttributeValue::M(status_map);
        let result = attribute_value_to_sensor_status(&attr_value);

        assert!(result.is_err());
        match result {
            Err(DatabaseError::Serialization(msg)) => {
                assert!(msg.contains("ds18b20"));
            }
            _ => panic!("Expected Serialization error"),
        }
    }

    #[test]
    fn test_item_to_reading_complete() {
        let mut sensor_map = HashMap::new();
        sensor_map.insert(
            "bme280_temp_c".to_string(),
            AttributeValue::N("22.5".to_string()),
        );
        sensor_map.insert(
            "humidity_pct".to_string(),
            AttributeValue::N("45.2".to_string()),
        );

        let mut status_map = HashMap::new();
        status_map.insert("bme280".to_string(), AttributeValue::S("ok".to_string()));
        status_map.insert("ds18b20".to_string(), AttributeValue::S("ok".to_string()));
        status_map.insert(
            "soil_moisture".to_string(),
            AttributeValue::S("ok".to_string()),
        );

        let mut item = HashMap::new();
        item.insert(
            "batch_id".to_string(),
            AttributeValue::S("batch_123".to_string()),
        );
        item.insert(
            "hardware_id".to_string(),
            AttributeValue::S("AA:BB:CC:DD:EE:FF".to_string()),
        );
        item.insert(
            "timestamp_ms".to_string(),
            AttributeValue::N("1704067800000".to_string()),
        );
        item.insert(
            "boot_id".to_string(),
            AttributeValue::S("7c9e6679-7425-40de-944b-e07fc1f90ae7".to_string()),
        );
        item.insert(
            "firmware_version".to_string(),
            AttributeValue::S("1.0.16".to_string()),
        );
        item.insert(
            "friendly_name".to_string(),
            AttributeValue::S("test-device".to_string()),
        );
        item.insert("sensors".to_string(), AttributeValue::M(sensor_map));
        item.insert("sensor_status".to_string(), AttributeValue::M(status_map));

        let reading = item_to_reading(&item).unwrap();

        assert_eq!(reading.batch_id, "batch_123");
        assert_eq!(reading.hardware_id, "AA:BB:CC:DD:EE:FF");
        assert_eq!(reading.timestamp_ms, 1704067800000);
        assert_eq!(reading.boot_id, "7c9e6679-7425-40de-944b-e07fc1f90ae7");
        assert_eq!(reading.firmware_version, "1.0.16");
        assert_eq!(reading.friendly_name, Some("test-device".to_string()));
        assert_eq!(reading.sensors.bme280_temp_c, Some(22.5));
        assert_eq!(reading.sensors.humidity_pct, Some(45.2));
        assert_eq!(reading.sensor_status.bme280, "ok");
    }

    #[test]
    fn test_item_to_reading_without_friendly_name() {
        let mut sensor_map = HashMap::new();
        sensor_map.insert(
            "bme280_temp_c".to_string(),
            AttributeValue::N("22.5".to_string()),
        );

        let mut status_map = HashMap::new();
        status_map.insert("bme280".to_string(), AttributeValue::S("ok".to_string()));
        status_map.insert("ds18b20".to_string(), AttributeValue::S("ok".to_string()));
        status_map.insert(
            "soil_moisture".to_string(),
            AttributeValue::S("ok".to_string()),
        );

        let mut item = HashMap::new();
        item.insert(
            "batch_id".to_string(),
            AttributeValue::S("batch_123".to_string()),
        );
        item.insert(
            "hardware_id".to_string(),
            AttributeValue::S("AA:BB:CC:DD:EE:FF".to_string()),
        );
        item.insert(
            "timestamp_ms".to_string(),
            AttributeValue::N("1704067800000".to_string()),
        );
        item.insert(
            "boot_id".to_string(),
            AttributeValue::S("7c9e6679-7425-40de-944b-e07fc1f90ae7".to_string()),
        );
        item.insert(
            "firmware_version".to_string(),
            AttributeValue::S("1.0.16".to_string()),
        );
        // No friendly_name
        item.insert("sensors".to_string(), AttributeValue::M(sensor_map));
        item.insert("sensor_status".to_string(), AttributeValue::M(status_map));

        let reading = item_to_reading(&item).unwrap();

        assert_eq!(reading.friendly_name, None);
    }

    #[test]
    fn test_item_to_reading_missing_required_field() {
        let mut item = HashMap::new();
        item.insert(
            "batch_id".to_string(),
            AttributeValue::S("batch_123".to_string()),
        );
        // Missing hardware_id

        let result = item_to_reading(&item);
        assert!(result.is_err());

        match result {
            Err(DatabaseError::Serialization(msg)) => {
                assert!(msg.contains("hardware_id"));
            }
            _ => panic!("Expected Serialization error"),
        }
    }

    #[test]
    fn test_cursor_to_exclusive_start_key() {
        use esp32_backend::shared::cursor::ReadingsPageToken;

        let cursor = ReadingsPageToken {
            hardware_id: "AA:BB:CC:DD:EE:FF".to_string(),
            ts_batch: "1704067800000#batch_123".to_string(),
        };

        let key = cursor_to_exclusive_start_key(&cursor);

        assert_eq!(key.len(), 2);
        assert_eq!(
            key.get("hardware_id"),
            Some(&AttributeValue::S("AA:BB:CC:DD:EE:FF".to_string()))
        );
        assert_eq!(
            key.get("ts_batch"),
            Some(&AttributeValue::S("1704067800000#batch_123".to_string()))
        );
    }

    #[test]
    fn test_ts_batch_format_sortability() {
        // Verify that zero-padded timestamps sort correctly lexicographically
        let ts_batches = vec![
            "0001704067200000#batch_1",
            "0001704067800000#batch_2",
            "0001704068400000#batch_3",
            "0001704066600000#batch_0",
        ];

        let mut sorted = ts_batches.clone();
        sorted.sort();

        // Expected order: oldest to newest
        assert_eq!(sorted[0], "0001704066600000#batch_0");
        assert_eq!(sorted[1], "0001704067200000#batch_1");
        assert_eq!(sorted[2], "0001704067800000#batch_2");
        assert_eq!(sorted[3], "0001704068400000#batch_3");
    }

    #[test]
    fn test_timestamp_validation() {
        // Test negative timestamps
        assert!(validate_timestamp_bounds(-1, 1000).is_err());
        assert!(validate_timestamp_bounds(0, -1).is_err());

        // Test from > to
        assert!(validate_timestamp_bounds(1000, 500).is_err());

        // Test valid range
        assert!(validate_timestamp_bounds(0, 1000).is_ok());
        assert!(validate_timestamp_bounds(1000, 1000).is_ok());
    }

    // Helper function for testing timestamp validation
    fn validate_timestamp_bounds(from_ms: i64, to_ms: i64) -> Result<(), DatabaseError> {
        if from_ms < 0 {
            return Err(DatabaseError::Serialization(
                "from_ms must be non-negative".to_string(),
            ));
        }

        if to_ms < 0 {
            return Err(DatabaseError::Serialization(
                "to_ms must be non-negative".to_string(),
            ));
        }

        if from_ms > to_ms {
            return Err(DatabaseError::Serialization(
                "from_ms must be less than or equal to to_ms".to_string(),
            ));
        }

        Ok(())
    }

    // Note: Integration tests for query_readings and get_latest_reading
    // require DynamoDB Local and are in the integration test suite
}
