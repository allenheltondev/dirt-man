use base64::{engine::general_purpose, Engine as _};
use serde::{Deserialize, Serialize};

/// Cursor for device list pagination
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DeviceListCursor {
    pub hardware_id: String,
    pub gsi1sk: String,
}

/// Cursor for readings list pagination
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ReadingsCursor {
    pub hardware_id: String,
    pub ts_batch: String,
}

/// Cursor for API key list pagination
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ApiKeyListCursor {
    pub key_id: String,
    pub gsi1sk: String,
}

/// Cursor encoding/decoding error
#[derive(Debug, Clone)]
pub struct CursorError {
    pub message: String,
}

impl CursorError {
    pub fn new(message: impl Into<String>) -> Self {
        Self {
            message: message.into(),
        }
    }
}

impl std::fmt::Display for CursorError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Cursor error: {}", self.message)
    }
}

impl std::error::Error for CursorError {}

/// Encode device list cursor to base64 string
pub fn encode_device_cursor(hardware_id: &str, gsi1sk: &str) -> Result<String, CursorError> {
    let cursor = DeviceListCursor {
        hardware_id: hardware_id.to_string(),
        gsi1sk: gsi1sk.to_string(),
    };

    let json = serde_json::to_string(&cursor)
        .map_err(|e| CursorError::new(format!("Failed to serialize cursor: {}", e)))?;

    Ok(general_purpose::STANDARD.encode(json.as_bytes()))
}

/// Decode device list cursor from base64 string
pub fn decode_device_cursor(cursor: &str) -> Result<DeviceListCursor, CursorError> {
    let bytes = general_purpose::STANDARD
        .decode(cursor)
        .map_err(|e| CursorError::new(format!("Failed to decode base64: {}", e)))?;

    let json = String::from_utf8(bytes)
        .map_err(|e| CursorError::new(format!("Failed to decode UTF-8: {}", e)))?;

    serde_json::from_str(&json)
        .map_err(|e| CursorError::new(format!("Failed to deserialize cursor: {}", e)))
}

/// Encode readings cursor to base64 string
pub fn encode_readings_cursor(hardware_id: &str, ts_batch: &str) -> Result<String, CursorError> {
    let cursor = ReadingsCursor {
        hardware_id: hardware_id.to_string(),
        ts_batch: ts_batch.to_string(),
    };

    let json = serde_json::to_string(&cursor)
        .map_err(|e| CursorError::new(format!("Failed to serialize cursor: {}", e)))?;

    Ok(general_purpose::STANDARD.encode(json.as_bytes()))
}

/// Decode readings cursor from base64 string
pub fn decode_readings_cursor(cursor: &str) -> Result<ReadingsCursor, CursorError> {
    let bytes = general_purpose::STANDARD
        .decode(cursor)
        .map_err(|e| CursorError::new(format!("Failed to decode base64: {}", e)))?;

    let json = String::from_utf8(bytes)
        .map_err(|e| CursorError::new(format!("Failed to decode UTF-8: {}", e)))?;

    serde_json::from_str(&json)
        .map_err(|e| CursorError::new(format!("Failed to deserialize cursor: {}", e)))
}

/// Encode API key list cursor to base64 string
pub fn encode_api_key_cursor(key_id: &str, gsi1sk: &str) -> Result<String, CursorError> {
    let cursor = ApiKeyListCursor {
        key_id: key_id.to_string(),
        gsi1sk: gsi1sk.to_string(),
    };

    let json = serde_json::to_string(&cursor)
        .map_err(|e| CursorError::new(format!("Failed to serialize cursor: {}", e)))?;

    Ok(general_purpose::STANDARD.encode(json.as_bytes()))
}

/// Decode API key list cursor from base64 string
pub fn decode_api_key_cursor(cursor: &str) -> Result<ApiKeyListCursor, CursorError> {
    let bytes = general_purpose::STANDARD
        .decode(cursor)
        .map_err(|e| CursorError::new(format!("Failed to decode base64: {}", e)))?;

    let json = String::from_utf8(bytes)
        .map_err(|e| CursorError::new(format!("Failed to decode UTF-8: {}", e)))?;

    serde_json::from_str(&json)
        .map_err(|e| CursorError::new(format!("Failed to deserialize cursor: {}", e)))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_encode_decode_device_cursor() {
        let hardware_id = "AA:BB:CC:DD:EE:FF";
        let gsi1sk = "2024-01-15T14:22:00Z";

        let encoded = encode_device_cursor(hardware_id, gsi1sk).unwrap();
        let decoded = decode_device_cursor(&encoded).unwrap();

        assert_eq!(decoded.hardware_id, hardware_id);
        assert_eq!(decoded.gsi1sk, gsi1sk);
    }

    #[test]
    fn test_encode_decode_readings_cursor() {
        let hardware_id = "AA:BB:CC:DD:EE:FF";
        let ts_batch = "1704067800000#batch_id_123";

        let encoded = encode_readings_cursor(hardware_id, ts_batch).unwrap();
        let decoded = decode_readings_cursor(&encoded).unwrap();

        assert_eq!(decoded.hardware_id, hardware_id);
        assert_eq!(decoded.ts_batch, ts_batch);
    }

    #[test]
    fn test_decode_invalid_cursor() {
        // Invalid base64
        assert!(decode_device_cursor("not-valid-base64!@#").is_err());

        // Valid base64 but invalid JSON
        let invalid_json = general_purpose::STANDARD.encode(b"not json");
        assert!(decode_device_cursor(&invalid_json).is_err());

        // Valid JSON but wrong structure
        let wrong_structure = general_purpose::STANDARD.encode(b"{\"wrong\":\"fields\"}");
        assert!(decode_device_cursor(&wrong_structure).is_err());
    }

    #[test]
    fn test_cursor_roundtrip_with_special_chars() {
        let hardware_id = "AA:BB:CC:DD:EE:FF";
        let gsi1sk = "2024-01-15T14:22:00+00:00";

        let encoded = encode_device_cursor(hardware_id, gsi1sk).unwrap();
        let decoded = decode_device_cursor(&encoded).unwrap();

        assert_eq!(decoded.hardware_id, hardware_id);
        assert_eq!(decoded.gsi1sk, gsi1sk);
    }

    #[test]
    fn test_encode_decode_api_key_cursor() {
        let key_id = "a1b2c3d4-e5f6-7890-abcd-ef1234567890";
        let gsi1sk = "2024-01-15T10:30:00Z";

        let encoded = encode_api_key_cursor(key_id, gsi1sk).unwrap();
        let decoded = decode_api_key_cursor(&encoded).unwrap();

        assert_eq!(decoded.key_id, key_id);
        assert_eq!(decoded.gsi1sk, gsi1sk);
    }

    #[test]
    fn test_decode_invalid_api_key_cursor() {
        // Invalid base64
        assert!(decode_api_key_cursor("not-valid-base64!@#").is_err());

        // Valid base64 but invalid JSON
        let invalid_json = general_purpose::STANDARD.encode(b"not json");
        assert!(decode_api_key_cursor(&invalid_json).is_err());

        // Valid JSON but wrong structure
        let wrong_structure = general_purpose::STANDARD.encode(b"{\"wrong\":\"fields\"}");
        assert!(decode_api_key_cursor(&wrong_structure).is_err());
    }
}
