use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Device domain type representing a registered ESP32 device
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Device {
    pub hardware_id: String,
    pub confirmation_id: String,
    pub friendly_name: Option<String>,
    pub firmware_version: String,
    pub capabilities: Capabilities,
    pub first_registered_at: String,
    pub last_seen_at: String,
    pub last_boot_id: String,
}

/// Device capabilities including sensors and features
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Capabilities {
    pub sensors: Vec<String>,
    pub features: HashMap<String, bool>,
}

/// Sensor reading from a device
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Reading {
    pub batch_id: String,
    pub hardware_id: String,
    pub timestamp_ms: i64,
    pub boot_id: String,
    pub firmware_version: String,
    pub friendly_name: Option<String>,
    pub sensors: SensorValues,
    pub sensor_status: SensorStatus,
}

/// Sensor values from various sensors
#[derive(Debug, Clone, Serialize, Deserialize)]
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

/// Status of each sensor (ok or error)
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SensorStatus {
    pub bme280: String,
    pub ds18b20: String,
    pub soil_moisture: String,
}

/// API Key domain type
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ApiKey {
    pub key_id: String,
    pub api_key_hash: String,
    pub created_at: String,
    pub last_used_at: Option<String>,
    pub is_active: bool,
    pub description: Option<String>,
}
