use lambda_http::{Body, Request, Response};
use serde::{Deserialize, Serialize};
use tracing::{info, warn};

use crate::auth::validate_api_key;
use crate::error::ApiError;
use crate::repo::devices::{create_device, get_device, update_device_timestamps};
use esp32_backend::{
    validate_mac_address, validate_uuid_v4, Capabilities, Clock, Device, IdGenerator,
};

/// Request payload for device registration
///
/// This struct represents the JSON payload sent by ESP32 devices when registering
/// with the backend. It includes device identification, boot information, firmware
/// version, and device capabilities.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RegisterRequest {
    /// Hardware ID (MAC address) in format XX:XX:XX:XX:XX:XX
    pub hardware_id: String,

    /// Boot ID (UUID v4) generated on each device boot
    pub boot_id: String,

    /// Firmware version string (e.g., "1.0.16")
    pub firmware_version: String,

    /// Optional friendly name for the device
    #[serde(skip_serializing_if = "Option::is_none")]
    pub friendly_name: Option<String>,

    /// Device capabilities including sensors and features
    pub capabilities: Capabilities,
}

/// Response payload for device registration
///
/// This struct represents the JSON response returned to devices after successful
/// registration. It includes the registration status, confirmation ID, and timestamps.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RegisterResponse {
    /// Registration status ("registered" for both new and existing devices)
    pub status: String,

    /// Confirmation ID (UUID v4) - unique identifier for this device
    pub confirmation_id: String,

    /// Hardware ID (MAC address) echoed back from the request
    pub hardware_id: String,

    /// Timestamp when the registration was processed (RFC3339 format)
    pub registered_at: String,
}

impl RegisterResponse {
    /// Create a new RegisterResponse from a Device record
    ///
    /// This helper method constructs a response using the device's confirmation_id
    /// and current timestamp. It's used for both new registrations and re-registrations.
    pub fn from_device(device: &Device, registered_at: String) -> Self {
        Self {
            status: "registered".to_string(),
            confirmation_id: device.confirmation_id.clone(),
            hardware_id: device.hardware_id.clone(),
            registered_at,
        }
    }
}

/// Handle device registration requests
///
/// This function processes POST /register requests from ESP32 devices.
/// It validates the API key, parses the request body, checks if the device
/// already exists, and either creates a new device or updates an existing one.
///
/// # Arguments
/// * `event` - Lambda HTTP request event
/// * `request_id` - Request ID for logging and error responses
/// * `dynamodb_client` - DynamoDB client for database operations
/// * `devices_table` - Name of the devices table
/// * `api_keys_table` - Name of the API keys table
/// * `clock` - Clock implementation for timestamp generation
/// * `id_generator` - IdGenerator implementation for UUID generation
///
/// # Returns
/// * `Ok(Response)` - HTTP 200 with RegisterResponse on success
/// * `Err(ApiError)` - Authentication, validation, or database error
pub async fn handle_register(
    event: Request,
    request_id: &str,
    dynamodb_client: &aws_sdk_dynamodb::Client,
    devices_table: &str,
    api_keys_table: &str,
    clock: &dyn Clock,
    id_generator: &dyn IdGenerator,
) -> Result<Response<Body>, ApiError> {
    info!(request_id = %request_id, "Processing registration request");

    // Step 1: Extract and validate API key from X-API-Key header
    let api_key = event
        .headers()
        .get("x-api-key")
        .and_then(|v| v.to_str().ok())
        .ok_or(crate::error::AuthError::MissingKey)?;

    info!(request_id = %request_id, "Validating API key");
    validate_api_key(dynamodb_client, api_keys_table, api_key, clock).await?;

    // Step 2: Parse and validate request body
    let body_bytes = match event.body() {
        Body::Text(text) => text.as_bytes(),
        Body::Binary(bytes) => bytes.as_slice(),
        Body::Empty => {
            return Err(ApiError::Validation(
                crate::error::ValidationError::MissingField("request body".to_string()),
            ));
        }
    };

    let request: RegisterRequest = serde_json::from_slice(body_bytes).map_err(|e| {
        warn!(request_id = %request_id, error = %e, "Failed to parse request body");
        ApiError::Validation(crate::error::ValidationError::InvalidFormat(format!(
            "Invalid JSON: {}",
            e
        )))
    })?;

    info!(
        request_id = %request_id,
        hardware_id = %request.hardware_id,
        boot_id = %request.boot_id,
        firmware_version = %request.firmware_version,
        "Parsed registration request"
    );

    // Step 3: Validate request fields using shared validators
    validate_mac_address(&request.hardware_id).map_err(|e| {
        ApiError::Validation(crate::error::ValidationError::InvalidFormat(e.to_string()))
    })?;

    validate_uuid_v4(&request.boot_id).map_err(|e| {
        ApiError::Validation(crate::error::ValidationError::InvalidFormat(e.to_string()))
    })?;

    // Validate firmware_version is not empty
    if request.firmware_version.is_empty() {
        return Err(ApiError::Validation(
            crate::error::ValidationError::MissingField("firmware_version".to_string()),
        ));
    }

    // Step 4: Check if device exists
    info!(
        request_id = %request_id,
        hardware_id = %request.hardware_id,
        "Checking if device exists"
    );

    let existing_device = get_device(dynamodb_client, devices_table, &request.hardware_id).await?;

    let response = match existing_device {
        // Device exists - update timestamps and last_boot_id
        Some(device) => {
            info!(
                request_id = %request_id,
                hardware_id = %request.hardware_id,
                confirmation_id = %device.confirmation_id,
                "Device already registered, updating timestamps"
            );

            let now = clock.now_rfc3339();

            // Update last_seen_at and last_boot_id
            update_device_timestamps(
                dynamodb_client,
                devices_table,
                &request.hardware_id,
                &now,
                &request.boot_id,
            )
            .await?;

            // Return existing confirmation_id
            RegisterResponse::from_device(&device, now)
        }

        // Device does not exist - create new device
        None => {
            info!(
                request_id = %request_id,
                hardware_id = %request.hardware_id,
                "New device, creating record"
            );

            // Generate confirmation_id using IdGenerator trait
            let confirmation_id = id_generator.uuid_v4();

            info!(
                request_id = %request_id,
                hardware_id = %request.hardware_id,
                confirmation_id = %confirmation_id,
                "Generated confirmation_id"
            );

            let now = clock.now_rfc3339();

            // Create new device record
            let device = Device {
                hardware_id: request.hardware_id.clone(),
                confirmation_id: confirmation_id.clone(),
                friendly_name: request.friendly_name.clone(),
                firmware_version: request.firmware_version.clone(),
                capabilities: request.capabilities.clone(),
                first_registered_at: now.clone(),
                last_seen_at: now.clone(),
                last_boot_id: request.boot_id.clone(),
            };

            // Store device in DynamoDB
            create_device(dynamodb_client, devices_table, &device).await?;

            info!(
                request_id = %request_id,
                hardware_id = %request.hardware_id,
                confirmation_id = %confirmation_id,
                "Device created successfully"
            );

            RegisterResponse::from_device(&device, now)
        }
    };

    // Step 5: Return RegisterResponse
    let response_body = serde_json::to_string(&response)
        .map_err(|e| ApiError::Internal(format!("Failed to serialize response: {}", e)))?;

    info!(
        request_id = %request_id,
        hardware_id = %request.hardware_id,
        confirmation_id = %response.confirmation_id,
        "Registration completed successfully"
    );

    Response::builder()
        .status(200)
        .header("content-type", "application/json")
        .body(Body::from(response_body))
        .map_err(|e| ApiError::Internal(format!("Failed to build response: {}", e)))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    #[test]
    fn test_register_request_deserialization() {
        let json = r#"{
            "hardware_id": "AA:BB:CC:DD:EE:FF",
            "boot_id": "550e8400-e29b-41d4-a716-446655440000",
            "firmware_version": "1.0.16",
            "friendly_name": "test-device",
            "capabilities": {
                "sensors": ["bme280", "ds18b20"],
                "features": {
                    "tft_display": true,
                    "offline_buffering": false
                }
            }
        }"#;

        let request: RegisterRequest = serde_json::from_str(json).unwrap();

        assert_eq!(request.hardware_id, "AA:BB:CC:DD:EE:FF");
        assert_eq!(request.boot_id, "550e8400-e29b-41d4-a716-446655440000");
        assert_eq!(request.firmware_version, "1.0.16");
        assert_eq!(request.friendly_name, Some("test-device".to_string()));
        assert_eq!(request.capabilities.sensors.len(), 2);
        assert_eq!(request.capabilities.features.len(), 2);
    }

    #[test]
    fn test_register_request_deserialization_without_friendly_name() {
        let json = r#"{
            "hardware_id": "AA:BB:CC:DD:EE:FF",
            "boot_id": "550e8400-e29b-41d4-a716-446655440000",
            "firmware_version": "1.0.16",
            "capabilities": {
                "sensors": ["bme280"],
                "features": {}
            }
        }"#;

        let request: RegisterRequest = serde_json::from_str(json).unwrap();

        assert_eq!(request.hardware_id, "AA:BB:CC:DD:EE:FF");
        assert_eq!(request.friendly_name, None);
    }

    #[test]
    fn test_register_request_serialization() {
        let mut features = HashMap::new();
        features.insert("tft_display".to_string(), true);

        let request = RegisterRequest {
            hardware_id: "AA:BB:CC:DD:EE:FF".to_string(),
            boot_id: "550e8400-e29b-41d4-a716-446655440000".to_string(),
            firmware_version: "1.0.16".to_string(),
            friendly_name: Some("test-device".to_string()),
            capabilities: Capabilities {
                sensors: vec!["bme280".to_string()],
                features,
            },
        };

        let json = serde_json::to_string(&request).unwrap();

        assert!(json.contains("AA:BB:CC:DD:EE:FF"));
        assert!(json.contains("550e8400-e29b-41d4-a716-446655440000"));
        assert!(json.contains("1.0.16"));
        assert!(json.contains("test-device"));
        assert!(json.contains("bme280"));
        assert!(json.contains("tft_display"));
    }

    #[test]
    fn test_register_response_serialization() {
        let response = RegisterResponse {
            status: "registered".to_string(),
            confirmation_id: "7c9e6679-7425-40de-944b-e07fc1f90ae7".to_string(),
            hardware_id: "AA:BB:CC:DD:EE:FF".to_string(),
            registered_at: "2024-01-15T10:30:00Z".to_string(),
        };

        let json = serde_json::to_string(&response).unwrap();

        assert!(json.contains("registered"));
        assert!(json.contains("7c9e6679-7425-40de-944b-e07fc1f90ae7"));
        assert!(json.contains("AA:BB:CC:DD:EE:FF"));
        assert!(json.contains("2024-01-15T10:30:00Z"));
    }

    #[test]
    fn test_register_response_deserialization() {
        let json = r#"{
            "status": "registered",
            "confirmation_id": "7c9e6679-7425-40de-944b-e07fc1f90ae7",
            "hardware_id": "AA:BB:CC:DD:EE:FF",
            "registered_at": "2024-01-15T10:30:00Z"
        }"#;

        let response: RegisterResponse = serde_json::from_str(json).unwrap();

        assert_eq!(response.status, "registered");
        assert_eq!(
            response.confirmation_id,
            "7c9e6679-7425-40de-944b-e07fc1f90ae7"
        );
        assert_eq!(response.hardware_id, "AA:BB:CC:DD:EE:FF");
        assert_eq!(response.registered_at, "2024-01-15T10:30:00Z");
    }

    #[test]
    fn test_register_response_from_device() {
        let mut features = HashMap::new();
        features.insert("tft_display".to_string(), true);

        let device = Device {
            hardware_id: "AA:BB:CC:DD:EE:FF".to_string(),
            confirmation_id: "7c9e6679-7425-40de-944b-e07fc1f90ae7".to_string(),
            friendly_name: Some("test-device".to_string()),
            firmware_version: "1.0.16".to_string(),
            capabilities: Capabilities {
                sensors: vec!["bme280".to_string()],
                features,
            },
            first_registered_at: "2024-01-15T10:30:00Z".to_string(),
            last_seen_at: "2024-01-15T14:22:00Z".to_string(),
            last_boot_id: "550e8400-e29b-41d4-a716-446655440000".to_string(),
        };

        let response = RegisterResponse::from_device(&device, "2024-01-15T14:22:00Z".to_string());

        assert_eq!(response.status, "registered");
        assert_eq!(
            response.confirmation_id,
            "7c9e6679-7425-40de-944b-e07fc1f90ae7"
        );
        assert_eq!(response.hardware_id, "AA:BB:CC:DD:EE:FF");
        assert_eq!(response.registered_at, "2024-01-15T14:22:00Z");
    }

    #[test]
    fn test_register_request_missing_required_field() {
        // Missing hardware_id
        let json = r#"{
            "boot_id": "550e8400-e29b-41d4-a716-446655440000",
            "firmware_version": "1.0.16",
            "capabilities": {
                "sensors": [],
                "features": {}
            }
        }"#;

        let result: Result<RegisterRequest, _> = serde_json::from_str(json);
        assert!(result.is_err());
    }

    #[test]
    fn test_register_request_missing_capabilities() {
        // Missing capabilities
        let json = r#"{
            "hardware_id": "AA:BB:CC:DD:EE:FF",
            "boot_id": "550e8400-e29b-41d4-a716-446655440000",
            "firmware_version": "1.0.16"
        }"#;

        let result: Result<RegisterRequest, _> = serde_json::from_str(json);
        assert!(result.is_err());
    }

    #[test]
    fn test_register_response_serialization_omits_nothing() {
        // All fields should be present in the response
        let response = RegisterResponse {
            status: "registered".to_string(),
            confirmation_id: "7c9e6679-7425-40de-944b-e07fc1f90ae7".to_string(),
            hardware_id: "AA:BB:CC:DD:EE:FF".to_string(),
            registered_at: "2024-01-15T10:30:00Z".to_string(),
        };

        let json = serde_json::to_value(&response).unwrap();

        // Verify all fields are present
        assert!(json.get("status").is_some());
        assert!(json.get("confirmation_id").is_some());
        assert!(json.get("hardware_id").is_some());
        assert!(json.get("registered_at").is_some());
    }
}
