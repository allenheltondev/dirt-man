use lambda_http::{Body, Response};
use thiserror::Error;

use esp32_backend::shared::error::{error_codes, ErrorResponse};

/// Main error type for the Data Plane API
#[derive(Debug, Error)]
pub enum ApiError {
    #[error("Authentication error: {0}")]
    Auth(#[from] AuthError),

    #[error("Validation error: {0}")]
    Validation(#[from] ValidationError),

    #[error("Database error: {0}")]
    Database(#[from] DatabaseError),

    #[error("Internal error: {0}")]
    Internal(String),
}

/// Authentication-specific errors
#[derive(Debug, Error)]
pub enum AuthError {
    #[error("X-API-Key header is missing")]
    MissingKey,

    #[error("API key is invalid or not found")]
    InvalidKey,

    #[error("API key has been revoked")]
    KeyRevoked,

    #[error("Failed to parse API key header")]
    InvalidFormat,

    #[error("API key configuration error")]
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

    #[error("Batch ID exceeds maximum length or contains invalid characters")]
    InvalidBatchId,

    #[error("Batch size exceeds maximum of 100 readings")]
    BatchSizeExceeded,

    #[error("Invalid request body: {0}")]
    InvalidBody(String),
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

    #[error("Transaction cancelled")]
    TransactionCancelled,

    #[error("Serialization error: {0}")]
    Serialization(String),
}

impl ApiError {
    /// Convert error to HTTP response with appropriate status code and error payload
    pub fn to_http_response(&self, request_id: &str) -> Response<Body> {
        let (status, error_code, message): (u16, &str, String) = match self {
            ApiError::Auth(AuthError::MissingKey) => (
                401,
                error_codes::MISSING_API_KEY,
                "X-API-Key header is required".to_string(),
            ),
            ApiError::Auth(AuthError::InvalidKey) => (
                401,
                error_codes::INVALID_API_KEY,
                "API key is invalid or not found".to_string(),
            ),
            ApiError::Auth(AuthError::KeyRevoked) => (
                401,
                error_codes::KEY_REVOKED,
                "API key has been revoked".to_string(),
            ),
            ApiError::Auth(AuthError::InvalidFormat) => (
                401,
                error_codes::UNAUTHORIZED,
                "Failed to parse API key header".to_string(),
            ),
            ApiError::Auth(AuthError::ConfigError) => (
                500,
                error_codes::INTERNAL_ERROR,
                "API key configuration error".to_string(),
            ),
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
                "MAC address must be in format XX:XX:XX:XX:XX:XX".to_string(),
            ),
            ApiError::Validation(ValidationError::InvalidUuid) => (
                400,
                error_codes::INVALID_UUID,
                "UUID must be valid v4 format".to_string(),
            ),
            ApiError::Validation(ValidationError::InvalidTimestamp) => (
                400,
                error_codes::INVALID_TIMESTAMP,
                "Timestamp must be valid epoch milliseconds".to_string(),
            ),
            ApiError::Validation(ValidationError::InvalidBatchId) => (
                400,
                error_codes::INVALID_BATCH_ID,
                "Batch ID exceeds maximum length or contains invalid characters".to_string(),
            ),
            ApiError::Validation(ValidationError::BatchSizeExceeded) => (
                400,
                error_codes::BATCH_SIZE_EXCEEDED,
                "Batch size exceeds maximum of 100 readings".to_string(),
            ),
            ApiError::Validation(ValidationError::InvalidBody(msg)) => {
                (400, error_codes::INVALID_FORMAT, msg.clone())
            }
            ApiError::Database(_) => (
                500,
                error_codes::DATABASE_ERROR,
                "Internal database error occurred".to_string(),
            ),
            ApiError::Internal(_) => (
                500,
                error_codes::INTERNAL_ERROR,
                "Internal server error occurred".to_string(),
            ),
        };

        let error_response = ErrorResponse::new(error_code, &message, request_id);

        let body = error_response
            .to_json()
            .unwrap_or_else(|_| r#"{"error":"INTERNAL_ERROR","message":"Failed to serialize error response","request_id":""}"#.to_string());

        Response::builder()
            .status(status)
            .header("content-type", "application/json")
            .body(Body::from(body))
            .unwrap_or_else(|_| {
                Response::builder()
                    .status(500)
                    .body(Body::from(
                        r#"{"error":"INTERNAL_ERROR","message":"Failed to build response"}"#,
                    ))
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
