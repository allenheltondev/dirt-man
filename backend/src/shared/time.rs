use chrono::{DateTime, Utc};

/// Clock trait for abstracting time operations
/// Provides methods for getting current time in different formats
pub trait Clock: Send + Sync {
    /// Get current time as RFC3339 string (for metadata timestamps)
    /// Format: "2024-01-15T10:30:00Z"
    fn now_rfc3339(&self) -> String;

    /// Get current time as epoch seconds (for TTL calculations)
    /// Returns seconds since Unix epoch (1970-01-01 00:00:00 UTC)
    fn now_epoch_seconds(&self) -> i64;
}

/// Production implementation of Clock using system time
#[derive(Debug, Clone, Default)]
pub struct SystemClock;

impl SystemClock {
    pub fn new() -> Self {
        Self
    }
}

impl Clock for SystemClock {
    fn now_rfc3339(&self) -> String {
        Utc::now().to_rfc3339()
    }

    fn now_epoch_seconds(&self) -> i64 {
        Utc::now().timestamp()
    }
}

/// Test implementation of Clock with fixed/controllable time
/// Useful for deterministic testing
#[derive(Debug, Clone)]
pub struct FixedClock {
    timestamp: DateTime<Utc>,
}

impl FixedClock {
    /// Create a new FixedClock with the given timestamp
    pub fn new(timestamp: DateTime<Utc>) -> Self {
        Self { timestamp }
    }

    /// Create a FixedClock from RFC3339 string
    pub fn from_rfc3339(timestamp_str: &str) -> Result<Self, chrono::ParseError> {
        let timestamp = DateTime::parse_from_rfc3339(timestamp_str)?.with_timezone(&Utc);
        Ok(Self { timestamp })
    }

    /// Create a FixedClock from epoch seconds
    pub fn from_epoch_seconds(seconds: i64) -> Self {
        let timestamp = DateTime::from_timestamp(seconds, 0).expect("Invalid timestamp");
        Self { timestamp }
    }

    /// Update the fixed time
    pub fn set_time(&mut self, timestamp: DateTime<Utc>) {
        self.timestamp = timestamp;
    }

    /// Advance time by the given number of seconds
    pub fn advance_seconds(&mut self, seconds: i64) {
        self.timestamp += chrono::Duration::seconds(seconds);
    }
}

impl Clock for FixedClock {
    fn now_rfc3339(&self) -> String {
        self.timestamp.to_rfc3339()
    }

    fn now_epoch_seconds(&self) -> i64 {
        self.timestamp.timestamp()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_system_clock_now_rfc3339() {
        let clock = SystemClock::new();
        let now = clock.now_rfc3339();

        // Verify it's a valid RFC3339 timestamp
        assert!(DateTime::parse_from_rfc3339(&now).is_ok());

        // Verify it contains expected format elements
        assert!(now.contains('T'));
        assert!(now.contains('Z') || now.contains('+') || now.contains('-'));
    }

    #[test]
    fn test_system_clock_now_epoch_seconds() {
        let clock = SystemClock::new();
        let now = clock.now_epoch_seconds();

        // Verify it's a reasonable timestamp (after 2020-01-01)
        assert!(now > 1577836800); // 2020-01-01 00:00:00 UTC

        // Verify it's before 2100-01-01
        assert!(now < 4102444800); // 2100-01-01 00:00:00 UTC
    }

    #[test]
    fn test_fixed_clock_from_rfc3339() {
        let clock = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        // Verify the RFC3339 format is correct
        let rfc3339 = clock.now_rfc3339();
        assert!(rfc3339.starts_with("2024-01-15T10:30:00"));

        // Verify epoch seconds
        let epoch = clock.now_epoch_seconds();
        assert!(epoch > 1705314000 && epoch < 1705318000); // Within reasonable range
    }

    #[test]
    fn test_fixed_clock_from_epoch_seconds() {
        let clock = FixedClock::from_epoch_seconds(1705316400);

        // Verify the RFC3339 format contains the date
        let rfc3339 = clock.now_rfc3339();
        assert!(rfc3339.contains("2024-01-15"));

        // Verify epoch seconds matches
        assert_eq!(clock.now_epoch_seconds(), 1705316400);
    }

    #[test]
    fn test_fixed_clock_advance_seconds() {
        let mut clock = FixedClock::from_epoch_seconds(1705316400);
        let initial_epoch = clock.now_epoch_seconds();

        // Advance by 1 hour (3600 seconds)
        clock.advance_seconds(3600);

        // Verify epoch seconds advanced by exactly 3600
        assert_eq!(clock.now_epoch_seconds(), initial_epoch + 3600);
        assert_eq!(clock.now_epoch_seconds(), 1705320000);
    }

    #[test]
    fn test_fixed_clock_set_time() {
        let mut clock = FixedClock::from_epoch_seconds(1705316400);

        let new_time = DateTime::parse_from_rfc3339("2024-12-25T00:00:00Z")
            .unwrap()
            .with_timezone(&Utc);
        clock.set_time(new_time);

        assert_eq!(clock.now_rfc3339(), "2024-12-25T00:00:00+00:00");
    }

    #[test]
    fn test_fixed_clock_deterministic() {
        let clock1 = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();
        let clock2 = FixedClock::from_rfc3339("2024-01-15T10:30:00Z").unwrap();

        // Multiple calls return the same value
        assert_eq!(clock1.now_rfc3339(), clock1.now_rfc3339());
        assert_eq!(clock1.now_epoch_seconds(), clock1.now_epoch_seconds());

        // Two clocks with same time return same values
        assert_eq!(clock1.now_rfc3339(), clock2.now_rfc3339());
        assert_eq!(clock1.now_epoch_seconds(), clock2.now_epoch_seconds());
    }

    #[test]
    fn test_clock_trait_object() {
        // Verify Clock trait can be used as a trait object
        let system_clock: Box<dyn Clock> = Box::new(SystemClock::new());
        let fixed_clock: Box<dyn Clock> = Box::new(FixedClock::from_epoch_seconds(1705316400));

        // Both should work through trait object
        let _ = system_clock.now_rfc3339();
        let _ = system_clock.now_epoch_seconds();

        assert_eq!(fixed_clock.now_epoch_seconds(), 1705316400);
    }
}
