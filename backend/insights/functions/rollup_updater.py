"""
Rollup Updater Lambda Handler

Maintains operational metrics in DynamoDB for dashboard queries:
- Processes DynamoDB Stream records from Readings, Events, Aggregates, and Insights tables
- Updates time-bucketed rollup counters atomically
- Tracks system-level metrics (throughput, lag, device counts)

CRITICAL: This Lambda MUST NOT write to any source tables (Readings, Events, Aggregates, Insights).
It ONLY writes to the Rollups table to prevent infinite stream loops.
"""

import os
import time
from typing import Dict, Any, List, Set
import boto3
from aws_lambda_powertools import Logger
from aws_lambda_powertools.utilities.typing import LambdaContext

from shared.dynamodb_helpers import extract_reading_from_stream_record, parse_dynamodb_item
from shared.rollup_helpers import (
    get_minute_bucket,
    get_hour_bucket,
    update_rollup_counter
)

logger = Logger()

# Initialize DynamoDB client
dynamodb_client = boto3.client("dynamodb")

# Get table name from environment
ROLLUPS_TABLE_NAME = os.environ.get("ROLLUPS_TABLE_NAME", "plant_rollups")

# Track devices reporting in this batch for approximate device count
devices_seen_in_batch: Set[str] = set()


@logger.inject_lambda_context
def lambda_handler(event: Dict[str, Any], context: LambdaContext) -> Dict[str, Any]:
    """
    Lambda handler for Rollup Updater.

    Processes DynamoDB Stream records from multiple source tables to update operational rollups.

    Args:
        event: DynamoDB Stream event containing Records
        context: Lambda context

    Returns:
        Response with batch item failures for partial batch failure handling
    """
    from shared.retry_utils import process_stream_batch_with_isolation

    logger.info("Rollup Updater Lambda invoked", extra={
        "record_count": len(event.get("Records", []))
    })

    # Reset devices seen in batch
    global devices_seen_in_batch
    devices_seen_in_batch = set()

    # Process records with error isolation
    batch_item_failures = process_stream_batch_with_isolation(
        records=event.get("Records", []),
        process_func=process_stream_record,
        logger_instance=logger
    )

    # After processing all records, update devices_reporting_count
    if devices_seen_in_batch:
        try:
            update_devices_reporting_count()
        except Exception as e:
            logger.error(
                "Failed to update devices_reporting_count",
                extra={"error": str(e), "error_type": type(e).__name__}
            )

    logger.info("Rollup Updater processing complete", extra={
        "total_records": len(event.get("Records", [])),
        "failed_records": len(batch_item_failures),
        "devices_seen": len(devices_seen_in_batch)
    })

    return {
        "batchItemFailures": batch_item_failures
    }


def process_stream_record(record: Dict[str, Any]) -> None:
    """
    Process a single DynamoDB Stream record.

    Determines the source table and routes to appropriate metric handler.

    Args:
        record: DynamoDB Stream record
    """
    event_name = record.get("eventName")

    # Only process INSERT and MODIFY events
    if event_name not in ["INSERT", "MODIFY"]:
        logger.debug("Skipping non-INSERT/MODIFY event", extra={"event_name": event_name})
        return

    # Extract the new image from the stream record
    new_image = record.get("dynamodb", {}).get("NewImage")
    if not new_image:
        logger.warning("Stream record missing NewImage")
        return

    # Determine source table from event source ARN
    event_source_arn = record.get("eventSourceARN", "")
    source_table = extract_table_name_from_arn(event_source_arn)

    logger.debug("Processing stream record", extra={
        "source_table": source_table,
        "event_name": event_name
    })

    # Parse the item from DynamoDB format
    item = parse_dynamodb_item(new_image)

    # Route to appropriate handler based on source table
    if "reading" in source_table.lower():
        process_reading_metrics(item, event_name)
    elif "event" in source_table.lower():
        process_event_metrics(item, event_name)
    elif "aggregate" in source_table.lower():
        process_aggregate_metrics(item, event_name)
    elif "insight" in source_table.lower():
        process_insight_metrics(item, event_name)
    else:
        logger.warning("Unknown source table", extra={"source_table": source_table})


def extract_table_name_from_arn(arn: str) -> str:
    """
    Extract table name from DynamoDB Stream ARN.

    Args:
        arn: Event source ARN

    Returns:
        Table name
    """
    # ARN format: arn:aws:dynamodb:region:account:table/TableName/stream/timestamp
    if not arn:
        return ""

    parts = arn.split("/")
    if len(parts) >= 2:
        return parts[1]

    return ""


def process_reading_metrics(item: Dict[str, Any], event_name: str) -> None:
    """
    Process metrics for a reading record.

    Tracks:
    - readings_ingested_count: Total readings ingested
    - readings_deduped_count: Duplicate readings (MODIFY events indicate deduplication)
    - readings_invalid_count: Readings with invalid sensor data
    - devices_reporting_count: Approximate count of active devices
    - pipeline_lag_seconds: Processing lag

    Args:
        item: Parsed reading item
        event_name: DynamoDB event name (INSERT or MODIFY)
    """
    hardware_id = item.get("hardware_id")
    ingest_time_ms = item.get("ingest_time_ms")
    timestamp_ms = item.get("timestamp_ms")

    if not ingest_time_ms:
        logger.warning("Reading missing ingest_time_ms", extra={"hardware_id": hardware_id})
        return

    # Get current time for lag calculation
    now_ms = int(time.time() * 1000)

    # Determine minute and hour buckets
    minute_bucket = get_minute_bucket(ingest_time_ms)
    hour_bucket = get_hour_bucket(ingest_time_ms)

    # Track readings_ingested_count
    if event_name == "INSERT":
        # New reading ingested
        update_rollup_counter(
            dynamodb_client,
            ROLLUPS_TABLE_NAME,
            "minute",
            minute_bucket,
            "readings_ingested_count"
        )
        update_rollup_counter(
            dynamodb_client,
            ROLLUPS_TABLE_NAME,
            "hour",
            hour_bucket,
            "readings_ingested_count"
        )

        logger.debug("Incremented readings_ingested_count", extra={
            "hardware_id": hardware_id,
            "minute_bucket": minute_bucket,
            "hour_bucket": hour_bucket
        })

    elif event_name == "MODIFY":
        # Modified reading indicates deduplication
        update_rollup_counter(
            dynamodb_client,
            ROLLUPS_TABLE_NAME,
            "minute",
            minute_bucket,
            "readings_deduped_count"
        )
        update_rollup_counter(
            dynamodb_client,
            ROLLUPS_TABLE_NAME,
            "hour",
            hour_bucket,
            "readings_deduped_count"
        )

        logger.debug("Incremented readings_deduped_count", extra={
            "hardware_id": hardware_id,
            "minute_bucket": minute_bucket,
            "hour_bucket": hour_bucket
        })

    # Check for invalid sensor data
    is_invalid = False
    sensor_statuses = [
        item.get("temperature_status"),
        item.get("humidity_status"),
        item.get("pressure_status"),
        item.get("soil_moisture_status")
    ]

    # If any sensor is marked as out_of_range or if critical fields are missing
    if "out_of_range" in sensor_statuses or not timestamp_ms:
        is_invalid = True

    if is_invalid:
        update_rollup_counter(
            dynamodb_client,
            ROLLUPS_TABLE_NAME,
            "minute",
            minute_bucket,
            "readings_invalid_count"
        )
        update_rollup_counter(
            dynamodb_client,
            ROLLUPS_TABLE_NAME,
            "hour",
            hour_bucket,
            "readings_invalid_count"
        )

        logger.debug("Incremented readings_invalid_count", extra={
            "hardware_id": hardware_id,
            "minute_bucket": minute_bucket,
            "hour_bucket": hour_bucket
        })

    # Track device for approximate device count
    if hardware_id:
        devices_seen_in_batch.add(hardware_id)

    # Track pipeline lag
    if timestamp_ms:
        lag_seconds = (now_ms - timestamp_ms) / 1000.0
        if lag_seconds >= 0:  # Only track positive lag
            update_rollup_counter(
                dynamodb_client,
                ROLLUPS_TABLE_NAME,
                "minute",
                minute_bucket,
                "pipeline_lag_seconds_sum",
                sum_increment=lag_seconds
            )
            update_rollup_counter(
                dynamodb_client,
                ROLLUPS_TABLE_NAME,
                "minute",
                minute_bucket,
                "pipeline_lag_seconds_count"
            )
            update_rollup_counter(
                dynamodb_client,
                ROLLUPS_TABLE_NAME,
                "hour",
                hour_bucket,
                "pipeline_lag_seconds_sum",
                sum_increment=lag_seconds
            )
            update_rollup_counter(
                dynamodb_client,
                ROLLUPS_TABLE_NAME,
                "hour",
                hour_bucket,
                "pipeline_lag_seconds_count"
            )


def process_event_metrics(item: Dict[str, Any], event_name: str) -> None:
    """
    Process metrics for an event record.

    Tracks:
    - events_detected_count: Total events detected with event_type dimension

    Args:
        item: Parsed event item
        event_name: DynamoDB event name (INSERT or MODIFY)
    """
    event_type = item.get("event_type")
    created_at_ms = item.get("created_at_ms") or item.get("start_time_ms")

    if not created_at_ms:
        logger.warning("Event missing timestamp", extra={"event_type": event_type})
        return

    # Only count INSERT events (new events detected)
    if event_name != "INSERT":
        return

    # Determine minute and hour buckets
    minute_bucket = get_minute_bucket(created_at_ms)
    hour_bucket = get_hour_bucket(created_at_ms)

    # Track events_detected_count with event_type dimension
    dimensions = {"event_type": event_type} if event_type else {}

    update_rollup_counter(
        dynamodb_client,
        ROLLUPS_TABLE_NAME,
        "minute",
        minute_bucket,
        "events_detected_count",
        dimensions=dimensions
    )
    update_rollup_counter(
        dynamodb_client,
        ROLLUPS_TABLE_NAME,
        "hour",
        hour_bucket,
        "events_detected_count",
        dimensions=dimensions
    )

    logger.debug("Incremented events_detected_count", extra={
        "event_type": event_type,
        "minute_bucket": minute_bucket,
        "hour_bucket": hour_bucket
    })


def process_aggregate_metrics(item: Dict[str, Any], event_name: str) -> None:
    """
    Process metrics for an aggregate record.

    Tracks:
    - aggregates_computed_count: Total aggregates computed with window_type dimension

    Args:
        item: Parsed aggregate item
        event_name: DynamoDB event name (INSERT or MODIFY)
    """
    window_type = item.get("window_type")
    computed_at_ms = item.get("computed_at_ms") or item.get("window_start_ms")

    if not computed_at_ms:
        logger.warning("Aggregate missing timestamp", extra={"window_type": window_type})
        return

    # Count both INSERT and MODIFY (recomputation) events
    # Determine minute and hour buckets
    minute_bucket = get_minute_bucket(computed_at_ms)
    hour_bucket = get_hour_bucket(computed_at_ms)

    # Track aggregates_computed_count with window_type dimension
    dimensions = {"window_type": window_type} if window_type else {}

    update_rollup_counter(
        dynamodb_client,
        ROLLUPS_TABLE_NAME,
        "minute",
        minute_bucket,
        "aggregates_computed_count",
        dimensions=dimensions
    )
    update_rollup_counter(
        dynamodb_client,
        ROLLUPS_TABLE_NAME,
        "hour",
        hour_bucket,
        "aggregates_computed_count",
        dimensions=dimensions
    )

    logger.debug("Incremented aggregates_computed_count", extra={
        "window_type": window_type,
        "minute_bucket": minute_bucket,
        "hour_bucket": hour_bucket
    })


def process_insight_metrics(item: Dict[str, Any], event_name: str) -> None:
    """
    Process metrics for an insight record.

    Tracks:
    - insights_generated_count: Total insights generated with status dimension (success|failure)
    - insight_generation_duration_ms_sum: Sum of generation durations
    - insight_generation_count: Count of insights with duration data

    Args:
        item: Parsed insight item
        event_name: DynamoDB event name (INSERT or MODIFY)
    """
    timestamp_ms = item.get("timestamp_ms")
    generation_duration_ms = item.get("generation_duration_ms")
    confidence = item.get("confidence")

    if not timestamp_ms:
        logger.warning("Insight missing timestamp")
        return

    # Only count INSERT events (new insights generated)
    if event_name != "INSERT":
        return

    # Determine minute and hour buckets
    minute_bucket = get_minute_bucket(timestamp_ms)
    hour_bucket = get_hour_bucket(timestamp_ms)

    # Determine status: success if insight has content, failure otherwise
    # We consider an insight successful if it has a summary or recommendations
    has_summary = bool(item.get("summary"))
    has_recommendations = bool(item.get("recommendations"))
    status = "success" if (has_summary or has_recommendations) else "failure"

    # Track insights_generated_count with status dimension
    dimensions = {"status": status}

    update_rollup_counter(
        dynamodb_client,
        ROLLUPS_TABLE_NAME,
        "minute",
        minute_bucket,
        "insights_generated_count",
        dimensions=dimensions
    )
    update_rollup_counter(
        dynamodb_client,
        ROLLUPS_TABLE_NAME,
        "hour",
        hour_bucket,
        "insights_generated_count",
        dimensions=dimensions
    )

    logger.debug("Incremented insights_generated_count", extra={
        "status": status,
        "minute_bucket": minute_bucket,
        "hour_bucket": hour_bucket
    })

    # Track generation duration if available
    if generation_duration_ms is not None and generation_duration_ms > 0:
        update_rollup_counter(
            dynamodb_client,
            ROLLUPS_TABLE_NAME,
            "minute",
            minute_bucket,
            "insight_generation_duration_ms_sum",
            sum_increment=generation_duration_ms
        )
        update_rollup_counter(
            dynamodb_client,
            ROLLUPS_TABLE_NAME,
            "minute",
            minute_bucket,
            "insight_generation_count"
        )
        update_rollup_counter(
            dynamodb_client,
            ROLLUPS_TABLE_NAME,
            "hour",
            hour_bucket,
            "insight_generation_duration_ms_sum",
            sum_increment=generation_duration_ms
        )
        update_rollup_counter(
            dynamodb_client,
            ROLLUPS_TABLE_NAME,
            "hour",
            hour_bucket,
            "insight_generation_count"
        )

        logger.debug("Incremented insight generation duration metrics", extra={
            "generation_duration_ms": generation_duration_ms,
            "minute_bucket": minute_bucket,
            "hour_bucket": hour_bucket
        })



def update_devices_reporting_count() -> None:
    """
    Update the approximate count of devices reporting in this batch.

    This provides a system-level metric for dashboard queries showing
    how many unique devices are actively reporting data.

    Uses the current time for bucketing since this is a system-level metric
    computed at processing time, not event time.
    """
    now_ms = int(time.time() * 1000)
    minute_bucket = get_minute_bucket(now_ms)
    hour_bucket = get_hour_bucket(now_ms)

    device_count = len(devices_seen_in_batch)

    # Update devices_reporting_count
    # Note: This is an approximate count since we're incrementing by the number
    # of unique devices seen in this batch. Multiple batches may see the same device.
    # For a more accurate count, the dashboard should deduplicate or use this as
    # an activity indicator rather than an exact unique device count.
    update_rollup_counter(
        dynamodb_client,
        ROLLUPS_TABLE_NAME,
        "minute",
        minute_bucket,
        "devices_reporting_count",
        count_increment=device_count
    )
    update_rollup_counter(
        dynamodb_client,
        ROLLUPS_TABLE_NAME,
        "hour",
        hour_bucket,
        "devices_reporting_count",
        count_increment=device_count
    )

    logger.debug("Updated devices_reporting_count", extra={
        "device_count": device_count,
        "minute_bucket": minute_bucket,
        "hour_bucket": hour_bucket
    })
