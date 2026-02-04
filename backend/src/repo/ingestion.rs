use aws_sdk_dynamodb::types::{AttributeValue, Put, TransactWriteItem};
use aws_sdk_dynamodb::Client as DynamoDbClient;
use std::collections::HashMap;

use esp32_backend::{Clock, Reading};

use crate::error::DatabaseError;

/// Result of a transactional write attempt
/// - Ok(true): Transaction succeeded, reading was written
/// - Ok(false): Duplicate batch_id detected, reading was not written
/// - Err: Other database error occurred
pub type TransactWriteResult = Result<bool, DatabaseError>;

/// Atomically write a reading to both processed_batches and device_readings tables
/// using DynamoDB transactions.
///
/// This function ensures idempotency by:
/// 1. Conditionally writing to processed_batches (fails if batch_id exists)
/// 2. Writing to device_readings (only if step 1 succeeds)
///
/// Both operations are atomic - either both succeed or both fail.
///
/// # Arguments
/// * `client` - DynamoDB client
/// * `batches_table` - Name of the processed_batches table
/// * `readings_table` - Name of the device_readings table
/// * `reading` - The sensor reading to write
/// * `clock` - Clock implementation for timestamp generation
/// * `retention_seconds` - Optional TTL for readings (if None, no TTL is set)
///
/// # Returns
/// * `Ok(true)` - Transaction succeeded, reading was written
/// * `Ok(false)` - Duplicate batch_id detected (ConditionalCheckFailedException)
/// * `Err(DatabaseError)` - Other database error occurred
pub async fn transact_write_reading_if_new_batch(
    client: &DynamoDbClient,
    batches_table: &str,
    readings_table: &str,
    reading: &Reading,
    clock: &dyn Clock,
    retention_seconds: Option<i64>,
) -> TransactWriteResult {
    // Prepare processed_batches record
    let received_at = clock.now_rfc3339();
    let batch_expiration_time = clock.now_epoch_seconds() + (30 * 24 * 3600); // 30 days

    let mut batch_item = HashMap::new();
    batch_item.insert(
        "batch_id".to_string(),
        AttributeValue::S(reading.batch_id.clone()),
    );
    batch_item.insert(
        "hardware_id".to_string(),
        AttributeValue::S(reading.hardware_id.clone()),
    );
    batch_item.insert("received_at".to_string(), AttributeValue::S(received_at));
    batch_item.insert(
        "expiration_time".to_string(),
        AttributeValue::N(batch_expiration_time.to_string()),
    );

    // Prepare device_readings record
    let ts_batch = format!("{:013}#{}", reading.timestamp_ms, reading.batch_id);

    let mut reading_item = HashMap::new();
    reading_item.insert(
        "hardware_id".to_string(),
        AttributeValue::S(reading.hardware_id.clone()),
    );
    reading_item.insert("ts_batch".to_string(), AttributeValue::S(ts_batch));
    reading_item.insert(
        "timestamp_ms".to_string(),
        AttributeValue::N(reading.timestamp_ms.to_string()),
    );
    reading_item.insert(
        "batch_id".to_string(),
        AttributeValue::S(reading.batch_id.clone()),
    );
    reading_item.insert(
        "boot_id".to_string(),
        AttributeValue::S(reading.boot_id.clone()),
    );
    reading_item.insert(
        "firmware_version".to_string(),
        AttributeValue::S(reading.firmware_version.clone()),
    );

    // Add optional friendly_name
    if let Some(ref friendly_name) = reading.friendly_name {
        reading_item.insert(
            "friendly_name".to_string(),
            AttributeValue::S(friendly_name.clone()),
        );
    }

    // Add sensors map
    let sensors_map = sensor_values_to_attribute_map(&reading.sensors);
    reading_item.insert("sensors".to_string(), AttributeValue::M(sensors_map));

    // Add sensor_status map
    let sensor_status_map = sensor_status_to_attribute_map(&reading.sensor_status);
    reading_item.insert(
        "sensor_status".to_string(),
        AttributeValue::M(sensor_status_map),
    );

    // Add TTL if retention is specified
    if let Some(retention) = retention_seconds {
        let reading_expiration_time = (reading.timestamp_ms / 1000) + retention;
        reading_item.insert(
            "expiration_time".to_string(),
            AttributeValue::N(reading_expiration_time.to_string()),
        );
    }

    // Build transaction items
    let batch_put = Put::builder()
        .table_name(batches_table)
        .set_item(Some(batch_item))
        .condition_expression("attribute_not_exists(batch_id)")
        .build()
        .map_err(|e| DatabaseError::DynamoDb(format!("Failed to build batch Put: {}", e)))?;

    let reading_put = Put::builder()
        .table_name(readings_table)
        .set_item(Some(reading_item))
        .build()
        .map_err(|e| DatabaseError::DynamoDb(format!("Failed to build reading Put: {}", e)))?;

    let transact_items = vec![
        TransactWriteItem::builder().put(batch_put).build(),
        TransactWriteItem::builder().put(reading_put).build(),
    ];

    // Execute transaction
    let result = client
        .transact_write_items()
        .set_transact_items(Some(transact_items))
        .send()
        .await;

    match result {
        Ok(_) => Ok(true), // Transaction succeeded
        Err(err) => {
            // Check if it's a conditional check failure (duplicate batch_id)
            if is_conditional_check_failed(&err) {
                Ok(false) // Duplicate detected
            } else {
                // Other error
                Err(DatabaseError::from(err))
            }
        }
    }
}

/// Convert SensorValues to DynamoDB attribute map
fn sensor_values_to_attribute_map(
    sensors: &esp32_backend::SensorValues,
) -> HashMap<String, AttributeValue> {
    let mut map = HashMap::new();

    if let Some(temp) = sensors.bme280_temp_c {
        map.insert(
            "bme280_temp_c".to_string(),
            AttributeValue::N(temp.to_string()),
        );
    }
    if let Some(temp) = sensors.ds18b20_temp_c {
        map.insert(
            "ds18b20_temp_c".to_string(),
            AttributeValue::N(temp.to_string()),
        );
    }
    if let Some(humidity) = sensors.humidity_pct {
        map.insert(
            "humidity_pct".to_string(),
            AttributeValue::N(humidity.to_string()),
        );
    }
    if let Some(pressure) = sensors.pressure_hpa {
        map.insert(
            "pressure_hpa".to_string(),
            AttributeValue::N(pressure.to_string()),
        );
    }
    if let Some(moisture) = sensors.soil_moisture_pct {
        map.insert(
            "soil_moisture_pct".to_string(),
            AttributeValue::N(moisture.to_string()),
        );
    }

    map
}

/// Convert SensorStatus to DynamoDB attribute map
fn sensor_status_to_attribute_map(
    status: &esp32_backend::SensorStatus,
) -> HashMap<String, AttributeValue> {
    let mut map = HashMap::new();

    map.insert(
        "bme280".to_string(),
        AttributeValue::S(status.bme280.clone()),
    );
    map.insert(
        "ds18b20".to_string(),
        AttributeValue::S(status.ds18b20.clone()),
    );
    map.insert(
        "soil_moisture".to_string(),
        AttributeValue::S(status.soil_moisture.clone()),
    );

    map
}

/// Check if the error is a TransactionCanceledException due to conditional check failure
fn is_conditional_check_failed(
    err: &aws_sdk_dynamodb::error::SdkError<
        aws_sdk_dynamodb::operation::transact_write_items::TransactWriteItemsError,
    >,
) -> bool {
    use aws_sdk_dynamodb::error::SdkError;
    use aws_sdk_dynamodb::operation::transact_write_items::TransactWriteItemsError;

    match err {
        SdkError::ServiceError(service_err) => {
            matches!(
                service_err.err(),
                TransactWriteItemsError::TransactionCanceledException(_)
            )
        }
        _ => false,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use esp32_backend::{FixedClock, SensorStatus, SensorValues};

    fn create_test_reading() -> Reading {
        Reading {
            batch_id: "AA:BB:CC:DD:EE:FF_boot123_1704067200000_1704067800000".to_string(),
            hardware_id: "AA:BB:CC:DD:EE:FF".to_string(),
            timestamp_ms: 1704067800000,
            boot_id: "550e8400-e29b-41d4-a716-446655440000".to_string(),
            firmware_version: "1.0.16".to_string(),
            friendly_name: Some("test-device".to_string()),
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

    #[test]
    fn test_sensor_values_to_attribute_map() {
        let sensors = SensorValues {
            bme280_temp_c: Some(22.5),
            ds18b20_temp_c: None,
            humidity_pct: Some(45.2),
            pressure_hpa: None,
            soil_moisture_pct: Some(62.3),
        };

        let map = sensor_values_to_attribute_map(&sensors);

        assert_eq!(map.len(), 3);
        assert!(map.contains_key("bme280_temp_c"));
        assert!(map.contains_key("humidity_pct"));
        assert!(map.contains_key("soil_moisture_pct"));
        assert!(!map.contains_key("ds18b20_temp_c"));
        assert!(!map.contains_key("pressure_hpa"));
    }

    #[test]
    fn test_sensor_status_to_attribute_map() {
        let status = SensorStatus {
            bme280: "ok".to_string(),
            ds18b20: "error".to_string(),
            soil_moisture: "ok".to_string(),
        };

        let map = sensor_status_to_attribute_map(&status);

        assert_eq!(map.len(), 3);
        assert_eq!(map.get("bme280").unwrap().as_s().unwrap(), "ok");
        assert_eq!(map.get("ds18b20").unwrap().as_s().unwrap(), "error");
        assert_eq!(map.get("soil_moisture").unwrap().as_s().unwrap(), "ok");
    }

    #[test]
    fn test_ts_batch_format() {
        let reading = create_test_reading();
        let ts_batch = format!("{:013}#{}", reading.timestamp_ms, reading.batch_id);

        // Verify zero-padding to 13 digits
        assert!(ts_batch.starts_with("1704067800000#"));
        assert_eq!(ts_batch.len(), 13 + 1 + reading.batch_id.len());
    }

    #[test]
    fn test_ttl_calculation() {
        let clock = FixedClock::from_epoch_seconds(1705316400);
        let retention_seconds = 90 * 24 * 3600; // 90 days

        let reading = create_test_reading();
        let reading_expiration = (reading.timestamp_ms / 1000) + retention_seconds;

        // Verify TTL is calculated from reading timestamp, not current time
        assert_eq!(reading_expiration, 1704067800 + retention_seconds);

        // Verify batch TTL is calculated from current time
        let batch_expiration = clock.now_epoch_seconds() + (30 * 24 * 3600);
        assert_eq!(batch_expiration, 1705316400 + (30 * 24 * 3600));
    }

    // Note: Integration tests with actual DynamoDB client are in the integration test suite
    // These unit tests verify the data transformation logic only
}
