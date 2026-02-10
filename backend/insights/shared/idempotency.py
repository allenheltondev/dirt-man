"""
Reading idempotency utilities for stage-aware processing.

Provides functions to check and mark readings as processed for different pipeline stages.
"""

import os
import time
from typing import Optional
import boto3
from botocore.exceptions import ClientError
from aws_lambda_powertools import Logger

logger = Logger(child=True)

# DynamoDB client
dynamodb = boto3.client("dynamodb")

# Table name from environment
PROCESSED_READINGS_TABLE = os.environ.get("PROCESSED_READINGS_TABLE", "plant_processed_readings")


def generate_reading_id(batch_id: str, timestamp_ms: int) -> str:
    """
    Generate reading_id from batch_id and timestamp_ms.

    Args:
        batch_id: Batch ID from the reading
        timestamp_ms: Timestamp in milliseconds

    Returns:
        Reading ID string in format: batch_id#timestamp_ms
    """
    return f"{batch_id}#{timestamp_ms}"


def is_event_processed(reading_id: str) -> bool:
    """
    Check if reading has already been processed for event detection.

    Args:
        reading_id: Reading ID

    Returns:
        True if already processed, False otherwise
    """
    try:
        response = dynamodb.get_item(
            TableName=PROCESSED_READINGS_TABLE,
            Key={"reading_id": {"S": reading_id}},
            ProjectionExpression="event_processed_at_ms"
        )

        item = response.get("Item", {})
        return "event_processed_at_ms" in item

    except ClientError as e:
        logger.error(
            "Error checking event processed status",
            extra={"reading_id": reading_id, "error": str(e)}
        )
        # On error, assume not processed to avoid data loss
        return False


def mark_event_processed_if_absent(reading_id: str, hardware_id: str) -> bool:
    """
    Mark reading as processed for event detection using conditional write.

    Only marks if not already marked to ensure idempotency.

    Args:
        reading_id: Reading ID
        hardware_id: Device hardware ID

    Returns:
        True if successfully marked (was not already marked), False if already marked
    """
    now_ms = int(time.time() * 1000)
    ttl = int(time.time()) + (30 * 24 * 60 * 60)  # 30 days from now

    try:
        dynamodb.update_item(
            TableName=PROCESSED_READINGS_TABLE,
            Key={"reading_id": {"S": reading_id}},
            UpdateExpression="SET event_processed_at_ms = :now, hardware_id = :hw_id, ttl = :ttl",
            ConditionExpression="attribute_not_exists(event_processed_at_ms)",
            ExpressionAttributeValues={
                ":now": {"N": str(now_ms)},
                ":hw_id": {"S": hardware_id},
                ":ttl": {"N": str(ttl)}
            }
        )
        return True

    except ClientError as e:
        if e.response["Error"]["Code"] == "ConditionalCheckFailedException":
            # Already processed
            logger.debug(
                "Reading already marked as event processed",
                extra={"reading_id": reading_id}
            )
            return False
        else:
            logger.error(
                "Error marking reading as event processed",
                extra={"reading_id": reading_id, "error": str(e)}
            )
            raise


def is_aggregate_processed(reading_id: str) -> bool:
    """
    Check if reading has already been processed for aggregation.

    Args:
        reading_id: Reading ID

    Returns:
        True if already processed, False otherwise
    """
    try:
        response = dynamodb.get_item(
            TableName=PROCESSED_READINGS_TABLE,
            Key={"reading_id": {"S": reading_id}},
            ProjectionExpression="aggregate_processed_at_ms"
        )

        item = response.get("Item", {})
        return "aggregate_processed_at_ms" in item

    except ClientError as e:
        logger.error(
            "Error checking aggregate processed status",
            extra={"reading_id": reading_id, "error": str(e)}
        )
        return False


def mark_aggregate_processed_if_absent(reading_id: str, hardware_id: str) -> bool:
    """
    Mark reading as processed for aggregation using conditional write.

    Args:
        reading_id: Reading ID
        hardware_id: Device hardware ID

    Returns:
        True if successfully marked, False if already marked
    """
    now_ms = int(time.time() * 1000)
    ttl = int(time.time()) + (30 * 24 * 60 * 60)  # 30 days

    try:
        dynamodb.update_item(
            TableName=PROCESSED_READINGS_TABLE,
            Key={"reading_id": {"S": reading_id}},
            UpdateExpression="SET aggregate_processed_at_ms = :now, hardware_id = :hw_id, ttl = :ttl",
            ConditionExpression="attribute_not_exists(aggregate_processed_at_ms)",
            ExpressionAttributeValues={
                ":now": {"N": str(now_ms)},
                ":hw_id": {"S": hardware_id},
                ":ttl": {"N": str(ttl)}
            }
        )
        return True

    except ClientError as e:
        if e.response["Error"]["Code"] == "ConditionalCheckFailedException":
            return False
        else:
            logger.error(
                "Error marking reading as aggregate processed",
                extra={"reading_id": reading_id, "error": str(e)}
            )
            raise


def is_status_processed(reading_id: str) -> bool:
    """
    Check if reading has already been processed for status update.

    Args:
        reading_id: Reading ID

    Returns:
        True if already processed, False otherwise
    """
    try:
        response = dynamodb.get_item(
            TableName=PROCESSED_READINGS_TABLE,
            Key={"reading_id": {"S": reading_id}},
            ProjectionExpression="status_processed_at_ms"
        )

        item = response.get("Item", {})
        return "status_processed_at_ms" in item

    except ClientError as e:
        logger.error(
            "Error checking status processed status",
            extra={"reading_id": reading_id, "error": str(e)}
        )
        return False


def mark_status_processed_if_absent(reading_id: str, hardware_id: str) -> bool:
    """
    Mark reading as processed for status update using conditional write.

    Args:
        reading_id: Reading ID
        hardware_id: Device hardware ID

    Returns:
        True if successfully marked, False if already marked
    """
    now_ms = int(time.time() * 1000)
    ttl = int(time.time()) + (30 * 24 * 60 * 60)  # 30 days

    try:
        dynamodb.update_item(
            TableName=PROCESSED_READINGS_TABLE,
            Key={"reading_id": {"S": reading_id}},
            UpdateExpression="SET status_processed_at_ms = :now, hardware_id = :hw_id, ttl = :ttl",
            ConditionExpression="attribute_not_exists(status_processed_at_ms)",
            ExpressionAttributeValues={
                ":now": {"N": str(now_ms)},
                ":hw_id": {"S": hardware_id},
                ":ttl": {"N": str(ttl)}
            }
        )
        return True

    except ClientError as e:
        if e.response["Error"]["Code"] == "ConditionalCheckFailedException":
            return False
        else:
            logger.error(
                "Error marking reading as status processed",
                extra={"reading_id": reading_id, "error": str(e)}
            )
            raise
