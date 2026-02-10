"""
Event Detector Lambda Handler

Processes DynamoDB Stream records from the Readings table to detect meaningful events.
"""

import os
from typing import Dict, Any
from aws_lambda_powertools import Logger
from aws_lambda_powertools.utilities.typing import LambdaContext

from shared.dynamodb_helpers import extract_reading_from_stream_record
from shared.idempotency import (
    generate_reading_id,
    is_event_processed,
    mark_event_processed_if_absent
)

logger = Logger()


@logger.inject_lambda_context
def lambda_handler(event: Dict[str, Any], context: LambdaContext) -> Dict[str, Any]:
    """
    Lambda handler for Event Detector.

    Processes DynamoDB Stream records from Readings table to detect events.

    Args:
        event: DynamoDB Stream event containing Records
        context: Lambda context

    Returns:
        Response with batch item failures for partial batch failure handling
    """
    from retry_utils import process_stream_batch_with_isolation

    logger.info("Event Detector Lambda invoked", extra={
        "record_count": len(event.get("Records", []))
    })

    # Process records with error isolation
    batch_item_failures = process_stream_batch_with_isolation(
        records=event.get("Records", []),
        process_func=process_stream_record,
        logger_instance=logger
    )

    logger.info("Event Detector processing complete", extra={
        "total_records": len(event.get("Records", [])),
        "failed_records": len(batch_item_failures)
    })

    return {
        "batchItemFailures": batch_item_failures
    }


def process_stream_record(record: Dict[str, Any]) -> None:
    """
    Process a single DynamoDB Stream record.

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

    # Parse the reading from DynamoDB format
    reading = extract_reading_from_stream_record(record)

    # Generate reading_id for idempotency check
    batch_id = reading.get("batch_id", "unknown")
    timestamp_ms = reading.get("timestamp_ms", 0)
    reading_id = generate_reading_id(batch_id, timestamp_ms)

    # Check if this reading has already been processed for event detection
    if is_event_processed(reading_id):
        logger.debug(
            "Reading already processed for event detection",
            extra={"reading_id": reading_id, "hardware_id": reading.get("hardware_id")}
        )
        return

    # Process the reading for event detection
    detect_events_for_reading(reading, reading_id)


def detect_events_for_reading(reading: Dict[str, Any], reading_id: str) -> None:
    """
    Detect events for a reading.

    Runs all event detection algorithms and persists detected events.

    Args:
        reading: Reading dict
        reading_id: Reading ID for idempotency
    """
    from shared.event_detection import (
        get_recent_readings,
        detect_watering_event,
        detect_drying_cycle,
        detect_temperature_stress,
        detect_humidity_anomaly,
        detect_environmental_change,
        check_cooldown,
        persist_event,
        update_device_status_after_event,
        create_insight_request_for_critical_event
    )

    hardware_id = reading.get("hardware_id")
    timestamp_ms = reading.get("timestamp_ms")

    logger.info(
        "Processing reading for event detection",
        extra={
            "hardware_id": hardware_id,
            "timestamp_ms": timestamp_ms,
            "reading_id": reading_id
        }
    )

    # Fetch recent readings for context (last 6 hours for drying, 2 hours for environmental)
    six_hours_ago = timestamp_ms - (6 * 60 * 60 * 1000)
    recent_readings = get_recent_readings(hardware_id, six_hours_ago, limit=200)

    events_detected = []

    # Run all detection algorithms
    detectors = [
        ("watering", lambda: detect_watering_event(recent_readings, reading)),
        ("drying", lambda: detect_drying_cycle(recent_readings, reading)),
        ("temperature_stress", lambda: detect_temperature_stress(reading)),
        ("humidity_anomaly", lambda: detect_humidity_anomaly(recent_readings, reading)),
        ("environmental_change", lambda: detect_environmental_change(recent_readings, reading))
    ]

    for detector_name, detector_func in detectors:
        try:
            event = detector_func()
            if event:
                # Check cooldown before persisting
                if not check_cooldown(hardware_id, event.event_type, timestamp_ms):
                    if persist_event(event):
                        events_detected.append(event)
                        logger.info(
                            f"{detector_name} event detected",
                            extra={
                                "hardware_id": hardware_id,
                                "event_type": event.event_type.value,
                                "start_time_ms": event.start_time_ms
                            }
                        )

                        # Create insight request for critical events
                        create_insight_request_for_critical_event(hardware_id, event.event_type)
                else:
                    logger.debug(
                        f"{detector_name} event in cooldown, skipping",
                        extra={
                            "hardware_id": hardware_id,
                            "event_type": event.event_type.value
                        }
                    )
        except Exception as e:
            logger.error(
                f"Error in {detector_name} detection",
                extra={
                    "hardware_id": hardware_id,
                    "error": str(e),
                    "error_type": type(e).__name__
                }
            )
            # Continue with other detectors

    # Update device status if any events were detected
    if events_detected:
        update_device_status_after_event(hardware_id, timestamp_ms)

    # Mark reading as processed for event detection
    mark_event_processed_if_absent(reading_id, hardware_id)
