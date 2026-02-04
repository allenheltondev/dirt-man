//! Test utilities for property-based testing
//!
//! This module provides generators and utilities for property-based tests
//! using the proptest framework. It includes generators for domain types
//! like MAC addresses, UUIDs, timestamps, batch IDs, and complete domain objects.

pub mod generators {
    use proptest::prelude::*;
    use std::collections::HashMap;

    /// Generate a valid MAC address in XX:XX:XX:XX:XX:XX format
    pub fn mac_address() -> impl Strategy<Value = String> {
        prop::collection::vec(0u8..=255, 6).prop_map(|bytes| {
            bytes
                .iter()
                .map(|b| format!("{:02X}", b))
                .collect::<Vec<_>>()
                .join(":")
        })
    }

    /// Generate an invalid MAC address (wrong format)
    pub fn invalid_mac_address() -> impl Strategy<Value = String> {
        prop_oneof![
            // Too short
            Just("AA:BB:CC:DD:EE".to_string()),
            // Too long
            Just("AA:BB:CC:DD:EE:FF:00".to_string()),
            // Wrong separator
            Just("AA-BB-CC-DD-EE-FF".to_string()),
            // Invalid characters
            Just("GG:HH:II:JJ:KK:LL".to_string()),
            // Missing colons
            Just("AABBCCDDEEFF".to_string()),
            // Empty
            Just("".to_string()),
        ]
    }

    /// Generate a valid UUID v4 string
    pub fn uuid_v4() -> impl Strategy<Value = String> {
        // Generate random bytes and format as UUID v4
        prop::collection::vec(any::<u8>(), 16).prop_map(|mut bytes| {
            // Set version to 4 (random)
            bytes[6] = (bytes[6] & 0x0f) | 0x40;
            // Set variant to RFC4122
            bytes[8] = (bytes[8] & 0x3f) | 0x80;

            format!(
                "{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
                bytes[0], bytes[1], bytes[2], bytes[3],
                bytes[4], bytes[5],
                bytes[6], bytes[7],
                bytes[8], bytes[9],
                bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]
            )
        })
    }

    /// Generate an invalid UUID string
    pub fn invalid_uuid() -> impl Strategy<Value = String> {
        prop_oneof![
            // Wrong version (not v4) - version 1
            Just("550e8400-e29b-11d4-a716-446655440000".to_string()),
            // Wrong version (not v4) - version 3
            Just("550e8400-e29b-31d4-a716-446655440000".to_string()),
            // Wrong format - not enough segments
            Just("not-a-uuid".to_string()),
            // Too short
            Just("550e8400-e29b-41d4".to_string()),
            // Empty
            Just("".to_string()),
            // Invalid characters
            Just("gggggggg-gggg-4ggg-aggg-gggggggggggg".to_string()),
        ]
    }

    /// Generate a valid timestamp in epoch milliseconds
    /// Range: 2020-01-01 to 2030-12-31 (reasonable bounds)
    pub fn timestamp_ms() -> impl Strategy<Value = i64> {
        // 2020-01-01 00:00:00 UTC = 1577836800000 ms
        // 2030-12-31 23:59:59 UTC = 1924991999000 ms
        1577836800000i64..1924991999000i64
    }

    /// Generate an invalid timestamp (out of reasonable bounds or negative)
    pub fn invalid_timestamp_ms() -> impl Strategy<Value = i64> {
        prop_oneof![
            // Negative
            Just(-1i64),
            // Too far in the past (before 1970)
            Just(0i64),
            // Too far in the future (year 3000+)
            Just(32503680000000i64),
        ]
    }

    /// Generate a valid batch_id string
    /// Format: opaque safe ASCII string, max 256 characters
    pub fn batch_id() -> impl Strategy<Value = String> {
        // Safe ASCII: alphanumeric, underscore, hyphen, colon
        prop::string::string_regex("[A-Za-z0-9_:.-]{1,256}").expect("Valid regex for batch_id")
    }

    /// Generate an invalid batch_id (exceeds max length or contains unsafe characters)
    pub fn invalid_batch_id() -> impl Strategy<Value = String> {
        prop_oneof![
            // Too long (>256 characters)
            prop::string::string_regex("[A-Za-z0-9]{257,300}").expect("Valid regex"),
            // Contains unsafe characters (spaces, special chars)
            Just("batch id with spaces".to_string()),
            Just("batch@id#with$special%chars".to_string()),
            // Empty
            Just("".to_string()),
        ]
    }

    /// Generate a valid firmware version string
    pub fn firmware_version() -> impl Strategy<Value = String> {
        (1u32..100, 0u32..100, 0u32..100)
            .prop_map(|(major, minor, patch)| format!("{}.{}.{}", major, minor, patch))
    }

    /// Generate a valid friendly name (optional)
    pub fn friendly_name() -> impl Strategy<Value = Option<String>> {
        prop::option::of(
            prop::string::string_regex("[a-z0-9-]{3,50}").expect("Valid regex for friendly_name"),
        )
    }

    /// Generate valid sensor values
    pub fn sensor_values() -> impl Strategy<Value = HashMap<String, f64>> {
        (
            prop::option::of(-40.0..85.0),   // bme280_temp_c
            prop::option::of(-55.0..125.0),  // ds18b20_temp_c
            prop::option::of(0.0..100.0),    // humidity_pct
            prop::option::of(300.0..1100.0), // pressure_hpa
            prop::option::of(0.0..100.0),    // soil_moisture_pct
        )
            .prop_map(|(bme_temp, ds_temp, humidity, pressure, soil)| {
                let mut map = HashMap::new();
                if let Some(t) = bme_temp {
                    map.insert("bme280_temp_c".to_string(), t);
                }
                if let Some(t) = ds_temp {
                    map.insert("ds18b20_temp_c".to_string(), t);
                }
                if let Some(h) = humidity {
                    map.insert("humidity_pct".to_string(), h);
                }
                if let Some(p) = pressure {
                    map.insert("pressure_hpa".to_string(), p);
                }
                if let Some(s) = soil {
                    map.insert("soil_moisture_pct".to_string(), s);
                }
                map
            })
    }

    /// Generate valid sensor status
    pub fn sensor_status() -> impl Strategy<Value = HashMap<String, String>> {
        (
            prop::sample::select(vec!["ok", "error"]),
            prop::sample::select(vec!["ok", "error"]),
            prop::sample::select(vec!["ok", "error"]),
        )
            .prop_map(|(bme, ds, soil)| {
                let mut map = HashMap::new();
                map.insert("bme280".to_string(), bme.to_string());
                map.insert("ds18b20".to_string(), ds.to_string());
                map.insert("soil_moisture".to_string(), soil.to_string());
                map
            })
    }

    /// Generate valid capabilities
    pub fn capabilities() -> impl Strategy<Value = (Vec<String>, HashMap<String, bool>)> {
        (
            prop::collection::vec(
                prop::sample::select(vec![
                    "bme280".to_string(),
                    "ds18b20".to_string(),
                    "soil_moisture".to_string(),
                ]),
                1..=3,
            ),
            prop::collection::hash_map(
                prop::sample::select(vec![
                    "tft_display".to_string(),
                    "offline_buffering".to_string(),
                ]),
                any::<bool>(),
                0..=2,
            ),
        )
    }

    /// Generate a valid API key (64-character hex string)
    pub fn api_key() -> impl Strategy<Value = String> {
        prop::collection::vec(0u8..=255, 32)
            .prop_map(|bytes| bytes.iter().map(|b| format!("{:02x}", b)).collect())
    }

    /// Generate an invalid API key
    pub fn invalid_api_key() -> impl Strategy<Value = String> {
        prop_oneof![
            // Too short
            Just("abc123".to_string()),
            // Too long
            prop::collection::vec(0u8..=255, 64)
                .prop_map(|bytes| { bytes.iter().map(|b| format!("{:02x}", b)).collect() }),
            // Invalid characters
            Just("not-a-hex-string-but-64-chars-long-xxxxxxxxxxxxxxxxxxxxxxxx".to_string()),
            // Empty
            Just("".to_string()),
        ]
    }

    /// Generate a batch of readings (1-150 readings)
    pub fn reading_batch(
        size: std::ops::Range<usize>,
    ) -> impl Strategy<Value = Vec<(String, String, i64, String)>> {
        prop::collection::vec((mac_address(), batch_id(), timestamp_ms(), uuid_v4()), size)
    }

    /// Generate a complete Reading struct with valid fields
    pub fn reading() -> impl Strategy<
        Value = (
            String,
            String,
            i64,
            String,
            String,
            Option<String>,
            HashMap<String, f64>,
            HashMap<String, String>,
        ),
    > {
        (
            batch_id(),
            mac_address(),
            timestamp_ms(),
            uuid_v4(),
            firmware_version(),
            friendly_name(),
            sensor_values(),
            sensor_status(),
        )
    }
}

pub mod helpers {
    /// Helper to create a test DynamoDB client configuration
    pub fn test_dynamodb_endpoint() -> String {
        std::env::var("DYNAMODB_ENDPOINT").unwrap_or_else(|_| "http://localhost:8000".to_string())
    }

    /// Helper to generate a test table name with random suffix
    pub fn test_table_name(prefix: &str) -> String {
        use rand::Rng;
        let suffix: u32 = rand::thread_rng().gen();
        format!("{}_{}", prefix, suffix)
    }

    /// Helper to check if a string is a valid UUID v4
    pub fn is_valid_uuid_v4(s: &str) -> bool {
        if s.len() != 36 {
            return false;
        }

        let parts: Vec<&str> = s.split('-').collect();
        if parts.len() != 5 {
            return false;
        }

        // Check lengths: 8-4-4-4-12
        if parts[0].len() != 8
            || parts[1].len() != 4
            || parts[2].len() != 4
            || parts[3].len() != 4
            || parts[4].len() != 12
        {
            return false;
        }

        // Check version (4) and variant (8, 9, a, or b)
        let version_char = parts[2].chars().next().unwrap();
        let variant_char = parts[3].chars().next().unwrap();

        version_char == '4' && matches!(variant_char, '8' | '9' | 'a' | 'b' | 'A' | 'B')
    }

    /// Helper to check if a string is a valid MAC address
    pub fn is_valid_mac_address(s: &str) -> bool {
        if s.len() != 17 {
            return false;
        }

        let parts: Vec<&str> = s.split(':').collect();
        if parts.len() != 6 {
            return false;
        }

        parts
            .iter()
            .all(|part| part.len() == 2 && part.chars().all(|c| c.is_ascii_hexdigit()))
    }

    /// Helper to check if timestamp is within reasonable bounds
    pub fn is_valid_timestamp_ms(ts: i64) -> bool {
        // Must be non-negative and within reasonable range
        // 2020-01-01 to 2030-12-31
        ts >= 1577836800000 && ts <= 1924991999000
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use proptest::prelude::*;

    proptest! {
        #![proptest_config(ProptestConfig::with_cases(100))]

        #[test]
        fn test_mac_address_generator(mac in generators::mac_address()) {
            assert_eq!(mac.len(), 17);
            assert!(helpers::is_valid_mac_address(&mac));
        }

        #[test]
        fn test_uuid_v4_generator(uuid in generators::uuid_v4()) {
            assert_eq!(uuid.len(), 36);
            assert!(helpers::is_valid_uuid_v4(&uuid));
        }

        #[test]
        fn test_timestamp_ms_generator(ts in generators::timestamp_ms()) {
            assert!(helpers::is_valid_timestamp_ms(ts));
        }

        #[test]
        fn test_batch_id_generator(batch_id in generators::batch_id()) {
            assert!(batch_id.len() > 0);
            assert!(batch_id.len() <= 256);
            // Check only safe ASCII characters
            assert!(batch_id.chars().all(|c| c.is_ascii_alphanumeric() || c == '_' || c == ':' || c == '.' || c == '-'));
        }

        #[test]
        fn test_api_key_generator(key in generators::api_key()) {
            assert_eq!(key.len(), 64);
            assert!(key.chars().all(|c| c.is_ascii_hexdigit()));
        }

        #[test]
        fn test_firmware_version_generator(version in generators::firmware_version()) {
            assert!(version.contains('.'));
            let parts: Vec<&str> = version.split('.').collect();
            assert_eq!(parts.len(), 3);
        }
    }
}
