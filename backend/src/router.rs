use lambda_http::http::Method;
use lambda_http::{Body, Request, Response};
use tracing::{info, warn};

use crate::config::Config;
use crate::error::ApiError;
use crate::handlers::data::handle_data;
use crate::handlers::register::handle_register;
use esp32_backend::{Clock, IdGenerator};

/// Route a health check request (no config needed)
pub fn route_request_health(request_id: &str) -> Result<Response<Body>, ApiError> {
    handle_health(request_id)
}

/// Route an incoming request to the appropriate handler
///
/// This function implements path-based routing for the Data Plane API.
/// It normalizes paths (handles trailing slashes), matches on (method, path) tuples,
/// and returns 404 for unknown routes.
pub async fn route_request(
    event: Request,
    request_id: &str,
    config: &Config,
    clock: &dyn Clock,
    id_generator: &dyn IdGenerator,
) -> Result<Response<Body>, ApiError> {
    let method = event.method();
    let path = normalize_path(event.uri().path());

    info!(
        request_id = %request_id,
        method = %method,
        path = %path,
        "Routing request"
    );

    // Match on (method, path) tuples
    match (method, path.as_str()) {
        // Health check endpoint (no authentication required)
        (&Method::GET, "/health") => {
            info!(request_id = %request_id, "Health check endpoint");
            handle_health(request_id)
        }

        // Device registration endpoint
        (&Method::POST, "/register") => {
            info!(request_id = %request_id, "Register endpoint");
            handle_register(
                event,
                request_id,
                &config.dynamodb_client,
                &config.devices_table,
                &config.api_keys_table,
                clock,
                id_generator,
            )
            .await
        }

        // Sensor data ingestion endpoint
        (&Method::POST, "/data") => {
            info!(request_id = %request_id, "Data ingestion endpoint");
            handle_data(event, request_id, config, clock).await
        }

        // Unknown route - return 404
        _ => {
            warn!(
                request_id = %request_id,
                method = %method,
                path = %path,
                "Unknown route"
            );
            handle_not_found(request_id, method, &path)
        }
    }
}

/// Normalize a path by removing trailing slashes
///
/// This ensures that /register and /register/ are treated the same.
/// The root path "/" is preserved as-is.
fn normalize_path(path: &str) -> String {
    if path == "/" {
        return path.to_string();
    }

    // Remove trailing slash if present
    path.trim_end_matches('/').to_string()
}

/// Handle health check requests
///
/// Returns a simple JSON response indicating the service is healthy.
/// This endpoint does not require authentication.
fn handle_health(request_id: &str) -> Result<Response<Body>, ApiError> {
    let body = serde_json::json!({
        "status": "healthy",
        "service": "data-plane-api",
        "request_id": request_id
    });

    Response::builder()
        .status(200)
        .header("content-type", "application/json")
        .body(Body::from(body.to_string()))
        .map_err(|e| ApiError::Internal(format!("Failed to build response: {}", e)))
}

/// Handle not implemented endpoints (placeholder for future implementation)
fn handle_not_implemented(request_id: &str, endpoint: &str) -> Result<Response<Body>, ApiError> {
    let body = serde_json::json!({
        "error": "NOT_IMPLEMENTED",
        "message": format!("Endpoint {} is not yet implemented", endpoint),
        "request_id": request_id
    });

    Response::builder()
        .status(501)
        .header("content-type", "application/json")
        .body(Body::from(body.to_string()))
        .map_err(|e| ApiError::Internal(format!("Failed to build response: {}", e)))
}

/// Handle 404 Not Found responses
fn handle_not_found(
    request_id: &str,
    method: &Method,
    path: &str,
) -> Result<Response<Body>, ApiError> {
    let body = serde_json::json!({
        "error": "NOT_FOUND",
        "message": format!("Route {} {} not found", method, path),
        "request_id": request_id
    });

    Response::builder()
        .status(404)
        .header("content-type", "application/json")
        .body(Body::from(body.to_string()))
        .map_err(|e| ApiError::Internal(format!("Failed to build response: {}", e)))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_normalize_path_root() {
        assert_eq!(normalize_path("/"), "/");
    }

    #[test]
    fn test_normalize_path_no_trailing_slash() {
        assert_eq!(normalize_path("/register"), "/register");
        assert_eq!(normalize_path("/data"), "/data");
        assert_eq!(normalize_path("/health"), "/health");
    }

    #[test]
    fn test_normalize_path_with_trailing_slash() {
        assert_eq!(normalize_path("/register/"), "/register");
        assert_eq!(normalize_path("/data/"), "/data");
        assert_eq!(normalize_path("/health/"), "/health");
    }

    #[test]
    fn test_normalize_path_multiple_trailing_slashes() {
        assert_eq!(normalize_path("/register///"), "/register");
        assert_eq!(normalize_path("/data//"), "/data");
    }

    #[test]
    fn test_normalize_path_nested() {
        assert_eq!(normalize_path("/api/v1/register"), "/api/v1/register");
        assert_eq!(normalize_path("/api/v1/register/"), "/api/v1/register");
    }

    #[tokio::test]
    async fn test_handle_health() {
        let response = handle_health("test-req-123").unwrap();

        assert_eq!(response.status(), 200);
        assert_eq!(
            response.headers().get("content-type").unwrap(),
            "application/json"
        );

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("healthy"));
        assert!(body.contains("test-req-123"));
        assert!(body.contains("data-plane-api"));
    }

    #[tokio::test]
    async fn test_handle_not_implemented() {
        let response = handle_not_implemented("test-req-456", "POST /register").unwrap();

        assert_eq!(response.status(), 501);

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("NOT_IMPLEMENTED"));
        assert!(body.contains("POST /register"));
        assert!(body.contains("test-req-456"));
    }

    #[tokio::test]
    async fn test_handle_not_found() {
        let response = handle_not_found("test-req-789", &Method::GET, "/unknown").unwrap();

        assert_eq!(response.status(), 404);

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("NOT_FOUND"));
        assert!(body.contains("GET /unknown"));
        assert!(body.contains("test-req-789"));
    }
}
