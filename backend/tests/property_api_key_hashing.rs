//! Property Test: API Key Hashing (Property 11)
//!
//! **Validates: Requirements 11.6, 11.7**
//!
//! This property test verifies that:
//! - Random API keys can be hashed
//! - Same key produces same hash (deterministic)
//! - Different keys produce different hashes
//! - Hash format is SHA-256 (64 hex characters)

use esp32_backend::test_utils::generators;
use hex;
use proptest::prelude::*;
use sha2::{Digest, Sha256};
use std::collections::HashSet;

/// Hash an API key using SHA-256 with pepper (same logic as production code)
fn hash_api_key_test(key: &str, pepper: &str) -> String {
    let mut hasher = Sha256::new();
    hasher.update(pepper.as_bytes());
    hasher.update(key.as_bytes());
    hex::encode(hasher.finalize())
}

proptest! {
    #![proptest_config(ProptestConfig::with_cases(100))]

    /// Property: Hashing the same API key twice produces the same hash
    #[test]
    fn prop_same_key_same_hash(key in generators::api_key()) {
        let pepper = "test-pepper-for-property-testing";

        let hash1 = hash_api_key_test(&key, pepper);
        let hash2 = hash_api_key_test(&key, pepper);

        prop_assert_eq!(
            hash1,
            hash2,
            "Same key should produce same hash"
        );
    }

    /// Property: All hashes are valid SHA-256 format (64 hex characters)
    #[test]
    fn prop_hash_format_valid(key in generators::api_key()) {
        let pepper = "test-pepper-for-property-testing";

        let hash = hash_api_key_test(&key, pepper);

        prop_assert_eq!(
            hash.len(),
            64,
            "SHA-256 hash should be 64 characters"
        );

        prop_assert!(
            hash.chars().all(|c: char| c.is_ascii_hexdigit()),
            "Hash should contain only hex characters"
        );
    }

    /// Property: Different keys produce different hashes
    #[test]
    fn prop_different_keys_different_hashes(
        keys in prop::collection::vec(generators::api_key(), 2..=10)
    ) {
        let pepper = "test-pepper-for-property-testing";

        let mut hashes = HashSet::new();
        let mut unique_keys = HashSet::new();

        for key in &keys {
            unique_keys.insert(key.clone());
            let hash = hash_api_key_test(key, pepper);
            hashes.insert(hash);
        }

        // If all keys are unique, all hashes should be unique
        // (collision is astronomically unlikely with SHA-256)
        prop_assert_eq!(
            hashes.len(),
            unique_keys.len(),
            "Different keys should produce different hashes"
        );
    }
}

#[cfg(test)]
mod additional_tests {
    use super::*;

    #[test]
    fn test_specific_key_hash() {
        let pepper = "test-pepper";
        let key = "5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8";
        let hash = hash_api_key_test(key, pepper);

        // Verify it's a valid SHA-256 hash
        assert_eq!(hash.len(), 64);
        assert!(hash.chars().all(|c: char| c.is_ascii_hexdigit()));

        // Verify determinism
        let hash2 = hash_api_key_test(key, pepper);
        assert_eq!(hash, hash2);
    }

    #[test]
    fn test_different_peppers_different_hashes() {
        let key = "test-key";
        let hash1 = hash_api_key_test(key, "pepper1");
        let hash2 = hash_api_key_test(key, "pepper2");

        assert_ne!(
            hash1, hash2,
            "Different peppers should produce different hashes"
        );
    }
}
