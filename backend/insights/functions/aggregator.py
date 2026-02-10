"""
Aggregator Lambda

Computes time-series aggregations from raw sensor data:
- Hourly aggregates (incremental updates)
- Daily aggregates (computed from hourly)
- Weekly aggregates (computed from daily)
"""

import os
import sys
import time
import boto3
import math
from typing import Dict, Any, List, Optional
from decimal import Decimal
from aws_lambda_powertools import Logger
from aws_lambda_powertools.utilities.typing import LambdaContext
from botocore.exceptions import ClientError

# Add shared directory to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'shared'))

from dynamodb_helpers import extract_reading_from_stream_record
from idempotency import generate_reading_id, is_aggregate_processed, mark_aggregate_processed_if_absent
from models import Reading, SensorStatus
from time_utils import get_hour_window, is_within_lateness_window, is_window_closed

logger = Logger()

# DynamoDB client
dynamodb = boto3.client("dynamodb")
dynamodb_resource = boto3.resource("dynamodb")

# Table names from environment
READINGS_TABLE = os.environ.get("READINGS_TABLE", "plant_readings")
AGGREGATES_TABLE = os.environ.get("AGGREGATES_TABLE", "plant_aggregates")
DEVICE_STATUS_TABLE = os.environ.get("DEVICE_STATUS_TABLE", "plant_device_status")


def parse_reading_from_item(item: Dict[str, Any]) -> Optional[Reading]:
    """
    Parse a Reading object from a DynamoDB item.

    Args:
        item: DynamoDB item dict

    Returns:
        Reading object or None if parsing fails
    """
    try:
        return Reading(
            hardware_id=item.get("hardware_id", ""),
            batch_id=item.get("batch_id", ""),
            timestamp_ms=int(item.get("timestamp_ms", 0)),
            ingest_time_ms=int(item.get("ingest_time_ms", 0)),
            temperature=item.get("temperature"),
            humidity=item.get("humidity"),
            pressure=item.get("pressure"),
            soil_moisture=item.get("soil_moisture"),
            temperature_status=SensorStatus(item.get("temperature_status", "ok")),
            humidity_status=SensorStatus(item.get("humidity_status", "ok")),
            pressure_status=SensorStatus(item.get("pressure_status", "ok")),
            soil_moisture_status=SensorStatus(item.get("soil_moisture_status", "ok"))
        )
    except Exception as e:
        logger.error("Failed to parse reading", extra={"error": str(e), "item": item})
        return None


def process_stream_records(records: List[Dict[str, Any]]) -> Dict[str, Any]:
    """
    Process DynamoDB Stream records for aggregation.

    Args:
        records: List of DynamoDB Stream records

    Returns:
        Processing summary dict
    """
    processed_count = 0
    skipped_count = 0
    error_count = 0

    for record in records:
        try:
            # Only process INSERT and MODIFY events
            event_name = record.get("eventName", "")
            if event_name not in ["INSERT", "MODIFY"]:
                continue

            # Extract reading from stream record
            item = extract_reading_from_stream_record(record)
            reading = parse_reading_from_item(item)

            if not reading:
                error_count += 1
                continue

            # Generate reading_id for idempotency
            reading_id = generate_reading_id(reading.batch_id, reading.timestamp_ms)

            # Check if already processed for aggregation
            if is_aggregate_processed(reading_id):
                logger.debug(
                    "Reading already processed for aggregation",
                    extra={"reading_id": reading_id, "hardware_id": reading.hardware_id}
                )
                skipped_count += 1
                continue

            # Mark as processed
            if not mark_aggregate_processed_if_absent(reading_id, reading.hardware_id):
                # Another process marked it first
                skipped_count += 1
                continue

            # Process the reading for aggregation
            process_reading_for_aggregation(reading)
            processed_count += 1

        except Exception as e:
            logger.error(
                "Error processing stream record",
                extra={"error": str(e), "record": record}
            )
            error_count += 1

    return {
        "processed": processed_count,
        "skipped": skipped_count,
        "errors": error_count
    }


def process_reading_for_aggregation(reading: Reading) -> None:
    """
    Process a single reading for aggregation.

    Determines the appropriate hour window and updates the aggregate.

    Args:
        reading: Reading object to process
    """
    from logging_utils import log_clock_skew_warning, log_late_arrival, log_aggregate_update

    now_ms = int(time.time() * 1000)

    # Check for clock skew (event_time more than 5 minutes ahead of ingest_time)
    if reading.timestamp_ms > reading.ingest_time_ms + (5 * 60 * 1000):
        log_clock_skew_warning(
            logger=logger,
            hardware_id=reading.hardware_id,
            event_time_ms=reading.timestamp_ms,
            ingest_time_ms=reading.ingest_time_ms,
            reading_id=generate_reading_id(reading.batch_id, reading.timestamp_ms)
        )

    # Get hour window for this reading
    window_start_ms, window_end_ms = get_hour_window(reading.timestamp_ms)

    # Check if window is closed and if reading is late
    if is_window_closed(window_end_ms, now_ms):
        # Window is closed - check lateness
        lateness_hours = (now_ms - window_end_ms) / (60 * 60 * 1000)
        within_lateness = is_within_lateness_window(reading.timestamp_ms, window_end_ms, now_ms)

        # Log late arrival
        log_late_arrival(
            logger=logger,
            hardware_id=reading.hardware_id,
            reading_timestamp_ms=reading.timestamp_ms,
            window_start_ms=window_start_ms,
            window_end_ms=window_end_ms,
            lateness_hours=lateness_hours,
            within_lateness_window=within_lateness,
            reading_id=generate_reading_id(reading.batch_id, reading.timestamp_ms)
        )

        if within_lateness:
            # Trigger hour rebuild
            rebuild_hourly_aggregate(reading.hardware_id, window_start_ms, window_end_ms)
        else:
            # Too late, skip aggregate update
            return
    else:
        # Window is still open - do incremental update
        logger.debug(
            "Processing reading for open window",
            extra={
                "hardware_id": reading.hardware_id,
                "window_start_ms": window_start_ms
            }
        )
        update_hourly_aggregate_incremental(reading, window_start_ms, window_end_ms)


def update_hourly_aggregate_incremental(reading: Reading, window_start_ms: int, window_end_ms: int) -> None:
    """
    Update hourly aggregate incrementally using accumulator fields.

    Uses atomic DynamoDB update operations to increment counters and update min/max.

    Args:
        reading: Reading to add to aggregate
        window_start_ms: Window start timestamp
        window_end_ms: Window end timestamp
    """
    device_window = f"{reading.hardware_id}#hourly"

    # Build update expression for each sensor
    update_parts = []
    expression_values = {}
    expression_names = {}

    # Always increment total_count for all sensors
    sensors = [
        ("temperature", reading.temperature, reading.temperature_status),
        ("humidity", reading.humidity, reading.humidity_status),
        ("pressure", reading.pressure, reading.pressure_status),
        ("soil_moisture", reading.soil_moisture, reading.soil_moisture_status)
    ]

    for sensor_name, value, status in sensors:
        stats_field = f"{sensor_name}_stats"
        expression_names[f"#{stats_field}"] = stats_field

        # Always increment total_count
        update_parts.append(f"#{stats_field}.total_count = if_not_exists(#{stats_field}.total_count, :zero) + :one")

        # Only update valid stats if sensor status is OK and value is not None
        if status == SensorStatus.OK and value is not None:
            # Increment valid_count
            update_parts.append(f"#{stats_field}.valid_count = if_not_exists(#{stats_field}.valid_count, :zero) + :one")

            # Add to sum
            value_key = f":val_{sensor_name}"
            expression_values[value_key] = {"N": str(value)}
            update_parts.append(f"#{stats_field}.#sum = if_not_exists(#{stats_field}.#sum, :zero_decimal) + {value_key}")

            # Add to sumsq
            sumsq_value = value * value
            sumsq_key = f":sumsq_{sensor_name}"
            expression_values[sumsq_key] = {"N": str(sumsq_value)}
            update_parts.append(f"#{stats_field}.sumsq = if_not_exists(#{stats_field}.sumsq, :zero_decimal) + {sumsq_key}")

            # Update min (use if_not_exists or compare)
            min_key = f":min_{sensor_name}"
            expression_values[min_key] = {"N": str(value)}
            update_parts.append(
                f"#{stats_field}.#min = if_not_exists(#{stats_field}.#min, {min_key})"
            )
            # For actual min comparison, we need a separate update or use a more complex expression
            # For simplicity, we'll handle min/max in the rebuild function
            # Here we just ensure the field exists

            # Update max
            max_key = f":max_{sensor_name}"
            expression_values[max_key] = {"N": str(value)}
            update_parts.append(
                f"#{stats_field}.#max = if_not_exists(#{stats_field}.#max, {max_key})"
            )

    # Add attribute name mappings for reserved words
    expression_names["#sum"] = "sum"
    expression_names["#min"] = "min"
    expression_names["#max"] = "max"

    # Set common values
    expression_values[":zero"] = {"N": "0"}
    expression_values[":zero_decimal"] = {"N": "0.0"}
    expression_values[":one"] = {"N": "1"}
    expression_values[":false"] = {"BOOL": False}
    expression_values[":window_start"] = {"N": str(window_start_ms)}
    expression_values[":window_end"] = {"N": str(window_end_ms)}
    expression_values[":window_type"] = {"S": "hourly"}
    expression_values[":hardware_id"] = {"S": reading.hardware_id}
    expression_values[":computed_at"] = {"N": str(int(time.time() * 1000))}

    # Set static fields if not exists
    update_parts.append("window_start_ms = if_not_exists(window_start_ms, :window_start)")
    update_parts.append("window_end_ms = if_not_exists(window_end_ms, :window_end)")
    update_parts.append("window_type = if_not_exists(window_type, :window_type)")
    update_parts.append("hardware_id = if_not_exists(hardware_id, :hardware_id)")
    update_parts.append("is_complete = if_not_exists(is_complete, :false)")
    update_parts.append("computed_at_ms = :computed_at")

    update_expression = "SET " + ", ".join(update_parts)

    try:
        dynamodb.update_item(
            TableName=AGGREGATES_TABLE,
            Key={
                "device_window": {"S": device_window},
                "window_start_ms": {"N": str(window_start_ms)}
            },
            UpdateExpression=update_expression,
            ExpressionAttributeNames=expression_names,
            ExpressionAttributeValues=expression_values
        )

        logger.debug(
            "Updated hourly aggregate incrementally",
            extra={
                "hardware_id": reading.hardware_id,
                "window_start_ms": window_start_ms
            }
        )

    except ClientError as e:
        logger.error(
            "Failed to update hourly aggregate",
            extra={
                "error": str(e),
                "hardware_id": reading.hardware_id,
                "window_start_ms": window_start_ms
            }
        )
        raise


def rebuild_hourly_aggregate(hardware_id: str, window_start_ms: int, window_end_ms: int) -> None:
    """
    Rebuild hourly aggregate from raw readings for a specific hour.

    Used for late data arrivals and reprocessing.

    Args:
        hardware_id: Device hardware ID
        window_start_ms: Window start timestamp
        window_end_ms: Window end timestamp
    """
    from logging_utils import log_aggregate_update

    logger.info(
        "Rebuilding hourly aggregate from raw readings",
        extra={
            "hardware_id": hardware_id,
            "window_start_ms": window_start_ms,
            "window_end_ms": window_end_ms
        }
    )

    # Query raw readings for this device and hour
    try:
        table = dynamodb_resource.Table(READINGS_TABLE)
        response = table.query(
            KeyConditionExpression="hardware_id = :hw_id AND timestamp_ms BETWEEN :start AND :end",
            ExpressionAttributeValues={
                ":hw_id": hardware_id,
                ":start": window_start_ms,
                ":end": window_end_ms - 1  # Exclusive end
            }
        )

        readings = response.get("Items", [])

        # Handle pagination if needed
        while "LastEvaluatedKey" in response:
            response = table.query(
                KeyConditionExpression="hardware_id = :hw_id AND timestamp_ms BETWEEN :start AND :end",
                ExpressionAttributeValues={
                    ":hw_id": hardware_id,
                    ":start": window_start_ms,
                    ":end": window_end_ms - 1
                },
                ExclusiveStartKey=response["LastEvaluatedKey"]
            )
            readings.extend(response.get("Items", []))

        logger.info(
            "Fetched readings for rebuild",
            extra={
                "hardware_id": hardware_id,
                "reading_count": len(readings)
            }
        )

        # Compute statistics from scratch
        stats = compute_statistics_from_readings(readings)

        # Write the aggregate
        write_aggregate(hardware_id, "hourly", window_start_ms, window_end_ms, stats, is_complete=False)

        # Log aggregate update
        log_aggregate_update(
            logger=logger,
            hardware_id=hardware_id,
            window_type="hourly",
            window_start_ms=window_start_ms,
            window_end_ms=window_end_ms,
            update_type="rebuild",
            reading_count=len(readings)
        )

        # Update device status with aggregation info
        # Get total_count from any sensor stats
        total_count = stats.get("temperature", {}).get("total_count", 0)
        update_device_status_after_aggregation(hardware_id, window_start_ms, total_count)

    except ClientError as e:
        logger.error(
            "Failed to rebuild hourly aggregate",
            extra={
                "error": str(e),
                "hardware_id": hardware_id,
                "window_start_ms": window_start_ms
            }
        )
        raise


def compute_statistics_from_readings(readings: List[Dict[str, Any]]) -> Dict[str, Dict[str, Any]]:
    """
    Compute statistics from a list of readings.

    Args:
        readings: List of reading dicts

    Returns:
        Dict mapping sensor names to their statistics
    """
    sensors = ["temperature", "humidity", "pressure", "soil_moisture"]
    stats = {}

    for sensor in sensors:
        valid_values = []
        total_count = 0

        for reading in readings:
            total_count += 1
            status_field = f"{sensor}_status"
            status = reading.get(status_field, "ok")

            # Only include OK readings in statistics
            if status == "ok" and sensor in reading and reading[sensor] is not None:
                valid_values.append(float(reading[sensor]))

        # Compute statistics
        if valid_values:
            min_val = min(valid_values)
            max_val = max(valid_values)
            sum_val = sum(valid_values)
            avg_val = sum_val / len(valid_values)

            # Compute standard deviation
            if len(valid_values) > 1:
                variance = sum((x - avg_val) ** 2 for x in valid_values) / len(valid_values)
                stddev_val = math.sqrt(variance)
            else:
                stddev_val = 0.0

            # Compute sum of squares
            sumsq_val = sum(x * x for x in valid_values)

            stats[sensor] = {
                "min": min_val,
                "max": max_val,
                "avg": avg_val,
                "stddev": stddev_val,
                "sum": sum_val,
                "sumsq": sumsq_val,
                "valid_count": len(valid_values),
                "total_count": total_count
            }
        else:
            # No valid readings
            stats[sensor] = {
                "valid_count": 0,
                "total_count": total_count,
                "sum": 0.0,
                "sumsq": 0.0
            }

    return stats


def write_aggregate(
    hardware_id: str,
    window_type: str,
    window_start_ms: int,
    window_end_ms: int,
    stats: Dict[str, Dict[str, Any]],
    is_complete: bool
) -> None:
    """
    Write an aggregate to DynamoDB.

    Args:
        hardware_id: Device hardware ID
        window_type: Window type (hourly, daily, weekly)
        window_start_ms: Window start timestamp
        window_end_ms: Window end timestamp
        stats: Statistics dict for each sensor
        is_complete: Whether the window is complete
    """
    device_window = f"{hardware_id}#{window_type}"
    computed_at_ms = int(time.time() * 1000)

    item = {
        "device_window": {"S": device_window},
        "window_start_ms": {"N": str(window_start_ms)},
        "window_end_ms": {"N": str(window_end_ms)},
        "window_type": {"S": window_type},
        "hardware_id": {"S": hardware_id},
        "is_complete": {"BOOL": is_complete},
        "computed_at_ms": {"N": str(computed_at_ms)}
    }

    # Add sensor stats
    for sensor in ["temperature", "humidity", "pressure", "soil_moisture"]:
        sensor_stats = stats.get(sensor, {})
        stats_field = f"{sensor}_stats"

        stats_map = {
            "valid_count": {"N": str(sensor_stats.get("valid_count", 0))},
            "total_count": {"N": str(sensor_stats.get("total_count", 0))},
            "sum": {"N": str(sensor_stats.get("sum", 0.0))},
            "sumsq": {"N": str(sensor_stats.get("sumsq", 0.0))}
        }

        if "min" in sensor_stats:
            stats_map["min"] = {"N": str(sensor_stats["min"])}
        if "max" in sensor_stats:
            stats_map["max"] = {"N": str(sensor_stats["max"])}
        if "avg" in sensor_stats:
            stats_map["avg"] = {"N": str(sensor_stats["avg"])}
        if "stddev" in sensor_stats:
            stats_map["stddev"] = {"N": str(sensor_stats["stddev"])}

        item[stats_field] = {"M": stats_map}

    try:
        dynamodb.put_item(
            TableName=AGGREGATES_TABLE,
            Item=item
        )

        logger.debug(
            "Wrote aggregate",
            extra={
                "hardware_id": hardware_id,
                "window_type": window_type,
                "window_start_ms": window_start_ms
            }
        )

    except ClientError as e:
        logger.error(
            "Failed to write aggregate",
            extra={
                "error": str(e),
                "hardware_id": hardware_id,
                "window_start_ms": window_start_ms
            }
        )
        raise


@logger.inject_lambda_context
def lambda_handler(event: Dict[str, Any], context: LambdaContext) -> Dict[str, Any]:
    """
    Lambda handler for aggregating sensor data into time-series windows.

    Handles two types of invocations:
    1. DynamoDB Stream events (for hourly incremental updates)
    2. EventBridge scheduled events (for daily/weekly computation)

    Args:
        event: Lambda event containing DynamoDB Stream records or scheduled event
        context: Lambda context

    Returns:
        Response dict with processing status
    """
    from retry_utils import process_stream_batch_with_isolation

    logger.info("Aggregator Lambda invoked")

    # Check if this is a scheduled event (EventBridge)
    if event.get("source") == "aws.events":
        detail_type = event.get("detail-type", "")
        logger.info("Processing scheduled event", extra={"detail_type": detail_type})

        # Determine which aggregation to run based on detail_type or rule name
        rule_name = event.get("resources", [""])[0].split("/")[-1] if event.get("resources") else ""

        if "daily" in rule_name.lower() or "daily" in detail_type.lower():
            compute_daily_aggregates()
        elif "weekly" in rule_name.lower() or "weekly" in detail_type.lower():
            compute_weekly_aggregates()
        else:
            logger.warning("Unknown scheduled event type", extra={"detail_type": detail_type, "rule_name": rule_name})

        return {
            "statusCode": 200,
            "body": "Scheduled aggregation processing complete"
        }

    # Otherwise, process DynamoDB Stream records
    records = event.get("Records", [])
    logger.info("Processing stream records", extra={"record_count": len(records)})

    # Process records with error isolation
    batch_item_failures = process_stream_batch_with_isolation(
        records=records,
        process_func=lambda record: process_stream_records([record]),
        logger_instance=logger
    )

    logger.info(
        "Stream processing complete",
        extra={
            "total_records": len(records),
            "failed_records": len(batch_item_failures)
        }
    )

    return {
        "statusCode": 200,
        "body": "Aggregation processing complete",
        "batchItemFailures": batch_item_failures
    }


def compute_daily_aggregates() -> None:
    """
    Compute daily aggregates from hourly aggregates for the previous day.

    Triggered by EventBridge schedule at 00:10 UTC.
    """
    from time_utils import get_day_window, align_to_day

    now_ms = int(time.time() * 1000)
    # Get yesterday's day window
    yesterday_ms = now_ms - (24 * 60 * 60 * 1000)
    day_start_ms, day_end_ms = get_day_window(yesterday_ms)

    logger.info(
        "Computing daily aggregates for previous day",
        extra={"day_start_ms": day_start_ms, "day_end_ms": day_end_ms}
    )

    # Get list of devices that have hourly aggregates for yesterday
    devices = get_devices_with_hourly_aggregates(day_start_ms, day_end_ms)

    logger.info(f"Found {len(devices)} devices with hourly data")

    for hardware_id in devices:
        try:
            compute_daily_aggregate_for_device(hardware_id, day_start_ms, day_end_ms)
        except Exception as e:
            logger.error(
                "Failed to compute daily aggregate for device",
                extra={"hardware_id": hardware_id, "error": str(e)}
            )


def get_devices_with_hourly_aggregates(start_ms: int, end_ms: int) -> List[str]:
    """
    Get list of devices that have hourly aggregates in the given time range.

    Args:
        start_ms: Start timestamp
        end_ms: End timestamp

    Returns:
        List of hardware_ids
    """
    # This is a simplified implementation
    # In production, you might want to maintain a device registry or scan more efficiently
    devices = set()

    try:
        # Scan for hourly aggregates in the time range
        # Note: This is not optimal for large datasets
        # Consider maintaining a device list or using a GSI
        response = dynamodb.scan(
            TableName=AGGREGATES_TABLE,
            FilterExpression="window_type = :window_type AND window_start_ms >= :start AND window_start_ms < :end",
            ExpressionAttributeValues={
                ":window_type": {"S": "hourly"},
                ":start": {"N": str(start_ms)},
                ":end": {"N": str(end_ms)}
            },
            ProjectionExpression="hardware_id"
        )

        for item in response.get("Items", []):
            devices.add(item["hardware_id"]["S"])

        # Handle pagination
        while "LastEvaluatedKey" in response:
            response = dynamodb.scan(
                TableName=AGGREGATES_TABLE,
                FilterExpression="window_type = :window_type AND window_start_ms >= :start AND window_start_ms < :end",
                ExpressionAttributeValues={
                    ":window_type": {"S": "hourly"},
                    ":start": {"N": str(start_ms)},
                    ":end": {"N": str(end_ms)}
                },
                ProjectionExpression="hardware_id",
                ExclusiveStartKey=response["LastEvaluatedKey"]
            )
            for item in response.get("Items", []):
                devices.add(item["hardware_id"]["S"])

    except ClientError as e:
        logger.error("Failed to get devices with hourly aggregates", extra={"error": str(e)})

    return list(devices)


def compute_daily_aggregate_for_device(hardware_id: str, day_start_ms: int, day_end_ms: int) -> None:
    """
    Compute daily aggregate for a device from its hourly aggregates.

    Args:
        hardware_id: Device hardware ID
        day_start_ms: Day start timestamp
        day_end_ms: Day end timestamp
    """
    device_window = f"{hardware_id}#hourly"

    try:
        # Query hourly aggregates for this device and day
        response = dynamodb.query(
            TableName=AGGREGATES_TABLE,
            KeyConditionExpression="device_window = :dw AND window_start_ms BETWEEN :start AND :end",
            ExpressionAttributeValues={
                ":dw": {"S": device_window},
                ":start": {"N": str(day_start_ms)},
                ":end": {"N": str(day_end_ms - 1)}
            }
        )

        hourly_items = response.get("Items", [])

        # Handle pagination
        while "LastEvaluatedKey" in response:
            response = dynamodb.query(
                TableName=AGGREGATES_TABLE,
                KeyConditionExpression="device_window = :dw AND window_start_ms BETWEEN :start AND :end",
                ExpressionAttributeValues={
                    ":dw": {"S": device_window},
                    ":start": {"N": str(day_start_ms)},
                    ":end": {"N": str(day_end_ms - 1)}
                },
                ExclusiveStartKey=response["LastEvaluatedKey"]
            )
            hourly_items.extend(response.get("Items", []))

        if not hourly_items:
            logger.warning(
                "No hourly aggregates found for device",
                extra={"hardware_id": hardware_id, "day_start_ms": day_start_ms}
            )
            return

        # Combine hourly aggregates into daily
        daily_stats = combine_hourly_aggregates(hourly_items)

        # Write daily aggregate
        write_aggregate(hardware_id, "daily", day_start_ms, day_end_ms, daily_stats, is_complete=True)

        logger.info(
            "Computed daily aggregate",
            extra={
                "hardware_id": hardware_id,
                "day_start_ms": day_start_ms,
                "hourly_count": len(hourly_items)
            }
        )

    except ClientError as e:
        logger.error(
            "Failed to compute daily aggregate",
            extra={"hardware_id": hardware_id, "error": str(e)}
        )
        raise


def combine_hourly_aggregates(hourly_items: List[Dict[str, Any]]) -> Dict[str, Dict[str, Any]]:
    """
    Combine hourly aggregates into a daily aggregate using weighted methods.

    Args:
        hourly_items: List of hourly aggregate DynamoDB items

    Returns:
        Dict mapping sensor names to their combined statistics
    """
    sensors = ["temperature", "humidity", "pressure", "soil_moisture"]
    combined_stats = {}

    for sensor in sensors:
        stats_field = f"{sensor}_stats"
        total_valid_count = 0
        total_count = 0
        total_sum = 0.0
        total_sumsq = 0.0
        min_val = None
        max_val = None

        for item in hourly_items:
            if stats_field not in item or "M" not in item[stats_field]:
                continue

            stats = item[stats_field]["M"]

            # Extract values
            valid_count = int(stats.get("valid_count", {}).get("N", "0"))
            count = int(stats.get("total_count", {}).get("N", "0"))
            sum_val = float(stats.get("sum", {}).get("N", "0.0"))
            sumsq_val = float(stats.get("sumsq", {}).get("N", "0.0"))

            total_valid_count += valid_count
            total_count += count
            total_sum += sum_val
            total_sumsq += sumsq_val

            # Update min/max
            if "min" in stats and "N" in stats["min"]:
                hour_min = float(stats["min"]["N"])
                if min_val is None or hour_min < min_val:
                    min_val = hour_min

            if "max" in stats and "N" in stats["max"]:
                hour_max = float(stats["max"]["N"])
                if max_val is None or hour_max > max_val:
                    max_val = hour_max

        # Compute combined statistics
        if total_valid_count > 0:
            avg_val = total_sum / total_valid_count

            # Compute standard deviation from sum of squares
            if total_valid_count > 1:
                variance = (total_sumsq / total_valid_count) - (avg_val ** 2)
                stddev_val = math.sqrt(max(0, variance))  # Ensure non-negative
            else:
                stddev_val = 0.0

            combined_stats[sensor] = {
                "min": min_val,
                "max": max_val,
                "avg": avg_val,
                "stddev": stddev_val,
                "sum": total_sum,
                "sumsq": total_sumsq,
                "valid_count": total_valid_count,
                "total_count": total_count
            }
        else:
            combined_stats[sensor] = {
                "valid_count": 0,
                "total_count": total_count,
                "sum": 0.0,
                "sumsq": 0.0
            }

    return combined_stats


def compute_weekly_aggregates() -> None:
    """
    Compute weekly aggregates from daily aggregates for the previous week.

    Triggered by EventBridge schedule at 00:20 UTC.
    Uses ISO week starting Monday.
    """
    from time_utils import get_week_window, align_to_week

    now_ms = int(time.time() * 1000)
    # Get last completed week
    # Go back 7 days to ensure we're in the previous week
    last_week_ms = now_ms - (7 * 24 * 60 * 60 * 1000)
    week_start_ms, week_end_ms = get_week_window(last_week_ms)

    logger.info(
        "Computing weekly aggregates for previous week",
        extra={"week_start_ms": week_start_ms, "week_end_ms": week_end_ms}
    )

    # Get list of devices that have daily aggregates for last week
    devices = get_devices_with_daily_aggregates(week_start_ms, week_end_ms)

    logger.info(f"Found {len(devices)} devices with daily data")

    for hardware_id in devices:
        try:
            compute_weekly_aggregate_for_device(hardware_id, week_start_ms, week_end_ms)
        except Exception as e:
            logger.error(
                "Failed to compute weekly aggregate for device",
                extra={"hardware_id": hardware_id, "error": str(e)}
            )


def get_devices_with_daily_aggregates(start_ms: int, end_ms: int) -> List[str]:
    """
    Get list of devices that have daily aggregates in the given time range.

    Args:
        start_ms: Start timestamp
        end_ms: End timestamp

    Returns:
        List of hardware_ids
    """
    devices = set()

    try:
        response = dynamodb.scan(
            TableName=AGGREGATES_TABLE,
            FilterExpression="window_type = :window_type AND window_start_ms >= :start AND window_start_ms < :end",
            ExpressionAttributeValues={
                ":window_type": {"S": "daily"},
                ":start": {"N": str(start_ms)},
                ":end": {"N": str(end_ms)}
            },
            ProjectionExpression="hardware_id"
        )

        for item in response.get("Items", []):
            devices.add(item["hardware_id"]["S"])

        # Handle pagination
        while "LastEvaluatedKey" in response:
            response = dynamodb.scan(
                TableName=AGGREGATES_TABLE,
                FilterExpression="window_type = :window_type AND window_start_ms >= :start AND window_start_ms < :end",
                ExpressionAttributeValues={
                    ":window_type": {"S": "daily"},
                    ":start": {"N": str(start_ms)},
                    ":end": {"N": str(end_ms)}
                },
                ProjectionExpression="hardware_id",
                ExclusiveStartKey=response["LastEvaluatedKey"]
            )
            for item in response.get("Items", []):
                devices.add(item["hardware_id"]["S"])

    except ClientError as e:
        logger.error("Failed to get devices with daily aggregates", extra={"error": str(e)})

    return list(devices)


def compute_weekly_aggregate_for_device(hardware_id: str, week_start_ms: int, week_end_ms: int) -> None:
    """
    Compute weekly aggregate for a device from its daily aggregates.

    Args:
        hardware_id: Device hardware ID
        week_start_ms: Week start timestamp (Monday 00:00:00 UTC)
        week_end_ms: Week end timestamp
    """
    device_window = f"{hardware_id}#daily"

    try:
        # Query daily aggregates for this device and week
        response = dynamodb.query(
            TableName=AGGREGATES_TABLE,
            KeyConditionExpression="device_window = :dw AND window_start_ms BETWEEN :start AND :end",
            ExpressionAttributeValues={
                ":dw": {"S": device_window},
                ":start": {"N": str(week_start_ms)},
                ":end": {"N": str(week_end_ms - 1)}
            }
        )

        daily_items = response.get("Items", [])

        # Handle pagination
        while "LastEvaluatedKey" in response:
            response = dynamodb.query(
                TableName=AGGREGATES_TABLE,
                KeyConditionExpression="device_window = :dw AND window_start_ms BETWEEN :start AND :end",
                ExpressionAttributeValues={
                    ":dw": {"S": device_window},
                    ":start": {"N": str(week_start_ms)},
                    ":end": {"N": str(week_end_ms - 1)}
                },
                ExclusiveStartKey=response["LastEvaluatedKey"]
            )
            daily_items.extend(response.get("Items", []))

        if not daily_items:
            logger.warning(
                "No daily aggregates found for device",
                extra={"hardware_id": hardware_id, "week_start_ms": week_start_ms}
            )
            return

        # Combine daily aggregates into weekly (same logic as combining hourly into daily)
        weekly_stats = combine_hourly_aggregates(daily_items)  # Reuse the same function

        # Write weekly aggregate
        write_aggregate(hardware_id, "weekly", week_start_ms, week_end_ms, weekly_stats, is_complete=True)

        logger.info(
            "Computed weekly aggregate",
            extra={
                "hardware_id": hardware_id,
                "week_start_ms": week_start_ms,
                "daily_count": len(daily_items)
            }
        )

    except ClientError as e:
        logger.error(
            "Failed to compute weekly aggregate",
            extra={"hardware_id": hardware_id, "error": str(e)}
        )
        raise


def update_device_status_after_aggregation(
    hardware_id: str,
    window_start_ms: int,
    total_count: int,
    expected_interval_sec: int = 300
) -> None:
    """
    Update Device Status after computing an aggregate.

    Updates (via field ownership discipline):
    - last_aggregate_computed_at
    - coverage_pct_last_hour
    - sensor_status_summary (based on recent conditions)

    Args:
        hardware_id: Device hardware ID
        window_start_ms: Window start timestamp
        total_count: Total number of readings in the aggregate
        expected_interval_sec: Expected interval between readings (default 300)
    """
    now_ms = int(time.time() * 1000)

    # Compute coverage percentage for the hour
    # Expected samples = 3600 / expected_interval_sec
    expected_samples = 3600 / expected_interval_sec
    coverage_pct = min(1.0, total_count / expected_samples) if expected_samples > 0 else 0.0

    # Determine sensor_status_summary based on coverage
    # This is a simplified heuristic - in production you might want more sophisticated logic
    if coverage_pct >= 0.8:
        sensor_status_summary = "ok"
    elif coverage_pct >= 0.3:
        sensor_status_summary = "degraded"
    else:
        sensor_status_summary = "missing"

    try:
        # Update only the fields owned by the aggregation pipeline
        dynamodb.update_item(
            TableName=DEVICE_STATUS_TABLE,
            Key={"hardware_id": {"S": hardware_id}},
            UpdateExpression="SET last_aggregate_computed_at_ms = :computed_at, "
                           "coverage_pct_last_hour = :coverage, "
                           "sensor_status_summary = :status, "
                           "updated_at_ms = :now",
            ExpressionAttributeValues={
                ":computed_at": {"N": str(window_start_ms)},
                ":coverage": {"N": str(coverage_pct)},
                ":status": {"S": sensor_status_summary},
                ":now": {"N": str(now_ms)}
            }
        )

        logger.debug(
            "Updated device status after aggregation",
            extra={
                "hardware_id": hardware_id,
                "coverage_pct": coverage_pct,
                "sensor_status_summary": sensor_status_summary
            }
        )

    except ClientError as e:
        logger.error(
            "Failed to update device status",
            extra={"hardware_id": hardware_id, "error": str(e)}
        )
        # Don't raise - this is a non-critical update
