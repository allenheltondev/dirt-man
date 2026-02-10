"""
Device Status update utilities.

Provides functions for:
- Field ownership enforcement per pipeline
- Update expression generation for owned fields only
- Health category derivation
- Error tracking and truncation
- Status summary computation
"""

from typing import Optional, List
from datetime import datetime
from shared.models import DeviceStatus, HealthCategory, ErrorRecord


def derive_health_category(
    last_seen_ingest_time_ms: Optional[int],
    last_error_at_ms: Optional[int],
    current_time_ms: Optional[int] = None
) -> HealthCategory:
    """
    Derive health category from last_seen_ingest_time and last_error_at.

    Rules (Requirements 23.20-23.24):
    - healthy: last_seen_ingest_time within 2 hours
    - stale: last_seen_ingest_time between 2 and 6 hours ago
    - missing: last_seen_ingest_time more than 6 hours ago
    - failing: last_error_at within 24 hours (takes precedence)

    Args:
        last_seen_ingest_time_ms: Last time data was ingested (ingest time, not event time)
        last_error_at_ms: Last time an error occurred
        current_time_ms: Current time in milliseconds (defaults to now)

    Returns:
        HealthCategory enum value
    """
    if current_time_ms is None:
        current_time_ms = int(datetime.utcnow().timestamp() * 1000)

    # Failing takes precedence if error within 24 hours
    if last_error_at_ms is not None:
        hours_since_error = (current_time_ms - last_error_at_ms) / (1000 * 3600)
        if hours_since_error <= 24:
            return HealthCategory.FAILING

    # If no last_seen_ingest_time, consider missing
    if last_seen_ingest_time_ms is None:
        return HealthCategory.MISSING

    # Calculate hours since last seen
    hours_since_seen = (current_time_ms - last_seen_ingest_time_ms) / (1000 * 3600)

    if hours_since_seen <= 2:
        return HealthCategory.HEALTHY
    elif hours_since_seen <= 6:
        return HealthCategory.STALE
    else:
        return HealthCategory.MISSING


def truncate_error_message(message: str, max_length: int = 256) -> str:
    """
    Truncate error message to specified length.

    Args:
        message: Error message to truncate
        max_length: Maximum length (default 256 chars per Requirement 23.15)

    Returns:
        Truncated message
    """
    if len(message) <= max_length:
        return message
    return message[:max_length]


def append_error_to_list(
    existing_errors: List[ErrorRecord],
    timestamp_ms: int,
    error_code: str,
    error_message: str,
    max_errors: int = 10
) -> List[ErrorRecord]:
    """
    Append error to error list, maintaining max size and truncating message.

    Requirements 23.15, 23.19:
    - Truncate error_message to 256 characters
    - Maintain max 10 error records

    Args:
        existing_errors: Current list of errors
        timestamp_ms: Error timestamp
        error_code: Error code
        error_message: Error message (will be truncated)
        max_errors: Maximum number of errors to keep (default 10)

    Returns:
        Updated error list with new error appended and oldest removed if needed
    """
    truncated_message = truncate_error_message(error_message)

    new_error = ErrorRecord(
        timestamp_ms=timestamp_ms,
        error_code=error_code,
        error_message=truncated_message
    )

    # Append new error
    updated_errors = existing_errors + [new_error]

    # Keep only the most recent max_errors
    if len(updated_errors) > max_errors:
        updated_errors = updated_errors[-max_errors:]

    return updated_errors


def update_device_status_with_error(
    device_status: DeviceStatus,
    error_code: str,
    error_message: str,
    error_timestamp_ms: Optional[int] = None
) -> DeviceStatus:
    """
    Update device status with a new error.

    Updates:
    - last_error_at_ms
    - last_error_code
    - last_errors list (appends with truncation)

    Args:
        device_status: Current device status
        error_code: Error code
        error_message: Error message
        error_timestamp_ms: Error timestamp (defaults to now)

    Returns:
        Updated device status
    """
    if error_timestamp_ms is None:
        error_timestamp_ms = int(datetime.utcnow().timestamp() * 1000)

    device_status.last_error_at_ms = error_timestamp_ms
    device_status.last_error_code = error_code
    device_status.last_errors = append_error_to_list(
        device_status.last_errors,
        error_timestamp_ms,
        error_code,
        error_message
    )

    return device_status


def update_device_status_field(
    hardware_id: str,
    field_name: str,
    field_value: any,
    table_name: str = 'plant_device_status'
) -> None:
    """
    Update a single field in Device Status table.

    Uses DynamoDB update expression to update only the specified field
    without overwriting other fields.

    Args:
        hardware_id: Device hardware ID
        field_name: Name of the field to update
        field_value: Value to set
        table_name: DynamoDB table name (defaults to plant_device_status)
    """
    import boto3
    import time

    dynamodb = boto3.resource('dynamodb')
    table = dynamodb.Table(table_name)

    now_ms = int(time.time() * 1000)

    try:
        table.update_item(
            Key={'hardware_id': hardware_id},
            UpdateExpression=f'SET {field_name} = :val, updated_at_ms = :now',
            ExpressionAttributeValues={
                ':val': field_value,
                ':now': now_ms
            }
        )
    except Exception as e:
        # Log error but don't fail the operation
        import logging
        logging.error(f"Error updating device status field {field_name} for {hardware_id}: {e}")


def record_device_error(
    hardware_id: str,
    error_code: str,
    error_message: str,
    table_name: str = 'plant_device_status'
) -> None:
    """
    Record an error in Device Status table.

    Updates last_error_at_ms, last_error_code, and appends to last_errors list.

    Args:
        hardware_id: Device hardware ID
        error_code: Error code
        error_message: Error message (will be truncated to 256 chars)
        table_name: DynamoDB table name (defaults to plant_device_status)
    """
    import boto3
    import time

    dynamodb = boto3.resource('dynamodb')
    table = dynamodb.Table(table_name)

    now_ms = int(time.time() * 1000)
    truncated_message = truncate_error_message(error_message)

    try:
        # First, get current errors list
        response = table.get_item(Key={'hardware_id': hardware_id})

        existing_errors = []
        if 'Item' in response:
            existing_errors_raw = response['Item'].get('last_errors', [])
            existing_errors = [
                ErrorRecord(
                    timestamp_ms=err['timestamp_ms'],
                    error_code=err['error_code'],
                    error_message=err['error_message']
                )
                for err in existing_errors_raw
            ]

        # Append new error
        updated_errors = append_error_to_list(
            existing_errors,
            now_ms,
            error_code,
            truncated_message
        )

        # Convert to DynamoDB format
        errors_list = [
            {
                'timestamp_ms': err.timestamp_ms,
                'error_code': err.error_code,
                'error_message': err.error_message
            }
            for err in updated_errors
        ]

        # Update device status
        table.update_item(
            Key={'hardware_id': hardware_id},
            UpdateExpression='SET last_error_at_ms = :error_at, last_error_code = :error_code, last_errors = :errors, updated_at_ms = :now',
            ExpressionAttributeValues={
                ':error_at': now_ms,
                ':error_code': error_code,
                ':errors': errors_list,
                ':now': now_ms
            }
        )
    except Exception as e:
        # Log error but don't fail the operation
        import logging
        logging.error(f"Error recording device error for {hardware_id}: {e}")

