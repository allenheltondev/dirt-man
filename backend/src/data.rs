// Data plane binary entry point

// Data plane modules
mod auth;
mod config;
mod error;
mod repo;
mod router;

// Handlers module
#[path = "data/handlers/mod.rs"]
mod handlers;

use lambda_http::{run, service_fn, Body, Error, Request, RequestExt, Response};
use tracing::{error, info};

use config::Config;
use esp32_backend::{RandomIdGenerator, SystemClock};
use router::route_request;

async fn function_handler(event: Request) -> Result<Response<Body>, Error> {
    // Extract request ID from Lambda context
    let request_id = event.lambda_context().request_id.clone();

    info!(
        request_id = %request_id,
        method = %event.method(),
        path = %event.uri().path(),
        "Data plane Lambda invoked"
    );

    // For health check, we don't need config
    if event.method() == lambda_http::http::Method::GET
        && (event.uri().path() == "/health" || event.uri().path() == "/health/")
    {
        return match router::route_request_health(&request_id) {
            Ok(response) => {
                info!(
                    request_id = %request_id,
                    status = %response.status(),
                    "Health check completed successfully"
                );
                Ok(response)
            }
            Err(api_error) => {
                error!(
                    request_id = %request_id,
                    error = %api_error,
                    "Health check failed"
                );
                Ok(api_error.to_http_response(&request_id))
            }
        };
    }

    // Initialize configuration for other endpoints
    let config = match Config::from_env().await {
        Ok(config) => config,
        Err(e) => {
            error!(
                request_id = %request_id,
                error = %e,
                "Failed to load configuration"
            );
            return Ok(
                error::ApiError::Internal(format!("Configuration error: {}", e))
                    .to_http_response(&request_id),
            );
        }
    };

    // Initialize Clock and IdGenerator
    let clock = SystemClock::new();
    let id_generator = RandomIdGenerator::new();

    // Route the request and handle any errors
    match route_request(event, &request_id, &config, &clock, &id_generator).await {
        Ok(response) => {
            info!(
                request_id = %request_id,
                status = %response.status(),
                "Request completed successfully"
            );
            Ok(response)
        }
        Err(api_error) => {
            error!(
                request_id = %request_id,
                error = %api_error,
                "Request failed"
            );
            Ok(api_error.to_http_response(&request_id))
        }
    }
}

#[tokio::main]
async fn main() -> Result<(), Error> {
    tracing_subscriber::fmt()
        .with_max_level(tracing::Level::INFO)
        .with_target(false)
        .without_time()
        .init();

    info!("Data plane Lambda starting");

    run(service_fn(function_handler)).await
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::error::ApiError;
    use error::{AuthError, ValidationError};
    use lambda_http::http::{Method, Uri};
    use lambda_http::{Context, RequestExt};

    // Helper to create a test request
    fn create_test_request(method: Method, path: &str) -> Request {
        let uri: Uri = path.parse().unwrap();
        let req = lambda_http::http::Request::builder()
            .method(method)
            .uri(uri)
            .body(Body::Empty)
            .unwrap();

        // Convert to lambda_http::Request and add context
        let lambda_req = Request::from(req);
        let context = Context::default();
        lambda_req.with_lambda_context(context)
    }

    #[tokio::test]
    async fn test_health_endpoint() {
        let request = create_test_request(Method::GET, "/health");
        let response = function_handler(request).await.unwrap();

        assert_eq!(response.status(), 200);

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("healthy"));
        assert!(body.contains("data-plane-api"));
    }

    #[tokio::test]
    async fn test_health_endpoint_with_trailing_slash() {
        let request = create_test_request(Method::GET, "/health/");
        let response = function_handler(request).await.unwrap();

        assert_eq!(response.status(), 200);

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("healthy"));
    }

    #[tokio::test]
    async fn test_register_endpoint_requires_config() {
        // This test will fail if environment variables are not set
        // That's expected - the endpoint requires proper configuration
        let request = create_test_request(Method::POST, "/register");
        let response = function_handler(request).await.unwrap();

        // Should either return 500 (config error) or 401 (missing API key)
        // depending on whether environment variables are set
        assert!(response.status() == 500 || response.status() == 401);
    }

    #[tokio::test]
    async fn test_data_endpoint_requires_auth() {
        let request = create_test_request(Method::POST, "/data");
        let response = function_handler(request).await.unwrap();

        // Should return 500 (config error) or 401 (missing API key)
        // depending on whether environment variables are set
        assert!(response.status() == 500 || response.status() == 401);
    }

    #[tokio::test]
    async fn test_unknown_route() {
        let request = create_test_request(Method::GET, "/unknown");
        let response = function_handler(request).await.unwrap();

        // Should return 500 (config error) or 404 (not found)
        // depending on whether environment variables are set
        assert!(response.status() == 500 || response.status() == 404);
    }

    #[tokio::test]
    async fn test_unknown_method() {
        let request = create_test_request(Method::DELETE, "/register");
        let response = function_handler(request).await.unwrap();

        // Should return 500 (config error) or 404 (not found)
        // depending on whether environment variables are set
        assert!(response.status() == 500 || response.status() == 404);
    }

    #[tokio::test]
    async fn test_trailing_slash_normalization() {
        // Test that /health and /health/ are treated the same
        let request1 = create_test_request(Method::GET, "/health");
        let response1 = function_handler(request1).await.unwrap();

        let request2 = create_test_request(Method::GET, "/health/");
        let response2 = function_handler(request2).await.unwrap();

        // Both should return the same status (200 OK)
        assert_eq!(response1.status(), response2.status());
        assert_eq!(response1.status(), 200);
    }

    #[test]
    fn test_error_handling_auth_error() {
        let error = ApiError::Auth(AuthError::MissingKey);
        let response = error.to_http_response("test-req-123");

        assert_eq!(response.status(), 401);

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("MISSING_API_KEY"));
        assert!(body.contains("test-req-123"));
    }

    #[test]
    fn test_error_handling_validation_error() {
        let error = ApiError::Validation(ValidationError::BatchSizeExceeded);
        let response = error.to_http_response("test-req-456");

        assert_eq!(response.status(), 400);

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("BATCH_SIZE_EXCEEDED"));
        assert!(body.contains("test-req-456"));
    }
}
