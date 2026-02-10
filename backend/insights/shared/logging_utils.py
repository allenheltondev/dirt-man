"""
Logging utilities for structured logging across all Lambda functions.

Provides helper functions for consistent structured logging with AWS Lambda Powertools.
"""

from typing import Dict, Any, Optional
from aws_lambda_powertools import Logger


def log_clock_skew_warning(
    logger: Logger,
    hardware_id: str,
    event_time_ms: int,
    ingest_time_ms: int,
    reading_id: Optional[str] = None
) -> None:
    """
    Log a clock skew warning when event_time is more than 5 minutes ahead of ingest_time.

    Args:
        logger: Logger instance
        hardware_id: Device hardware ID
        event_time_ms: Event timestamp from device
        ingest_time_ms: Ingest timestamp from backend
        reading_id: Optional reading ID for correlation
    """
    skew_seconds = (event_time_ms - ingest_time_ms) / 1000.0
    skew_minutes = skew_seconds / 60.0

    logger.warning(
        "Clock skew detected - event_time ahead of ingest_time",
        extra={
            "hardware_id": hardware_id,
            "reading_id": reading_id,
            "event_time_ms": event_time_ms,
            "ingest_time_ms": ingest_time_ms,
            "skew_seconds": skew_seconds,
            "skew_minutes": skew_minutes,
            "warning_type": "clock_skew"
        }
    )


def log_late_arrival(
    logger: Logger,
    hardware_id: str,
    reading_timestamp_ms: int,
    window_start_ms: int,
    window_end_ms: int,
    lateness_hours: float,
    within_lateness_window: bool,
    reading_id: Optional[str] = None
) -> None:
    """
    Log a late data arrival event.

    Args:
        logger: Logger instance
        hardware_id: Device hardware ID
        reading_timestamp_ms: Reading timestamp
        window_start_ms: Window start timestamp
        window_end_ms: Window end timestamp
        lateness_hours: Hours late
        within_lateness_window: Whether within 24-hour lateness window
        reading_id: Optional reading ID for correlation
    """
    log_level = "info" if within_lateness_window else "warning"
    action = "triggering_rebuild" if within_lateness_window else "skipping_aggregate_update"

    l
    "event_type": "late_arrival"
        }
    )


def log_event_detection(
    logger: Logger,
    hardware_id: str,
    event_type: str,
    start_time_ms: int,
    end_time_ms: Optional[int] = None,
    detection_metadata: Optional[Dict[str, Any]] = None,
    reading_id: Optional[str] = None
) -> None:
    """
    Log an event detection.

    Args:
        logger: Logger instance
        hardware_id: Device hardware ID
        event_type: Type of event detected
        start_time_ms: Event start timestamp
        end_time_ms: Optional event end timestamp
        detection_metadata: Optional metadata about detection
        reading_id: Optional reading ID that triggered detection
    """
    extra_data = {
        "hardware_id": hardware_id,
        "reading_id": reading_id,
        "event_type": event_type,
        "start_time_ms": start_time_ms,
        "event_category": "event_detection"
    }

    if end_time_ms:
        extra_data["end_time_ms"] = end_time_ms
        extra_data["duration_ms"] = end_time_ms - start_time_ms

    if detection_metadata:
        extra_data["detection_metadata"] = detection_metadata

    logger.info(
        f"Event detected: {event_type}",
        extra=extra_data
    )


def log_event_cooldown_skip(
    logger: Logger,
    hardware_id: str,
    event_type: str,
    last_event_time_ms: int,
    cooldown_minutes: int,
    reading_id: Optional[str] = None
) -> None:
    """
    Log when an event is skipped due to cooldown.

    Args:
        logger: Logger instance
        hardware_id: Device hardware ID
        event_type: Type of event
        last_event_time_ms: Timestamp of last event of this type
        cooldown_minutes: Cooldown period in minutes
        reading_id: Optional reading ID
    """
    logger.debug(
        f"Event skipped due to cooldown: {event_type}",
        extra={
            "hardware_id": hardware_id,
            "reading_id": reading_id,
            "event_type": event_type,
            "last_event_time_ms": last_event_time_ms,
            "cooldown_minutes": cooldown_minutes,
            "event_category": "event_cooldown"
        }
    )


def log_aggregate_update(
    logger: Logger,
    hardware_id: str,
    window_type: str,
    window_start_ms: int,
    window_end_ms: int,
    update_type: str,  # "incremental" or "rebuild"
    reading_count: Optional[int] = None,
    reading_id: Optional[str] = None
) -> None:
    """
    Log an aggregate update.

    Args:
        logger: Logger instance
        hardware_id: Device hardware ID
        window_type: Type of window (hourly, daily, weekly)
        window_start_ms: Window start timestamp
        window_end_ms: Window end timestamp
        update_type: Type of update (incremental or rebuild)
        reading_count: Optional count of readings in aggregate
        reading_id: Optional reading ID that triggered update
    """
    extra_data = {
        "hardware_id": hardware_id,
        "reading_id": reading_id,
        "window_type": window_type,
        "window_start_ms": window_start_ms,
        "window_end_ms": window_end_ms,
        "update_type": update_type,
        "event_category": "aggregate_update"
    }

    if reading_count is not None:
        extra_data["reading_count"] = reading_count

    logger.info(
        f"Aggregate updated: {window_type} {update_type}",
        extra=extra_data
    )


def log_insight_generation_start(
    logger: Logger,
    hardware_id: str,
    request_type: str,
    request_id: Optional[str] = None
) -> None:
    """
    Log the start of insight generation.

    Args:
        logger: Logger instance
        hardware_id: Device hardware ID
        request_type: Type of request (scheduled or event)
        request_id: Optional request ID for correlation
    """
    logger.info(
        "Starting insight generation",
        extra={
            "hardware_id": hardware_id,
            "request_id": request_id,
            "request_type": request_type,
            "event_category": "insight_generation",
            "phase": "start"
        }
    )


def log_insight_generation_success(
    logger: Logger,
    hardware_id: str,
    confidence: str,
    trend: str,
    generation_duration_ms: int,
    request_id: Optional[str] = None,
    aggregate_count: Optional[int] = None,
    event_count: Optional[int] = None
) -> None:
    """
    Log successful insight generation.

    Args:
        logger: Logger instance
        hardware_id: Device hardware ID
        confidence: Confidence level
        trend: Trend classification
        generation_duration_ms: Generation duration in milliseconds
        request_id: Optional request ID for correlation
        aggregate_count: Optional count of aggregates used
        event_count: Optional count of events used
    """
    extra_data = {
        "hardware_id": hardware_id,
        "request_id": request_id,
        "confidence": confidence,
        "trend": trend,
        "generation_duration_ms": generation_duration_ms,
        "event_category": "insight_generation",
        "phase": "success",
        "outcome": "success"
    }

    if aggregate_count is not None:
        extra_data["aggregate_count"] = aggregate_count

    if event_count is not None:
        extra_data["event_count"] = event_count

    logger.info(
        "Insight generation completed successfully",
        extra=extra_data
    )


def log_insight_generation_failure(
    logger: Logger,
    hardware_id: str,
    error_message: str,
    error_type: str,
    generation_duration_ms: Optional[int] = None,
    request_id: Optional[str] = None
) -> None:
    """
    Log failed insight generation.

    Args:
        logger: Logger instance
        hardware_id: Device hardware ID
        error_message: Error message
        error_type: Type of error
        generation_duration_ms: Optional generation duration before failure
        request_id: Optional request ID for correlation
    """
    extra_data = {
        "hardware_id": hardware_id,
        "request_id": request_id,
        "error_message": error_message[:256],  # Truncate
        "error_type": error_type,
        "event_category": "insight_generation",
        "phase": "failure",
        "outcome": "failure"
    }

    if generation_duration_ms is not None:
        extra_data["generation_duration_ms"] = generation_duration_ms

    logger.error(
        "Insight generation failed",
        extra=extra_data
    )


def log_llm_api_call(
    logger: Logger,
    hardware_id: str,
    attempt: int,
    max_retries: int,
    success: bool,
    duration_ms: Optional[int] = None,
    error_message: Optional[str] = None,
    request_id: Optional[str] = None
) -> None:
    """
    Log an LLM API call attempt.

    Args:
        logger: Logger instance
        hardware_id: Device hardware ID
        attempt: Attempt number (1-indexed)
        max_retries: Maximum number of retries
        success: Whether the call succeeded
        duration_ms: Optional call duration
        error_message: Optional error message if failed
        request_id: Optional request ID for correlation
    """
    extra_data = {
        "hardware_id": hardware_id,
        "request_id": request_id,
        "attempt": attempt,
        "max_retries": max_retries,
        "success": success,
        "event_category": "llm_api_call"
    }

    if duration_ms is not None:
        extra_data["duration_ms"] = duration_ms

    if success:
        logger.info(
            f"LLM API call succeeded (attempt {attempt}/{max_retries})",
            extra=extra_data
        )
    else:
        extra_data["error_message"] = error_message[:256] if error_message else None
        log_func = logger.warning if attempt < max_retries else logger.error
        log_func(
            f"LLM API call failed (attempt {attempt}/{max_retries})",
            extra=extra_data
        )


def log_stream_processing_error(
    logger: Logger,
    sequence_number: str,
    error_message: str,
    error_type: str,
    hardware_id: Optional[str] = None,
    reading_id: Optional[str] = None
) -> None:
    """
    Log a stream processing error.

    Args:
        logger: Logger instance
        sequence_number: DynamoDB Stream sequence number
        error_message: Error message
        error_type: Type of error
        hardware_id: Optional device hardware ID
        reading_id: Optional reading ID
    """
    logger.error(
        "Stream record processing failed",
        extra={
            "sequence_number": sequence_number,
            "hardware_id": hardware_id,
            "reading_id": reading_id,
            "error_message": error_message[:256],
            "error_type": error_type,
            "event_category": "stream_processing_error"
        }
    )


def log_device_status_update(
    logger: Logger,
    hardware_id: str,
    fields_updated: Dict[str, Any],
    update_source: str  # "event_detector", "aggregator", "insight_generator", etc.
) -> None:
    """
    Log a device status update.

    Args:
        logger: Logger instance
        hardware_id: Device hardware ID
        fields_updated: Dict of fields that were updated
        update_source: Source of the update
    """
    logger.debug(
        f"Device status updated by {update_source}",
        extra={
            "hardware_id": hardware_id,
            "fields_updated": list(fields_updated.keys()),
            "update_source": update_source,
            "event_category": "device_status_update"
        }
    )
