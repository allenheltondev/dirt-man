use crate::plant_insights::{SensorStatus, SensorValues};

pub const TEMP_MIN_C: f64 = -40.0;
pub const TEMP_MAX_C: f64 = 85.0;
pub const HUMIDITY_MIN_PCT: f64 = 0.0;
pub const HUMIDITY_MAX_PCT: f64 = 100.0;
pub const PRESSURE_MIN_HPA: f64 = 300.0;
pub const PRESSURE_MAX_HPA: f64 = 1100.0;
pub const SOIL_MOISTURE_MIN_PCT: f64 = 0.0;
pub const SOIL_MOISTURE_MAX_PCT: f64 = 100.0;

pub fn validate_temperature(temp_c: Option<f64>) -> SensorStatus {
    match temp_c {
        None => SensorStatus::Missing,
        Some(t) if t < TEMP_MIN_C || t > TEMP_MAX_C => SensorStatus::OutOfRange,
        Some(_) => SensorStatus::Ok,
    }
}

pub fn validate_humidity(humidity_pct: Option<f64>) -> SensorStatus {
    match humidity_pct {
        None => SensorStatus::Missing,
        Some(h) if h < HUMIDITY_MIN_PCT || h > HUMIDITY_MAX_PCT => SensorStatus::OutOfRange,
        Some(_) => SensorStatus::Ok,
    }
}

pub fn validate_pressure(pressure_hpa: Option<f64>) -> SensorStatus {
    match pressure_hpa {
        None => SensorStatus::Missing,
        Some(p) if p < PRESSURE_MIN_HPA || p > PRESSURE_MAX_HPA => SensorStatus::OutOfRange,
        Some(_) => SensorStatus::Ok,
    }
}

pub fn validate_soil_moisture(soil_moisture_pct: Option<f64>) -> SensorStatus {
    match soil_moisture_pct {
        None => SensorStatus::Missing,
        Some(m) if m < SOIL_MOISTURE_MIN_PCT || m > SOIL_MOISTURE_MAX_PCT => SensorStatus::OutOfRange,
        Some(_) => SensorStatus::Ok,
    }
}

pub fn is_stale(recent_values: &[f64]) -> bool {
    if recent_values.len() < 6 {
        return false;
    }
    let last_6 = &recent_values[recent_values.len() - 6..];
    let first = last_6[0];
    last_6.iter().all(|&v| (v - first).abs() < 0.001)
}

pub fn is_noisy(prev_value: f64, current_value: f64) -> bool {
    if prev_value == 0.0 {
        return false;
    }
    let change_pct = ((current_value - prev_value).abs() / prev_value.abs()) * 100.0;
    change_pct > 50.0
}

pub fn is_missing_sensor(last_seen_ms: i64, now_ms: i64, _expected_interval_sec: i64) -> bool {
    let hours_since_seen = (now_ms - last_seen_ms) / (1000 * 3600);
    hours_since_seen >= 2
}

pub fn validate_sensor_values(sensors: &SensorValues) -> (SensorStatus, SensorStatus, SensorStatus) {
    let bme280_status = validate_temperature(sensors.bme280_temp_c);
    let ds18b20_status = validate_temperature(sensors.ds18b20_temp_c);
    let soil_moisture_status = validate_soil_moisture(sensors.soil_moisture_pct);
    (bme280_status, ds18b20_status, soil_moisture_status)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_validate_temperature_ok() {
        assert_eq!(validate_temperature(Some(25.0)), SensorStatus::Ok);
        assert_eq!(validate_temperature(Some(-40.0)), SensorStatus::Ok);
        assert_eq!(validate_temperature(Some(85.0)), SensorStatus::Ok);
    }

    #[test]
    fn test_validate_temperature_out_of_range() {
        assert_eq!(validate_temperature(Some(-41.0)), SensorStatus::OutOfRange);
        assert_eq!(validate_temperature(Some(86.0)), SensorStatus::OutOfRange);
    }

    #[test]
    fn test_is_stale_identical_readings() {
        let values = vec![25.0, 25.0, 25.0, 25.0, 25.0, 25.0];
        assert!(is_stale(&values));
    }

    #[test]
    fn test_is_noisy_large_change() {
        assert!(is_noisy(25.0, 40.0));
    }

    #[test]
    fn test_is_missing_sensor_at_threshold() {
        let now_ms = 1000000;
        let last_seen_ms = now_ms - (2 * 3600 * 1000);
        assert!(is_missing_sensor(last_seen_ms, now_ms, 300));
    }

    #[test]
    fn test_validate_sensor_values() {
        let sensors = SensorValues {
            bme280_temp_c: Some(25.0),
            ds18b20_temp_c: Some(24.5),
            humidity_pct: Some(60.0),
            pressure_hpa: Some(1013.0),
            soil_moisture_pct: Some(45.0),
        };
        let (bme280, ds18b20, soil) = validate_sensor_values(&sensors);
        assert_eq!(bme280, SensorStatus::Ok);
        assert_eq!(ds18b20, SensorStatus::Ok);
        assert_eq!(soil, SensorStatus::Ok);
    }
}
