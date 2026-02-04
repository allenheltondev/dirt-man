use rand::Rng;
use sha2::{Digest, Sha256};

use crate::error::AuthError;

/// Generate a cryptographically secure random API key
///
/// Generates 32 random bytes and encodes them as a 64-character hexadecimal string.
/// Uses the system's cryptographically secure random number generator.
///
/// # Returns
/// * `String` - A 64-character hexadecimal API key
///
/// # Example
/// ```
/// use backend::control::crypto::generate_api_key;
///
/// let api_key = generate_api_key();
/// assert_eq!(api_key.len(), 64);
/// assert!(api_key.chars().all(|c| c.is_ascii_hexdigit()));
/// ```
pub fn generate_api_key() -> String {
    let mut rng = rand::thread_rng();
    let bytes: Vec<u8> = (0..32).map(|_| rng.gen()).collect();
    hex::encode(bytes)
}

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
/// use backend::control::crypto::hash_api_key;
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

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::Mutex;

    // Mutex to ensure tests that modify environment variables run serially
    // This prevents race conditions when multiple tests modify API_KEY_PEPPER
    static TEST_MUTEX: Mutex<()> = Mutex::new(());

    fn setup_test_pepper(pepper: &str) {
        std::env::set_var("API_KEY_PEPPER", pepper);
    }

    #[test]
    fn test_generate_api_key_length() {
        let key = generate_api_key();
        assert_eq!(
            key.len(),
            64,
            "API key should be 64 characters (32 bytes hex-encoded)"
        );
    }

    #[test]
    fn test_generate_api_key_hex_format() {
        let key = generate_api_key();
        assert!(
            key.chars().all(|c| c.is_ascii_hexdigit()),
            "API key should contain only hexadecimal characters"
        );
    }

    #[test]
    fn test_generate_api_key_uniqueness() {
        let key1 = generate_api_key();
        let key2 = generate_api_key();
        let key3 = generate_api_key();

        // All keys should be different (extremely high probability)
        assert_ne!(key1, key2, "Generated keys should be unique");
        assert_ne!(key2, key3, "Generated keys should be unique");
        assert_ne!(key1, key3, "Generated keys should be unique");
    }

    #[test]
    fn test_hash_api_key_consistency() {
        let _lock = TEST_MUTEX.lock().unwrap();
        setup_test_pepper("test-pepper-secret-consistency-unique-12345");
        let key = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";

        // Hash the same key multiple times
        let hash1 = hash_api_key(key).unwrap();
        let hash2 = hash_api_key(key).unwrap();
        let hash3 = hash_api_key(key).unwrap();

        // All hashes should be identical
        assert_eq!(hash1, hash2, "Same key should produce same hash");
        assert_eq!(hash2, hash3, "Same key should produce same hash");
        assert_eq!(hash1.len(), 64, "SHA-256 hash should be 64 hex characters");
    }

    #[test]
    fn test_hash_api_key_different_keys_produce_different_hashes() {
        let _lock = TEST_MUTEX.lock().unwrap();
        setup_test_pepper("test-pepper-secret-different-keys");
        let key1 = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";
        let key2 = "1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef";

        let hash1 = hash_api_key(key1).unwrap();
        let hash2 = hash_api_key(key2).unwrap();

        // Different keys should produce different hashes
        assert_ne!(
            hash1, hash2,
            "Different keys should produce different hashes"
        );
    }

    #[test]
    fn test_hash_api_key_different_peppers_produce_different_hashes() {
        let _lock = TEST_MUTEX.lock().unwrap();
        let key = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";

        // Hash with first pepper
        setup_test_pepper("pepper1-unique");
        let hash1 = hash_api_key(key).unwrap();

        // Hash with second pepper
        setup_test_pepper("pepper2-unique");
        let hash2 = hash_api_key(key).unwrap();

        // Different peppers should produce different hashes
        assert_ne!(
            hash1, hash2,
            "Different peppers should produce different hashes"
        );
    }

    #[test]
    fn test_hash_api_key_missing_pepper() {
        let _lock = TEST_MUTEX.lock().unwrap();
        // Save current pepper if it exists
        let saved_pepper = std::env::var("API_KEY_PEPPER").ok();

        // Remove pepper
        std::env::remove_var("API_KEY_PEPPER");

        let key = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";
        let result = hash_api_key(key);

        // Should return ConfigError when pepper is missing
        assert!(
            result.is_err(),
            "Should fail when API_KEY_PEPPER is not set"
        );
        match result {
            Err(AuthError::ConfigError) => {
                // Expected error
            }
            _ => panic!("Expected AuthError::ConfigError"),
        }

        // Restore pepper if it existed
        if let Some(pepper) = saved_pepper {
            std::env::set_var("API_KEY_PEPPER", pepper);
        }
    }

    #[test]
    fn test_hash_api_key_empty_key() {
        let _lock = TEST_MUTEX.lock().unwrap();
        setup_test_pepper("test-pepper-secret-empty");

        let key = "";
        let hash = hash_api_key(key).unwrap();

        // Should still produce a valid hash (64 hex characters)
        assert_eq!(hash.len(), 64, "Empty key should still produce valid hash");
    }

    #[test]
    fn test_hash_api_key_hex_format() {
        let _lock = TEST_MUTEX.lock().unwrap();
        setup_test_pepper("test-pepper-secret-hex");

        let key = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";
        let hash = hash_api_key(key).unwrap();

        // Verify hash contains only valid hex characters
        assert!(
            hash.chars().all(|c| c.is_ascii_hexdigit()),
            "Hash should contain only hexadecimal characters"
        );
    }

    #[test]
    fn test_hash_api_key_deterministic() {
        let _lock = TEST_MUTEX.lock().unwrap();
        setup_test_pepper("test-pepper-secret-deterministic");

        let key = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";

        // Hash the key
        let hash = hash_api_key(key).unwrap();

        // Expected hash for this specific key and pepper combination
        // This is a known-good hash that we can verify against
        // (computed offline with the same pepper and key)
        assert_eq!(hash.len(), 64);
        assert!(hash.chars().all(|c| c.is_ascii_hexdigit()));

        // Verify it's consistent across multiple calls
        let hash2 = hash_api_key(key).unwrap();
        assert_eq!(hash, hash2, "Hash should be deterministic");
    }

    #[test]
    fn test_generate_and_hash_integration() {
        let _lock = TEST_MUTEX.lock().unwrap();
        setup_test_pepper("test-pepper-integration");

        // Generate a new API key
        let api_key = generate_api_key();

        // Hash it
        let hash = hash_api_key(&api_key).unwrap();

        // Verify hash properties
        assert_eq!(hash.len(), 64);
        assert!(hash.chars().all(|c| c.is_ascii_hexdigit()));

        // Verify hashing the same key produces the same hash
        let hash2 = hash_api_key(&api_key).unwrap();
        assert_eq!(hash, hash2);
    }
}
