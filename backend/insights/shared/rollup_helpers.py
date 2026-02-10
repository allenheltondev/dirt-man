"""
Rollup key generation and bucket alignment utilities.

Provides functions for:
- Bucket alignment (minute, hour)
- Bucket key generation
- Metric key generation with sorted dimensions
- TTL calculation
"""

from datetime import datetime, timezone, timedelta
from typing import Dict, Optional


# TTL constants
MINUTE_BUCKET_TTL_DAYS = 7
HOUR_BUCKET_TTL_DAYS = 90


def align_to_minute(timestamp_ms: int) -> int:
    """
    Align timestamp to the start of its UTC minute.

    Args:
        timestamp_ms: Timestamp in milliseconds

    Returns:
        Timestamp in milliseconds aligned to minute start
    """
    dt = datetime.fromtimestamp(timestamp_ms / 1000, tz=timezone.utc)
    aligned = dt.replace(second=0, microsecond=0)
    return int(aligned.timestamp() * 1000)


def align_to_hour_bucket(timestamp_ms: int) -> int:
    """
    Align timestamp to the start of its UTC hour for rollup buckets.

    Args:
        timestamp_ms: Timestamp in milliseconds

    Returns:
        Timestamp in milliseconds aligned to hour start
    """
    dt = datetime.fromtimestamp(timestamp_ms / 1000, tz=timezone.utc)
    aligned = dt.replace(minute=0, second=0, microsecond=0)
    return int(aligned.timestamp() * 1000)


def generate_bucket_key(bucket_type: str, bucket_start_ms: int) -> str:
    """
    Generate a bucket key for DynamoDB partition key.

    Format: "{bucket_type}#{bucket_start_ms}"

    Args:
        bucket_type: "minute" or "hour"
        bucket_start_ms: Bucket start timestamp in milliseconds

    Returns:
        Bucket key string
    """
    return f"{bucket_type}#{bucket_start_ms}"


def generate_metric_key(metric_name: str, dimensions: Optional[Dict[str, str]] = None) -> str:
    """
    Generate a metric key for DynamoDB sort key.

    Format: "{metric_name}#{sorted_dimensions}"
    Dimensions are sorted alphabetically by key for consistency.

    Args:
        metric_name: Name of the metric
        dimensions: Optional dictionary of dimension key-value pairs

    Returns:
        Metric key string

    Examples:
        >>> generate_metric_key("events_detected_count", {"event_type": "Watering_Event"})
        'events_detected_count#event_type=Watering_Event'

        >>> generate_metric_key("readings_ingested_count")
        'readings_ingested_count#'

        >>> generate_metric_key("test", {"b": "2", "a": "1"})
        'test#a=1,b=2'
    """
    if not dimensions:
        return f"{metric_name}#"

    # Sort dimensions by key for consistent ordering
    sorted_dims = sorted(dimensions.items())
    dim_str = ",".join(f"{k}={v}" for k, v in sorted_dims)

    return f"{metric_name}#{dim_str}"


def calculate_ttl(bucket_type: str, bucket_start_ms: int) -> int:
    """
    Calculate TTL timestamp for a rollup bucket.

    Args:
        bucket_type: "minute" or "hour"
        bucket_start_ms: Bucket start timestamp in milliseconds

    Returns:
        TTL timestamp in seconds (Unix epoch)
    """
    bucket_start_dt = datetime.fromtimestamp(bucket_start_ms / 1000, tz=timezone.utc)

    if bucket_type == "minute":
        ttl_dt = bucket_start_dt + timedelta(days=MINUTE_BUCKET_TTL_DAYS)
    elif bucket_type == "hour":
        ttl_dt = bucket_start_dt + timedelta(days=HOUR_BUCKET_TTL_DAYS)
    else:
        # Default to 7 days for unknown bucket types
        ttl_dt = bucket_start_dt + timedelta(days=7)

    # DynamoDB TTL expects seconds, not milliseconds
    return int(ttl_dt.timestamp())


def get_minute_bucket(timestamp_ms: int) -> int:
    """
    Get the minute bucket start for a timestamp.

    Args:
        timestamp_ms: Timestamp in milliseconds

    Returns:
        Minute bucket start in milliseconds
    """
    return align_to_minute(timestamp_ms)


def get_hour_bucket(timestamp_ms: int) -> int:
    """
    Get the hour bucket start for a timestamp.

    Args:
        timestamp_ms: Timestamp in milliseconds

    Returns:
        Hour bucket start in milliseconds
    """
    return align_to_hour_bucket(timestamp_ms)


def update_rollup_counter(
    dynamodb_client,
    table_name: str,
    bucket_type: str,
    bucket_start_ms: int,
    metric_name: str,
    dimensions: Optional[Dict[str, str]] = None,
    count_increment: int = 1,
    sum_increment: Optional[float] = None
) -> None:
    """
    Atomically update a rollup counter in DynamoDB.

    Uses ADD operation for atomic increments and if_not_exists for static attributes.

    Args:
        dynamodb_client: boto3 DynamoDB client
        table_name: Name of the rollups table
        bucket_type: "minute" or "hour"
        bucket_start_ms: Bucket start timestamp in milliseconds
        metric_name: Name of the metric
        dimensions: Optional dictionary of dimension key-value pairs
        count_increment: Amount to increment count by (default 1)
        sum_increment: Optional amount to increment sum by
    """
    bucket_key = generate_bucket_key(bucket_type, bucket_start_ms)
    metric_key = generate_metric_key(metric_name, dimensions)
    ttl = calculate_ttl(bucket_type, bucket_start_ms)

    # Build update expression
    update_parts = []
    expression_attribute_values = {}
    expression_attribute_names = {}

    # ADD count increment
    update_parts.append("#count = if_not_exists(#count, :zero) + :count_inc")
    expression_attribute_names["#count"] = "count"
    expression_attribute_values[":zero"] = 0
    expression_attribute_values[":count_inc"] = count_increment

    # ADD sum increment if provided
    if sum_increment is not None:
        update_parts.append("#sum = if_not_exists(#sum, :zero_sum) + :sum_inc")
        expression_attribute_names["#sum"] = "sum"
        expression_attribute_values[":zero_sum"] = 0
        expression_attribute_values[":sum_inc"] = sum_increment

    # SET static attributes if they don't exist
    set_parts = []
    set_parts.append("bucket_start_ms = if_not_exists(bucket_start_ms, :bucket_start_ms)")
    set_parts.append("bucket_type = if_not_exists(bucket_type, :bucket_type)")
    set_parts.append("metric_name = if_not_exists(metric_name, :metric_name)")
    set_parts.append("ttl = if_not_exists(ttl, :ttl)")

    expression_attribute_values[":bucket_start_ms"] = bucket_start_ms
    expression_attribute_values[":bucket_type"] = bucket_type
    expression_attribute_values[":metric_name"] = metric_name
    expression_attribute_values[":ttl"] = ttl

    # SET dimensions if provided
    if dimensions:
        set_parts.append("dimensions = if_not_exists(dimensions, :dimensions)")
        expression_attribute_values[":dimensions"] = dimensions

    # Combine ADD and SET parts
    update_expression = "ADD " + ", ".join(update_parts)
    if set_parts:
        update_expression += " SET " + ", ".join(set_parts)

    # Execute atomic update
    dynamodb_client.update_item(
        TableName=table_name,
        Key={
            "bucket_key": {"S": bucket_key},
            "metric_key": {"S": metric_key}
        },
        UpdateExpression=update_expression,
        ExpressionAttributeNames=expression_attribute_names,
        ExpressionAttributeValues={
            k: {"N": str(v)} if isinstance(v, (int, float)) else {"S": v} if isinstance(v, str) else {"M": {mk: {"S": mv} for mk, mv in v.items()}}
            for k, v in expression_attribute_values.items()
        }
    )
