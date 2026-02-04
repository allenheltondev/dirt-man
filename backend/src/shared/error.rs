use serde::{Deserialize, Serialize};

/// Standard error response payload
/// Contains stable machine-readable error code, human-readable message, and request ID
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ErrorResponse {
    /// Stable machine-readable error code (e.g., "INVALID_MAC", "UNAUTHORIZED")
    pub error: String,

    /// Human-readable error message
    pub message: String,

    /// Request ID for tracing and debugging
    pub request_id: String,
}

impl ErrorResponse {
    /// Create a new error response
    pub fn new(
        error: impl Into<String>,
        message: impl Into<String>,
        request_id: impl Into<String>,
    ) -> Self {
        Self {
            error: error.into(),
            message: message.into(),
            request_id: request_id.into(),
        }
    }

    /// Convert to JSON string
    pub fn to_json(&self) -> Result<String, serde_json::Error> {
        serde_json::to_string(self)
    }

    /// Convert to pretty JSON string
    pub fn to_json_pretty(&self) -> Result<String, serde_json::Error> {
        serde_json::to_string_pretty(self)
    }
}

/// Common error codes used across the API
pub mod error_codes {
    // Authentication errors
    pub const MISSING_API_KEY: &str = "MISSING_API_KEY";
    pub const INVALID_API_KEY: &str = "INVALID_API_KEY";
    pub const KEY_REVOKED: &str = "KEY_REVOKED";
    pub const MISSING_TOKEN: &str = "MISSING_TOKEN";
    pub const INVALID_TOKEN: &str = "INVALID_TOKEN";
    pub const UNAUTHORIZED: &str = "UNAUTHORIZED";

    // Validation errors
    pub const MISSING_FIELD: &str = "MISSING_FIELD";
    pub const INVALID_FORMAT: &str = "INVALID_FORMAT";
    pub const INVALID_VALUE: &str = "INVALID_VALUE";
    pub const INVALID_MAC: &str = "INVALID_MAC";
    pub const INVALID_UUID: &str = "INVALID_UUID";
    pub const INVALID_TIMESTAMP: &str = "INVALID_TIMESTAMP";
    pub const INVALID_BATCH_ID: &str = "INVALID_BATCH_ID";
    pub const BATCH_SIZE_EXCEEDED: &str = "BATCH_SIZE_EXCEEDED";

    // Not found errors
    pub const DEVICE_NOT_FOUND: &str = "DEVICE_NOT_FOUND";
    pub const NO_READINGS: &str = "NO_READINGS";
    pub const API_KEY_NOT_FOUND: &str = "API_KEY_NOT_FOUND";

    // Database errors
    pub const DATABASE_ERROR: &str = "DATABASE_ERROR";

    // Internal errors
    pub const INTERNAL_ERROR: &str = "INTERNAL_ERROR";
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_error_response_creation() {
        let error = ErrorResponse::new(
            "INVALID_MAC",
            "MAC address must be in format XX:XX:XX:XX:XX:XX",
            "req-123",
        );

        assert_eq!(error.error, "INVALID_MAC");
        assert_eq!(
            error.message,
            "MAC address must be in format XX:XX:XX:XX:XX:XX"
        );
        assert_eq!(error.request_id, "req-123");
    }

    #[test]
    fn test_error_response_to_json() {
        let error = ErrorResponse::new("UNAUTHORIZED", "Invalid API key", "req-456");

        let json = error.to_json().unwrap();
        assert!(json.contains("UNAUTHORIZED"));
        assert!(json.contains("Invalid API key"));
        assert!(json.contains("req-456"));

        // Verify it can be deserialized back
        let deserialized: ErrorResponse = serde_json::from_str(&json).unwrap();
        assert_eq!(deserialized.error, error.error);
        assert_eq!(deserialized.message, error.message);
        assert_eq!(deserialized.request_id, error.request_id);
    }

    #[test]
    fn test_error_codes_constants() {
        assert_eq!(error_codes::MISSING_API_KEY, "MISSING_API_KEY");
        assert_eq!(error_codes::INVALID_MAC, "INVALID_MAC");
        assert_eq!(error_codes::DEVICE_NOT_FOUND, "DEVICE_NOT_FOUND");
    }
}
