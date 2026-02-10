use chrono::{DateTime, Datelike, Timelike, Utc};

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

// ============================================================================
// Time Window Utilities for Plant Insights
// ============================================================================

/// Align timestamp to minute boundary (UTC)
pub fn align_to_minute(timestamp_ms: i64) -> i64 {
    let seconds = timestamp_ms / 1000;
    let aligned_seconds = (seconds / 60) * 60;
    aligned_seconds * 1000
}

/// Align timestamp to hour boundary (UTC)
pub fn align_to_hour(timestamp_ms: i64) -> i64 {
    let seconds = timestamp_ms / 1000;
    let aligned_seconds = (seconds / 3600) * 3600;
    aligned_seconds * 1000
}

/// Align timestamp to day boundary (UTC)
pub fn align_to_day(timestamp_ms: i64) -> i64 {
    let seconds = timestamp_ms / 1000;
    let aligned_seconds = (seconds / 86400) * 86400;
    aligned_seconds * 1000
}

/// Align timestamp to week boundary (UTC, Monday start)
/// ISO 8601 week starts on Monday
pub fn align_to_week(timestamp_ms: i64) -> i64 {
    let seconds = timestamp_ms / 1000;
    let dt = DateTime::from_timestamp(seconds, 0).expect("Invalid timestamp");

    // Get the day of week (Monday = 0, Sunday = 6)
    let weekday = dt.weekday().num_days_from_monday();

    // Calculate seconds to subtract to get to Monday 00:00:00
    let seconds_since_monday = (weekday as i64 * 86400) + (dt.hour() as i64 * 3600) + (dt.minute() as i64 * 60) + (dt.second() as i64);

    let aligned_seconds = seconds - seconds_since_monday;
    aligned_seconds * 1000
}

/// Check if data is within the lateness window (24 hours after window close)
/// Returns true if the data should be accepted for reprocessing
pub fn is_within_lateness_window(event_time_ms: i64, window_end_ms: i64, now_ms: i64) -> bool {
    // Data must be for a closed window
    if event_time_ms >= window_end_ms {
        return false;
    }

    // Check if we're within 24 hours of window close
    let hours_since_close = (now_ms - window_end_ms) / (1000 * 3600);
    hours_since_close <= 24
}

/// Detect clock skew between event_time and ingest_time
/// Returns true if event_time is more than 5 minutes ahead of ingest_time
pub fn has_clock_skew(event_time_ms: i64, ingest_time_ms: i64) -> bool {
    let skew_ms = event_time_ms - ingest_time_ms;
    skew_ms > (5 * 60 * 1000) // 5 minutes in milliseconds
}

/// Calculate clock skew in seconds
pub fn calculate_skew_seconds(event_time_ms: i64, ingest_time_ms: i64) -> i64 {
    (ingest_time_ms - event_time_ms) / 1000
}

#[cfg(test)]
mod time_window_tests {
    use super::*;

    #[test]
    fn test_align_to_minute() {
        // 2024-01-15 10:30:45.123 -> 2024-01-15 10:30:00.000
        let timestamp_ms = 1705316445123;
        let aligned = align_to_minute(timestamp_ms);
        assert_eq!(aligned, 1705316400000);

        // Already aligned
        let timestamp_ms = 1705316400000;
        let aligned = align_to_minute(timestamp_ms);
        assert_eq!(aligned, 1705316400000);
    }

    #[test]
    fn test_align_to_hour() {
        // 2024-01-15 10:30:45.123 -> 2024-01-15 10:00:00.000
        let timestamp_ms = 1705316445123;
        let aligned = align_to_hour(timestamp_ms);
        assert_eq!(aligned, 1705316400000);

        // Already aligned
        let timestamp_ms = 1705316400000;
        let aligned = align_to_hour(timestamp_ms);
        assert_eq!(aligned, 1705316400000);
    }

    #[test]
    fn test_align_to_day() {
        // 2024-01-15 10:30:45.123 -> 2024-01-15 00:00:00.000
        let timestamp_ms = 1705316445123;
        let aligned = align_to_day(timestamp_ms);
        assert_eq!(aligned, 1705276800000);

        // Already aligned
        let timestamp_ms = 1705276800000;
        let aligned = align_to_day(timestamp_ms);
        assert_eq!(aligned, 1705276800000);
    }

    #[test]
    fn test_align_to_week() {
        // 2024-01-15 (Monday) 10:30:45.123 -> 2024-01-15 00:00:00.000
        let timestamp_ms = 1705316445123;
        let aligned = align_to_week(timestamp_ms);
        assert_eq!(aligned, 1705276800000);

        // 2024-01-17 (Wednesday) 10:30:45.123 -> 2024-01-15 00:00:00.000 (previous Monday)
        let timestamp_ms = 1705489845123;
        let aligned = align_to_week(timestamp_ms);
        assert_eq!(aligned, 1705276800000);
    }

    #[test]
    fn test_is_within_lateness_window_accept() {
        let window_end_ms = 1705314000000; // Hour boundary
        let event_time_ms = window_end_ms - 1000; // 1 second before window close
        let now_ms = window_end_ms + (12 * 3600 * 1000); // 12 hours after window close

        assert!(is_within_lateness_window(event_time_ms, window_end_ms, now_ms));
    }

    #[test]
    fn test_is_within_lateness_window_reject_too_late() {
        let window_end_ms = 1705314000000; // Hour boundary
        let event_time_ms = window_end_ms - 1000; // 1 second before window close
        let now_ms = window_end_ms + (25 * 3600 * 1000); // 25 hours after window close

        assert!(!is_within_lateness_window(event_time_ms, window_end_ms, now_ms));
    }

    #[test]
    fn test_is_within_lateness_window_reject_future_event() {
        let window_end_ms = 1705314000000; // Hour boundary
        let event_time_ms = window_end_ms + 1000; // 1 second after window close
        let now_ms = window_end_ms + (12 * 3600 * 1000); // 12 hours after window close

        assert!(!is_within_lateness_window(event_time_ms, window_end_ms, now_ms));
    }

    #[test]
    fn test_has_clock_skew_no_skew() {
        let event_time_ms = 1705316400000;
        let ingest_time_ms = 1705316410000; // 10 seconds later

        assert!(!has_clock_skew(event_time_ms, ingest_time_ms));
    }

    #[test]
    fn test_has_clock_skew_small_skew() {
        let event_time_ms = 1705316400000;
        let ingest_time_ms = 1705316100000; // 5 minutes earlier (event is 5 min ahead)

        assert!(!has_clock_skew(event_time_ms, ingest_time_ms));
    }

    #[test]
    fn test_has_clock_skew_large_skew() {
        let event_time_ms = 1705316400000;
        let ingest_time_ms = 1705316000000; // 6.67 minutes earlier (event is 6.67 min ahead)

        assert!(has_clock_skew(event_time_ms, ingest_time_ms));
    }

    #[test]
    fn test_calculate_skew_seconds_positive() {
        let event_time_ms = 1705316400000;
        let ingest_time_ms = 1705316410000; // 10 seconds later

        assert_eq!(calculate_skew_seconds(event_time_ms, ingest_time_ms), 10);
    }

    #[test]
    fn test_calculate_skew_seconds_negative() {
        let event_time_ms = 1705316410000;
        let ingest_time_ms = 1705316400000; // 10 seconds earlier

        assert_eq!(calculate_skew_seconds(event_time_ms, ingest_time_ms), -10);
    }
}
