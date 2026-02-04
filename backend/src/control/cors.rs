use lambda_http::{Body, Response};

/// Add CORS headers to a response
///
/// Reads the CORS_ALLOWED_ORIGIN environment variable to determine the allowed origin.
/// If not set, defaults to "*" (allow all origins).
///
/// # Arguments
/// * `response` - The response to add CORS headers to
///
/// # Returns
/// The response with CORS headers added
pub fn add_cors_headers(mut response: Response<Body>) -> Response<Body> {
    let headers = response.headers_mut();

    let allowed_origin = std::env::var("CORS_ALLOWED_ORIGIN").unwrap_or_else(|_| "*".to_string());

    headers.insert(
        "Access-Control-Allow-Origin",
        allowed_origin.parse().unwrap(),
    );
    headers.insert(
        "Access-Control-Allow-Methods",
        "GET, POST, PUT, DELETE, OPTIONS".parse().unwrap(),
    );
    headers.insert(
        "Access-Control-Allow-Headers",
        "Content-Type, Authorization, X-API-Key".parse().unwrap(),
    );
    headers.insert("Access-Control-Max-Age", "3600".parse().unwrap());

    response
}

/// Create a preflight response for OPTIONS requests
///
/// Returns a 200 OK response with CORS headers and an empty body.
/// This is used to handle CORS preflight requests.
///
/// # Returns
/// A response with status 200 and CORS headers
pub fn preflight_response() -> Response<Body> {
    let response = Response::builder().status(200).body(Body::Empty).unwrap();

    add_cors_headers(response)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::Mutex;

    // Use a mutex to ensure tests that modify environment variables run serially
    static ENV_LOCK: Mutex<()> = Mutex::new(());

    #[test]
    fn test_add_cors_headers_with_env_var() {
        let _lock = ENV_LOCK.lock().unwrap();

        // Set environment variable
        std::env::set_var("CORS_ALLOWED_ORIGIN", "https://example.com");

        let response = Response::builder().status(200).body(Body::Empty).unwrap();

        let response_with_cors = add_cors_headers(response);

        let headers = response_with_cors.headers();
        assert_eq!(
            headers.get("Access-Control-Allow-Origin").unwrap(),
            "https://example.com"
        );
        assert_eq!(
            headers.get("Access-Control-Allow-Methods").unwrap(),
            "GET, POST, PUT, DELETE, OPTIONS"
        );
        assert_eq!(
            headers.get("Access-Control-Allow-Headers").unwrap(),
            "Content-Type, Authorization, X-API-Key"
        );
        assert_eq!(headers.get("Access-Control-Max-Age").unwrap(), "3600");

        // Clean up
        std::env::remove_var("CORS_ALLOWED_ORIGIN");
    }

    #[test]
    fn test_add_cors_headers_without_env_var() {
        let _lock = ENV_LOCK.lock().unwrap();

        // Ensure environment variable is not set
        std::env::remove_var("CORS_ALLOWED_ORIGIN");

        let response = Response::builder().status(200).body(Body::Empty).unwrap();

        let response_with_cors = add_cors_headers(response);

        let headers = response_with_cors.headers();
        assert_eq!(headers.get("Access-Control-Allow-Origin").unwrap(), "*");
    }

    #[test]
    fn test_preflight_response() {
        let _lock = ENV_LOCK.lock().unwrap();

        // Ensure environment variable is not set to test default behavior
        std::env::remove_var("CORS_ALLOWED_ORIGIN");

        let response = preflight_response();

        assert_eq!(response.status(), 200);

        let headers = response.headers();
        // Should default to "*" when env var is not set
        assert_eq!(headers.get("Access-Control-Allow-Origin").unwrap(), "*");
        assert_eq!(
            headers.get("Access-Control-Allow-Methods").unwrap(),
            "GET, POST, PUT, DELETE, OPTIONS"
        );
        assert_eq!(
            headers.get("Access-Control-Allow-Headers").unwrap(),
            "Content-Type, Authorization, X-API-Key"
        );
        assert_eq!(headers.get("Access-Control-Max-Age").unwrap(), "3600");

        // Verify body is empty
        match response.body() {
            Body::Empty => (),
            _ => panic!("Expected empty body"),
        }
    }

    #[test]
    fn test_preflight_response_with_custom_origin() {
        let _lock = ENV_LOCK.lock().unwrap();

        std::env::set_var("CORS_ALLOWED_ORIGIN", "https://admin.example.com");

        let response = preflight_response();

        assert_eq!(response.status(), 200);

        let headers = response.headers();
        assert_eq!(
            headers.get("Access-Control-Allow-Origin").unwrap(),
            "https://admin.example.com"
        );
        assert_eq!(
            headers.get("Access-Control-Allow-Methods").unwrap(),
            "GET, POST, PUT, DELETE, OPTIONS"
        );
        assert_eq!(
            headers.get("Access-Control-Allow-Headers").unwrap(),
            "Content-Type, Authorization, X-API-Key"
        );
        assert_eq!(headers.get("Access-Control-Max-Age").unwrap(), "3600");

        // Verify body is empty
        match response.body() {
            Body::Empty => (),
            _ => panic!("Expected empty body"),
        }

        // Clean up
        std::env::remove_var("CORS_ALLOWED_ORIGIN");
    }

    #[test]
    fn test_cors_headers_on_existing_response() {
        let _lock = ENV_LOCK.lock().unwrap();

        std::env::set_var("CORS_ALLOWED_ORIGIN", "https://test.com");

        let response = Response::builder()
            .status(404)
            .header("Content-Type", "application/json")
            .body(Body::Text(r#"{"error": "not_found"}"#.to_string()))
            .unwrap();

        let response_with_cors = add_cors_headers(response);

        // Verify original headers are preserved
        assert_eq!(response_with_cors.status(), 404);
        assert_eq!(
            response_with_cors.headers().get("Content-Type").unwrap(),
            "application/json"
        );

        // Verify CORS headers are added
        assert_eq!(
            response_with_cors
                .headers()
                .get("Access-Control-Allow-Origin")
                .unwrap(),
            "https://test.com"
        );

        // Clean up
        std::env::remove_var("CORS_ALLOWED_ORIGIN");
    }
}
