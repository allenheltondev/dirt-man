use lambda_http::{Body, Request, RequestExt, Response};
use serde::{Deserialize, Serialize};
use tracing::{error, info};

use crate::auth::validate_bearer_token;
use crate::config::ControlConfig;
use crate::crypto::{generate_api_key, hash_api_key};
use crate::error::ApiError;
use esp32_backend::shared::id_generator::{IdGenerator, RandomIdGenerator};
use esp32_backend::shared::time::{Clock, SystemClock};

/// Request payload for creating a new API key
#[derive(Debug, Deserialize)]
pub struct CreateApiKeyRequest {
    /// Optional description for the API key
    pub description: Option<String>,
}

/// Response payload for API key creation
#[derive(Debug, Serialize)]
pub struct CreateApiKeyResponse {
    /// UUID v4 identifier for the API key
    pub key_id: String,
    /// The raw API key value (only shown once)
    pub api_key: String,
    /// RFC3339 timestamp when the key was created
    pub created_at: String,
    /// Warning message to save the key
    pub message: String,
}

/// Response item for API key listing
#[derive(Debug, Serialize)]
pub struct ApiKeyListItem {
    /// UUID v4 identifier for the API key
    pub key_id: String,
    /// RFC3339 timestamp when the key was created
    pub created_at: String,
    /// RFC3339 timestamp when the key was last used (optional)
    pub last_used_at: Option<String>,
    /// Whether the API key is active
    pub is_active: bool,
    /// Optional description for the API key
    pub description: Option<String>,
}

/// Response payload for API key listing
#[derive(Debug, Serialize)]
pub struct ListApiKeysResponse {
    /// List of API keys
    pub api_keys: Vec<ApiKeyListItem>,
    /// Optional pageToken for pagination
    #[serde(rename = "pageToken", skip_serializing_if = "Option::is_none")]
    pub page_token: Option<String>,
}

/// Response payload for API key revocation
#[derive(Debug, Serialize)]
pub struct RevokeApiKeyResponse {
    /// Success message
    pub message: String,
    /// The revoked key_id
    pub key_id: String,
}

/// Handler for POST /api-keys endpoint
pub async fn create_api_key(
    event: Request,
    config: &ControlConfig,
) -> Result<Response<Body>, ApiError> {
    let request_id = event.lambda_context().request_id.clone();

    info!(
        request_id = %request_id,
        "Processing create API key request"
    );

    // Validate Bearer token
    validate_bearer_token(&event)?;

    // Parse request body
    let body = match event.body() {
        Body::Text(text) => text,
        Body::Binary(bytes) => std::str::from_utf8(bytes).map_err(|e| {
            error!(request_id = %request_id, error = %e, "Failed to parse request body as UTF-8");
            ApiError::Validation(crate::error::ValidationError::InvalidBody(
                "Request body must be valid UTF-8".to_string(),
            ))
        })?,
        Body::Empty => "{}",
    };

    let request: CreateApiKeyRequest = serde_json::from_str(body).map_err(|e| {
        error!(request_id = %request_id, error = %e, "Failed to deserialize request body");
        ApiError::Validation(crate::error::ValidationError::InvalidBody(format!(
            "Invalid JSON: {}",
            e
        )))
    })?;

    info!(
        request_id = %request_id,
        has_description = request.description.is_some(),
        "Parsed create API key request"
    );

    // Generate new API key
    let api_key = generate_api_key();
    info!(
        request_id = %request_id,
        "Generated new API key"
    );

    // Hash the API key
    let api_key_hash = hash_api_key(&api_key)?;
    info!(
        request_id = %request_id,
        "Hashed API key"
    );

    // Generate UUID v4 for key_id
    let id_generator: Box<dyn IdGenerator> = Box::new(RandomIdGenerator::new());
    let key_id = id_generator.uuid_v4();

    // Get current timestamp
    let clock: Box<dyn Clock> = Box::new(SystemClock::new());
    let created_at = clock.now_rfc3339();

    info!(
        request_id = %request_id,
        key_id = %key_id,
        created_at = %created_at,
        "Generated key_id and timestamp"
    );

    // Store in DynamoDB
    crate::repo::api_keys::create_api_key(
        &config.dynamodb_client,
        &config.api_keys_table,
        &key_id,
        &api_key_hash,
        &created_at,
        request.description,
    )
    .await?;

    info!(
        request_id = %request_id,
        key_id = %key_id,
        "API key created successfully in DynamoDB"
    );

    // Build response
    let response = CreateApiKeyResponse {
        key_id,
        api_key,
        created_at,
        message: "API key created successfully. Save this key - it will not be shown again."
            .to_string(),
    };

    let response_body = serde_json::to_string(&response).map_err(|e| {
        error!(request_id = %request_id, error = %e, "Failed to serialize response");
        ApiError::Internal(format!("Failed to serialize response: {}", e))
    })?;

    info!(
        request_id = %request_id,
        "Returning successful create API key response"
    );

    Ok(Response::builder()
        .status(200)
        .header("content-type", "application/json")
        .body(Body::from(response_body))
        .unwrap())
}

/// Handler for GET /api-keys endpoint
pub async fn list_api_keys(
    event: Request,
    config: &ControlConfig,
) -> Result<Response<Body>, ApiError> {
    let request_id = event.lambda_context().request_id.clone();

    info!(
        request_id = %request_id,
        "Processing list API keys request"
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

    let page_token = query_params.first("pageToken").map(|s| s.to_string());

    info!(
        request_id = %request_id,
        limit = limit,
        has_page_token = page_token.is_some(),
        "Parsed query parameters"
    );

    // Query DynamoDB
    let (api_keys, page_token) = crate::repo::api_keys::list_api_keys(
        &config.dynamodb_client,
        &config.api_keys_table,
        limit,
        page_token,
    )
    .await?;

    info!(
        request_id = %request_id,
        count = api_keys.len(),
        has_next_page_token = page_token.is_some(),
        "Retrieved API keys from DynamoDB"
    );

    // Convert to response items (exclude api_key_hash)
    let api_key_items: Vec<ApiKeyListItem> = api_keys
        .into_iter()
        .map(|key| ApiKeyListItem {
            key_id: key.key_id,
            created_at: key.created_at,
            last_used_at: key.last_used_at,
            is_active: key.is_active,
            description: key.description,
        })
        .collect();

    // Build response
    let response = ListApiKeysResponse {
        api_keys: api_key_items,
        page_token,
    };

    let response_body = serde_json::to_string(&response).map_err(|e| {
        error!(request_id = %request_id, error = %e, "Failed to serialize response");
        ApiError::Internal(format!("Failed to serialize response: {}", e))
    })?;

    info!(
        request_id = %request_id,
        "Returning successful list API keys response"
    );

    Ok(Response::builder()
        .status(200)
        .header("content-type", "application/json")
        .body(Body::from(response_body))
        .unwrap())
}

/// Handler for DELETE /api-keys/{key_id} endpoint
pub async fn revoke_api_key(
    event: Request,
    config: &ControlConfig,
    key_id: &str,
) -> Result<Response<Body>, ApiError> {
    let request_id = event.lambda_context().request_id.clone();

    info!(
        request_id = %request_id,
        key_id = %key_id,
        "Processing revoke API key request"
    );

    // Validate Bearer token
    validate_bearer_token(&event)?;

    info!(
        request_id = %request_id,
        key_id = %key_id,
        "Bearer token validated, revoking API key"
    );

    // Call revoke_api_key() to set is_active=false
    crate::repo::api_keys::revoke_api_key(&config.dynamodb_client, &config.api_keys_table, key_id)
        .await?;

    info!(
        request_id = %request_id,
        key_id = %key_id,
        "API key revoked successfully"
    );

    // Build success response
    let response = RevokeApiKeyResponse {
        message: "API key revoked successfully".to_string(),
        key_id: key_id.to_string(),
    };

    let response_body = serde_json::to_string(&response).map_err(|e| {
        error!(request_id = %request_id, error = %e, "Failed to serialize response");
        ApiError::Internal(format!("Failed to serialize response: {}", e))
    })?;

    info!(
        request_id = %request_id,
        "Returning successful revoke API key response"
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

    fn create_test_request(
        method: Method,
        uri: &str,
        body: &str,
        auth_header: Option<&str>,
    ) -> Request {
        let mut builder = lambda_http::http::Request::builder()
            .method(method)
            .uri(uri);

        if let Some(auth) = auth_header {
            builder = builder.header("authorization", auth);
        }

        let mut request = builder.body(Body::from(body)).unwrap();
        request.extensions_mut().insert(Context::default());
        request
    }

    #[tokio::test]
    async fn test_create_api_key_request_deserialization() {
        let json = r#"{"description":"Test API key"}"#;
        let request: CreateApiKeyRequest = serde_json::from_str(json).unwrap();
        assert_eq!(request.description, Some("Test API key".to_string()));
    }

    #[tokio::test]
    async fn test_create_api_key_request_deserialization_no_description() {
        let json = r#"{}"#;
        let request: CreateApiKeyRequest = serde_json::from_str(json).unwrap();
        assert_eq!(request.description, None);
    }

    #[tokio::test]
    async fn test_create_api_key_response_serialization() {
        let response = CreateApiKeyResponse {
            key_id: "a1b2c3d4-e5f6-7890-abcd-ef1234567890".to_string(),
            api_key: "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8".to_string(),
            created_at: "2024-01-15T10:30:00Z".to_string(),
            message: "API key created successfully. Save this key - it will not be shown again."
                .to_string(),
        };

        let json = serde_json::to_string(&response).unwrap();
        assert!(json.contains("key_id"));
        assert!(json.contains("api_key"));
        assert!(json.contains("created_at"));
        assert!(json.contains("message"));
        assert!(json.contains("a1b2c3d4-e5f6-7890-abcd-ef1234567890"));
        assert!(json.contains("5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8"));
    }

    #[tokio::test]
    async fn test_create_api_key_missing_auth_header() {
        // Set up environment
        std::env::set_var("ADMIN_TOKEN", "test-token");
        std::env::set_var("API_KEY_PEPPER", "test-pepper");

        let config = ControlConfig::for_test(
            "http://localhost:8000",
            "test-devices".to_string(),
            "test-api-keys".to_string(),
            "test-device-readings".to_string(),
            "test-admin-token".to_string(),
            "*".to_string(),
        )
        .await;

        let request = create_test_request(
            Method::POST,
            "/api-keys",
            r#"{"description":"Test key"}"#,
            None,
        );

        let result = create_api_key(request, &config).await;
        assert!(result.is_err());

        match result.unwrap_err() {
            ApiError::Auth(crate::error::AuthError::MissingToken) => {
                // Expected error
            }
            e => panic!("Expected MissingToken error, got: {:?}", e),
        }

        // Clean up
        std::env::remove_var("ADMIN_TOKEN");
        std::env::remove_var("API_KEY_PEPPER");
    }

    #[tokio::test]
    async fn test_create_api_key_invalid_token() {
        // Set up environment
        std::env::set_var("ADMIN_TOKEN", "correct-token");
        std::env::set_var("API_KEY_PEPPER", "test-pepper");

        let config = ControlConfig::for_test(
            "http://localhost:8000",
            "test-devices".to_string(),
            "test-api-keys".to_string(),
            "test-device-readings".to_string(),
            "test-admin-token".to_string(),
            "*".to_string(),
        )
        .await;

        let request = create_test_request(
            Method::POST,
            "/api-keys",
            r#"{"description":"Test key"}"#,
            Some("Bearer wrong-token"),
        );

        let result = create_api_key(request, &config).await;
        assert!(result.is_err());

        match result.unwrap_err() {
            ApiError::Auth(crate::error::AuthError::InvalidToken) => {
                // Expected error
            }
            e => panic!("Expected InvalidToken error, got: {:?}", e),
        }

        // Clean up
        std::env::remove_var("ADMIN_TOKEN");
        std::env::remove_var("API_KEY_PEPPER");
    }

    #[tokio::test]
    async fn test_create_api_key_invalid_json() {
        // Set up environment
        std::env::set_var("ADMIN_TOKEN", "test-token");
        std::env::set_var("API_KEY_PEPPER", "test-pepper");

        let config = ControlConfig::for_test(
            "http://localhost:8000",
            "test-devices".to_string(),
            "test-api-keys".to_string(),
            "test-device-readings".to_string(),
            "test-admin-token".to_string(),
            "*".to_string(),
        )
        .await;

        let request = create_test_request(
            Method::POST,
            "/api-keys",
            "invalid json",
            Some("Bearer test-token"),
        );

        let result = create_api_key(request, &config).await;
        assert!(result.is_err());

        match result.unwrap_err() {
            ApiError::Validation(crate::error::ValidationError::InvalidBody(_)) => {
                // Expected error
            }
            e => panic!("Expected InvalidBody error, got: {:?}", e),
        }

        // Clean up
        std::env::remove_var("ADMIN_TOKEN");
        std::env::remove_var("API_KEY_PEPPER");
    }

    #[tokio::test]
    async fn test_list_api_keys_response_serialization() {
        let response = ListApiKeysResponse {
            api_keys: vec![
                ApiKeyListItem {
                    key_id: "key-1".to_string(),
                    created_at: "2024-01-15T10:30:00Z".to_string(),
                    last_used_at: Some("2024-01-15T14:22:00Z".to_string()),
                    is_active: true,
                    description: Some("Test key 1".to_string()),
                },
                ApiKeyListItem {
                    key_id: "key-2".to_string(),
                    created_at: "2024-01-14T10:30:00Z".to_string(),
                    last_used_at: None,
                    is_active: false,
                    description: None,
                },
            ],
            page_token: Some("base64pagetoken".to_string()),
        };

        let json = serde_json::to_string(&response).unwrap();
        assert!(json.contains("api_keys"));
        assert!(json.contains("key-1"));
        assert!(json.contains("key-2"));
        assert!(json.contains("nextPageToken"));
        assert!(json.contains("base64pagetoken"));

        // Verify api_key_hash is NOT in the response
        assert!(!json.contains("api_key_hash"));
    }

    #[tokio::test]
    async fn test_list_api_keys_missing_auth_header() {
        // Set up environment
        std::env::set_var("ADMIN_TOKEN", "test-token");
        std::env::set_var("API_KEY_PEPPER", "test-pepper");

        let config = ControlConfig::for_test(
            "http://localhost:8000",
            "test-devices".to_string(),
            "test-api-keys".to_string(),
            "test-device-readings".to_string(),
            "test-admin-token".to_string(),
            "*".to_string(),
        )
        .await;

        let request = create_test_request(Method::GET, "/api-keys", "", None);

        let result = list_api_keys(request, &config).await;
        assert!(result.is_err());

        match result.unwrap_err() {
            ApiError::Auth(crate::error::AuthError::MissingToken) => {
                // Expected error
            }
            e => panic!("Expected MissingToken error, got: {:?}", e),
        }

        // Clean up
        std::env::remove_var("ADMIN_TOKEN");
        std::env::remove_var("API_KEY_PEPPER");
    }

    #[tokio::test]
    async fn test_list_api_keys_invalid_token() {
        // Set up environment
        std::env::set_var("ADMIN_TOKEN", "correct-token");
        std::env::set_var("API_KEY_PEPPER", "test-pepper");

        let config = ControlConfig::for_test(
            "http://localhost:8000",
            "test-devices".to_string(),
            "test-api-keys".to_string(),
            "test-device-readings".to_string(),
            "test-admin-token".to_string(),
            "*".to_string(),
        )
        .await;

        let request = create_test_request(Method::GET, "/api-keys", "", Some("Bearer wrong-token"));

        let result = list_api_keys(request, &config).await;
        assert!(result.is_err());

        match result.unwrap_err() {
            ApiError::Auth(crate::error::AuthError::InvalidToken) => {
                // Expected error
            }
            e => panic!("Expected InvalidToken error, got: {:?}", e),
        }

        // Clean up
        std::env::remove_var("ADMIN_TOKEN");
        std::env::remove_var("API_KEY_PEPPER");
    }

    #[tokio::test]
    async fn test_revoke_api_key_response_serialization() {
        let response = RevokeApiKeyResponse {
            message: "API key revoked successfully".to_string(),
            key_id: "test-key-id".to_string(),
        };

        let json = serde_json::to_string(&response).unwrap();
        assert!(json.contains("message"));
        assert!(json.contains("key_id"));
        assert!(json.contains("test-key-id"));
        assert!(json.contains("API key revoked successfully"));
    }

    #[tokio::test]
    async fn test_revoke_api_key_missing_auth_header() {
        // Set up environment
        std::env::set_var("ADMIN_TOKEN", "test-token");
        std::env::set_var("API_KEY_PEPPER", "test-pepper");

        let config = ControlConfig::for_test(
            "http://localhost:8000",
            "test-devices".to_string(),
            "test-api-keys".to_string(),
            "test-device-readings".to_string(),
            "test-admin-token".to_string(),
            "*".to_string(),
        )
        .await;

        let request = create_test_request(Method::DELETE, "/api-keys/test-key-id", "", None);

        let result = revoke_api_key(request, &config, "test-key-id").await;
        assert!(result.is_err());

        match result.unwrap_err() {
            ApiError::Auth(crate::error::AuthError::MissingToken) => {
                // Expected error
            }
            e => panic!("Expected MissingToken error, got: {:?}", e),
        }

        // Clean up
        std::env::remove_var("ADMIN_TOKEN");
        std::env::remove_var("API_KEY_PEPPER");
    }

    #[tokio::test]
    async fn test_revoke_api_key_invalid_token() {
        // Set up environment
        std::env::set_var("ADMIN_TOKEN", "correct-token");
        std::env::set_var("API_KEY_PEPPER", "test-pepper");

        let config = ControlConfig::for_test(
            "http://localhost:8000",
            "test-devices".to_string(),
            "test-api-keys".to_string(),
            "test-device-readings".to_string(),
            "test-admin-token".to_string(),
            "*".to_string(),
        )
        .await;

        let request = create_test_request(
            Method::DELETE,
            "/api-keys/test-key-id",
            "",
            Some("Bearer wrong-token"),
        );

        let result = revoke_api_key(request, &config, "test-key-id").await;
        assert!(result.is_err());

        match result.unwrap_err() {
            ApiError::Auth(crate::error::AuthError::InvalidToken) => {
                // Expected error
            }
            e => panic!("Expected InvalidToken error, got: {:?}", e),
        }

        // Clean up
        std::env::remove_var("ADMIN_TOKEN");
        std::env::remove_var("API_KEY_PEPPER");
    }
}
