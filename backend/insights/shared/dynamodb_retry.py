"""
DynamoDB retry wrappers with exponential backoff.

Provides additional retry logic on top of boto3's built-in retries for critical operations.
"""

from typing import Dict, Any, Optional, Callable
from botocore.exceptions import ClientError
from aws_lambda_powertools import Logger
from retry_utils import exponential_backoff_retry

logger = Logger(child=True)

# Retryable DynamoDB exceptions
RETRYABLE_EXCEPTIONS = (
    ClientError,  # Will check error code inside
)


def is_retryable_dynamodb_error(error: ClientError) -> bool:
    """
    Check if a DynamoDB ClientError is retryable.

    Args:
        error: ClientError from boto3

    Returns:
        True if the error is retryable
    """
    if not isinstance(error, ClientError):
        return False

    error_code = error.response.get('Error', {}).get('Code', '')

    # Retryable error codes
    retryable_codes = [
        'ProvisionedThroughputExceededException',
        'ThrottlingException',
        'RequestLimitExceeded',
        'InternalServerError',
        'ServiceUnavailable'
    ]

    return error_code in retryable_codes


@exponential_backoff_retry(
    max_retries=3,
    base_delay=0.5,
    max_delay=10.0,
    exponential_base=2.0,
    exceptions=(ClientError,)
)
def retry_dynamodb_put_item(
    dynamodb_client: Any,
    table_name: str,
    item: Dict[str, Any],
    **kwargs
) -> Dict[str, Any]:
    """
    Put item to DynamoDB with retry logic.

    Args:
        dynamodb_client: boto3 DynamoDB client
        table_name: Table name
        item: Item to put
        **kwargs: Additional arguments for put_item

    Returns:
        Response from put_item

    Raises:
        ClientError if all retries fail
    """
    try:
        return dynamodb_client.put_item(
            TableName=table_name,
            Item=item,
            **kwargs
        )
    except ClientError as e:
        if is_retryable_dynamodb_error(e):
            raise  # Will be retried by decorator
        else:
            # Non-retryable error, log and re-raise
            logger.error(
                "Non-retryable DynamoDB error",
                extra={
                    "error_code": e.response.get('Error', {}).get('Code'),
                    "error_message": str(e),
                    "table_name": table_name
                }
            )
            raise


@exponential_backoff_retry(
    max_retries=3,
    base_delay=0.5,
    max_delay=10.0,
    exponential_base=2.0,
    exceptions=(ClientError,)
)
def retry_dynamodb_update_item(
    dynamodb_client: Any,
    table_name: str,
    key: Dict[str, Any],
    update_expression: str,
    **kwargs
) -> Dict[str, Any]:
    """
    Update item in DynamoDB with retry logic.

    Args:
        dynamodb_client: boto3 DynamoDB client
        table_name: Table name
        key: Primary key of item to update
        update_expression: Update expression
        **kwargs: Additional arguments for update_item

    Returns:
        Response from update_item

    Raises:
        ClientError if all retries fail
    """
    try:
        return dynamodb_client.update_item(
            TableName=table_name,
            Key=key,
            UpdateExpression=update_expression,
            **kwargs
        )
    except ClientError as e:
        if is_retryable_dynamodb_error(e):
            raise  # Will be retried by decorator
        else:
            # Non-retryable error, log and re-raise
            logger.error(
                "Non-retryable DynamoDB error",
                extra={
                    "error_code": e.response.get('Error', {}).get('Code'),
                    "error_message": str(e),
                    "table_name": table_name
                }
            )
            raise


@exponential_backoff_retry(
    max_retries=3,
    base_delay=0.5,
    max_delay=10.0,
    exponential_base=2.0,
    exceptions=(ClientError,)
)
def retry_dynamodb_query(
    dynamodb_client: Any,
    table_name: str,
    **kwargs
) -> Dict[str, Any]:
    """
    Query DynamoDB with retry logic.

    Args:
        dynamodb_client: boto3 DynamoDB client or resource
        table_name: Table name
        **kwargs: Arguments for query

    Returns:
        Response from query

    Raises:
        ClientError if all retries fail
    """
    try:
        # Check if this is a client or resource
        if hasattr(dynamodb_client, 'query'):
            # It's a client
            return dynamodb_client.query(
                TableName=table_name,
                **kwargs
            )
        else:
            # It's a table resource
            return dynamodb_client.query(**kwargs)
    except ClientError as e:
        if is_retryable_dynamodb_error(e):
            raise  # Will be retried by decorator
        else:
            # Non-retryable error, log and re-raise
            logger.error(
                "Non-retryable DynamoDB error",
                extra={
                    "error_code": e.response.get('Error', {}).get('Code'),
                    "error_message": str(e),
                    "table_name": table_name
                }
            )
            raise


def safe_dynamodb_operation(
    operation: Callable,
    operation_name: str,
    logger_instance: Optional[Logger] = None,
    **kwargs
) -> Optional[Dict[str, Any]]:
    """
    Execute a DynamoDB operation with error handling and logging.

    Args:
        operation: DynamoDB operation function to execute
        operation_name: Name of the operation for logging
        logger_instance: Optional logger instance
        **kwargs: Arguments to pass to the operation

    Returns:
        Operation result or None if failed

    Example:
        result = safe_dynamodb_operation(
            operation=lambda: table.put_item(Item=item),
            operation_name="put_insight",
            table_name="insights"
        )
    """
    log = logger_instance or logger

    try:
        return operation(**kwargs)
    except ClientError as e:
        error_code = e.response.get('Error', {}).get('Code', 'Unknown')
        error_message = str(e)

        log.error(
            f"DynamoDB operation failed: {operation_name}",
            extra={
                "operation": operation_name,
                "error_code": error_code,
                "error_message": error_message[:256]
            }
        )
        return None
    except Exception as e:
        log.error(
            f"Unexpected error in DynamoDB operation: {operation_name}",
            extra={
                "operation": operation_name,
                "error": str(e)[:256],
                "error_type": type(e).__name__
            }
        )
        return None
