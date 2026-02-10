"""
Device Status Updater Lambda Handler

Maintains per-device health status in DynamoDB:
- Updates last_seen timestamps
- Computes health categories
- Tracks sensor status summary
- Records processing errors

Processes DynamoDB Stream records from the Readings table to update device status.
"""

import os
from typing import Dict, Any
from aws_lambda_powertools import Logger
from aws_lambda_powertools.utilities.typing import LambdaContext

from shared.dynamodb_helpers import extract_reading_from_stream_record
from shared.device_status import derive_health_category
from shared.idempotency import generate_reading_id

logger = Logger()


@logger.inject_lambda_context
def lambda_handler(event: Dict[str, Any], context: LambdaContext) -> Dict[str, Any]:
    """
    Lambda handler for Device Status Updater.

    Processes DynamoDB Stream records from Readings table to update device status.

    Args:
        event: DynamoDB Stream event containing Records
        context: Lambda context

    Returns:
        Response with batch item failures for partial batch failure handling
    """
    from shared.retry_utils import process_stream_batch_with_isolation

    logger.info("Device Status Updater Lambda invoked", extra={
        "record_count": len(event.get("Records", []))
    })

    # Process records with error isolation
    batch_item_failures = process_stream_batch_with_isolation(
        records=event.get("Records", []),
        process_func=process_stream_record,
        logger_instance=logger
    )

    logger.info("Device Status Updater processing complete", extra={
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

    # Update device status based on the reading
    update_device_status_from_reading(reading)


def update_device_status_from_reading(reading: Dict[str, Any]) -> None:
    """
    Update device status based on a reading.

    Args:
        reading: Reading dict
    """
    hardware_id = reading.get("hardware_id")
    timestamp_ms = reading.get("timestamp_ms")

    logger.info(
        "Updating device status from reading",
        extra={
            "hardware_id": hardware_id,
            "timestamp_ms": timestamp_ms
        }
    )

    # TODO: Implement device status update logic
    # This will include:
    # - Fetching current device status from DynamoDB
    # - Updating last_seen_ingest_time_ms
    # - Deriving health category using derive_health_category()
    # - Updating sensor status summary
    # - Writing updated status back to DynamoDB
