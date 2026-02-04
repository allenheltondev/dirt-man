#[path = "control/error.rs"]
mod error;

#[path = "control/config.rs"]
mod config;

#[path = "control/cors.rs"]
pub mod cors;

#[path = "control/auth.rs"]
pub mod auth;

#[path = "control/crypto.rs"]
pub mod crypto;

#[path = "control/router.rs"]
mod router;

#[path = "control/handlers/mod.rs"]
mod handlers;

// Repo module (control plane specific)
#[path = "control/repo/mod.rs"]
mod repo;

use lambda_http::{run, service_fn, Error, Request};

async fn function_handler(
    event: Request,
) -> Result<lambda_http::Response<lambda_http::Body>, Error> {
    // Load configuration from environment
    let config = config::ControlConfig::from_env().await.map_err(|e| {
        tracing::error!("Failed to load configuration: {}", e);
        Error::from(format!("Configuration error: {}", e))
    })?;

    // Route the request using the router
    router::route_request(event, &config).await
}

#[tokio::main]
async fn main() -> Result<(), Error> {
    tracing_subscriber::fmt()
        .with_max_level(tracing::Level::INFO)
        .with_target(false)
        .without_time()
        .init();

    run(service_fn(function_handler)).await
}

#[cfg(test)]
mod tests {
    use super::*;
    use lambda_http::{http::Method, Body, Context};

    fn create_test_request(method: Method, uri: &str) -> Request {
        let mut request = lambda_http::http::Request::builder()
            .method(method)
            .uri(uri)
            .body(Body::Empty)
            .unwrap();

        // Add Lambda context
        request.extensions_mut().insert(Context::default());
        request
    }

    #[tokio::test]
    async fn test_control_plane_health_endpoint() {
        // Set up required environment variables
        std::env::set_var("DEVICES_TABLE", "test-devices");
        std::env::set_var("API_KEYS_TABLE", "test-api-keys");
        std::env::set_var("DEVICE_READINGS_TABLE", "test-device-readings");
        std::env::set_var("ADMIN_TOKEN", "test-admin-token");
        std::env::set_var("API_KEY_PEPPER", "test-pepper");

        let request = create_test_request(Method::GET, "/health");

        let response = function_handler(request).await;

        assert!(response.is_ok());
        let resp = response.unwrap();
        assert_eq!(resp.status(), 200);

        let body = match resp.body() {
            Body::Text(text) => text.clone(),
            _ => String::new(),
        };
        assert!(body.contains("healthy"));
        assert!(body.contains("control-plane-api"));

        // Clean up
        std::env::remove_var("DEVICES_TABLE");
        std::env::remove_var("API_KEYS_TABLE");
        std::env::remove_var("DEVICE_READINGS_TABLE");
        std::env::remove_var("ADMIN_TOKEN");
        std::env::remove_var("API_KEY_PEPPER");
    }

    #[tokio::test]
    async fn test_control_plane_cors_preflight() {
        // Set up required environment variables
        std::env::set_var("DEVICES_TABLE", "test-devices");
        std::env::set_var("API_KEYS_TABLE", "test-api-keys");
        std::env::set_var("DEVICE_READINGS_TABLE", "test-device-readings");
        std::env::set_var("ADMIN_TOKEN", "test-admin-token");
        std::env::set_var("API_KEY_PEPPER", "test-pepper");

        let request = create_test_request(Method::OPTIONS, "/api-keys");

        let response = function_handler(request).await;

        assert!(response.is_ok());
        let resp = response.unwrap();
        assert_eq!(resp.status(), 200);

        let headers = resp.headers();
        assert!(headers.contains_key("access-control-allow-origin"));
        assert!(headers.contains_key("access-control-allow-methods"));
        assert!(headers.contains_key("access-control-allow-headers"));

        // Clean up
        std::env::remove_var("DEVICES_TABLE");
        std::env::remove_var("API_KEYS_TABLE");
        std::env::remove_var("DEVICE_READINGS_TABLE");
        std::env::remove_var("ADMIN_TOKEN");
        std::env::remove_var("API_KEY_PEPPER");
    }

    #[tokio::test]
    async fn test_control_plane_unknown_route() {
        // Set up required environment variables
        std::env::set_var("DEVICES_TABLE", "test-devices");
        std::env::set_var("API_KEYS_TABLE", "test-api-keys");
        std::env::set_var("DEVICE_READINGS_TABLE", "test-device-readings");
        std::env::set_var("ADMIN_TOKEN", "test-admin-token");
        std::env::set_var("API_KEY_PEPPER", "test-pepper");

        let request = create_test_request(Method::GET, "/unknown");

        let response = function_handler(request).await;

        assert!(response.is_ok());
        let resp = response.unwrap();
        assert_eq!(resp.status(), 404);

        let headers = resp.headers();
        assert!(headers.contains_key("access-control-allow-origin"));

        // Clean up
        std::env::remove_var("DEVICES_TABLE");
        std::env::remove_var("API_KEYS_TABLE");
        std::env::remove_var("DEVICE_READINGS_TABLE");
        std::env::remove_var("ADMIN_TOKEN");
        std::env::remove_var("API_KEY_PEPPER");
    }

    #[tokio::test]
    async fn test_control_plane_trailing_slash_normalization() {
        // Set up required environment variables
        std::env::set_var("DEVICES_TABLE", "test-devices");
        std::env::set_var("API_KEYS_TABLE", "test-api-keys");
        std::env::set_var("DEVICE_READINGS_TABLE", "test-device-readings");
        std::env::set_var("ADMIN_TOKEN", "test-admin-token");
        std::env::set_var("API_KEY_PEPPER", "test-pepper");

        let request1 = create_test_request(Method::GET, "/health");
        let request2 = create_test_request(Method::GET, "/health/");

        let response1 = function_handler(request1).await;
        let response2 = function_handler(request2).await;

        assert!(response1.is_ok());
        assert!(response2.is_ok());

        assert_eq!(response1.unwrap().status(), 200);
        assert_eq!(response2.unwrap().status(), 200);

        // Clean up
        std::env::remove_var("DEVICES_TABLE");
        std::env::remove_var("API_KEYS_TABLE");
        std::env::remove_var("DEVICE_READINGS_TABLE");
        std::env::remove_var("ADMIN_TOKEN");
        std::env::remove_var("API_KEY_PEPPER");
    }

    #[tokio::test]
    async fn test_control_plane_not_implemented_endpoints() {
        // Set up required environment variables
        std::env::set_var("DEVICES_TABLE", "test-devices");
        std::env::set_var("API_KEYS_TABLE", "test-api-keys");
        std::env::set_var("DEVICE_READINGS_TABLE", "test-device-readings");
        std::env::set_var("ADMIN_TOKEN", "test-admin-token");
        std::env::set_var("API_KEY_PEPPER", "test-pepper");

        let endpoints = vec![
            // POST /api-keys, GET /api-keys, DELETE /api-keys/{key_id}, and GET /devices are now implemented
            // GET /devices/{hardware_id} is now implemented (task 9.3)
            (Method::GET, "/devices/AA:BB:CC:DD:EE:FF/readings"),
            (Method::GET, "/devices/AA:BB:CC:DD:EE:FF/latest"),
        ];

        for (method, uri) in endpoints {
            let request = create_test_request(method.clone(), uri);

            let response = function_handler(request).await;

            assert!(response.is_ok(), "Failed for {} {}", method, uri);
            let resp = response.unwrap();
            assert_eq!(resp.status(), 501, "Expected 501 for {} {}", method, uri);

            let headers = resp.headers();
            assert!(headers.contains_key("access-control-allow-origin"));
        }

        // Clean up
        std::env::remove_var("DEVICES_TABLE");
        std::env::remove_var("API_KEYS_TABLE");
        std::env::remove_var("DEVICE_READINGS_TABLE");
        std::env::remove_var("ADMIN_TOKEN");
        std::env::remove_var("API_KEY_PEPPER");
    }
}
