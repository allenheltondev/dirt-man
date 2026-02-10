use lambda_http::{http::Method, Body, Request, RequestExt, Response};
use tracing::{info, warn};

use super::config::ControlConfig;
use super::cors;
use super::error::ApiError;
use super::handlers;

pub async fn route_request(
    event: Request,
    config: &ControlConfig,
) -> Result<Response<Body>, lambda_http::Error> {
    let path = normalize_path(event.uri().path());
    let method = event.method();

    let request_id = event.lambda_context().request_id.clone();

    info!(
        request_id = %request_id,
        method = %method,
        path = %path,
        "Routing control plane request"
    );

    if method == Method::OPTIONS {
        info!(
            request_id = %request_id,
            "Handling CORS preflight request"
        );
        return Ok(cors::preflight_response());
    }

    let response = match (method, path.as_str()) {
        (&Method::GET, "/health") => {
            info!(request_id = %request_id, "Health check endpoint");
            handle_health(&request_id)
        }

        (&Method::POST, "/api-keys") => {
            info!(request_id = %request_id, "Create API key endpoint");
            match handlers::api_keys::create_api_key(event, config).await {
                Ok(response) => response,
                Err(e) => e.to_http_response(&request_id),
            }
        }
        (&Method::GET, "/api-keys") => {
            info!(request_id = %request_id, "List API keys endpoint");
            match handlers::api_keys::list_api_keys(event, config).await {
                Ok(response) => response,
                Err(e) => e.to_http_response(&request_id),
            }
        }
        (&Method::DELETE, path) if path.starts_with("/api-keys/") => {
            info!(request_id = %request_id, path = %path, "Delete API key endpoint");
            let key_id = path.trim_start_matches("/api-keys/");
            match handlers::api_keys::revoke_api_key(event, config, key_id).await {
                Ok(response) => response,
                Err(e) => e.to_http_response(&request_id),
            }
        }

        (&Method::GET, "/devices") => {
            info!(request_id = %request_id, "List devices endpoint");
            match handlers::devices::list_devices(event, config).await {
                Ok(response) => response,
                Err(e) => e.to_http_response(&request_id),
            }
        }
        (&Method::GET, path) if path.starts_with("/devices/") => {
            info!(request_id = %request_id, path = %path, "Device detail/readings endpoint");
            route_device_path(event, config, path).await
        }

        _ => {
            warn!(
                request_id = %request_id,
                method = %method,
                path = %path,
                "Unknown route"
            );
            not_found(&request_id)
        }
    };

    Ok(cors::add_cors_headers(response))
}

fn normalize_path(path: &str) -> String {
    if path == "/" {
        return path.to_string();
    }

    // Strip CloudFront path prefixes if present
    let path = path
        .strip_prefix("/api/control")
        .or_else(|| path.strip_prefix("/api/data"))
        .unwrap_or(path);

    path.trim_end_matches('/').to_string()
}

async fn route_device_path(event: Request, config: &ControlConfig, path: &str) -> Response<Body> {
    let request_id = event.lambda_context().request_id.clone();
    let method = event.method();
    let parts: Vec<&str> = path.trim_start_matches("/devices/").split('/').collect();

    match parts.as_slice() {
        [hardware_id] => {
            match method {
                &Method::GET => {
                    info!(request_id = %request_id, hardware_id = %hardware_id, "Device detail endpoint");
                    match handlers::devices::get_device_detail(event, config, hardware_id).await {
                        Ok(response) => response,
                        Err(e) => e.to_http_response(&request_id),
                    }
                }
                &Method::PUT => {
                    info!(request_id = %request_id, hardware_id = %hardware_id, "Update device endpoint");
                    match handlers::devices::update_device_friendly_name(event, config, hardware_id).await {
                        Ok(response) => response,
                        Err(e) => e.to_http_response(&request_id),
                    }
                }
                _ => not_found(&request_id),
            }
        }
        [hardware_id, "readings"] => {
            info!(request_id = %request_id, hardware_id = %hardware_id, "Query readings endpoint");
            match handlers::readings::query_readings(event, config, hardware_id).await {
                Ok(response) => response,
                Err(e) => e.to_http_response(&request_id),
            }
        }
        [hardware_id, "latest"] => {
            info!(request_id = %request_id, hardware_id = %hardware_id, "Get latest reading endpoint");
            match handlers::readings::get_latest_reading(event, config, hardware_id).await {
                Ok(response) => response,
                Err(e) => e.to_http_response(&request_id),
            }
        }
        _ => not_found(&request_id),
    }
}

fn handle_health(request_id: &str) -> Response<Body> {
    let body = serde_json::json!({
        "status": "healthy",
        "service": "control-plane-api",
        "request_id": request_id
    });

    Response::builder()
        .status(200)
        .header("content-type", "application/json")
        .body(Body::from(body.to_string()))
        .unwrap()
}

fn not_found(request_id: &str) -> Response<Body> {
    let error = ApiError::NotFound(super::error::NotFoundError::ResourceNotFound);
    error.to_http_response(request_id)
}

fn not_implemented(request_id: &str, endpoint: &str) -> Response<Body> {
    let body = serde_json::json!({
        "error": "NOT_IMPLEMENTED",
        "message": format!("Endpoint {} is not yet implemented", endpoint),
        "request_id": request_id
    });

    Response::builder()
        .status(501)
        .header("content-type", "application/json")
        .body(Body::from(body.to_string()))
        .unwrap()
}

#[cfg(test)]
mod tests {
    use super::*;
    use lambda_http::Context;

    fn create_test_request(method: Method, uri: &str) -> Request {
        let mut request = lambda_http::http::Request::builder()
            .method(method)
            .uri(uri)
            .body(Body::Empty)
            .unwrap();
        request.extensions_mut().insert(Context::default());
        request
    }

    #[test]
    fn test_normalize_path() {
        assert_eq!(normalize_path("/"), "/");
        assert_eq!(normalize_path("/health"), "/health");
        assert_eq!(normalize_path("/health/"), "/health");
        assert_eq!(normalize_path("/api-keys"), "/api-keys");
        assert_eq!(normalize_path("/api-keys/"), "/api-keys");
        assert_eq!(
            normalize_path("/devices/AA:BB:CC:DD:EE:FF"),
            "/devices/AA:BB:CC:DD:EE:FF"
        );
        assert_eq!(
            normalize_path("/devices/AA:BB:CC:DD:EE:FF/"),
            "/devices/AA:BB:CC:DD:EE:FF"
        );
    }

    #[test]
    fn test_handle_health() {
        let response = handle_health("test-request-id");

        assert_eq!(response.status(), 200);

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("healthy"));
        assert!(body.contains("control-plane-api"));
        assert!(body.contains("test-request-id"));
    }

    #[test]
    fn test_not_found() {
        let response = not_found("test-request-id");

        assert_eq!(response.status(), 404);

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("test-request-id"));
    }

    #[test]
    fn test_not_implemented() {
        let response = not_implemented("test-request-id", "POST /api-keys");

        assert_eq!(response.status(), 501);

        let body = match response.body() {
            Body::Text(text) => text.clone(),
            _ => panic!("Expected text body"),
        };

        assert!(body.contains("NOT_IMPLEMENTED"));
        assert!(body.contains("POST /api-keys"));
        assert!(body.contains("test-request-id"));
    }

    // Note: Tests for route_device_path require async context and config,
    // so they are tested via integration tests with DynamoDB Local
}
