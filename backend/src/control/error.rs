use lambda_http::{Body, Response};
use thiserror::Error;

use esp32_backend::shared::error::{error_codes, ErrorResponse};

/// Main error type for the Control Plane API
#[derive(Debug, Error)]
pub enum ApiError {
    #[error("Authentication error: {0}")]
    Auth(#[from] AuthError),

    #[error("Validation error: {0}")]
    Validation(#[from] ValidationError),

    #[error("Not found error: {0}")]
    NotFound(#[from] NotFoundError),

    #[error("Database error: {0}")]
    Database(#[from] DatabaseError),

    #[error("Internal error: {0}")]
    Internal(String),
}

/// Authentication-specific errors
#[derive(Debug, Error)]
pub enum AuthError {
    #[error("Authorization header is missing")]
    MissingToken,

    #[error("Bearer token is invalid")]
    InvalidToken,

    #[error("Authorization header format is invalid")]
    InvalidFormat,

    #[error("Admin token configuration error")]
    ConfigError,
}

/// Validation-specific errors
#[derive(Debug, Error)]
pub enum ValidationError {
    #[error("Required field missing: {0}")]
    MissingField(String),

    #[error("Invalid format for field: {0}")]
    InvalidFormat(String),

    #[error("Invalid value for field: {0}")]
    InvalidValue(String),

    #[error("MAC address must be in format XX:XX:XX:XX:XX:XX")]
    InvalidMac,

    #[error("UUID must be valid v4 format")]
    InvalidUuid,

    #[error("Timestamp must be valid epoch milliseconds")]
    InvalidTimestamp,

    #[error("Invalid request body: {0}")]
    InvalidBody(String),

    #[error("Invalid cursor format")]
    InvalidCursor,

    #[error("Invalid pagination parameters")]
    InvalidPagination,
}

/// Not found errors
#[derive(Debug, Error)]
pub enum NotFoundError {
    #[error("Device not found")]
    DeviceNotFound,

    #[error("Device exists but has no readings")]
    NoReadings,

    #[error("API key not found")]
    ApiKeyNotFound,

    #[error("Resource not found")]
    ResourceNotFound,
}

/// Database-specific errors
#[derive(Debug, Error)]
pub enum DatabaseError {
    #[error("DynamoDB error: {0}")]
    DynamoDb(String),

    #[error("Item not found")]
    NotFound,

    #[error("Conditional check failed")]
    ConditionalCheckFailed,

    #[error("Serialization error: {0}")]
    Serialization(String),
}

impl ApiError {
    /// Convert error to HTTP response with appropriate status code and error payload
    pub fn to_http_response(&self, request_id: &str) -> Response<Body> {
        let (status, error_code, message): (u16, &str, String) = match self {
            // Authentication errors
            ApiError::Auth(AuthError::MissingToken) => (
                401,
                error_codes::MISSING_TOKEN,
                String::from("Authorization header is required"),
            ),
            ApiError::Auth(AuthError::InvalidToken) => (
                401,
                error_codes::INVALID_TOKEN,
                String::from("Bearer token is invalid"),
            ),
            ApiError::Auth(AuthError::InvalidFormat) => (
                401,
                error_codes::UNAUTHORIZED,
                String::from("Authorization header format is invalid"),
            ),
            ApiError::Auth(AuthError::ConfigError) => (
                500,
                error_codes::INTERNAL_ERROR,
                String::from("Admin token configuration error"),
            ),

            // Validation errors
            ApiError::Validation(ValidationError::MissingField(field)) => (
                400,
                error_codes::MISSING_FIELD,
                format!("Required field missing: {}", field),
            ),
            ApiError::Validation(ValidationError::InvalidFormat(field)) => (
                400,
                error_codes::INVALID_FORMAT,
                format!("Invalid format for field: {}", field),
            ),
            ApiError::Validation(ValidationError::InvalidValue(field)) => (
                400,
                error_codes::INVALID_VALUE,
                format!("Invalid value for field: {}", field),
            ),
            ApiError::Validation(ValidationError::InvalidMac) => (
                400,
                error_codes::INVALID_MAC,
                String::from("MAC address must be in format XX:XX:XX:XX:XX:XX"),
            ),
            ApiError::Validation(ValidationError::InvalidUuid) => (
                400,
                error_codes::INVALID_UUID,
                String::from("UUID must be valid v4 format"),
            ),
            ApiError::Validation(ValidationError::InvalidTimestamp) => (
                400,
                error_codes::INVALID_TIMESTAMP,
                String::from("Timestamp must be valid epoch milliseconds"),
            ),
            ApiError::Validation(ValidationError::InvalidBody(msg)) => {
                (400, error_codes::INVALID_FORMAT, msg.clone())
            }
            ApiError::Validation(ValidationError::InvalidCursor) => (
                400,
                error_codes::INVALID_FORMAT,
                String::from("Invalid cursor format"),
            ),
            ApiError::Validation(ValidationError::InvalidPagination) => (
                400,
                error_codes::INVALID_VALUE,
                String::from("Invalid pagination parameters"),
            ),

            // Not found errors
            ApiError::NotFound(NotFoundError::DeviceNotFound) => (
                404,
                error_codes::DEVICE_NOT_FOUND,
                String::from("Device not found"),
            ),
            ApiError::NotFound(NotFoundError::NoReadings) => (
                404,
                error_codes::NO_READINGS,
                String::from("Device exists but has no readings"),
            ),
            ApiError::NotFound(NotFoundError::ApiKeyNotFound) => (
                404,
                error_codes::API_KEY_NOT_FOUND,
                String::from("API key not found"),
            ),
            ApiError::NotFound(NotFoundError::ResourceNotFound) => (
                404,
                error_codes::DEVICE_NOT_FOUND,
                String::from("Resource not found"),
            ),

            // Database errors
            ApiError::Database(_) => (
                500,
                error_codes::DATABASE_ERROR,
                String::from("Internal database error occurred"),
            ),

            // Internal errors
            ApiError::Internal(_) => (
                500,
                error_codes::INTERNAL_ERROR,
                String::from("Internal server error occurred"),
            ),
        };

        let error_response = ErrorResponse::new(error_code, &message, request_id);

        let body = error_response
            .to_json()
            .unwrap_or_else(|_| String::from(r#"{"error":"INTERNAL_ERROR","message":"Failed to serialize error response","request_id":""}"#));

        Response::builder()
            .status(status)
            .header("content-type", "application/json")
            .body(Body::from(body))
            .unwrap_or_else(|_| {
                Response::builder()
                    .status(500)
                    .body(Body::from(String::from(
                        r#"{"error":"INTERNAL_ERROR","message":"Failed to build response"}"#,
                    )))
                    .unwrap()
            })
    }
}

impl From<aws_sdk_dynamodb::Error> for DatabaseError {
    fn from(err: aws_sdk_dynamodb::Error) -> Self {
        DatabaseError::DynamoDb(err.to_string())
    }
}

impl<E> From<aws_sdk_dynamodb::error::SdkError<E>> for DatabaseError
where
    E: std::fmt::Debug,
{
    fn from(err: aws_sdk_dynamodb::error::SdkError<E>) -> Self {
        DatabaseError::DynamoDb(format!("{:?}", err))
    }
}

impl From<serde_json::Error> for DatabaseError {
    fn from(err: serde_json::Error) -> Self {
        DatabaseError::Serialization(err.to_string())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_auth_error_to_http_response() {
        let error = ApiError::Auth(AuthError::MissingToken);
        let response = error.to_http_response("req-123");

        assert_eq!(response.status(), 401);

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("MISSING_TOKEN"));
        assert!(body.contains("req-123"));
    }

    #[test]
    fn test_validation_error_to_http_response() {
        let error =
            ApiError::Validation(ValidationError::MissingField(String::from("hardware_id")));
        let response = error.to_http_response("req-456");

        assert_eq!(response.status(), 400);

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("MISSING_FIELD"));
        assert!(body.contains("hardware_id"));
        assert!(body.contains("req-456"));
    }

    #[test]
    fn test_not_found_error_to_http_response() {
        let error = ApiError::NotFound(NotFoundError::DeviceNotFound);
        let response = error.to_http_response("req-789");

        assert_eq!(response.status(), 404);

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("DEVICE_NOT_FOUND"));
        assert!(body.contains("req-789"));
    }

    #[test]
    fn test_no_readings_error_to_http_response() {
        let error = ApiError::NotFound(NotFoundError::NoReadings);
        let response = error.to_http_response("req-101");

        assert_eq!(response.status(), 404);

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("NO_READINGS"));
        assert!(body.contains("req-101"));
    }

    #[test]
    fn test_database_error_to_http_response() {
        let error = ApiError::Database(DatabaseError::DynamoDb(String::from("Connection failed")));
        let response = error.to_http_response("req-202");

        assert_eq!(response.status(), 500);

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("DATABASE_ERROR"));
        assert!(body.contains("req-202"));
    }

    #[test]
    fn test_internal_error_to_http_response() {
        let error = ApiError::Internal(String::from("Unexpected error"));
        let response = error.to_http_response("req-303");

        assert_eq!(response.status(), 500);

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("INTERNAL_ERROR"));
        assert!(body.contains("req-303"));
    }

    #[test]
    fn test_error_response_includes_request_id() {
        let errors = vec![
            ApiError::Auth(AuthError::InvalidToken),
            ApiError::Validation(ValidationError::InvalidMac),
            ApiError::NotFound(NotFoundError::ApiKeyNotFound),
            ApiError::Database(DatabaseError::NotFound),
        ];

        for error in errors {
            let response = error.to_http_response("test-request-id");
            let body = match response.body() {
                Body::Text(text) => text.clone(),
                _ => panic!("Expected text body"),
            };

            assert!(
                body.contains("test-request-id"),
                "Error response should include request_id"
            );
        }
    }

    #[test]
    fn test_error_codes_are_stable() {
        // Verify that error codes match the constants from shared module
        let error = ApiError::Auth(AuthError::MissingToken);
        let response = error.to_http_response("req-test");
        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains(error_codes::MISSING_TOKEN));

        let error = ApiError::NotFound(NotFoundError::DeviceNotFound);
        let response = error.to_http_response("req-test");
        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains(error_codes::DEVICE_NOT_FOUND));
    }
}
