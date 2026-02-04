//! Property Test: MAC Address Format Validation (Property 4)
//!
//! **Validates: Requirements 3.5**
//!
//! This property test verifies that:
//! - Valid MAC addresses (XX:XX:XX:XX:XX:XX format) are accepted
//! - Invalid MAC addresses (wrong format, wrong length, invalid chars) return 400

use esp32_backend::test_utils::generators;
use esp32_backend::validators::validate_mac_address;
use proptest::prelude::*;

proptest! {
    #![proptest_config(ProptestConfig::with_cases(100))]

    /// Property: All generated valid MAC addresses should pass validation
    #[test]
    fn prop_valid_mac_addresses_accepted(mac in generators::mac_address()) {
        let result = validate_mac_address(&mac);
        prop_assert!(
            result.is_ok(),
            "Valid MAC address {} should be accepted, but got error: {:?}",
            mac,
            result.err()
        );
    }

    /// Property: All generated invalid MAC addresses should fail validation
    #[test]
    fn prop_invalid_mac_addresses_rejected(mac in generators::invalid_mac_address()) {
        let result = validate_mac_address(&mac);
        prop_assert!(
            result.is_err(),
            "Invalid MAC address {} should be rejected, but was accepted",
            mac
        );
    }
}

#[cfg(test)]
mod additional_tests {
    use super::*;

    #[test]
    fn test_specific_valid_macs() {
        // Test some specific known-good MAC addresses
        assert!(validate_mac_address("AA:BB:CC:DD:EE:FF").is_ok());
        assert!(validate_mac_address("00:00:00:00:00:00").is_ok());
        assert!(validate_mac_address("FF:FF:FF:FF:FF:FF").is_ok());
    }

    #[test]
    fn test_specific_invalid_macs() {
        // Test some specific known-bad MAC addresses
        assert!(validate_mac_address("aa:bb:cc:dd:ee:ff").is_err()); // lowercase
        assert!(validate_mac_address("AA:BB:CC:DD:EE").is_err()); // too short
        assert!(validate_mac_address("AA-BB-CC-DD-EE-FF").is_err()); // wrong separator
        assert!(validate_mac_address("").is_err()); // empty
    }
}
