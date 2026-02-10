"""
Time utilities for Plant Insights and Events.

Provides functions for:
- UTC time window alignment (minute, hour, day, week)
- Lateness window checks
- Clock skew detection
- Time boundary calculations
"""

from datetime import datetime, timezone, timedelta
from typing import Tuple
from aws_lambda_powertools import Logger

logger = Logger(child=True)

# Constants
LATENESS_WINDOW_HOURS = 24
CLOCK_SKEW_THRESHOLD_MINUTES = 5


def align_to_hour(timestamp_ms: int) -> int:
    """
    Align timestamp to the start of its UTC hour.

    Args:
        timestamp_ms: Timestamp in milliseconds

    Returns:
        Timestamp in milliseconds aligned to hour start
    """
    dt = datetime.fromtimestamp(timestamp_ms / 1000, tz=timezone.utc)
    aligned = dt.replace(minute=0, second=0, microsecond=0)
    return int(aligned.timestamp() * 1000)


def align_to_day(timestamp_ms: int) -> int:
    """
    Align timestamp to the start of its UTC day.

    Args:
        timestamp_ms: Timestamp in milliseconds

    Returns:
        Timestamp in milliseconds aligned to day start
    """
    dt = datetime.fromtimestamp(timestamp_ms / 1000, tz=timezone.utc)
    aligned = dt.replace(hour=0, minute=0, second=0, microsecond=0)
    return int(aligned.timestamp() * 1000)


def align_to_week(timestamp_ms: int) -> int:
    """
    Align timestamp to the start of its ISO week (Monday).

    Args:
        timestamp_ms: Timestamp in milliseconds

    Returns:
        Timestamp in milliseconds aligned to week start (Monday 00:00:00 UTC)
    """
    dt = datetime.fromtimestamp(timestamp_ms / 1000, tz=timezone.utc)
    # Get the start of the current day
    day_start = dt.replace(hour=0, minute=0, second=0, microsecond=0)
    # Calculate days to subtract to get to Monday (0 = Monday, 6 = Sunday)
    days_since_monday = day_start.weekday()
    week_start = day_start - timedelta(days=days_since_monday)
    return int(week_start.timestamp() * 1000)


def get_hour_window(timestamp_ms: int) -> Tuple[int, int]:
    """
    Get the hour window boundaries for a timestamp.

    Args:
        timestamp_ms: Timestamp in milliseconds

    Returns:
        Tuple of (window_start_ms, window_end_ms)
    """
    window_start = align_to_hour(timestamp_ms)
    window_end = window_start + (60 * 60 * 1000)  # +1 hour
    return window_start, window_end


def get_day_window(timestamp_ms: int) -> Tuple[int, int]:
    """
    Get the day window boundaries for a timestamp.

    Args:
        timestamp_ms: Timestamp in milliseconds

    Returns:
        Tuple of (window_start_ms, window_end_ms)
    """
    window_start = align_to_day(timestamp_ms)
    window_end = window_start + (24 * 60 * 60 * 1000)  # +1 day
    return window_start, window_end


def get_week_window(timestamp_ms: int) -> Tuple[int, int]:
    """
    Get the week window boundaries for a timestamp.

    Args:
        timestamp_ms: Timestamp in milliseconds

    Returns:
        Tuple of (window_start_ms, window_end_ms)
    """
    window_start = align_to_week(timestamp_ms)
    window_end = window_start + (7 * 24 * 60 * 60 * 1000)  # +1 week
    return window_start, window_end


def is_within_lateness_window(event_time_ms: int, window_end_ms: int, now_ms: int) -> bool:
    """
    Check if a reading is within the lateness window for a closed time window.

    Args:
        event_time_ms: Event timestamp in milliseconds
        window_end_ms: Window end timestamp in milliseconds
        now_ms: Current timestamp in milliseconds

    Returns:
        True if within lateness window (24 hours after window close)
    """
    if now_ms <= window_end_ms:
        # Window is still open
        return True

    time_since_window_close_ms = now_ms - window_end_ms
    lateness_window_ms = LATENESS_WINDOW_HOURS * 60 * 60 * 1000

    return time_since_window_close_ms <= lateness_window_ms


def check_clock_skew(event_time_ms: int, ingest_time_ms: int) -> bool:
    """
    Check if there's significant clock skew between event time and ingest time.

    Logs a warning if event_time is more than 5 minutes ahead of ingest_time.

    Args:
        event_time_ms: Event timestamp in milliseconds
        ingest_time_ms: Ingest timestamp in milliseconds

    Returns:
        True if clock skew detected, False otherwise
    """
    skew_ms = event_time_ms - ingest_time_ms
    threshold_ms = CLOCK_SKEW_THRESHOLD_MINUTES * 60 * 1000

    if skew_ms > threshold_ms:
        logger.warning(
            "Clock skew detected",
            extra={
                "event_time_ms": event_time_ms,
                "ingest_time_ms": ingest_time_ms,
                "skew_minutes": skew_ms / (60 * 1000)
            }
        )
        return True

    return False


def is_window_closed(window_end_ms: int, now_ms: int) -> bool:
    """
    Check if a time window is closed.

    Args:
        window_end_ms: Window end timestamp in milliseconds
        now_ms: Current timestamp in milliseconds

    Returns:
        True if window is closed
    """
    return now_ms > window_end_ms
