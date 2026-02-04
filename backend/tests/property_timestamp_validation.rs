//! Property Test: Timestamp Type Validation (Property 8)
//!
//! **Validates: Requirements 6.4**
//!
//! This property test verifies that:
//! - Valid i64 timestamps (epoch milliseconds) are accepted
//! - Invalid timestamps (out of range, negative) return 400

use esp32_backend::test_utils::generators;
use esp32_backend::validators::validate_epoch_millis;
use proptest::prelude::*;

proptest! {
    #![proptest_config(ProptestConfig::with_cases(100))]

    /// Property: All generated valid timestamps should pass validation
    #[test]
    fn prop_valid_timestamps_accepted(ts in generators::timestamp_ms()) {
        let result = validate_epoch_millis(ts);
        prop_assert!(
            result.is_ok(),
            "Valid timestamp {} should be accepted, but got error: {:?}",
            ts,
            result.err()
        );
    }

    /// Property: All generated invalid timestamps should fail validation
    #[test]
    fn prop_invalid_timestamps_rejected(ts in generators::invalid_timestamp_ms()) {
        let result = validate_epoch_millis(ts);
        prop_assert!(
            result.is_err(),
            "Invalid timestamp {} should be rejected, but was accepted",
            ts
        );
    }
}

#[cfg(test)]
mod additional_tests {
    use super::*;

    #[test]
    fn test_specific_valid_timestamps() {
        // Test some specific known-good timestamps
        assert!(validate_epoch_millis(1704067800000).is_ok()); // 2024-01-01
        assert!(validate_epoch_millis(946684800000).is_ok()); // 2000-01-01 (minimum)
    }

    #[test]
    fn test_specific_invalid_timestamps() {
        // Test some specific known-bad timestamps
        assert!(validate_epoch_millis(-1).is_err()); // negative
        assert!(validate_epoch_millis(0).is_err()); // before 2000
        assert!(validate_epoch_millis(5000000000000).is_err()); // far future
    }
}
