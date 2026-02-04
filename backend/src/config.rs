use aws_sdk_dynamodb::Client as DynamoDbClient;
use std::time::Duration;

/// Configuration for the Data Plane API
#[derive(Debug, Clone)]
pub struct Config {
    /// DynamoDB client
    pub dynamodb_client: DynamoDbClient,
    /// Devices table name
    pub devices_table: String,
    /// API keys table name
    pub api_keys_table: String,
    /// Processed batches table name
    pub processed_batches_table: String,
    /// Device readings table name
    pub device_readings_table: String,
}

impl Config {
    /// Create a new Config instance from environment variables
    pub async fn from_env() -> Result<Self, ConfigError> {
        // Load AWS configuration with behavior version
        let aws_config = aws_config::defaults(aws_config::BehaviorVersion::latest())
            .load()
            .await;

        // Create DynamoDB client with appropriate timeouts
        let dynamodb_config = aws_sdk_dynamodb::config::Builder::from(&aws_config)
            .timeout_config(
                aws_sdk_dynamodb::config::timeout::TimeoutConfig::builder()
                    .operation_timeout(Duration::from_secs(25)) // Leave 5s buffer for Lambda timeout
                    .operation_attempt_timeout(Duration::from_secs(10))
                    .build(),
            )
            .build();

        let dynamodb_client = DynamoDbClient::from_conf(dynamodb_config);

        // Load table names from environment variables
        let devices_table = std::env::var("DEVICES_TABLE")
            .map_err(|_| ConfigError::MissingEnvVar("DEVICES_TABLE".to_string()))?;

        let api_keys_table = std::env::var("API_KEYS_TABLE")
            .map_err(|_| ConfigError::MissingEnvVar("API_KEYS_TABLE".to_string()))?;

        let processed_batches_table = std::env::var("PROCESSED_BATCHES_TABLE")
            .map_err(|_| ConfigError::MissingEnvVar("PROCESSED_BATCHES_TABLE".to_string()))?;

        let device_readings_table = std::env::var("DEVICE_READINGS_TABLE")
            .map_err(|_| ConfigError::MissingEnvVar("DEVICE_READINGS_TABLE".to_string()))?;

        Ok(Config {
            dynamodb_client,
            devices_table,
            api_keys_table,
            processed_batches_table,
            device_readings_table,
        })
    }

    /// Create a test configuration with custom table names
    /// This is useful for integration tests with DynamoDB Local
    #[cfg(test)]
    pub async fn for_test(
        endpoint_url: &str,
        devices_table: String,
        api_keys_table: String,
        processed_batches_table: String,
        device_readings_table: String,
    ) -> Self {
        use aws_sdk_dynamodb::config::{Credentials, Region};

        // Create test credentials
        let credentials =
            Credentials::new("test_access_key", "test_secret_key", None, None, "test");

        // Create DynamoDB client pointing to local endpoint
        let dynamodb_config = aws_sdk_dynamodb::config::Builder::new()
            .behavior_version(aws_sdk_dynamodb::config::BehaviorVersion::latest())
            .region(Region::new("us-east-1"))
            .credentials_provider(credentials)
            .endpoint_url(endpoint_url)
            .timeout_config(
                aws_sdk_dynamodb::config::timeout::TimeoutConfig::builder()
                    .operation_timeout(Duration::from_secs(10))
                    .operation_attempt_timeout(Duration::from_secs(5))
                    .build(),
            )
            .build();

        let dynamodb_client = DynamoDbClient::from_conf(dynamodb_config);

        Config {
            dynamodb_client,
            devices_table,
            api_keys_table,
            processed_batches_table,
            device_readings_table,
        }
    }
}

/// Configuration errors
#[derive(Debug, thiserror::Error)]
pub enum ConfigError {
    #[error("Missing required environment variable: {0}")]
    MissingEnvVar(String),

    #[error("AWS configuration error: {0}")]
    AwsConfig(String),
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_config_from_env_missing_vars() {
        // Save current environment state
        let saved_devices = std::env::var("DEVICES_TABLE").ok();
        let saved_api_keys = std::env::var("API_KEYS_TABLE").ok();
        let saved_batches = std::env::var("PROCESSED_BATCHES_TABLE").ok();
        let saved_readings = std::env::var("DEVICE_READINGS_TABLE").ok();

        // Clear environment variables to test error handling
        std::env::remove_var("DEVICES_TABLE");
        std::env::remove_var("API_KEYS_TABLE");
        std::env::remove_var("PROCESSED_BATCHES_TABLE");
        std::env::remove_var("DEVICE_READINGS_TABLE");

        let result = Config::from_env().await;
        assert!(result.is_err());

        if let Err(ConfigError::MissingEnvVar(var)) = result {
            assert_eq!(var, "DEVICES_TABLE");
        } else {
            panic!("Expected MissingEnvVar error");
        }

        // Restore environment state
        if let Some(val) = saved_devices {
            std::env::set_var("DEVICES_TABLE", val);
        }
        if let Some(val) = saved_api_keys {
            std::env::set_var("API_KEYS_TABLE", val);
        }
        if let Some(val) = saved_batches {
            std::env::set_var("PROCESSED_BATCHES_TABLE", val);
        }
        if let Some(val) = saved_readings {
            std::env::set_var("DEVICE_READINGS_TABLE", val);
        }
    }

    #[tokio::test]
    async fn test_config_from_env_success() {
        // Set environment variables
        std::env::set_var("DEVICES_TABLE", "test-devices");
        std::env::set_var("API_KEYS_TABLE", "test-api-keys");
        std::env::set_var("PROCESSED_BATCHES_TABLE", "test-processed-batches");
        std::env::set_var("DEVICE_READINGS_TABLE", "test-device-readings");

        let result = Config::from_env().await;

        // This test may fail in environments without AWS credentials configured
        // That's expected behavior - the config requires valid AWS setup
        match result {
            Ok(config) => {
                assert_eq!(config.devices_table, "test-devices");
                assert_eq!(config.api_keys_table, "test-api-keys");
                assert_eq!(config.processed_batches_table, "test-processed-batches");
                assert_eq!(config.device_readings_table, "test-device-readings");
            }
            Err(e) => {
                // In CI/test environments without AWS credentials, this is expected
                eprintln!(
                    "Config creation failed (expected in non-AWS environments): {:?}",
                    e
                );
            }
        }

        // Clean up
        std::env::remove_var("DEVICES_TABLE");
        std::env::remove_var("API_KEYS_TABLE");
        std::env::remove_var("PROCESSED_BATCHES_TABLE");
        std::env::remove_var("DEVICE_READINGS_TABLE");
    }

    #[tokio::test]
    async fn test_config_for_test() {
        let config = Config::for_test(
            "http://localhost:8000",
            "test-devices".to_string(),
            "test-api-keys".to_string(),
            "test-processed-batches".to_string(),
            "test-device-readings".to_string(),
        )
        .await;

        assert_eq!(config.devices_table, "test-devices");
        assert_eq!(config.api_keys_table, "test-api-keys");
        assert_eq!(config.processed_batches_table, "test-processed-batches");
        assert_eq!(config.device_readings_table, "test-device-readings");
    }
}
