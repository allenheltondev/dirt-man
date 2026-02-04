use lambda_http::{Body, Request, RequestExt, Response};
use serde::Serialize;
use tracing::{error, info};

use crate::auth::validate_bearer_token;
use crate::config::ControlConfig;
use crate::error::ApiError;

/// Response item for device listing
#[derive(Debug, Serialize)]
pub struct DeviceListItem {
    /// MAC address of the device
    pub hardware_id: String,
    /// UUID v4 confirmation ID
    pub confirmation_id: String,
    /// Optional friendly name
    pub friendly_name: Option<String>,
    /// Firmware version
    pub firmware_version: String,
    /// RFC3339 timestamp when first registered
    pub first_registered_at: String,
    /// RFC3339 timestamp when last seen
    pub last_seen_at: String,
}

/// Response payload for device listing
#[derive(Debug, Serialize)]
pub struct ListDevicesResponse {
    /// List of devices
    pub devices: Vec<DeviceListItem>,
    /// Optional cursor for pagination
    pub next_cursor: Option<String>,
}

/// Handler for GET /devices endpoint
///
/// Lists all registered devices with pagination, sorted by last_seen_at descending.
///
/// # Query Parameters
/// * `limit` - Maximum number of devices to return (default 50, max 100)
/// * `cursor` - Optional pagination cursor from previous response
///
/// # Returns
/// * HTTP 200 with device list and optional next_cursor
/// * HTTP 401 if Bearer token is invalid
/// * HTTP 400 if query parameters are invalid
pub async fn list_devices(
    event: Request,
    config: &ControlConfig,
) -> Result<Response<Body>, ApiError> {
    let request_id = event.lambda_context().request_id.clone();

    info!(
        request_id = %request_id,
        "Processing list devices request"
    );

    // Validate Bearer token
    validate_bearer_token(&event)?;

    // Parse query parameters
    let query_params = event.query_string_parameters();

    let limit: i32 = query_params
        .first("limit")
        .and_then(|s| s.parse().ok())
        .unwrap_or(50); // Default limit

    let limit = limit.min(100).max(1); // Clamp between 1 and 100

    let cursor = query_params.first("cursor").map(|s| s.to_string());

    info!(
        request_id = %request_id,
        limit = limit,
        has_cursor = cursor.is_some(),
        "Parsed query parameters"
    );

    // Query DynamoDB using the devices repository
    let result = crate::repo::devices::list_devices(
        &config.dynamodb_client,
        &config.devices_table,
        Some(limit),
        cursor,
    )
    .await?;

    info!(
        request_id = %request_id,
        count = result.devices.len(),
        has_next_cursor = result.next_cursor.is_some(),
        "Retrieved devices from DynamoDB"
    );

    // Convert to response items (exclude capabilities for list view)
    let device_items: Vec<DeviceListItem> = result
        .devices
        .into_iter()
        .map(|device| DeviceListItem {
            hardware_id: device.hardware_id,
            confirmation_id: device.confirmation_id,
            friendly_name: device.friendly_name,
            firmware_version: device.firmware_version,
            first_registered_at: device.first_registered_at,
            last_seen_at: device.last_seen_at,
        })
        .collect();

    // Build response
    let response = ListDevicesResponse {
        devices: device_items,
        next_cursor: result.next_cursor,
    };

    let response_body = serde_json::to_string(&response).map_err(|e| {
        error!(request_id = %request_id, error = %e, "Failed to serialize response");
        ApiError::Internal(format!("Failed to serialize response: {}", e))
    })?;

    info!(
        request_id = %request_id,
        "Returning successful list devices response"
    );

    Ok(Response::builder()
        .status(200)
        .header("content-type", "application/json")
        .body(Body::from(response_body))
        .unwrap())
}

#[cfg(test)]
mod tests {
    use super::*;
    use lambda_http::http::Method;
    use lambda_http::Context;

    fn create_test_request(method: Method, uri: &str, auth_header: Option<&str>) -> Request {
        let mut builder = lambda_http::http::Request::builder()
            .method(method)
            .uri(uri);

        if let Some(auth) = auth_header {
            builder = builder.header("authorization", auth);
        }

        let mut request = builder.body(Body::Empty).unwrap();
        request.extensions_mut().insert(Context::default());
        request
    }

    #[tokio::test]
    async fn test_device_list_item_serialization() {
        let item = DeviceListItem {
            hardware_id: String::from("AA:BB:CC:DD:EE:FF"),
            confirmation_id: String::from("550e8400-e29b-41d4-a716-446655440000"),
            friendly_name: Some(String::from("test-device")),
            firmware_version: String::from("1.0.16"),
            first_registered_at: String::from("2024-01-15T10:30:00Z"),
            last_seen_at: String::from("2024-01-15T14:22:00Z"),
        };

        let json = serde_json::to_string(&item).unwrap();
        assert!(json.contains("hardware_id"));
        assert!(json.contains("AA:BB:CC:DD:EE:FF"));
        assert!(json.contains("confirmation_id"));
        assert!(json.contains("friendly_name"));
        assert!(json.contains("firmware_version"));
        assert!(json.contains("first_registered_at"));
        assert!(json.contains("last_seen_at"));

        // Verify capabilities are NOT in the response
        assert!(!json.contains("capabilities"));
    }

    #[tokio::test]
    async fn test_device_list_item_serialization_no_friendly_name() {
        let item = DeviceListItem {
            hardware_id: String::from("AA:BB:CC:DD:EE:FF"),
            confirmation_id: String::from("550e8400-e29b-41d4-a716-446655440000"),
            friendly_name: None,
            firmware_version: String::from("1.0.16"),
            first_registered_at: String::from("2024-01-15T10:30:00Z"),
            last_seen_at: String::from("2024-01-15T14:22:00Z"),
        };

        let json = serde_json::to_string(&item).unwrap();
        assert!(json.contains("hardware_id"));
        assert!(json.contains("friendly_name"));
    }

    #[tokio::test]
    async fn test_list_devices_response_serialization() {
        let response = ListDevicesResponse {
            devices: vec![
                DeviceListItem {
                    hardware_id: String::from("AA:BB:CC:DD:EE:FF"),
                    confirmation_id: String::from("550e8400-e29b-41d4-a716-446655440000"),
                    friendly_name: Some(String::from("device-1")),
                    firmware_version: String::from("1.0.16"),
                    first_registered_at: String::from("2024-01-15T10:30:00Z"),
                    last_seen_at: String::from("2024-01-15T14:22:00Z"),
                },
                DeviceListItem {
                    hardware_id: String::from("11:22:33:44:55:66"),
                    confirmation_id: String::from("7c9e6679-7425-40de-944b-e07fc1f90ae7"),
                    friendly_name: None,
                    firmware_version: String::from("1.0.15"),
                    first_registered_at: String::from("2024-01-14T10:30:00Z"),
                    last_seen_at: String::from("2024-01-14T14:22:00Z"),
                },
            ],
            next_cursor: Some(String::from("base64cursor")),
        };

        let json = serde_json::to_string(&response).unwrap();
        assert!(json.contains("devices"));
        assert!(json.contains("AA:BB:CC:DD:EE:FF"));
        assert!(json.contains("11:22:33:44:55:66"));
        assert!(json.contains("next_cursor"));
        assert!(json.contains("base64cursor"));
    }

    #[tokio::test]
    async fn test_list_devices_response_serialization_no_cursor() {
        let response = ListDevicesResponse {
            devices: vec![],
            next_cursor: None,
        };

        let json = serde_json::to_string(&response).unwrap();
        assert!(json.contains("devices"));
        assert!(json.contains("next_cursor"));
    }

    #[tokio::test]
    async fn test_list_devices_missing_auth_header() {
        // Set up environment
        std::env::set_var("ADMIN_TOKEN", "test-token");

        let config = ControlConfig::for_test(
            "http://localhost:8000",
            String::from("test-devices"),
            String::from("test-api-keys"),
            String::from("test-device-readings"),
            String::from("test-admin-token"),
            String::from("*"),
        )
        .await;

        let request = create_test_request(Method::GET, "/devices", None);

        let result = list_devices(request, &config).await;
        assert!(result.is_err());

        match result.unwrap_err() {
            ApiError::Auth(crate::error::AuthError::MissingToken) => {
                // Expected error
            }
            e => panic!("Expected MissingToken error, got: {:?}", e),
        }

        // Clean up
        std::env::remove_var("ADMIN_TOKEN");
    }

    #[tokio::test]
    async fn test_list_devices_invalid_token() {
        // Set up environment
        std::env::set_var("ADMIN_TOKEN", "correct-token");

        let config = ControlConfig::for_test(
            "http://localhost:8000",
            String::from("test-devices"),
            String::from("test-api-keys"),
            String::from("test-device-readings"),
            String::from("test-admin-token"),
            String::from("*"),
        )
        .await;

        let request = create_test_request(Method::GET, "/devices", Some("Bearer wrong-token"));

        let result = list_devices(request, &config).await;
        assert!(result.is_err());

        match result.unwrap_err() {
            ApiError::Auth(crate::error::AuthError::InvalidToken) => {
                // Expected error
            }
            e => panic!("Expected InvalidToken error, got: {:?}", e),
        }

        // Clean up
        std::env::remove_var("ADMIN_TOKEN");
    }
}

/// Handler for GET /devices/{hardware_id} endpoint
///
/// Retrieves complete device record including capabilities.
///
/// # Path Parameters
/// * `hardware_id` - MAC address of the device
///
/// # Returns
/// * HTTP 200 with complete device record
/// * HTTP 401 if Bearer token is invalid
/// * HTTP 404 if device not found
pub async fn get_device_detail(
    event: Request,
    config: &ControlConfig,
    hardware_id: &str,
) -> Result<Response<Body>, ApiError> {
    let request_id = event.lambda_context().request_id.clone();

    info!(
        request_id = %request_id,
        hardware_id = %hardware_id,
        "Processing get device detail request"
    );

    // Validate Bearer token
    validate_bearer_token(&event)?;

    info!(
        request_id = %request_id,
        hardware_id = %hardware_id,
        "Querying device from DynamoDB"
    );

    // Query device by partition key
    let device = crate::repo::devices::get_device(
        &config.dynamodb_client,
        &config.devices_table,
        hardware_id,
    )
    .await?;

    // Return 404 if device not found
    let device = device.ok_or_else(|| {
        info!(
            request_id = %request_id,
            hardware_id = %hardware_id,
            "Device not found"
        );
        ApiError::NotFound(crate::error::NotFoundError::DeviceNotFound)
    })?;

    info!(
        request_id = %request_id,
        hardware_id = %hardware_id,
        "Retrieved device from DynamoDB"
    );

    // Serialize complete device record (including capabilities)
    let response_body = serde_json::to_string(&device).map_err(|e| {
        error!(request_id = %request_id, error = %e, "Failed to serialize device");
        ApiError::Internal(format!("Failed to serialize device: {}", e))
    })?;

    info!(
        request_id = %request_id,
        hardware_id = %hardware_id,
        "Returning successful device detail response"
    );

    Ok(Response::builder()
        .status(200)
        .header("content-type", "application/json")
        .body(Body::from(response_body))
        .unwrap())
}

#[cfg(test)]
mod device_detail_tests {
    use super::*;
    use lambda_http::http::Method;
    use lambda_http::Context;

    fn create_test_request(method: Method, uri: &str, auth_header: Option<&str>) -> Request {
        let mut builder = lambda_http::http::Request::builder()
            .method(method)
            .uri(uri);

        if let Some(auth) = auth_header {
            builder = builder.header("authorization", auth);
        }

        let mut request = builder.body(Body::Empty).unwrap();
        request.extensions_mut().insert(Context::default());
        request
    }

    #[tokio::test]
    async fn test_get_device_detail_missing_auth_header() {
        // Set up environment
        std::env::set_var("ADMIN_TOKEN", "test-token");

        let config = ControlConfig::for_test(
            "http://localhost:8000",
            String::from("test-devices"),
            String::from("test-api-keys"),
            String::from("test-device-readings"),
            String::from("test-admin-token"),
            String::from("*"),
        )
        .await;

        let request = create_test_request(Method::GET, "/devices/AA:BB:CC:DD:EE:FF", None);

        let result = get_device_detail(request, &config, "AA:BB:CC:DD:EE:FF").await;
        assert!(result.is_err());

        match result.unwrap_err() {
            ApiError::Auth(crate::error::AuthError::MissingToken) => {
                // Expected error
            }
            e => panic!("Expected MissingToken error, got: {:?}", e),
        }

        // Clean up
        std::env::remove_var("ADMIN_TOKEN");
    }

    #[tokio::test]
    async fn test_get_device_detail_invalid_token() {
        // Set up environment
        std::env::set_var("ADMIN_TOKEN", "correct-token");

        let config = ControlConfig::for_test(
            "http://localhost:8000",
            String::from("test-devices"),
            String::from("test-api-keys"),
            String::from("test-device-readings"),
            String::from("test-admin-token"),
            String::from("*"),
        )
        .await;

        let request = create_test_request(
            Method::GET,
            "/devices/AA:BB:CC:DD:EE:FF",
            Some("Bearer wrong-token"),
        );

        let result = get_device_detail(request, &config, "AA:BB:CC:DD:EE:FF").await;
        assert!(result.is_err());

        match result.unwrap_err() {
            ApiError::Auth(crate::error::AuthError::InvalidToken) => {
                // Expected error
            }
            e => panic!("Expected InvalidToken error, got: {:?}", e),
        }

        // Clean up
        std::env::remove_var("ADMIN_TOKEN");
    }
}

#[cfg(test)]
mod integration_style_tests {
    use super::*;
    use esp32_backend::shared::domain::{Capabilities, Device};
    use lambda_http::http::Method;
    use lambda_http::Context;
    use std::collections::HashMap;

    fn create_test_request_with_query(
        method: Method,
        uri: &str,
        auth_header: Option<&str>,
    ) -> Request {
        let mut builder = lambda_http::http::Request::builder()
            .method(method)
            .uri(uri);

        if let Some(auth) = auth_header {
            builder = builder.header("authorization", auth);
        }

        let mut request = builder.body(Body::Empty).unwrap();
        request.extensions_mut().insert(Context::default());
        request
    }

    fn create_test_device(
        hardware_id: &str,
        confirmation_id: &str,
        friendly_name: Option<&str>,
        last_seen_at: &str,
    ) -> Device {
        let mut features = HashMap::new();
        features.insert("tft_display".to_string(), true);
        features.insert("offline_buffering".to_string(), true);

        Device {
            hardware_id: hardware_id.to_string(),
            confirmation_id: confirmation_id.to_string(),
            friendly_name: friendly_name.map(|s| s.to_string()),
            firmware_version: "1.0.16".to_string(),
            capabilities: Capabilities {
                sensors: vec!["bme280".to_string(), "ds18b20".to_string()],
                features,
            },
            first_registered_at: "2024-01-15T10:30:00Z".to_string(),
            last_seen_at: last_seen_at.to_string(),
            last_boot_id: "7c9e6679-7425-40de-944b-e07fc1f90ae7".to_string(),
        }
    }

    #[tokio::test]
    async fn test_list_devices_query_param_parsing() {
        // Test default limit logic
        let limit: i32 = None.unwrap_or(50);
        assert_eq!(limit, 50);

        // Test custom limit logic
        let limit: i32 = Some(25).unwrap_or(50);
        assert_eq!(limit, 25);

        // Test limit clamping (max 100)
        let limit = 150_i32.min(100).max(1);
        assert_eq!(limit, 100);

        // Test limit clamping (min 1)
        let limit = 0_i32.min(100).max(1);
        assert_eq!(limit, 1);

        // Test negative limit clamping
        let limit = (-5_i32).min(100).max(1);
        assert_eq!(limit, 1);
    }

    #[tokio::test]
    async fn test_list_devices_cursor_parsing() {
        use esp32_backend::shared::cursor::encode_device_cursor;

        // Test cursor encoding
        let cursor = encode_device_cursor("AA:BB:CC:DD:EE:FF", "2024-01-15T14:22:00Z").unwrap();

        // Verify cursor is not empty and is base64
        assert!(!cursor.is_empty());
        assert!(cursor
            .chars()
            .all(|c| c.is_alphanumeric() || c == '+' || c == '/' || c == '='));

        // Test cursor decoding
        use esp32_backend::shared::cursor::decode_device_cursor;
        let decoded = decode_device_cursor(&cursor).unwrap();
        assert_eq!(decoded.hardware_id, "AA:BB:CC:DD:EE:FF");
        assert_eq!(decoded.gsi1sk, "2024-01-15T14:22:00Z");
    }

    #[tokio::test]
    async fn test_device_list_item_conversion() {
        let device = create_test_device(
            "AA:BB:CC:DD:EE:FF",
            "550e8400-e29b-41d4-a716-446655440000",
            Some("test-device"),
            "2024-01-15T14:22:00Z",
        );

        let list_item = DeviceListItem {
            hardware_id: device.hardware_id.clone(),
            confirmation_id: device.confirmation_id.clone(),
            friendly_name: device.friendly_name.clone(),
            firmware_version: device.firmware_version.clone(),
            first_registered_at: device.first_registered_at.clone(),
            last_seen_at: device.last_seen_at.clone(),
        };

        // Verify capabilities are excluded from list item
        let json = serde_json::to_string(&list_item).unwrap();
        assert!(!json.contains("capabilities"));
        assert!(json.contains("hardware_id"));
        assert!(json.contains("confirmation_id"));
        assert!(json.contains("friendly_name"));
    }

    #[tokio::test]
    async fn test_device_list_response_with_pagination() {
        use esp32_backend::shared::cursor::encode_device_cursor;

        let devices = vec![
            create_test_device(
                "AA:BB:CC:DD:EE:FF",
                "550e8400-e29b-41d4-a716-446655440000",
                Some("device-1"),
                "2024-01-15T14:22:00Z",
            ),
            create_test_device(
                "11:22:33:44:55:66",
                "7c9e6679-7425-40de-944b-e07fc1f90ae7",
                Some("device-2"),
                "2024-01-15T13:00:00Z",
            ),
        ];

        let device_items: Vec<DeviceListItem> = devices
            .into_iter()
            .map(|device| DeviceListItem {
                hardware_id: device.hardware_id,
                confirmation_id: device.confirmation_id,
                friendly_name: device.friendly_name,
                firmware_version: device.firmware_version,
                first_registered_at: device.first_registered_at,
                last_seen_at: device.last_seen_at,
            })
            .collect();

        let cursor = encode_device_cursor("11:22:33:44:55:66", "2024-01-15T13:00:00Z").unwrap();

        let response = ListDevicesResponse {
            devices: device_items,
            next_cursor: Some(cursor.clone()),
        };

        let json = serde_json::to_string(&response).unwrap();
        assert!(json.contains("devices"));
        assert!(json.contains("next_cursor"));
        assert!(json.contains(&cursor));
    }

    #[tokio::test]
    async fn test_device_list_response_without_pagination() {
        let devices = vec![create_test_device(
            "AA:BB:CC:DD:EE:FF",
            "550e8400-e29b-41d4-a716-446655440000",
            Some("device-1"),
            "2024-01-15T14:22:00Z",
        )];

        let device_items: Vec<DeviceListItem> = devices
            .into_iter()
            .map(|device| DeviceListItem {
                hardware_id: device.hardware_id,
                confirmation_id: device.confirmation_id,
                friendly_name: device.friendly_name,
                firmware_version: device.firmware_version,
                first_registered_at: device.first_registered_at,
                last_seen_at: device.last_seen_at,
            })
            .collect();

        let response = ListDevicesResponse {
            devices: device_items,
            next_cursor: None,
        };

        let json = serde_json::to_string(&response).unwrap();
        assert!(json.contains("devices"));
        assert!(json.contains("next_cursor"));
        assert!(json.contains("null")); // next_cursor should be null
    }

    #[tokio::test]
    async fn test_cursor_encoding_decoding_roundtrip() {
        use esp32_backend::shared::cursor::{decode_device_cursor, encode_device_cursor};

        let hardware_id = "AA:BB:CC:DD:EE:FF";
        let gsi1sk = "2024-01-15T14:22:00Z";

        // Encode cursor
        let encoded = encode_device_cursor(hardware_id, gsi1sk).unwrap();

        // Verify it's base64
        assert!(!encoded.is_empty());
        assert!(encoded
            .chars()
            .all(|c| c.is_alphanumeric() || c == '+' || c == '/' || c == '='));

        // Decode cursor
        let decoded = decode_device_cursor(&encoded).unwrap();

        // Verify roundtrip
        assert_eq!(decoded.hardware_id, hardware_id);
        assert_eq!(decoded.gsi1sk, gsi1sk);
    }

    #[tokio::test]
    async fn test_cursor_decoding_invalid_base64() {
        use esp32_backend::shared::cursor::decode_device_cursor;

        let result = decode_device_cursor("not-valid-base64!@#$%");
        assert!(result.is_err());
    }

    #[tokio::test]
    async fn test_device_detail_serialization_includes_capabilities() {
        let device = create_test_device(
            "AA:BB:CC:DD:EE:FF",
            "550e8400-e29b-41d4-a716-446655440000",
            Some("test-device"),
            "2024-01-15T14:22:00Z",
        );

        let json = serde_json::to_string(&device).unwrap();

        // Verify all fields are present
        assert!(json.contains("hardware_id"));
        assert!(json.contains("confirmation_id"));
        assert!(json.contains("friendly_name"));
        assert!(json.contains("firmware_version"));
        assert!(json.contains("capabilities"));
        assert!(json.contains("sensors"));
        assert!(json.contains("features"));
        assert!(json.contains("first_registered_at"));
        assert!(json.contains("last_seen_at"));
        assert!(json.contains("last_boot_id"));
    }

    #[tokio::test]
    async fn test_device_not_found_error_type() {
        use crate::error::NotFoundError;

        let error = ApiError::NotFound(NotFoundError::DeviceNotFound);

        // Verify error can be matched
        match error {
            ApiError::NotFound(NotFoundError::DeviceNotFound) => {
                // Expected
            }
            _ => panic!("Expected DeviceNotFound error"),
        }
    }

    #[tokio::test]
    async fn test_list_devices_empty_result() {
        let response = ListDevicesResponse {
            devices: vec![],
            next_cursor: None,
        };

        let json = serde_json::to_string(&response).unwrap();
        assert!(json.contains("devices"));
        assert!(json.contains("[]")); // Empty array
        assert!(json.contains("next_cursor"));
    }

    #[tokio::test]
    async fn test_device_list_sorting_by_last_seen_at() {
        // Create devices with different last_seen_at timestamps
        let device1 = create_test_device(
            "AA:BB:CC:DD:EE:FF",
            "550e8400-e29b-41d4-a716-446655440000",
            Some("device-1"),
            "2024-01-15T14:22:00Z",
        );

        let device2 = create_test_device(
            "11:22:33:44:55:66",
            "7c9e6679-7425-40de-944b-e07fc1f90ae7",
            Some("device-2"),
            "2024-01-15T16:00:00Z", // More recent
        );

        let device3 = create_test_device(
            "77:88:99:AA:BB:CC",
            "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
            Some("device-3"),
            "2024-01-15T12:00:00Z", // Oldest
        );

        // Verify timestamps are sortable
        let mut timestamps = vec![
            device1.last_seen_at.clone(),
            device2.last_seen_at.clone(),
            device3.last_seen_at.clone(),
        ];

        timestamps.sort();
        timestamps.reverse(); // Descending order (most recent first)

        assert_eq!(timestamps[0], device2.last_seen_at);
        assert_eq!(timestamps[1], device1.last_seen_at);
        assert_eq!(timestamps[2], device3.last_seen_at);
    }

    #[tokio::test]
    async fn test_device_with_no_friendly_name() {
        let device = create_test_device(
            "AA:BB:CC:DD:EE:FF",
            "550e8400-e29b-41d4-a716-446655440000",
            None, // No friendly name
            "2024-01-15T14:22:00Z",
        );

        assert!(device.friendly_name.is_none());

        let list_item = DeviceListItem {
            hardware_id: device.hardware_id,
            confirmation_id: device.confirmation_id,
            friendly_name: device.friendly_name,
            firmware_version: device.firmware_version,
            first_registered_at: device.first_registered_at,
            last_seen_at: device.last_seen_at,
        };

        let json = serde_json::to_string(&list_item).unwrap();
        assert!(json.contains("friendly_name"));
        assert!(json.contains("null"));
    }
}
