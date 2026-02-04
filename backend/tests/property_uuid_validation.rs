//! Property Test: UUID v4 Format Validation (Property 5)
//!
//! **Validates: Requirements 3.6**
//!
//! This property test verifies that:
//! - Valid UUID v4 strings are accepted
//! - Invalid UUID strings (wrong version, wrong format) return 400

use esp32_backend::test_utils::generators;
use esp32_backend::validators::validate_uuid_v4;
use proptest::prelude::*;

proptest! {
    #![proptest_config(ProptestConfig::with_cases(100))]

    /// Property: All generated valid UUID v4 strings should pass validation
    #[test]
    fn prop_valid_uuid_v4_accepted(uuid in generators::uuid_v4()) {
        let result = validate_uuid_v4(&uuid);
        prop_assert!(
            result.is_ok(),
            "Valid UUID v4 {} should be accepted, but got error: {:?}",
            uuid,
            result.err()
        );
    }

    /// Property: All generated invalid UUID strings should fail validation
    #[test]
    fn prop_invalid_uuid_rejected(uuid in generators::invalid_uuid()) {
        let result = validate_uuid_v4(&uuid);
        prop_assert!(
            result.is_err(),
            "Invalid UUID {} should be rejected, but was accepted",
            uuid
        );
    }
}

#[cfg(test)]
mod additional_tests {
    use super::*;

    #[test]
    fn test_specific_valid_uuids() {
        // Test some specific known-good UUID v4 strings
        assert!(validate_uuid_v4("550e8400-e29b-41d4-a716-446655440000").is_ok());
        assert!(validate_uuid_v4("7c9e6679-7425-40de-944b-e07fc1f90ae7").is_ok());
    }

    #[test]
    fn test_specific_invalid_uuids() {
        // Test some specific known-bad UUID strings
        assert!(validate_uuid_v4("not-a-uuid").is_err()); // wrong format
        assert!(validate_uuid_v4("550e8400-e29b-11d4-a716-446655440000").is_err()); // UUID v1
        assert!(validate_uuid_v4("").is_err()); // empty
    }
}
