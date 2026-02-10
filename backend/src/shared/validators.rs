use regex::Regex;
use std::sync::OnceLock;

/// Validation error type
#[derive(Debug, Clone)]
pub struct ValidationError {
    pub field: String,
    pub message: String,
}

impl ValidationError {
    pub fn new(field: impl Into<String>, message: impl Into<String>) -> Self {
        Self {
            field: field.into(),
            message: message.into(),
        }
    }
}

impl std::fmt::Display for ValidationError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "Validation error for field '{}': {}",
            self.field, self.message
        )
    }
}

impl std::error::Error for ValidationError {}

/// Validate MAC address format (XX:XX:XX:XX:XX:XX with uppercase hex)
pub fn validate_mac_address(mac: &str) -> Result<(), ValidationError> {
    static MAC_REGEX: OnceLock<Regex> = OnceLock::new();
    let regex = MAC_REGEX.get_or_init(|| {
        Regex::new(r"^[0-9A-F]{2}:[0-9A-F]{2}:[0-9A-F]{2}:[0-9A-F]{2}:[0-9A-F]{2}:[0-9A-F]{2}$")
            .unwrap()
    });

    if regex.is_match(mac) {
        Ok(())
    } else {
        Err(ValidationError::new(
            "hardware_id",
            "MAC address must be in format XX:XX:XX:XX:XX:XX with uppercase hexadecimal",
        ))
    }
}

/// Validate UUID v4 format
pub fn validate_uuid_v4(uuid_str: &str) -> Result<(), ValidationError> {
    match uuid::Uuid::parse_str(uuid_str) {
        Ok(uuid) => {
            // Check version is 4
            if uuid.get_version_num() != 4 {
                return Err(ValidationError::new(
                    "uuid",
                    format!(
                        "UUID must be version 4, got version {}",
                        uuid.get_version_num()
                    ),
                ));
            }
            Ok(())
        }
        Err(_) => Err(ValidationError::new("uuid", "Invalid UUID format")),
    }
}

/// Validate RFC3339 timestamp string for metadata
pub fn validate_rfc3339_timestamp(timestamp: &str) -> Result<(), ValidationError> {
    match chrono::DateTime::parse_from_rfc3339(timestamp) {
        Ok(_) => Ok(()),
        Err(_) => Err(ValidationError::new(
            "timestamp",
            "Timestamp must be in RFC3339 format (e.g., 2024-01-15T10:30:00Z)",
        )),
    }
}

/// Validate epoch milliseconds timestamp for readings
/// Ensures non-negative and within reasonable range (year 2000 to 2100)
pub fn validate_epoch_millis(timestamp_ms: i64) -> Result<(), ValidationError> {
    // Minimum: 2000-01-01 00:00:00 UTC = 946684800000 ms
    const MIN_TIMESTAMP_MS: i64 = 946_684_800_000;
    // Maximum: 2100-01-01 00:00:00 UTC = 4102444800000 ms
    const MAX_TIMESTAMP_MS: i64 = 4_102_444_800_000;

    if timestamp_ms < 0 {
        return Err(ValidationError::new(
            "timestamp_ms",
            "Timestamp must be non-negative",
        ));
    }

    if timestamp_ms < MIN_TIMESTAMP_MS {
        return Err(ValidationError::new(
            "timestamp_ms",
            format!("Timestamp {} is before year 2000", timestamp_ms),
        ));
    }

    if timestamp_ms > MAX_TIMESTAMP_MS {
        return Err(ValidationError::new(
            "timestamp_ms",
            format!("Timestamp {} is after year 2100", timestamp_ms),
        ));
    }

    Ok(())
}

/// Validate batch_id format
/// Batch ID is opaque, max 256 chars, safe ASCII only
pub fn validate_batch_id(batch_id: &str) -> Result<(), ValidationError> {
    if batch_id.is_empty() {
        return Err(ValidationError::new("batch_id", "Batch ID cannot be empty"));
    }

    if batch_id.len() > 256 {
        return Err(ValidationError::new(
            "batch_id",
            format!(
                "Batch ID length {} exceeds maximum of 256 characters",
                batch_id.len()
            ),
        ));
    }

    // Safe ASCII: printable ASCII characters (0x20-0x7E) excluding control characters
    // This includes alphanumeric, punctuation, and common symbols
    if !batch_id
        .chars()
        .all(|c| c.is_ascii() && (' '..='~').contains(&c))
    {
        return Err(ValidationError::new(
            "batch_id",
            "Batch ID must contain only safe ASCII characters (printable ASCII 0x20-0x7E)",
        ));
    }

    Ok(())
}

/// Validate friendly_name format
/// Friendly name is optional, max 64 chars, safe ASCII only
pub fn validate_friendly_name(friendly_name: &str) -> Result<(), ValidationError> {
    if friendly_name.is_empty() {
        return Err(ValidationError::new(
            "friendly_name",
            "Friendly name cannot be empty",
        ));
    }

    if friendly_name.len() > 64 {
        return Err(ValidationError::new(
            "friendly_name",
            format!(
                "Friendly name length {} exceeds maximum of 64 characters",
                friendly_name.len()
            ),
        ));
    }

    // Safe ASCII: printable ASCII characters (0x20-0x7E) excluding control characters
    // This includes alphanumeric, punctuation, and common symbols
    if !friendly_name
        .chars()
        .all(|c| c.is_ascii() && (' '..='~').contains(&c))
    {
        return Err(ValidationError::new(
            "friendly_name",
            "Friendly name must contain only safe ASCII characters (printable ASCII 0x20-0x7E)",
        ));
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_validate_mac_address() {
        // Valid MAC addresses
        assert!(validate_mac_address("AA:BB:CC:DD:EE:FF").is_ok());
        assert!(validate_mac_address("00:11:22:33:44:55").is_ok());
        assert!(validate_mac_address("FF:FF:FF:FF:FF:FF").is_ok());

        // Invalid MAC addresses
        assert!(validate_mac_address("aa:bb:cc:dd:ee:ff").is_err()); // lowercase
        assert!(validate_mac_address("AA:BB:CC:DD:EE").is_err()); // too short
        assert!(validate_mac_address("AA:BB:CC:DD:EE:FF:00").is_err()); // too long
        assert!(validate_mac_address("AA-BB-CC-DD-EE-FF").is_err()); // wrong separator
        assert!(validate_mac_address("AABBCCDDEEFF").is_err()); // no separator
        assert!(validate_mac_address("GG:BB:CC:DD:EE:FF").is_err()); // invalid hex
    }

    #[test]
    fn test_validate_uuid_v4() {
        // Valid UUID v4
        assert!(validate_uuid_v4("550e8400-e29b-41d4-a716-446655440000").is_ok());
        assert!(validate_uuid_v4("7c9e6679-7425-40de-944b-e07fc1f90ae7").is_ok());

        // Invalid UUIDs
        assert!(validate_uuid_v4("not-a-uuid").is_err());
        assert!(validate_uuid_v4("550e8400-e29b-11d4-a716-446655440000").is_err()); // UUID v1
        assert!(validate_uuid_v4("").is_err());
    }

    #[test]
    fn test_validate_rfc3339_timestamp() {
        // Valid RFC3339 timestamps
        assert!(validate_rfc3339_timestamp("2024-01-15T10:30:00Z").is_ok());
        assert!(validate_rfc3339_timestamp("2024-01-15T10:30:00+00:00").is_ok());
        assert!(validate_rfc3339_timestamp("2024-01-15T10:30:00.123Z").is_ok());

        // Invalid timestamps
        assert!(validate_rfc3339_timestamp("2024-01-15").is_err());
        assert!(validate_rfc3339_timestamp("not-a-timestamp").is_err());
        assert!(validate_rfc3339_timestamp("").is_err());
    }

    #[test]
    fn test_validate_epoch_millis() {
        // Valid timestamps
        assert!(validate_epoch_millis(1704067800000).is_ok()); // 2024-01-01
        assert!(validate_epoch_millis(946684800000).is_ok()); // 2000-01-01 (minimum)
        assert!(validate_epoch_millis(4102444800000).is_ok()); // 2100-01-01 (maximum)

        // Invalid timestamps
        assert!(validate_epoch_millis(-1).is_err()); // negative
        assert!(validate_epoch_millis(0).is_err()); // before 2000
        assert!(validate_epoch_millis(946684799999).is_err()); // just before 2000
        assert!(validate_epoch_millis(4102444800001).is_err()); // after 2100
    }

    #[test]
    fn test_validate_batch_id() {
        // Valid batch IDs
        assert!(
            validate_batch_id("AA:BB:CC:DD:EE:FF_7c9e6679_1704067200000_1704067800000").is_ok()
        );
        assert!(validate_batch_id("simple-batch-id").is_ok());
        assert!(validate_batch_id("batch_123").is_ok());
        assert!(validate_batch_id("a").is_ok()); // single char
        assert!(validate_batch_id(&"a".repeat(256)).is_ok()); // exactly 256 chars

        // Invalid batch IDs
        assert!(validate_batch_id("").is_err()); // empty
        assert!(validate_batch_id(&"a".repeat(257)).is_err()); // too long
        assert!(validate_batch_id("batch\nid").is_err()); // control character
        assert!(validate_batch_id("batch\tid").is_err()); // tab
        assert!(validate_batch_id("batch\x00id").is_err()); // null byte
    }

    #[test]
    fn test_validate_friendly_name() {
        // Valid friendly names
        assert!(validate_friendly_name("greenhouse-sensor-01").is_ok());
        assert!(validate_friendly_name("My Device").is_ok());
        assert!(validate_friendly_name("sensor_123").is_ok());
        assert!(validate_friendly_name("a").is_ok()); // single char
        assert!(validate_friendly_name(&"a".repeat(64)).is_ok()); // exactly 64 chars

        // Invalid friendly names
        assert!(validate_friendly_name("").is_err()); // empty
        assert!(validate_friendly_name(&"a".repeat(65)).is_err()); // too long
        assert!(validate_friendly_name("device\nname").is_err()); // control character
        assert!(validate_friendly_name("device\tname").is_err()); // tab
        assert!(validate_friendly_name("device\x00name").is_err()); // null byte
    }
}
