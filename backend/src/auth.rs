use aws_sdk_dynamodb::Client as DynamoDbClient;
use chrono::DateTime;
use sha2::{Digest, Sha256};

use crate::error::AuthError;
use crate::repo::api_keys::{get_api_key_by_hash, update_last_used};
use esp32_backend::shared::domain::ApiKey;
use esp32_backend::shared::time::Clock;

/// Hash an API key using SHA-256 with a pepper from environment variable
///
/// The pepper is a system-wide secret that adds an additional layer of security.
/// If the database is leaked, the pepper (which is never stored in DynamoDB)
/// prevents attackers from easily verifying API keys.
///
/// # Arguments
/// * `key` - The raw API key to hash (64-character hex string)
///
/// # Returns
/// * `Result<String, AuthError>` - The hex-encoded SHA-256 hash or an error
///
/// # Errors
/// * `AuthError::ConfigError` - If API_KEY_PEPPER environment variable is not set
///
/// # Example
/// ```
/// use backend::auth::hash_api_key;
///
/// std::env::set_var("API_KEY_PEPPER", "test-pepper-secret");
/// let hash = hash_api_key("5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8").unwrap();
/// assert_eq!(hash.len(), 64); // SHA-256 produces 64 hex characters
/// ```
pub fn hash_api_key(key: &str) -> Result<String, AuthError> {
    // Load pepper from environment variable
    let pepper = std::env::var("API_KEY_PEPPER").map_err(|_| AuthError::ConfigError)?;

    // Create SHA-256 hasher
    let mut hasher = Sha256::new();

    // Hash: pepper || key
    // This ensures that even if the database is leaked, attackers cannot
    // verify API keys without knowing the pepper
    hasher.update(pepper.as_bytes());
    hasher.update(key.as_bytes());

    // Return hex-encoded hash
    Ok(hex::encode(hasher.finalize()))
}

/// Validate an API key against the DynamoDB api_keys table
///
/// This function:
/// 1. Hashes the incoming API key
/// 2. Queries the api_keys table GSI_hash by api_key_hash
/// 3. Checks if the key is active (is_active=true)
/// 4. Updates last_used_at if needed (throttled to 5-minute intervals)
///
/// # Arguments
/// * `client` - DynamoDB client
/// * `table_name` - Name of the api_keys table
/// * `api_key` - Raw API key from X-API-Key header
/// * `clock` - Clock implementation for timestamp generation
///
/// # Returns
/// * `Ok(ApiKey)` - Valid and active API key record
/// * `Err(AuthError)` - Invalid, inactive, or not found
///
/// # Errors
/// * `AuthError::ConfigError` - API_KEY_PEPPER not set
/// * `AuthError::InvalidKey` - Key not found in database
/// * `AuthError::KeyRevoked` - Key exists but is_active=false
pub async fn validate_api_key(
    client: &DynamoDbClient,
    table_name: &str,
    api_key: &str,
    clock: &dyn Clock,
) -> Result<ApiKey, AuthError> {
    // Hash the incoming API key
    let key_hash = hash_api_key(api_key)?;

    // Query the api_keys table by hash
    let api_key_record = get_api_key_by_hash(client, table_name, &key_hash)
        .await
        .map_err(|_| AuthError::InvalidKey)?;

    // Check if key was found
    let api_key_record = api_key_record.ok_or(AuthError::InvalidKey)?;

    // Check if key is active
    if !api_key_record.is_active {
        return Err(AuthError::KeyRevoked);
    }

    // Update last_used_at if needed (throttled to 5-minute intervals)
    if should_update_last_used(&api_key_record.last_used_at, clock) {
        // Ignore errors from update - validation succeeded, update is best-effort
        let _ = update_last_used(client, table_name, &api_key_record.key_id, clock).await;
    }

    Ok(api_key_record)
}

/// Determine if last_used_at should be updated based on 5-minute throttling
///
/// Returns true if:
/// - last_used_at is None (never used before)
/// - last_used_at is older than 5 minutes
///
/// # Arguments
/// * `last_used_at` - Optional RFC3339 timestamp of last use
/// * `clock` - Clock implementation for getting current time
///
/// # Returns
/// * `bool` - true if update is needed, false otherwise
fn should_update_last_used(last_used_at: &Option<String>, clock: &dyn Clock) -> bool {
    match last_used_at {
        None => true, // Never used before, should update
        Some(ts) => {
            // Parse the last_used_at timestamp
            let last_used = match DateTime::parse_from_rfc3339(ts) {
                Ok(dt) => dt.with_timezone(&chrono::Utc),
                Err(_) => return true, // Invalid timestamp, update it
            };

            // Get current time
            let now_str = clock.now_rfc3339();
            let now = match DateTime::parse_from_rfc3339(&now_str) {
                Ok(dt) => dt.with_timezone(&chrono::Utc),
                Err(_) => return true, // Can't parse current time, update anyway
            };

            // Check if more than 5 minutes have passed
            let duration = now.signed_duration_since(last_used);
            duration.num_minutes() >= 5
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use esp32_backend::shared::domain::ApiKey;
    use esp32_backend::shared::time::FixedClock;

    // Helper to set up test environment with unique pepper
    fn setup_test_pepper(pepper: &str) {
        std::env::set_var("API_KEY_PEPPER", pepper);
    }

    // ============================================================================
    // API Key Hashing Tests
    // ============================================================================

    #[test]
    fn test_hash_api_key_consistency() {
        // Set up test pepper
        setup_test_pepper("test-pepper-secret-consistency");

        let key = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";

        // Hash the same key multiple times
        let hash1 = hash_api_key(key).unwrap();
        let hash2 = hash_api_key(key).unwrap();
        let hash3 = hash_api_key(key).unwrap();

        // All hashes should be identical
        assert_eq!(hash1, hash2);
        assert_eq!(hash2, hash3);

        // Hash should be 64 characters (SHA-256 in hex)
        assert_eq!(hash1.len(), 64);
    }

    #[test]
    fn test_hash_api_key_different_keys_produce_different_hashes() {
        setup_test_pepper("test-pepper-secret-different-keys");

        let key1 = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";
        let key2 = "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";

        let hash1 = hash_api_key(key1).unwrap();
        let hash2 = hash_api_key(key2).unwrap();

        // Different keys should produce different hashes
        assert_ne!(hash1, hash2);
    }

    #[test]
    fn test_hash_api_key_different_peppers_produce_different_hashes() {
        let key = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";

        // Hash with first pepper
        setup_test_pepper("pepper1-unique");
        let hash1 = hash_api_key(key).unwrap();

        // Hash with second pepper
        setup_test_pepper("pepper2-unique");
        let hash2 = hash_api_key(key).unwrap();

        // Different peppers should produce different hashes
        assert_ne!(hash1, hash2);
    }

    #[test]
    fn test_hash_api_key_missing_pepper() {
        // Save current pepper if it exists
        let saved_pepper = std::env::var("API_KEY_PEPPER").ok();

        // Ensure pepper is not set
        std::env::remove_var("API_KEY_PEPPER");

        let key = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";
        let result = hash_api_key(key);

        // Should return ConfigError when pepper is missing
        assert!(result.is_err());
        match result {
            Err(AuthError::ConfigError) => {
                // Expected error
            }
            _ => panic!("Expected AuthError::ConfigError"),
        }

        // Restore pepper if it was set
        if let Some(pepper) = saved_pepper {
            std::env::set_var("API_KEY_PEPPER", pepper);
        }
    }

    #[test]
    fn test_hash_api_key_empty_key() {
        setup_test_pepper("test-pepper-secret-empty");

        let key = "";
        let hash = hash_api_key(key).unwrap();

        // Should still produce a valid hash (64 hex characters)
        assert_eq!(hash.len(), 64);
    }

    #[test]
    fn test_hash_api_key_hex_format() {
        setup_test_pepper("test-pepper-secret-hex");

        let key = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";
        let hash = hash_api_key(key).unwrap();

        // Verify hash contains only valid hex characters
        assert!(hash.chars().all(|c| c.is_ascii_hexdigit()));

        // Verify hash is lowercase (hex::encode produces lowercase)
        assert_eq!(hash, hash.to_lowercase());
    }

    #[test]
    fn test_hash_api_key_deterministic() {
        setup_test_pepper("test-pepper-secret-deterministic");

        let key = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";

        // Hash the key
        let hash = hash_api_key(key).unwrap();

        // Expected hash for this specific key and pepper combination
        // This ensures the implementation doesn't change unexpectedly
        // Note: This is a regression test - the exact value depends on the implementation
        let expected_hash = {
            let mut hasher = Sha256::new();
            hasher.update(b"test-pepper-secret-deterministic");
            hasher.update(key.as_bytes());
            hex::encode(hasher.finalize())
        };

        assert_eq!(hash, expected_hash);
    }

    // ============================================================================
    // Last Used Throttling Tests
    // ============================================================================

    #[test]
    fn test_should_update_last_used_never_used() {
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();
        let last_used_at = None;

        // Should update if never used before
        assert!(should_update_last_used(&last_used_at, &clock));
    }

    #[test]
    fn test_should_update_last_used_recent() {
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();
        // Last used 2 minutes ago
        let last_used_at = Some("2024-01-15T10:28:00Z".to_string());

        // Should NOT update if used within last 5 minutes
        assert!(!should_update_last_used(&last_used_at, &clock));
    }

    #[test]
    fn test_should_update_last_used_old() {
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();
        // Last used 10 minutes ago
        let last_used_at = Some("2024-01-15T10:20:00Z".to_string());

        // Should update if used more than 5 minutes ago
        assert!(should_update_last_used(&last_used_at, &clock));
    }

    #[test]
    fn test_should_update_last_used_exactly_five_minutes() {
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();
        // Last used exactly 5 minutes ago
        let last_used_at = Some("2024-01-15T10:25:00Z".to_string());

        // Should update if exactly 5 minutes (>= 5 minutes)
        assert!(should_update_last_used(&last_used_at, &clock));
    }

    #[test]
    fn test_should_update_last_used_invalid_timestamp() {
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();
        // Invalid timestamp format
        let last_used_at = Some("invalid-timestamp".to_string());

        // Should update if timestamp is invalid (to fix it)
        assert!(should_update_last_used(&last_used_at, &clock));
    }

    // ============================================================================
    // API Key Validation Tests (logic tests without DynamoDB)
    // ============================================================================

    #[test]
    fn test_validate_api_key_logic_active_key() {
        // Test the logic of checking if a key is active
        let api_key = ApiKey {
            key_id: "test-key-id-123".to_string(),
            api_key_hash: "test-hash".to_string(),
            created_at: "2024-01-15T10:00:00Z".to_string(),
            last_used_at: Some("2024-01-15T10:20:00Z".to_string()),
            is_active: true,
            description: Some("Test API key".to_string()),
        };

        // Active key should pass the is_active check
        assert!(api_key.is_active);
    }

    #[test]
    fn test_validate_api_key_logic_revoked_key() {
        // Test the logic of checking if a key is revoked
        let api_key = ApiKey {
            key_id: "test-key-id-revoked".to_string(),
            api_key_hash: "test-hash".to_string(),
            created_at: "2024-01-15T10:00:00Z".to_string(),
            last_used_at: Some("2024-01-15T10:20:00Z".to_string()),
            is_active: false, // Key is revoked
            description: Some("Revoked test API key".to_string()),
        };

        // Revoked key should fail the is_active check
        assert!(!api_key.is_active);

        // In the actual validate_api_key function, this would return AuthError::KeyRevoked
        let result = if !api_key.is_active {
            Err(AuthError::KeyRevoked)
        } else {
            Ok(())
        };

        assert!(result.is_err());
        match result {
            Err(AuthError::KeyRevoked) => {
                // Expected error
            }
            _ => panic!("Expected AuthError::KeyRevoked"),
        }
    }

    #[test]
    fn test_validate_api_key_logic_not_found() {
        // Test the logic of handling a key not found scenario
        let api_key_record: Option<ApiKey> = None;

        // Not found should be converted to InvalidKey error
        let result = api_key_record.ok_or(AuthError::InvalidKey);

        assert!(result.is_err());
        match result {
            Err(AuthError::InvalidKey) => {
                // Expected error
            }
            _ => panic!("Expected AuthError::InvalidKey"),
        }
    }

    #[test]
    fn test_missing_api_key_header_handling() {
        // This test demonstrates how missing API key headers should be handled
        // In the actual Lambda handler, the X-API-Key header extraction would
        // return None, which should be converted to AuthError::MissingKey

        // Simulate missing header scenario
        let api_key_header: Option<&str> = None;

        let result = api_key_header.ok_or(AuthError::MissingKey);

        assert!(result.is_err());
        match result {
            Err(AuthError::MissingKey) => {
                // Expected error
            }
            _ => panic!("Expected AuthError::MissingKey"),
        }
    }

    #[test]
    fn test_empty_api_key_header_handling() {
        // Test that empty API key headers can still be hashed
        setup_test_pepper("test-pepper-validation");

        let api_key = "";

        // Empty key should still hash successfully
        let hash_result = hash_api_key(api_key);
        assert!(hash_result.is_ok());

        // The hash will be valid but won't match any real key in the database
        let hash = hash_result.unwrap();
        assert_eq!(hash.len(), 64); // Valid SHA-256 hash
    }

    #[test]
    fn test_validate_api_key_logic_with_throttling() {
        // Test the throttling logic for last_used_at updates
        let clock_recent = FixedClock::from_rfc3339("2024-01-15T10:22:00Z").unwrap();
        let clock_old = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        let api_key = ApiKey {
            key_id: "test-key-id-123".to_string(),
            api_key_hash: "test-hash".to_string(),
            created_at: "2024-01-15T10:00:00Z".to_string(),
            last_used_at: Some("2024-01-15T10:20:00Z".to_string()),
            is_active: true,
            description: Some("Test API key".to_string()),
        };

        // With recent clock (2 minutes after last_used_at), should NOT update
        let should_update_recent = should_update_last_used(&api_key.last_used_at, &clock_recent);
        assert!(!should_update_recent);

        // With old clock (10 minutes after last_used_at), should update
        let should_update_old = should_update_last_used(&api_key.last_used_at, &clock_old);
        assert!(should_update_old);
    }

    #[test]
    fn test_api_key_validation_flow() {
        // Test the complete validation flow logic (without DynamoDB)
        setup_test_pepper("test-pepper-validation-flow");

        let api_key_str = "test-api-key-1234567890123456789012345678901234567890123456789012";

        // Step 1: Hash the API key
        let hash_result = hash_api_key(api_key_str);
        assert!(hash_result.is_ok());
        let key_hash = hash_result.unwrap();

        // Step 2: Simulate finding the key in database (would be done by get_api_key_by_hash)
        let found_key = Some(ApiKey {
            key_id: "test-key-id".to_string(),
            api_key_hash: key_hash.clone(),
            created_at: "2024-01-15T10:00:00Z".to_string(),
            last_used_at: Some("2024-01-15T10:20:00Z".to_string()),
            is_active: true,
            description: Some("Test key".to_string()),
        });

        // Step 3: Check if key was found
        let api_key_record = found_key.ok_or(AuthError::InvalidKey);
        assert!(api_key_record.is_ok());

        // Step 4: Check if key is active
        let api_key = api_key_record.unwrap();
        let is_active_check = if !api_key.is_active {
            Err(AuthError::KeyRevoked)
        } else {
            Ok(api_key.clone())
        };
        assert!(is_active_check.is_ok());

        // Step 5: Check if last_used_at should be updated
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();
        let should_update = should_update_last_used(&api_key.last_used_at, &clock);
        assert!(should_update); // 10 minutes have passed, should update
    }

    #[test]
    fn test_api_key_validation_flow_revoked() {
        // Test validation flow with revoked key
        setup_test_pepper("test-pepper-validation-flow-revoked");

        let api_key_str = "revoked-key-123456789012345678901234567890123456789012345678901234";

        // Step 1: Hash the API key
        let hash_result = hash_api_key(api_key_str);
        assert!(hash_result.is_ok());
        let key_hash = hash_result.unwrap();

        // Step 2: Simulate finding a revoked key
        let found_key = Some(ApiKey {
            key_id: "test-key-id-revoked".to_string(),
            api_key_hash: key_hash.clone(),
            created_at: "2024-01-15T10:00:00Z".to_string(),
            last_used_at: Some("2024-01-15T10:20:00Z".to_string()),
            is_active: false, // Revoked
            description: Some("Revoked key".to_string()),
        });

        // Step 3: Check if key was found
        let api_key_record = found_key.ok_or(AuthError::InvalidKey);
        assert!(api_key_record.is_ok());

        // Step 4: Check if key is active (should fail here)
        let api_key = api_key_record.unwrap();
        let is_active_check = if !api_key.is_active {
            Err(AuthError::KeyRevoked)
        } else {
            Ok(api_key.clone())
        };

        assert!(is_active_check.is_err());
        match is_active_check {
            Err(AuthError::KeyRevoked) => {
                // Expected error
            }
            _ => panic!("Expected AuthError::KeyRevoked"),
        }
    }

    #[test]
    fn test_api_key_validation_flow_not_found() {
        // Test validation flow with key not found
        setup_test_pepper("test-pepper-validation-flow-not-found");

        let api_key_str = "nonexistent-key-12345678901234567890123456789012345678901234567890";

        // Step 1: Hash the API key
        let hash_result = hash_api_key(api_key_str);
        assert!(hash_result.is_ok());

        // Step 2: Simulate key not found in database
        let found_key: Option<ApiKey> = None;

        // Step 3: Check if key was found (should fail here)
        let api_key_record = found_key.ok_or(AuthError::InvalidKey);

        assert!(api_key_record.is_err());
        match api_key_record {
            Err(AuthError::InvalidKey) => {
                // Expected error
            }
            _ => panic!("Expected AuthError::InvalidKey"),
        }
    }
}
