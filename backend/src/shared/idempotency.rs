use crate::plant_insights::ProcessedReading;

pub fn generate_reading_id(batch_id: &str, timestamp_ms: i64) -> String {
    format!("{}#{}", batch_id, timestamp_ms)
}

pub fn create_processed_reading(
    batch_id: &str,
    timestamp_ms: i64,
    hardware_id: &str,
    ttl: i64,
) -> ProcessedReading {
    ProcessedReading {
        reading_id: generate_reading_id(batch_id, timestamp_ms),
        hardware_id: hardware_id.to_string(),
        event_processed_at_ms: None,
        aggregate_processed_at_ms: None,
        status_processed_at_ms: None,
        ttl,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_generate_reading_id() {
        let id = generate_reading_id("batch123", 1234567890);
        assert_eq!(id, "batch123#1234567890");
    }

    #[test]
    fn test_create_processed_reading() {
        let reading = create_processed_reading("batch123", 1234567890, "device1", 2592000);
        assert_eq!(reading.reading_id, "batch123#1234567890");
        assert_eq!(reading.hardware_id, "device1");
        assert_eq!(reading.ttl, 2592000);
        assert!(reading.event_processed_at_ms.is_none());
        assert!(reading.aggregate_processed_at_ms.is_none());
        assert!(reading.status_processed_at_ms.is_none());
    }
}
