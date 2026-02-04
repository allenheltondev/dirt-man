use super::error::AuthError;
use lambda_http::Request;

/// Validates Bearer token from Authorization header against ADMIN_TOKEN environment variable
///
/// # Arguments
/// * `event` - The Lambda HTTP request event
///
/// # Returns
/// * `Ok(())` if token is valid
/// * `Err(AuthError)` if token is missing, invalid, or configuration error
///
/// # Security Note
/// Uses constant-time comparison to prevent timing attacks
pub fn validate_bearer_token(event: &Request) -> Result<(), AuthError> {
    // Extract Authorization header
    let auth_header = event
        .headers()
        .get("authorization")
        .and_then(|v| v.to_str().ok())
        .ok_or(AuthError::MissingToken)?;

    // Check Bearer prefix
    if !auth_header.starts_with("Bearer ") {
        return Err(AuthError::InvalidFormat);
    }

    // Extract token (skip "Bearer " prefix which is 7 characters)
    let token = &auth_header[7..];

    // Reject empty tokens
    if token.is_empty() {
        return Err(AuthError::InvalidToken);
    }

    // Load expected token from environment
    let expected_token = std::env::var("ADMIN_TOKEN").map_err(|_| AuthError::ConfigError)?;

    // Constant-time comparison to prevent timing attacks
    if !constant_time_compare(token, &expected_token) {
        return Err(AuthError::InvalidToken);
    }

    Ok(())
}

/// Performs constant-time string comparison to prevent timing attacks
///
/// # Arguments
/// * `a` - First string to compare
/// * `b` - Second string to compare
///
/// # Returns
/// * `true` if strings are equal, `false` otherwise
///
/// # Security Note
/// This function always compares all bytes to prevent timing-based attacks
/// that could leak information about the expected token
fn constant_time_compare(a: &str, b: &str) -> bool {
    let a_bytes = a.as_bytes();
    let b_bytes = b.as_bytes();

    // If lengths differ, still compare to avoid timing leak
    if a_bytes.len() != b_bytes.len() {
        return false;
    }

    // XOR all bytes and accumulate result
    let mut result = 0u8;
    for (a_byte, b_byte) in a_bytes.iter().zip(b_bytes.iter()) {
        result |= a_byte ^ b_byte;
    }

    result == 0
}

#[cfg(test)]
mod tests {
    use super::*;
    use lambda_http::{http::Method, Body};

    fn create_test_request(method: Method, uri: &str, auth_header: Option<&str>) -> Request {
        let mut builder = lambda_http::http::Request::builder()
            .method(method)
            .uri(uri);

        if let Some(auth) = auth_header {
            builder = builder.header("authorization", auth);
        }

        builder.body(Body::Empty).unwrap()
    }

    #[test]
    fn test_validate_bearer_token_success() {
        // Set up environment variable
        std::env::set_var("ADMIN_TOKEN", "test-secret-token-123");

        let request = create_test_request(
            Method::GET,
            "/api-keys",
            Some("Bearer test-secret-token-123"),
        );

        let result = validate_bearer_token(&request);
        assert!(result.is_ok());

        // Clean up
        std::env::remove_var("ADMIN_TOKEN");
    }

    #[test]
    fn test_validate_bearer_token_invalid_token() {
        std::env::set_var("ADMIN_TOKEN", "correct-token");

        let request = create_test_request(Method::GET, "/api-keys", Some("Bearer wrong-token"));

        let result = validate_bearer_token(&request);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), AuthError::InvalidToken));

        std::env::remove_var("ADMIN_TOKEN");
    }

    #[test]
    fn test_validate_bearer_token_missing_header() {
        std::env::set_var("ADMIN_TOKEN", "test-token");

        let request = create_test_request(Method::GET, "/api-keys", None);

        let result = validate_bearer_token(&request);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), AuthError::MissingToken));

        std::env::remove_var("ADMIN_TOKEN");
    }

    #[test]
    fn test_validate_bearer_token_malformed_header_no_bearer() {
        std::env::set_var("ADMIN_TOKEN", "test-token");

        let request = create_test_request(Method::GET, "/api-keys", Some("test-token"));

        let result = validate_bearer_token(&request);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), AuthError::InvalidFormat));

        std::env::remove_var("ADMIN_TOKEN");
    }

    #[test]
    fn test_validate_bearer_token_malformed_header_wrong_scheme() {
        std::env::set_var("ADMIN_TOKEN", "test-token");

        let request = create_test_request(Method::GET, "/api-keys", Some("Basic dGVzdDp0b2tlbg=="));

        let result = validate_bearer_token(&request);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), AuthError::InvalidFormat));

        std::env::remove_var("ADMIN_TOKEN");
    }

    #[test]
    fn test_validate_bearer_token_admin_token_not_set() {
        // Ensure ADMIN_TOKEN is not set
        std::env::remove_var("ADMIN_TOKEN");

        let request = create_test_request(Method::GET, "/api-keys", Some("Bearer test-token"));

        let result = validate_bearer_token(&request);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), AuthError::ConfigError));
    }

    #[test]
    fn test_validate_bearer_token_empty_token() {
        std::env::set_var("ADMIN_TOKEN", "test-token");

        let request = create_test_request(Method::GET, "/api-keys", Some("Bearer "));

        let result = validate_bearer_token(&request);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), AuthError::InvalidToken));

        std::env::remove_var("ADMIN_TOKEN");
    }

    #[test]
    fn test_validate_bearer_token_case_sensitive() {
        std::env::set_var("ADMIN_TOKEN", "TestToken");

        let request = create_test_request(Method::GET, "/api-keys", Some("Bearer testtoken"));

        let result = validate_bearer_token(&request);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), AuthError::InvalidToken));

        std::env::remove_var("ADMIN_TOKEN");
    }

    #[test]
    fn test_constant_time_compare_equal() {
        assert!(constant_time_compare("test", "test"));
        assert!(constant_time_compare("", ""));
        assert!(constant_time_compare(
            "long-secret-token-123",
            "long-secret-token-123"
        ));
    }

    #[test]
    fn test_constant_time_compare_not_equal() {
        assert!(!constant_time_compare("test", "Test"));
        assert!(!constant_time_compare("test", "test1"));
        assert!(!constant_time_compare("test1", "test"));
        assert!(!constant_time_compare("abc", "def"));
    }

    #[test]
    fn test_constant_time_compare_different_lengths() {
        assert!(!constant_time_compare("short", "longer"));
        assert!(!constant_time_compare("longer", "short"));
        assert!(!constant_time_compare("", "nonempty"));
    }

    #[test]
    fn test_constant_time_compare_special_characters() {
        assert!(constant_time_compare("token!@#$%", "token!@#$%"));
        assert!(!constant_time_compare("token!@#$%", "token!@#$&"));
    }
}
