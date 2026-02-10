"""
Profile learning utilities for Device Profiles.

Implements learning algorithms for:
- Watering interval calculation from event history
- Baseline moisture range from historical aggregates
- Profile-based stress detection
"""

from typing import List, Optional, Dict, Any
from statistics import mean
import boto3
from .models import DeviceProfile, EventType


def calculate_watering_interval(watering_events_ms: List[int]) -> Optional[int]:
    """
    Calculate average watering interval from event timestamps.

    Args:
        watering_events_ms: List of watering event timestamps in milliseconds

    Returns:
        Average interval in seconds, or None if insufficient data

    Requirements: 10.3
    """
    if len(watering_events_ms) < 2:
        return None

    # Sort timestamps to ensure correct ordering
    sorted_events = sorted(watering_events_ms)

    # Calculate intervals between consecutive events
    intervals_ms = []
    for i in range(1, len(sorted_events)):
        interval = sorted_events[i] - sorted_events[i-1]
        intervals_ms.append(interval)

    # Return average interval in seconds
    avg_interval_ms = mean(intervals_ms)
    return int(avg_interval_ms / 1000)


def update_profile_with_watering_event(
    profile: DeviceProfile,
    event_timestamp_ms: int,
    max_events_tracked: int = 20
) -> DeviceProfile:
    """
    Update device profile with a new watering event.

    Maintains a rolling window of recent watering events and recalculates
    the typical watering interval.

    Args:
        profile: Current device profile
        event_timestamp_ms: Timestamp of the new watering event
        max_events_tracked: Maximum number of events to track (default 20)

    Returns:
        Updated device profile

    Requirements: 10.3
    """
    # Add new event to the list
    profile.last_watering_events.append(event_timestamp_ms)

    # Keep only the most recent events
    if len(profile.last_watering_events) > max_events_tracked:
        profile.last_watering_events = profile.last_watering_events[-max_events_tracked:]

    # Recalculate typical watering interval if we have enough data
    profile.typical_watering_interval_sec = calculate_watering_interval(
        profile.last_watering_events
    )

    return profile


def calculate_baseline_moisture_range(
    aggregates: List[Dict[str, Any]],
    percentile_low: float = 0.1,
    percentile_high: float = 0.9
) -> Optional[Dict[str, float]]:
    """
    Calculate baseline moisture range from historical aggregates.

    Uses percentiles to establish typical moisture range, excluding
    extreme values that might represent stress or watering events.

    Args:
        aggregates: List of aggregate records with soil_moisture_stats
        percentile_low: Lower percentile for baseline (default 0.1 = 10th percentile)
        percentile_high: Upper percentile for baseline (default 0.9 = 90th percentile)

    Returns:
        Dict with "min" and "max" keys, or None if insufficient data

    Requirements: 10.1
    """
    if not aggregates:
        return None

    # Extract average moisture values from aggregates
    moisture_values = []
    for agg in aggregates:
        soil_stats = agg.get("soil_moisture_stats", {})
        avg = soil_stats.get("avg")
        if avg is not None and soil_stats.get("valid_count", 0) > 0:
            moisture_values.append(avg)

    if len(moisture_values) < 10:  # Need at least 10 data points
        return None

    # Sort values
    sorted_values = sorted(moisture_values)
    n = len(sorted_values)

    # Calculate percentile indices
    low_idx = int(n * percentile_low)
    high_idx = int(n * percentile_high)

    # Ensure indices are within bounds
    low_idx = max(0, min(low_idx, n - 1))
    high_idx = max(0, min(high_idx, n - 1))

    return {
        "min": sorted_values[low_idx],
        "max": sorted_values[high_idx]
    }


def check_stress_condition(
    profile: DeviceProfile,
    current_moisture_pct: float,
    last_watering_event_ms: Optional[int],
    current_time_ms: int
) -> bool:
    """
    Check if device is in a stress condition based on profile.

    Stress condition: moisture < 30% AND no watering in 48 hours.

    Args:
        profile: Device profile with learned patterns
        current_moisture_pct: Current soil moisture percentage
        last_watering_event_ms: Timestamp of last watering event, or None
        current_time_ms: Current timestamp in milliseconds

    Returns:
        True if stress condition detected, False otherwise

    Requirements: 10.7
    """
    # Check moisture threshold
    if current_moisture_pct >= 30.0:
        return False

    # Check time since last watering
    if last_watering_event_ms is None:
        # No watering history - consider it stress if moisture is low
        return True

    hours_since_watering = (current_time_ms - last_watering_event_ms) / (1000 * 3600)

    return hours_since_watering >= 48.0


async def fetch_recent_watering_events(
    dynamodb_client,
    table_name: str,
    hardware_id: str,
    lookback_days: int = 14
) -> List[int]:
    """
    Fetch recent watering events for a device.

    Args:
        dynamodb_client: boto3 DynamoDB client
        table_name: Name of the Events table
        hardware_id: Device hardware ID
        lookback_days: Number of days to look back (default 14)

    Returns:
        List of watering event timestamps in milliseconds
    """
    import time

    # Calculate lookback timestamp
    lookback_ms = int((time.time() - (lookback_days * 24 * 3600)) * 1000)

    response = dynamodb_client.query(
        TableName=table_name,
        KeyConditionExpression="hardware_id = :hid AND start_time_ms >= :start",
        FilterExpression="event_type = :etype",
        ExpressionAttributeValues={
            ":hid": {"S": hardware_id},
            ":start": {"N": str(lookback_ms)},
            ":etype": {"S": EventType.WATERING_EVENT.value}
        }
    )

    timestamps = []
    for item in response.get("Items", []):
        start_time = int(item["start_time_ms"]["N"])
        timestamps.append(start_time)

    return sorted(timestamps)


async def fetch_historical_aggregates(
    dynamodb_client,
    table_name: str,
    hardware_id: str,
    window_type: str = "daily",
    lookback_days: int = 30
) -> List[Dict[str, Any]]:
    """
    Fetch historical aggregates for baseline calculation.

    Args:
        dynamodb_client: boto3 DynamoDB client
        table_name: Name of the Aggregates table
        hardware_id: Device hardware ID
        window_type: Type of window ("hourly", "daily", "weekly")
        lookback_days: Number of days to look back (default 30)

    Returns:
        List of aggregate records
    """
    import time

    # Calculate lookback timestamp
    lookback_ms = int((time.time() - (lookback_days * 24 * 3600)) * 1000)

    device_window = f"{hardware_id}#{window_type}"

    response = dynamodb_client.query(
        TableName=table_name,
        KeyConditionExpression="device_window = :dw AND window_start_ms >= :start",
        ExpressionAttributeValues={
            ":dw": {"S": device_window},
            ":start": {"N": str(lookback_ms)}
        }
    )

    # Convert DynamoDB items to dicts
    aggregates = []
    for item in response.get("Items", []):
        # Simplified conversion - in production would use proper deserializer
        agg = {
            "window_start_ms": int(item["window_start_ms"]["N"]),
            "soil_moisture_stats": {}
        }

        if "soil_moisture_stats" in item:
            stats = item["soil_moisture_stats"]["M"]
            agg["soil_moisture_stats"] = {
                "avg": float(stats["avg"]["N"]) if "avg" in stats else None,
                "valid_count": int(stats["valid_count"]["N"]) if "valid_count" in stats else 0
            }

        aggregates.append(agg)

    return aggregates
