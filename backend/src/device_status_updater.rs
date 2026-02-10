use aws_lambda_events::event::dynamodb::{Event as DynamoDbEvent, EventRecord};
use aws_sdk_dynamodb::Client as DynamoDbClient;
use lambda_runtime::{run, service_fn, Error, LambdaEvent};
use std::collections::HashMap;
use std::env;
use tracing::{error, info, warn};

use esp32_backend::plant_insights::Reading;

#[tokio::main]
async fn main() -> Result<(), Error> {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::from_default_env()
                .add_directive(tracing::Level::INFO.into()),
        )
        .init();

    let config = aws_config::defaults(aws_config::BehaviorVersion::latest())
        .load()
        .await;
    let dynamodb_client = DynamoDbClient::new(&config);

    let device_status_table = env::var("DEVICE_STATUS_TABLE")
        .expect("DEVICE_STATUS_TABLE environment variable must be set");
    let device_profiles_table = env::var("DEVICE_PROFILES_TABLE")
        .expect("DEVICE_PROFILES_TABLE environment variable must be set");
    let processed_readings_table = env::var("PROCESSED_READINGS_TABLE")
        .expect("PROCESSED_READINGS_TABLE environment variable must be set");

    run(service_fn(|event: LambdaEvent<DynamoDbEvent>| {
        function_handler(
            event,
            dynamodb_client.clone(),
            device_status_table.clone(),
            device_profiles_table.clone(),
            processed_readings_table.clone(),
        )
    }))
    .await
}

async fn function_handler(
    event: LambdaEvent<DynamoDbEvent>,
    dynamodb_client: DynamoDbClient,
    device_status_table: String,
    device_profiles_table: String,
    processed_readings_table: String,
) -> Result<(), Error> {
    info!(
        "Processing DynamoDB stream batch with {} records",
        event.payload.records.len()
    );

    let mut processed_count = 0;
    let mut skipped_count = 0;
    let mut error_count = 0;

    for record in event.payload.records {
        match process_record(
            &record,
            &dynamodb_client,
            &device_status_table,
            &device_profiles_table,
            &processed_readings_table,
        )
        .await
        {
            Ok(true) => processed_count += 1,
            Ok(false) => skipped_count += 1,
            Err(e) => {
                error_count += 1;
                error!("Error processing record: {}", e);
                // Continue processing other records
            }
        }
    }

    info!(
        "Batch processing complete: {} processed, {} skipped, {} errors",
        processed_count, skipped_count, error_count
    );

    Ok(())
}

async fn process_record(
    record: &EventRecord,
    dynamodb_client: &DynamoDbClient,
    device_status_table: &str,
    device_profiles_table: &str,
    processed_readings_table: &str,
) -> Result<bool, Error> {
    // Only process INSERT and MODIFY events
    let event_name = record.event_name.as_str();
    if event_name != "INSERT" && event_name != "MODIFY" {
        return Ok(false);
    }

    // Extract the new image - use serde_dynamo to convert
    let new_image = &record.change.new_image;

    // Parse the reading from DynamoDB format using serde_dynamo
    let reading: Reading = serde_dynamo::from_item(new_image.clone())
        .map_err(|e| format!("Failed to deserialize reading: {}", e))?;

    // Generate reading_id
    let reading_id = reading.reading_id();

    // Check if already processed for status stage
    if is_status_processed(dynamodb_client, processed_readings_table, &reading_id).await? {
        info!(
            "Reading {} already processed for status stage, skipping",
            reading_id
        );
        return Ok(false);
    }

    // Fetch device profile to get expected_interval_sec
    let expected_interval_sec =
        get_expected_interval(dynamodb_client, device_profiles_table, &reading.hardware_id).await?;

    // Process the status update
    update_device_status(
        dynamodb_client,
        device_status_table,
        &reading,
        expected_interval_sec,
    )
    .await?;

    // Mark as processed for status stage
    mark_status_processed(dynamodb_client, processed_readings_table, &reading).await?;

    info!(
        "Successfully processed reading {} for device {}",
        reading_id, reading.hardware_id
    );

    Ok(true)
}

/// Check if a reading has been processed for the status stage
async fn is_status_processed(
    dynamodb_client: &DynamoDbClient,
    processed_readings_table: &str,
    reading_id: &str,
) -> Result<bool, Error> {
    let result = dynamodb_client
        .get_item()
        .table_name(processed_readings_table)
        .key(
            "reading_id",
            aws_sdk_dynamodb::types::AttributeValue::S(reading_id.to_string()),
        )
        .send()
        .await
        .map_err(|e| format!("Failed to check processed reading: {}", e))?;

    if let Some(item) = result.item {
        // Check if status_processed_at_ms is set
        Ok(item.contains_key("status_processed_at_ms"))
    } else {
        Ok(false)
    }
}

/// Mark a reading as processed for the status stage
async fn mark_status_processed(
    dynamodb_client: &DynamoDbClient,
    processed_readings_table: &str,
    reading: &Reading,
) -> Result<(), Error> {
    let now_ms = chrono::Utc::now().timestamp_millis();
    let reading_id = reading.reading_id();

    // Calculate TTL (30 days from now)
    let ttl = (chrono::Utc::now() + chrono::Duration::days(30)).timestamp();

    // Use conditional update to set status_processed_at_ms if not already set
    let update_expression =
        "SET status_processed_at_ms = if_not_exists(status_processed_at_ms, :now), \
         hardware_id = if_not_exists(hardware_id, :hardware_id), \
         #ttl = if_not_exists(#ttl, :ttl)";

    let mut expression_values = HashMap::new();
    expression_values.insert(
        ":now".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::N(now_ms.to_string()),
    );
    expression_values.insert(
        ":hardware_id".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::S(reading.hardware_id.clone()),
    );
    expression_values.insert(
        ":ttl".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::N(ttl.to_string()),
    );

    let mut expression_names = HashMap::new();
    expression_names.insert("#ttl".to_string(), "ttl".to_string());

    dynamodb_client
        .update_item()
        .table_name(processed_readings_table)
        .key(
            "reading_id",
            aws_sdk_dynamodb::types::AttributeValue::S(reading_id.clone()),
        )
        .update_expression(update_expression)
        .set_expression_attribute_values(Some(expression_values))
        .set_expression_attribute_names(Some(expression_names))
        .send()
        .await
        .map_err(|e| format!("Failed to mark reading as processed: {}", e))?;

    info!(
        "Marked reading {} as processed for status stage",
        reading_id
    );

    Ok(())
}

/// Get expected_interval_sec from device profile, defaulting to 300
async fn get_expected_interval(
    dynamodb_client: &DynamoDbClient,
    device_profiles_table: &str,
    hardware_id: &str,
) -> Result<i64, Error> {
    let result = dynamodb_client
        .get_item()
        .table_name(device_profiles_table)
        .key(
            "hardware_id",
            aws_sdk_dynamodb::types::AttributeValue::S(hardware_id.to_string()),
        )
        .send()
        .await
        .map_err(|e| format!("Failed to get device profile: {}", e))?;

    if let Some(item) = result.item {
        if let Some(interval) = item.get("expected_interval_sec") {
            if let Ok(interval_str) = interval.as_n() {
                if let Ok(interval_val) = interval_str.parse::<i64>() {
                    return Ok(interval_val);
                }
            }
        }
    }

    // Default to 300 seconds if profile doesn't exist or doesn't have the field
    Ok(300)
}

/// Update device status based on a new reading
async fn update_device_status(
    dynamodb_client: &DynamoDbClient,
    device_status_table: &str,
    reading: &Reading,
    expected_interval_sec: i64,
) -> Result<(), Error> {
    let now_ms = chrono::Utc::now().timestamp_millis();

    // Compute derived fields
    let ingest_event_skew_seconds = (reading.ingest_time_ms - reading.timestamp_ms) / 1000;
    let pipeline_lag_seconds = (now_ms - reading.timestamp_ms) / 1000;

    // Log clock skew warning if event_time is more than 5 minutes ahead of ingest_time
    if reading.timestamp_ms > reading.ingest_time_ms + (5 * 60 * 1000) {
        warn!(
            "Clock skew detected for device {}: event_time is {} ms ahead of ingest_time",
            reading.hardware_id,
            reading.timestamp_ms - reading.ingest_time_ms
        );
    }

    // Derive sensor_status_summary from reading sensor statuses
    let sensor_status_summary = derive_sensor_status_summary(reading);

    // Derive health_category based on last_seen_ingest_time
    let health_category = derive_health_category(reading.ingest_time_ms, now_ms);

    // Build update expression for field-owned updates
    // Status Updater owns: last_seen_event_time_ms, last_seen_ingest_time_ms,
    // ingest_event_skew_seconds, pipeline_lag_seconds, expected_interval_sec,
    // sensor_status_summary, health_category, updated_at_ms
    let update_expression = "SET \
        last_seen_event_time_ms = :event_time, \
        last_seen_ingest_time_ms = :ingest_time, \
        ingest_event_skew_seconds = :skew, \
        pipeline_lag_seconds = :lag, \
        expected_interval_sec = :interval, \
        last_processed_event_time_ms = :event_time, \
        sensor_status_summary = :sensor_summary, \
        health_category = :health_cat, \
        updated_at_ms = :now";

    let mut expression_values = HashMap::new();
    expression_values.insert(
        ":event_time".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::N(reading.timestamp_ms.to_string()),
    );
    expression_values.insert(
        ":ingest_time".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::N(reading.ingest_time_ms.to_string()),
    );
    expression_values.insert(
        ":skew".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::N(ingest_event_skew_seconds.to_string()),
    );
    expression_values.insert(
        ":lag".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::N(pipeline_lag_seconds.to_string()),
    );
    expression_values.insert(
        ":interval".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::N(expected_interval_sec.to_string()),
    );
    expression_values.insert(
        ":sensor_summary".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::S(sensor_status_summary.to_string()),
    );
    expression_values.insert(
        ":health_cat".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::S(health_category.to_string()),
    );
    expression_values.insert(
        ":now".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::N(now_ms.to_string()),
    );

    // Execute update
    dynamodb_client
        .update_item()
        .table_name(device_status_table)
        .key(
            "hardware_id",
            aws_sdk_dynamodb::types::AttributeValue::S(reading.hardware_id.clone()),
        )
        .update_expression(update_expression)
        .set_expression_attribute_values(Some(expression_values))
        .send()
        .await
        .map_err(|e| format!("Failed to update device status: {}", e))?;

    info!(
        "Updated device status for device {}: event_time={}, ingest_time={}, skew={}s, lag={}s, sensor_summary={}, health={}",
        reading.hardware_id,
        reading.timestamp_ms,
        reading.ingest_time_ms,
        ingest_event_skew_seconds,
        pipeline_lag_seconds,
        sensor_status_summary,
        health_category
    );

    Ok(())
}

/// Derive sensor_status_summary from reading sensor statuses
fn derive_sensor_status_summary(reading: &Reading) -> &'static str {
    use esp32_backend::plant_insights::SensorStatus;

    // Check if any sensor is missing
    let has_missing = reading.sensor_status.bme280 == SensorStatus::Missing
        || reading.sensor_status.ds18b20 == SensorStatus::Missing
        || reading.sensor_status.soil_moisture == SensorStatus::Missing;

    if has_missing {
        return "missing";
    }

    // Check if any sensor is degraded (stale, out_of_range, noisy)
    let has_degraded = reading.sensor_status.bme280 != SensorStatus::Ok
        || reading.sensor_status.ds18b20 != SensorStatus::Ok
        || reading.sensor_status.soil_moisture != SensorStatus::Ok;

    if has_degraded {
        return "degraded";
    }

    "ok"
}

/// Derive health_category based on last_seen_ingest_time
fn derive_health_category(last_seen_ingest_time_ms: i64, now_ms: i64) -> &'static str {
    let hours_since_seen = (now_ms - last_seen_ingest_time_ms) / (1000 * 3600);

    if hours_since_seen <= 2 {
        "healthy"
    } else if hours_since_seen <= 6 {
        "stale"
    } else {
        "missing"
    }
}

/// Record an error in device status
/// This helper can be used by all pipelines to track errors
pub async fn record_device_error(
    dynamodb_client: &DynamoDbClient,
    device_status_table: &str,
    hardware_id: &str,
    error_code: &str,
    error_message: &str,
) -> Result<(), Error> {
    let now_ms = chrono::Utc::now().timestamp_millis();

    // Truncate error message to 256 characters
    let truncated_message = if error_message.len() <= 256 {
        error_message.to_string()
    } else {
        error_message.chars().take(256).collect()
    };

    // Create error record as a map
    let mut error_record_map = HashMap::new();
    error_record_map.insert(
        "timestamp_ms".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::N(now_ms.to_string()),
    );
    error_record_map.insert(
        "error_code".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::S(error_code.to_string()),
    );
    error_record_map.insert(
        "error_message".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::S(truncated_message.clone()),
    );

    // Build update expression to append error to last_errors list (max 10)
    // We use a list_append with conditional logic to maintain max 10 items
    let update_expression = "SET \
        last_error_at_ms = :now, \
        last_error_code = :code, \
        last_errors = list_append(if_not_exists(last_errors, :empty_list), :new_error), \
        updated_at_ms = :now";

    let mut expression_values = HashMap::new();
    expression_values.insert(
        ":now".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::N(now_ms.to_string()),
    );
    expression_values.insert(
        ":code".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::S(error_code.to_string()),
    );
    expression_values.insert(
        ":empty_list".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::L(vec![]),
    );
    expression_values.insert(
        ":new_error".to_string(),
        aws_sdk_dynamodb::types::AttributeValue::L(vec![
            aws_sdk_dynamodb::types::AttributeValue::M(error_record_map),
        ]),
    );

    // Execute update
    dynamodb_client
        .update_item()
        .table_name(device_status_table)
        .key(
            "hardware_id",
            aws_sdk_dynamodb::types::AttributeValue::S(hardware_id.to_string()),
        )
        .update_expression(update_expression)
        .set_expression_attribute_values(Some(expression_values))
        .send()
        .await
        .map_err(|e| format!("Failed to record device error: {}", e))?;

    // Now trim the list to max 10 items if needed
    // We need to read the current list, trim it, and write it back
    let result = dynamodb_client
        .get_item()
        .table_name(device_status_table)
        .key(
            "hardware_id",
            aws_sdk_dynamodb::types::AttributeValue::S(hardware_id.to_string()),
        )
        .send()
        .await
        .map_err(|e| format!("Failed to get device status for trimming: {}", e))?;

    if let Some(item) = result.item {
        if let Some(errors_attr) = item.get("last_errors") {
            if let Ok(errors_list) = errors_attr.as_l() {
                if errors_list.len() > 10 {
                    // Trim to last 10 errors
                    let trimmed_errors: Vec<_> = errors_list
                        .iter()
                        .skip(errors_list.len() - 10)
                        .cloned()
                        .collect();

                    let trim_expression = "SET last_errors = :trimmed_errors";
                    let mut trim_values = HashMap::new();
                    trim_values.insert(
                        ":trimmed_errors".to_string(),
                        aws_sdk_dynamodb::types::AttributeValue::L(trimmed_errors),
                    );

                    dynamodb_client
                        .update_item()
                        .table_name(device_status_table)
                        .key(
                            "hardware_id",
                            aws_sdk_dynamodb::types::AttributeValue::S(hardware_id.to_string()),
                        )
                        .update_expression(trim_expression)
                        .set_expression_attribute_values(Some(trim_values))
                        .send()
                        .await
                        .map_err(|e| format!("Failed to trim error list: {}", e))?;
                }
            }
        }
    }

    info!(
        "Recorded error for device {}: code={}, message={}",
        hardware_id, error_code, truncated_message
    );

    Ok(())
}
