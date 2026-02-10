use crate::plant_insights::{DeviceStatus, ErrorRecord};

pub enum DeviceStatusUpdate {
    StatusUpdater {
        last_seen_event_time_ms: i64,
        last_seen_ingest_time_ms: i64,
        ingest_event_skew_seconds: i64,
        pipeline_lag_seconds: i64,
    },
    EventDetector {
        last_event_detected_at_ms: i64,
        last_processed_event_time_ms: i64,
    },
    Aggregator {
        last_aggregate_computed_at_ms: i64,
        coverage_pct_last_hour: f64,
    },
    InsightGenerator {
        last_insight_generated_at_ms: i64,
    },
    Error {
        last_error_at_ms: i64,
        last_error_code: String,
        error_record: ErrorRecord,
    },
}

pub fn apply_status_update(status: &mut DeviceStatus, update: DeviceStatusUpdate, now_ms: i64) {
    match update {
        DeviceStatusUpdate::StatusUpdater {
            last_seen_event_time_ms,
            last_seen_ingest_time_ms,
            ingest_event_skew_seconds,
            pipeline_lag_seconds,
        } => {
            status.last_seen_event_time_ms = last_seen_event_time_ms;
            status.last_seen_ingest_time_ms = last_seen_ingest_time_ms;
            status.ingest_event_skew_seconds = ingest_event_skew_seconds;
            status.pipeline_lag_seconds = pipeline_lag_seconds;
            status.updated_at_ms = now_ms;
        }
        DeviceStatusUpdate::EventDetector {
            last_event_detected_at_ms,
            last_processed_event_time_ms,
        } => {
            status.last_event_detected_at_ms = Some(last_event_detected_at_ms);
            status.last_processed_event_time_ms = last_processed_event_time_ms;
            status.updated_at_ms = now_ms;
        }
        DeviceStatusUpdate::Aggregator {
            last_aggregate_computed_at_ms,
            coverage_pct_last_hour,
        } => {
            status.last_aggregate_computed_at_ms = Some(last_aggregate_computed_at_ms);
            status.coverage_pct_last_hour = coverage_pct_last_hour;
            status.updated_at_ms = now_ms;
        }
        DeviceStatusUpdate::InsightGenerator {
            last_insight_generated_at_ms,
        } => {
            status.last_insight_generated_at_ms = Some(last_insight_generated_at_ms);
            status.updated_at_ms = now_ms;
        }
        DeviceStatusUpdate::Error {
            last_error_at_ms,
            last_error_code,
            error_record,
        } => {
            status.last_error_at_ms = Some(last_error_at_ms);
            status.last_error_code = Some(last_error_code);
            status.last_errors.push(error_record);
            if status.last_errors.len() > 10 {
                status.last_errors.remove(0);
            }
            status.updated_at_ms = now_ms;
        }
    }
}

pub fn truncate_error_message(message: &str) -> String {
    if message.len() <= 256 {
        message.to_string()
    } else {
        message.chars().take(256).collect()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::plant_insights::SensorStatusSummary;

    fn create_test_status() -> DeviceStatus {
        DeviceStatus {
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
        }
    }

    #[test]
    fn test_apply_status_updater() {
        let mut status = create_test_status();
        let update = DeviceStatusUpdate::StatusUpdater {
            last_seen_event_time_ms: 2000,
            last_seen_ingest_time_ms: 2010,
            ingest_event_skew_seconds: 10,
            pipeline_lag_seconds: 5,
        };
        apply_status_update(&mut status, update, 2020);
        assert_eq!(status.last_seen_event_time_ms, 2000);
        assert_eq!(status.last_seen_ingest_time_ms, 2010);
        assert_eq!(status.ingest_event_skew_seconds, 10);
        assert_eq!(status.pipeline_lag_seconds, 5);
        assert_eq!(status.updated_at_ms, 2020);
    }

    #[test]
    fn test_apply_event_detector() {
        let mut status = create_test_status();
        let update = DeviceStatusUpdate::EventDetector {
            last_event_detected_at_ms: 3000,
            last_processed_event_time_ms: 2900,
        };
        apply_status_update(&mut status, update, 3010);
        assert_eq!(status.last_event_detected_at_ms, Some(3000));
        assert_eq!(status.last_processed_event_time_ms, 2900);
        assert_eq!(status.updated_at_ms, 3010);
    }

    #[test]
    fn test_apply_error_truncates_list() {
        let mut status = create_test_status();
        for i in 0..12 {
            let update = DeviceStatusUpdate::Error {
                last_error_at_ms: 1000 + i,
                last_error_code: format!("ERROR_{}", i),
                error_record: ErrorRecord {
                    timestamp_ms: 1000 + i,
                    error_code: format!("ERROR_{}", i),
                    error_message: format!("Error message {}", i),
                },
            };
            apply_status_update(&mut status, update, 1000 + i);
        }
        assert_eq!(status.last_errors.len(), 10);
        assert_eq!(status.last_errors[0].error_code, "ERROR_2");
        assert_eq!(status.last_errors[9].error_code, "ERROR_11");
    }

    #[test]
    fn test_truncate_error_message_short() {
        let message = "Short message";
        assert_eq!(truncate_error_message(message), "Short message");
    }

    #[test]
    fn test_truncate_error_message_long() {
        let message = "a".repeat(300);
        let truncated = truncate_error_message(&message);
        assert_eq!(truncated.len(), 256);
    }
}
