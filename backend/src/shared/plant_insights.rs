use serde::{Deserialize, Serialize};
use std::collections::HashMap;

// ============================================================================
// Core Domain Models
// ============================================================================

/// Reading represents a sensor reading with event_time and ingest_time semantics
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Reading {
    pub batch_id: String,
    pub hardware_id: String,
    pub timestamp_ms: i64, // event_time
    pub ingest_time_ms: i64,
    pub boot_id: String,
    pub firmware_version: String,
    pub friendly_name: Option<String>,
    pub sensors: SensorValues,
    pub sensor_status: ReadingSensorStatus,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ttl: Option<i64>,
}

/// Sensor values from various sensors
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct SensorValues {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub bme280_temp_c: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ds18b20_temp_c: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub humidity_pct: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub pressure_hpa: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub soil_moisture_pct: Option<f64>,
}

/// Status of each sensor in a reading
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct ReadingSensorStatus {
    pub bme280: SensorStatus,
    pub ds18b20: SensorStatus,
    pub soil_moisture: SensorStatus,
}

/// Sensor status enum
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum SensorStatus {
    Ok,
    Missing,
    Stale,
    OutOfRange,
    Noisy,
}

impl SensorStatus {
    pub fn as_str(&self) -> &'static str {
        match self {
            SensorStatus::Ok => "ok",
            SensorStatus::Missing => "missing",
            SensorStatus::Stale => "stale",
            SensorStatus::OutOfRange => "out_of_range",
            SensorStatus::Noisy => "noisy",
        }
    }
}

// ============================================================================
// Event Models
// ============================================================================

/// Event represents a detected occurrence of interest
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Event {
    pub hardware_id: String,
    pub start_time_ms: i64,
    pub end_time_ms: i64,
    pub event_type: EventType,
    pub sensor_values: HashMap<String, f64>,
    pub detection_metadata: HashMap<String, String>,
    pub created_at_ms: i64,
}

/// Event type enum
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq, Hash)]
pub enum EventType {
    #[serde(rename = "Watering_Event")]
    WateringEvent,
    #[serde(rename = "Drying_Cycle")]
    DryingCycle,
    #[serde(rename = "Temperature_Stress")]
    TemperatureStress,
    #[serde(rename = "Humidity_Anomaly")]
    HumidityAnomaly,
    #[serde(rename = "Environmental_Change")]
    EnvironmentalChange,
}

impl EventType {
    pub fn as_str(&self) -> &'static str {
        match self {
            EventType::WateringEvent => "Watering_Event",
            EventType::DryingCycle => "Drying_Cycle",
            EventType::TemperatureStress => "Temperature_Stress",
            EventType::HumidityAnomaly => "Humidity_Anomaly",
            EventType::EnvironmentalChange => "Environmental_Change",
        }
    }
}

// ============================================================================
// Aggregate Models
// ============================================================================

/// Aggregate represents statistical summaries over a time window
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Aggregate {
    pub device_window: String, // format: {hardware_id}#{window_type}
    pub hardware_id: String,
    pub window_type: WindowType,
    pub window_start_ms: i64,
    pub window_end_ms: i64,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub temperature_stats: Option<SensorStats>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub humidity_stats: Option<SensorStats>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub pressure_stats: Option<SensorStats>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub soil_moisture_stats: Option<SensorStats>,
    pub computed_at_ms: i64,
    pub is_complete: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ttl: Option<i64>,
}

/// Window type for aggregation
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq, Hash)]
#[serde(rename_all = "lowercase")]
pub enum WindowType {
    Hourly,
    Daily,
    Weekly,
}

impl WindowType {
    pub fn as_str(&self) -> &'static str {
        match self {
            WindowType::Hourly => "hourly",
            WindowType::Daily => "daily",
            WindowType::Weekly => "weekly",
        }
    }
}

/// Sensor statistics with accumulators for incremental updates
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct SensorStats {
    pub min: f64,
    pub max: f64,
    pub avg: f64,
    pub stddev: f64,
    pub valid_count: i64,
    pub total_count: i64,
    // Accumulators for incremental computation
    pub sum: f64,
    pub sumsq: f64,
}

// ============================================================================
// Insight Models
// ============================================================================

/// Insight represents LLM-generated analysis and recommendations
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Insight {
    pub hardware_id: String,
    pub timestamp_ms: i64,
    pub summary: String,
    pub recommendations: Vec<Recommendation>,
    pub confidence: ConfidenceLevel,
    pub trend: Trend,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub growth_stage_suggestion: Option<String>,
    pub evidence: Evidence,
    pub llm_model: String,
    pub generation_duration_ms: i64,
}

/// Recommendation with action, reason, and urgency
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Recommendation {
    pub action: String,
    pub reason: String,
    pub urgency: Urgency,
}

/// Confidence level for insights
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum ConfidenceLevel {
    Low,
    Medium,
    High,
}

/// Trend classification
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum Trend {
    Improving,
    Declining,
    Stable,
}

/// Urgency level for recommendations
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum Urgency {
    Low,
    Medium,
    High,
}

/// Evidence references for insight generation
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Evidence {
    pub aggregate_refs: Vec<String>,
    pub event_refs: Vec<String>,
    pub profile_snapshot: Option<HashMap<String, String>>,
}

// ============================================================================
// Device Profile Models
// ============================================================================

/// Device profile with learned patterns and configuration
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct DeviceProfile {
    pub hardware_id: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub plant_type: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub soil_type: Option<SoilType>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub pot_size_liters: Option<f64>,
    pub expected_interval_sec: i64, // default 300
    #[serde(skip_serializing_if = "Option::is_none")]
    pub baseline_moisture_range: Option<MoistureRange>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub typical_watering_interval_sec: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub last_watering_events: Option<Vec<i64>>,
    pub updated_at_ms: i64,
}

/// Soil type enum
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "snake_case")]
pub enum SoilType {
    PottingMix,
    CocoCoir,
    Peat,
    Soil,
    Hydroponic,
}

/// Moisture range for baseline learning
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct MoistureRange {
    pub min: f64,
    pub max: f64,
}

// ============================================================================
// Device Status Models
// ============================================================================

/// Device status for health monitoring and dashboard
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct DeviceStatus {
    pub hardware_id: String,
    pub last_seen_event_time_ms: i64,
    pub last_seen_ingest_time_ms: i64,
    pub expected_interval_sec: i64,
    pub last_processed_event_time_ms: i64,
    pub ingest_event_skew_seconds: i64,
    pub pipeline_lag_seconds: i64,
    pub coverage_pct_last_hour: f64,
    pub sensor_status_summary: SensorStatusSummary,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub last_event_detected_at_ms: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub last_aggregate_computed_at_ms: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub last_insight_generated_at_ms: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub last_error_at_ms: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub last_error_code: Option<String>,
    pub last_errors: Vec<ErrorRecord>,
    pub updated_at_ms: i64,
}

/// Sensor status summary
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum SensorStatusSummary {
    Ok,
    Degraded,
    Missing,
}

/// Health category derived from device status
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum HealthCategory {
    Healthy,
    Stale,
    Missing,
    Failing,
}

/// Error record for device status
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct ErrorRecord {
    pub timestamp_ms: i64,
    pub error_code: String,
    pub error_message: String, // truncated to 256 chars
}

// ============================================================================
// Rollup Models
// ============================================================================

/// Rollup represents operational metrics in time buckets
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Rollup {
    pub bucket_key: String, // format: {bucket_type}#{bucket_start_ms}
    pub metric_key: String, // format: {metric_name}#{sorted_dimensions}
    pub bucket_start_ms: i64,
    pub bucket_type: BucketType,
    pub metric_name: String,
    pub dimensions: HashMap<String, String>,
    pub count: i64,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub sum: Option<f64>,
    pub ttl: i64,
}

/// Bucket type for rollups
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum BucketType {
    Minute,
    Hour,
}

impl BucketType {
    pub fn as_str(&self) -> &'static str {
        match self {
            BucketType::Minute => "minute",
            BucketType::Hour => "hour",
        }
    }
}

// ============================================================================
// Insight Request Models
// ============================================================================

/// Insight request for rate limiting and batching
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct InsightRequest {
    pub hardware_id: String,
    pub request_time_ms: i64,
    pub request_type: RequestType,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub event_type: Option<EventType>,
    pub status: RequestStatus,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub ttl: Option<i64>,
}

/// Request type for insight generation
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum RequestType {
    Scheduled,
    Event,
}

/// Request status for insight generation
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
#[serde(rename_all = "lowercase")]
pub enum RequestStatus {
    Pending,
    Processing,
    Done,
    Failed,
}

// ============================================================================
// Processed Readings Index Models
// ============================================================================

/// Processed readings index for stage-aware idempotency
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct ProcessedReading {
    pub reading_id: String, // format: {batch_id}#{timestamp_ms}
    pub hardware_id: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub event_processed_at_ms: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub aggregate_processed_at_ms: Option<i64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub status_processed_at_ms: Option<i64>,
    pub ttl: i64,
}

// ============================================================================
// Helper Functions
// ============================================================================

impl Reading {
    /// Generate reading_id from batch_id and timestamp_ms
    pub fn reading_id(&self) -> String {
        format!("{}#{}", self.batch_id, self.timestamp_ms)
    }
}

impl Aggregate {
    /// Generate device_window key
    pub fn device_window_key(hardware_id: &str, window_type: WindowType) -> String {
        format!("{}#{}", hardware_id, window_type.as_str())
    }
}

impl Rollup {
    /// Generate bucket_key
    pub fn bucket_key(bucket_type: BucketType, bucket_start_ms: i64) -> String {
        format!("{}#{}", bucket_type.as_str(), bucket_start_ms)
    }

    /// Generate metric_key with sorted dimensions
    pub fn metric_key(metric_name: &str, dimensions: &HashMap<String, String>) -> String {
        let mut dim_pairs: Vec<_> = dimensions.iter().collect();
        dim_pairs.sort_by_key(|(k, _)| *k);
        let dim_str = dim_pairs
            .iter()
            .map(|(k, v)| format!("{}={}", k, v))
            .collect::<Vec<_>>()
            .join(",");
        if dim_str.is_empty() {
            metric_name.to_string()
        } else {
            format!("{}#{}", metric_name, dim_str)
        }
    }
}

impl DeviceStatus {
    /// Derive health category from last_seen_ingest_time_ms
    pub fn health_category(&self, now_ms: i64) -> HealthCategory {
        let hours_since_seen = (now_ms - self.last_seen_ingest_time_ms) / (1000 * 3600);

        if self.last_error_at_ms.is_some() {
            let hours_since_error = (now_ms - self.last_error_at_ms.unwrap()) / (1000 * 3600);
            if hours_since_error < 24 {
                return HealthCategory::Failing;
            }
        }

        if hours_since_seen <= 2 {
            HealthCategory::Healthy
        } else if hours_since_seen <= 6 {
            HealthCategory::Stale
        } else {
            HealthCategory::Missing
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_reading_id_generation() {
        let reading = Reading {
            batch_id: "batch123".to_string(),
            hardware_id: "device1".to_string(),
            timestamp_ms: 1234567890,
            ingest_time_ms: 1234567900,
            boot_id: "boot1".to_string(),
            firmware_version: "1.0.0".to_string(),
            friendly_name: None,
            sensors: SensorValues {
                bme280_temp_c: Some(25.0),
                ds18b20_temp_c: None,
                humidity_pct: Some(60.0),
                pressure_hpa: Some(1013.0),
                soil_moisture_pct: Some(45.0),
            },
            sensor_status: ReadingSensorStatus {
                bme280: SensorStatus::Ok,
                ds18b20: SensorStatus::Missing,
                soil_moisture: SensorStatus::Ok,
            },
            ttl: None,
        };

        assert_eq!(reading.reading_id(), "batch123#1234567890");
    }

    #[test]
    fn test_device_window_key() {
        assert_eq!(
            Aggregate::device_window_key("device1", WindowType::Hourly),
            "device1#hourly"
        );
        assert_eq!(
            Aggregate::device_window_key("device2", WindowType::Daily),
            "device2#daily"
        );
    }

    #[test]
    fn test_bucket_key() {
        assert_eq!(
            Rollup::bucket_key(BucketType::Minute, 1234567890000),
            "minute#1234567890000"
        );
        assert_eq!(
            Rollup::bucket_key(BucketType::Hour, 1234567890000),
            "hour#1234567890000"
        );
    }

    #[test]
    fn test_metric_key_no_dimensions() {
        let dimensions = HashMap::new();
        assert_eq!(
            Rollup::metric_key("readings_ingested_count", &dimensions),
            "readings_ingested_count"
        );
    }

    #[test]
    fn test_metric_key_with_dimensions() {
        let mut dimensions = HashMap::new();
        dimensions.insert("event_type".to_string(), "Watering_Event".to_string());
        dimensions.insert("status".to_string(), "success".to_string());

        let key = Rollup::metric_key("events_detected_count", &dimensions);
        // Dimensions should be sorted alphabetically
        assert_eq!(key, "events_detected_count#event_type=Watering_Event,status=success");
    }

    #[test]
    fn test_health_category_healthy() {
        let status = DeviceStatus {
            hardware_id: "device1".to_string(),
            last_seen_event_time_ms: 1000,
            last_seen_ingest_time_ms: 1000,
            expected_interval_sec: 300,
            last_processed_event_time_ms: 1000,
            ingest_event_skew_seconds: 0,
            pipeline_lag_seconds: 0,
            coverage_pct_last_hour: 1.0,
            sensor_status_summary: SensorStatusSummary::Ok,
            last_event_detected_at_ms: None,
            last_aggregate_computed_at_ms: None,
            last_insight_generated_at_ms: None,
            last_error_at_ms: None,
            last_error_code: None,
            last_errors: vec![],
            updated_at_ms: 1000,
        };

        // Within 2 hours
        let now_ms = 1000 + (1 * 3600 * 1000);
        assert_eq!(status.health_category(now_ms), HealthCategory::Healthy);
    }

    #[test]
    fn test_health_category_stale() {
        let status = DeviceStatus {
            hardware_id: "device1".to_string(),
            last_seen_event_time_ms: 1000,
            last_seen_ingest_time_ms: 1000,
            expected_interval_sec: 300,
            last_processed_event_time_ms: 1000,
            ingest_event_skew_seconds: 0,
            pipeline_lag_seconds: 0,
            coverage_pct_last_hour: 1.0,
            sensor_status_summary: SensorStatusSummary::Ok,
            last_event_detected_at_ms: None,
            last_aggregate_computed_at_ms: None,
            last_insight_generated_at_ms: None,
            last_error_at_ms: None,
            last_error_code: None,
            last_errors: vec![],
            updated_at_ms: 1000,
        };

        // Between 2 and 6 hours
        let now_ms = 1000 + (4 * 3600 * 1000);
        assert_eq!(status.health_category(now_ms), HealthCategory::Stale);
    }

    #[test]
    fn test_health_category_missing() {
        let status = DeviceStatus {
            hardware_id: "device1".to_string(),
            last_seen_event_time_ms: 1000,
            last_seen_ingest_time_ms: 1000,
            expected_interval_sec: 300,
            last_processed_event_time_ms: 1000,
            ingest_event_skew_seconds: 0,
            pipeline_lag_seconds: 0,
            coverage_pct_last_hour: 1.0,
            sensor_status_summary: SensorStatusSummary::Ok,
            last_event_detected_at_ms: None,
            last_aggregate_computed_at_ms: None,
            last_insight_generated_at_ms: None,
            last_error_at_ms: None,
            last_error_code: None,
            last_errors: vec![],
            updated_at_ms: 1000,
        };

        // More than 6 hours
        let now_ms = 1000 + (7 * 3600 * 1000);
        assert_eq!(status.health_category(now_ms), HealthCategory::Missing);
    }

    #[test]
    fn test_health_category_failing() {
        let status = DeviceStatus {
            hardware_id: "device1".to_string(),
            last_seen_event_time_ms: 1000,
            last_seen_ingest_time_ms: 1000,
            expected_interval_sec: 300,
            last_processed_event_time_ms: 1000,
            ingest_event_skew_seconds: 0,
            pipeline_lag_seconds: 0,
            coverage_pct_last_hour: 1.0,
            sensor_status_summary: SensorStatusSummary::Ok,
            last_event_detected_at_ms: None,
            last_aggregate_computed_at_ms: None,
            last_insight_generated_at_ms: None,
            last_error_at_ms: Some(1000 + (1 * 3600 * 1000)), // 1 hour ago
            last_error_code: Some("ERROR_CODE".to_string()),
            last_errors: vec![],
            updated_at_ms: 1000,
        };

        // Error within 24 hours
        let now_ms = 1000 + (2 * 3600 * 1000);
        assert_eq!(status.health_category(now_ms), HealthCategory::Failing);
    }
}
