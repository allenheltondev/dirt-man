use aws_sdk_dynamodb::types::AttributeValue;
use aws_sdk_dynamodb::Client as DynamoDbClient;
use std::collections::HashMap;

use crate::error::DatabaseError;
use esp32_backend::shared::domain::{Capabilities, Device};

/// Get a device by hardware_id from the devices table
///
/// Uses GetItem to retrieve a device record by its partition key.
///
/// # Arguments
/// * `client` - DynamoDB client
/// * `table_name` - Name of the devices table
/// * `hardware_id` - MAC address of the device (partition key)
///
/// # Returns
/// * `Ok(Some(Device))` - Device found
/// * `Ok(None)` - Device not found
/// * `Err(DatabaseError)` - DynamoDB error occurred
pub async fn get_device(
    client: &DynamoDbClient,
    table_name: &str,
    hardware_id: &str,
) -> Result<Option<Device>, DatabaseError> {
    let result = client
        .get_item()
        .table_name(table_name)
        .key("hardware_id", AttributeValue::S(hardware_id.to_string()))
        .send()
        .await
        .map_err(|e| DatabaseError::DynamoDb(format!("{:?}", e)))?;

    // Check if item exists
    match result.item {
        Some(item) => {
            let device = item_to_device(&item)?;
            Ok(Some(device))
        }
        None => Ok(None),
    }
}

/// Create a new device record in the devices table
///
/// Uses PutItem to create a new device record with GSI attributes.
/// Sets gsi1pk="devices" and gsi1sk=last_seen_at for listing devices sorted by activity.
///
/// # Arguments
/// * `client` - DynamoDB client
/// * `table_name` - Name of the devices table
/// * `device` - Device record to create
///
/// # Returns
/// * `Ok(())` - Device created successfully
/// * `Err(DatabaseError)` - DynamoDB error occurred
pub async fn create_device(
    client: &DynamoDbClient,
    table_name: &str,
    device: &Device,
) -> Result<(), DatabaseError> {
    let mut item = HashMap::new();

    // Partition key
    item.insert(
        "hardware_id".to_string(),
        AttributeValue::S(device.hardware_id.clone()),
    );

    // Device attributes
    item.insert(
        "confirmation_id".to_string(),
        AttributeValue::S(device.confirmation_id.clone()),
    );

    if let Some(ref friendly_name) = device.friendly_name {
        item.insert(
            "friendly_name".to_string(),
            AttributeValue::S(friendly_name.clone()),
        );
    }

    item.insert(
        "firmware_version".to_string(),
        AttributeValue::S(device.firmware_version.clone()),
    );

    // Capabilities (nested map)
    item.insert(
        "capabilities".to_string(),
        capabilities_to_attribute_value(&device.capabilities),
    );

    // Timestamps (RFC3339 strings)
    item.insert(
        "first_registered_at".to_string(),
        AttributeValue::S(device.first_registered_at.clone()),
    );

    item.insert(
        "last_seen_at".to_string(),
        AttributeValue::S(device.last_seen_at.clone()),
    );

    item.insert(
        "last_boot_id".to_string(),
        AttributeValue::S(device.last_boot_id.clone()),
    );

    // GSI attributes for listing devices sorted by last_seen_at
    item.insert(
        "gsi1pk".to_string(),
        AttributeValue::S("devices".to_string()),
    );

    item.insert(
        "gsi1sk".to_string(),
        AttributeValue::S(device.last_seen_at.clone()),
    );

    client
        .put_item()
        .table_name(table_name)
        .set_item(Some(item))
        .send()
        .await
        .map_err(|e| DatabaseError::DynamoDb(format!("{:?}", e)))?;

    Ok(())
}

/// Update device timestamps and last_boot_id
///
/// Uses UpdateItem to update last_seen_at, last_boot_id, and gsi1sk (for GSI sorting).
/// This is called when a device re-registers.
///
/// # Arguments
/// * `client` - DynamoDB client
/// * `table_name` - Name of the devices table
/// * `hardware_id` - MAC address of the device (partition key)
/// * `last_seen_at` - New last_seen_at timestamp (RFC3339 string)
/// * `last_boot_id` - New boot_id from the device
///
/// # Returns
/// * `Ok(())` - Update successful
/// * `Err(DatabaseError)` - DynamoDB error occurred
pub async fn update_device_timestamps(
    client: &DynamoDbClient,
    table_name: &str,
    hardware_id: &str,
    last_seen_at: &str,
    last_boot_id: &str,
) -> Result<(), DatabaseError> {
    client
        .update_item()
        .table_name(table_name)
        .key("hardware_id", AttributeValue::S(hardware_id.to_string()))
        .update_expression(
            "SET last_seen_at = :last_seen, last_boot_id = :boot_id, gsi1sk = :gsi1sk",
        )
        .expression_attribute_values(":last_seen", AttributeValue::S(last_seen_at.to_string()))
        .expression_attribute_values(":boot_id", AttributeValue::S(last_boot_id.to_string()))
        .expression_attribute_values(":gsi1sk", AttributeValue::S(last_seen_at.to_string()))
        .send()
        .await
        .map_err(|e| DatabaseError::DynamoDb(format!("{:?}", e)))?;

    Ok(())
}

/// Convert a DynamoDB item to a Device struct
fn item_to_device(item: &HashMap<String, AttributeValue>) -> Result<Device, DatabaseError> {
    let hardware_id = item
        .get("hardware_id")
        .and_then(|v| v.as_s().ok())
        .ok_or_else(|| DatabaseError::Serialization("Missing hardware_id".to_string()))?
        .clone();

    let confirmation_id = item
        .get("confirmation_id")
        .and_then(|v| v.as_s().ok())
        .ok_or_else(|| DatabaseError::Serialization("Missing confirmation_id".to_string()))?
        .clone();

    let friendly_name = item
        .get("friendly_name")
        .and_then(|v| v.as_s().ok())
        .cloned();

    let firmware_version = item
        .get("firmware_version")
        .and_then(|v| v.as_s().ok())
        .ok_or_else(|| DatabaseError::Serialization("Missing firmware_version".to_string()))?
        .clone();

    let capabilities = item
        .get("capabilities")
        .ok_or_else(|| DatabaseError::Serialization("Missing capabilities".to_string()))
        .and_then(attribute_value_to_capabilities)?;

    let first_registered_at = item
        .get("first_registered_at")
        .and_then(|v| v.as_s().ok())
        .ok_or_else(|| DatabaseError::Serialization("Missing first_registered_at".to_string()))?
        .clone();

    let last_seen_at = item
        .get("last_seen_at")
        .and_then(|v| v.as_s().ok())
        .ok_or_else(|| DatabaseError::Serialization("Missing last_seen_at".to_string()))?
        .clone();

    let last_boot_id = item
        .get("last_boot_id")
        .and_then(|v| v.as_s().ok())
        .ok_or_else(|| DatabaseError::Serialization("Missing last_boot_id".to_string()))?
        .clone();

    Ok(Device {
        hardware_id,
        confirmation_id,
        friendly_name,
        firmware_version,
        capabilities,
        first_registered_at,
        last_seen_at,
        last_boot_id,
    })
}

/// Convert Capabilities struct to DynamoDB AttributeValue (Map)
fn capabilities_to_attribute_value(capabilities: &Capabilities) -> AttributeValue {
    let mut cap_map = HashMap::new();

    // Convert sensors list to DynamoDB List
    let sensors_list: Vec<AttributeValue> = capabilities
        .sensors
        .iter()
        .map(|s| AttributeValue::S(s.clone()))
        .collect();
    cap_map.insert("sensors".to_string(), AttributeValue::L(sensors_list));

    // Convert features map to DynamoDB Map
    let features_map: HashMap<String, AttributeValue> = capabilities
        .features
        .iter()
        .map(|(k, v)| (k.clone(), AttributeValue::Bool(*v)))
        .collect();
    cap_map.insert("features".to_string(), AttributeValue::M(features_map));

    AttributeValue::M(cap_map)
}

/// Convert DynamoDB AttributeValue (Map) to Capabilities struct
fn attribute_value_to_capabilities(attr: &AttributeValue) -> Result<Capabilities, DatabaseError> {
    let cap_map = attr
        .as_m()
        .map_err(|_| DatabaseError::Serialization("capabilities is not a map".to_string()))?;

    // Extract sensors list
    let sensors_attr = cap_map.get("sensors").ok_or_else(|| {
        DatabaseError::Serialization("Missing sensors in capabilities".to_string())
    })?;

    let sensors_list = sensors_attr
        .as_l()
        .map_err(|_| DatabaseError::Serialization("sensors is not a list".to_string()))?;

    let sensors: Vec<String> = sensors_list
        .iter()
        .filter_map(|v| v.as_s().ok().cloned())
        .collect();

    // Extract features map
    let features_attr = cap_map.get("features").ok_or_else(|| {
        DatabaseError::Serialization("Missing features in capabilities".to_string())
    })?;

    let features_map = features_attr
        .as_m()
        .map_err(|_| DatabaseError::Serialization("features is not a map".to_string()))?;

    let features: HashMap<String, bool> = features_map
        .iter()
        .filter_map(|(k, v)| v.as_bool().ok().map(|b| (k.clone(), *b)))
        .collect();

    Ok(Capabilities { sensors, features })
}

/// Response for device list query
#[derive(Debug, Clone)]
pub struct DeviceListResponse {
    pub devices: Vec<Device>,
    pub next_cursor: Option<String>,
}

/// List devices with pagination, sorted by last_seen_at descending
///
/// Uses GSI1 (gsi1pk="devices", gsi1sk=last_seen_at) for efficient querying
/// sorted by activity. RFC3339 timestamps sort correctly lexicographically.
///
/// # Arguments
/// * `client` - DynamoDB client
/// * `table_name` - Name of the devices table
/// * `limit` - Maximum number of devices to return (default 50, max 1000)
/// * `cursor` - Optional pagination cursor from previous response
///
/// # Returns
/// * `DeviceListResponse` with devices and optional next_cursor
pub async fn list_devices(
    client: &DynamoDbClient,
    table_name: &str,
    limit: Option<i32>,
    cursor: Option<String>,
) -> Result<DeviceListResponse, DatabaseError> {
    use esp32_backend::shared::cursor::{decode_device_page_token, encode_device_page_token};

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

    // Build query
    let mut query = client
        .query()
        .table_name(table_name)
        .index_name("gsi1")
        .key_condition_expression("gsi1pk = :pk")
        .expression_attribute_values(":pk", AttributeValue::S("devices".to_string()))
        .scan_index_forward(false) // Most recent first (descending order)
        .limit(limit);

    // Add cursor if provided
    if let Some(cursor_str) = cursor {
        let cursor = decode_device_page_token(&cursor_str)
            .map_err(|e| DatabaseError::Serialization(format!("Invalid cursor: {}", e.message)))?;

        let start_key = cursor_to_exclusive_start_key(&cursor);
        query = query.set_exclusive_start_key(Some(start_key));
    }

    // Execute query
    let result = query
        .send()
        .await
        .map_err(|e| DatabaseError::DynamoDb(format!("{:?}", e)))?;

    // Parse devices from items
    let devices: Vec<Device> = result
        .items
        .unwrap_or_default()
        .into_iter()
        .map(|item| item_to_device(&item))
        .collect::<Result<Vec<_>, _>>()?;

    // Encode next cursor if more results available
    let next_cursor = result.last_evaluated_key.and_then(|key| {
        let hardware_id = key.get("hardware_id")?.as_s().ok()?;
        let gsi1sk = key.get("gsi1sk")?.as_s().ok()?;
        encode_device_page_token(hardware_id, gsi1sk).ok()
    });

    Ok(DeviceListResponse {
        devices,
        next_cursor,
    })
}

/// Convert cursor to DynamoDB exclusive start key
fn cursor_to_exclusive_start_key(
    cursor: &esp32_backend::shared::cursor::DeviceListPageToken,
) -> HashMap<String, AttributeValue> {
    let mut key = HashMap::new();
    key.insert(
        "hardware_id".to_string(),
        AttributeValue::S(cursor.hardware_id.clone()),
    );
    key.insert(
        "gsi1pk".to_string(),
        AttributeValue::S("devices".to_string()),
    );
    key.insert(
        "gsi1sk".to_string(),
        AttributeValue::S(cursor.gsi1sk.clone()),
    );
    key
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_capabilities_to_attribute_value() {
        let mut features = HashMap::new();
        features.insert("tft_display".to_string(), true);
        features.insert("offline_buffering".to_string(), false);

        let capabilities = Capabilities {
            sensors: vec!["bme280".to_string(), "ds18b20".to_string()],
            features,
        };

        let attr_value = capabilities_to_attribute_value(&capabilities);

        // Verify it's a map
        assert!(attr_value.as_m().is_ok());

        let cap_map = attr_value.as_m().unwrap();

        // Verify sensors list
        let sensors = cap_map.get("sensors").unwrap().as_l().unwrap();
        assert_eq!(sensors.len(), 2);

        // Verify features map
        let features = cap_map.get("features").unwrap().as_m().unwrap();
        assert_eq!(features.len(), 2);
        assert_eq!(
            features.get("tft_display").unwrap().as_bool().unwrap(),
            &true
        );
        assert_eq!(
            features
                .get("offline_buffering")
                .unwrap()
                .as_bool()
                .unwrap(),
            &false
        );
    }

    #[test]
    fn test_attribute_value_to_capabilities() {
        let mut features_map = HashMap::new();
        features_map.insert("tft_display".to_string(), AttributeValue::Bool(true));
        features_map.insert("offline_buffering".to_string(), AttributeValue::Bool(false));

        let sensors_list = vec![
            AttributeValue::S("bme280".to_string()),
            AttributeValue::S("ds18b20".to_string()),
        ];

        let mut cap_map = HashMap::new();
        cap_map.insert("sensors".to_string(), AttributeValue::L(sensors_list));
        cap_map.insert("features".to_string(), AttributeValue::M(features_map));

        let attr_value = AttributeValue::M(cap_map);

        let capabilities = attribute_value_to_capabilities(&attr_value).unwrap();

        assert_eq!(capabilities.sensors.len(), 2);
        assert!(capabilities.sensors.contains(&"bme280".to_string()));
        assert!(capabilities.sensors.contains(&"ds18b20".to_string()));

        assert_eq!(capabilities.features.len(), 2);
        assert_eq!(capabilities.features.get("tft_display"), Some(&true));
        assert_eq!(capabilities.features.get("offline_buffering"), Some(&false));
    }

    #[test]
    fn test_item_to_device_complete() {
        let mut features_map = HashMap::new();
        features_map.insert("tft_display".to_string(), AttributeValue::Bool(true));

        let sensors_list = vec![AttributeValue::S("bme280".to_string())];

        let mut cap_map = HashMap::new();
        cap_map.insert("sensors".to_string(), AttributeValue::L(sensors_list));
        cap_map.insert("features".to_string(), AttributeValue::M(features_map));

        let mut item = HashMap::new();
        item.insert(
            "hardware_id".to_string(),
            AttributeValue::S("AA:BB:CC:DD:EE:FF".to_string()),
        );
        item.insert(
            "confirmation_id".to_string(),
            AttributeValue::S("550e8400-e29b-41d4-a716-446655440000".to_string()),
        );
        item.insert(
            "friendly_name".to_string(),
            AttributeValue::S("test-device".to_string()),
        );
        item.insert(
            "firmware_version".to_string(),
            AttributeValue::S("1.0.16".to_string()),
        );
        item.insert("capabilities".to_string(), AttributeValue::M(cap_map));
        item.insert(
            "first_registered_at".to_string(),
            AttributeValue::S("2024-01-15T10:30:00Z".to_string()),
        );
        item.insert(
            "last_seen_at".to_string(),
            AttributeValue::S("2024-01-15T14:22:00Z".to_string()),
        );
        item.insert(
            "last_boot_id".to_string(),
            AttributeValue::S("7c9e6679-7425-40de-944b-e07fc1f90ae7".to_string()),
        );

        let device = item_to_device(&item).unwrap();

        assert_eq!(device.hardware_id, "AA:BB:CC:DD:EE:FF");
        assert_eq!(
            device.confirmation_id,
            "550e8400-e29b-41d4-a716-446655440000"
        );
        assert_eq!(device.friendly_name, Some("test-device".to_string()));
        assert_eq!(device.firmware_version, "1.0.16");
        assert_eq!(device.capabilities.sensors.len(), 1);
        assert_eq!(device.capabilities.features.len(), 1);
        assert_eq!(device.first_registered_at, "2024-01-15T10:30:00Z");
        assert_eq!(device.last_seen_at, "2024-01-15T14:22:00Z");
        assert_eq!(device.last_boot_id, "7c9e6679-7425-40de-944b-e07fc1f90ae7");
    }

    #[test]
    fn test_item_to_device_without_friendly_name() {
        let mut features_map = HashMap::new();
        features_map.insert("tft_display".to_string(), AttributeValue::Bool(true));

        let sensors_list = vec![AttributeValue::S("bme280".to_string())];

        let mut cap_map = HashMap::new();
        cap_map.insert("sensors".to_string(), AttributeValue::L(sensors_list));
        cap_map.insert("features".to_string(), AttributeValue::M(features_map));

        let mut item = HashMap::new();
        item.insert(
            "hardware_id".to_string(),
            AttributeValue::S("AA:BB:CC:DD:EE:FF".to_string()),
        );
        item.insert(
            "confirmation_id".to_string(),
            AttributeValue::S("550e8400-e29b-41d4-a716-446655440000".to_string()),
        );
        // No friendly_name
        item.insert(
            "firmware_version".to_string(),
            AttributeValue::S("1.0.16".to_string()),
        );
        item.insert("capabilities".to_string(), AttributeValue::M(cap_map));
        item.insert(
            "first_registered_at".to_string(),
            AttributeValue::S("2024-01-15T10:30:00Z".to_string()),
        );
        item.insert(
            "last_seen_at".to_string(),
            AttributeValue::S("2024-01-15T14:22:00Z".to_string()),
        );
        item.insert(
            "last_boot_id".to_string(),
            AttributeValue::S("7c9e6679-7425-40de-944b-e07fc1f90ae7".to_string()),
        );

        let device = item_to_device(&item).unwrap();

        assert_eq!(device.hardware_id, "AA:BB:CC:DD:EE:FF");
        assert_eq!(device.friendly_name, None);
    }

    #[test]
    fn test_item_to_device_missing_required_field() {
        let mut item = HashMap::new();
        item.insert(
            "hardware_id".to_string(),
            AttributeValue::S("AA:BB:CC:DD:EE:FF".to_string()),
        );
        // Missing confirmation_id

        let result = item_to_device(&item);
        assert!(result.is_err());

        match result {
            Err(DatabaseError::Serialization(msg)) => {
                assert!(msg.contains("confirmation_id"));
            }
            _ => panic!("Expected Serialization error"),
        }
    }

    #[test]
    fn test_cursor_to_exclusive_start_key() {
        use esp32_backend::shared::cursor::DeviceListCursor;

        let cursor = DeviceListCursor {
            hardware_id: "AA:BB:CC:DD:EE:FF".to_string(),
            gsi1sk: "2024-01-15T14:22:00Z".to_string(),
        };

        let key = cursor_to_exclusive_start_key(&cursor);

        assert_eq!(key.len(), 3);
        assert_eq!(
            key.get("hardware_id"),
            Some(&AttributeValue::S("AA:BB:CC:DD:EE:FF".to_string()))
        );
        assert_eq!(
            key.get("gsi1pk"),
            Some(&AttributeValue::S("devices".to_string()))
        );
        assert_eq!(
            key.get("gsi1sk"),
            Some(&AttributeValue::S("2024-01-15T14:22:00Z".to_string()))
        );
    }

    #[test]
    fn test_rfc3339_timestamp_sortability() {
        // Verify that RFC3339 timestamps sort correctly lexicographically
        let timestamps = vec![
            "2024-01-15T10:30:00Z",
            "2024-01-15T14:22:00Z",
            "2024-01-16T08:00:00Z",
            "2024-01-14T23:59:59Z",
        ];

        let mut sorted = timestamps.clone();
        sorted.sort();

        // Expected order: oldest to newest
        assert_eq!(sorted[0], "2024-01-14T23:59:59Z");
        assert_eq!(sorted[1], "2024-01-15T10:30:00Z");
        assert_eq!(sorted[2], "2024-01-15T14:22:00Z");
        assert_eq!(sorted[3], "2024-01-16T08:00:00Z");

        // Verify descending order (most recent first) for GSI query
        let mut descending = timestamps.clone();
        descending.sort_by(|a, b| b.cmp(a));

        assert_eq!(descending[0], "2024-01-16T08:00:00Z");
        assert_eq!(descending[1], "2024-01-15T14:22:00Z");
        assert_eq!(descending[2], "2024-01-15T10:30:00Z");
        assert_eq!(descending[3], "2024-01-14T23:59:59Z");
    }

    // Note: Integration tests for get_device, create_device, update_device_timestamps, and list_devices
    // require DynamoDB Local and are in the integration test suite
}
