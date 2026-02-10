"""
Retry utilities with exponential backoff for Lambda functions.

Provides decorators and helper functions for retrying operations with exponential backoff.
"""

import time
import functools
from typing import Callable, TypeVar, Any, Optional, Tuple, Type
from aws_lambda_powertools import Logger

logger = Logger(child=True)

T = TypeVar('T')


def exponential_backoff_retry(
    max_retries: int = 3,
    base_delay: float = 1.0,
    max_delay: float = 60.0,
    exponential_base: float = 2.0,
    exceptions: Tuple[Type[Exception], ...] = (Exception,),
    logger_instance: Optional[Logger] = None
) -> Callable:
    """
    Decorator for retrying a function with exponential backoff.

    Args:
        max_retries: Maximum number of retry attempts (default 3)
        base_delay: Base delay in seconds (default 1.0)
        max_delay: Maximum delay in seconds (default 60.0)
        exponential_base: Base for exponential calculation (default 2.0)
        exceptions: Tuple of exception types to catch and retry
        logger_instance: Optional logger instance for logging retries

    Returns:
        Decorated function with retry logic

    Example:
        @exponential_backoff_retry(max_retries=3, base_delay=1.0)
        def call_external_api():
            # API call that might fail
            pass
    """
    def decorator(func: Callable[..., T]) -> Callable[..., T]:
        @functools.wraps(func)
        def wrapper(*args: Any, **kwargs: Any) -> T:
            log = logger_instance or logger
            last_exception = None

            for attempt in range(1, max_retries + 1):
                try:
                    result = func(*args, **kwargs)
                    if attempt > 1:
                        log.info(
                            f"Function {func.__name__} succeeded on attempt {attempt}",
                            extra={"function": func.__name__, "attempt": attempt}
                        )
                    return result

                except exceptions as e:
                    last_exception = e

                    if attempt < max_retries:
                        # Calculate delay with exponential backoff
                        delay = min(base_delay * (exponential_base ** (attempt - 1)), max_delay)

                        log.warning(
                            f"Function {func.__name__} failed on attempt {attempt}/{max_retries}, "
                            f"retrying in {delay:.2f}s",
                            extra={
                                "function": func.__name__,
                                "attempt": attempt,
                                "max_retries": max_retries,
                                "delay_seconds": delay,
                                "error": str(e),
                                "error_type": type(e).__name__
                            }
                        )

                        time.sleep(delay)
                    else:
                        log.error(
                            f"Function {func.__name__} failed after {max_retries} attempts",
                            extra={
                                "function": func.__name__,
                                "max_retries": max_retries,
                                "error": str(e),
                                "error_type": type(e).__name__
                            }
                        )

            # All retries exhausted, raise the last exception
            raise last_exception

        return wrapper
    return decorator


def retry_with_backoff(
    func: Callable[..., T],
    max_retries: int = 3,
    base_delay: float = 1.0,
    max_delay: float = 60.0,
    exponential_base: float = 2.0,
    exceptions: Tuple[Type[Exception], ...] = (Exception,),
    logger_instance: Optional[Logger] = None,
    *args: Any,
    **kwargs: Any
) -> T:
    """
    Retry a function with exponential backoff (non-decorator version).

    Useful for inline retry logic without decorating the function.

    Args:
        func: Function to retry
        max_retries: Maximum number of retry attempts
        base_delay: Base delay in seconds
        max_delay: Maximum delay in seconds
        exponential_base: Base for exponential calculation
        exceptions: Tuple of exception types to catch and retry
        logger_instance: Optional logger instance
        *args: Positional arguments to pass to func
        **kwargs: Keyword arguments to pass to func

    Returns:
        Result of the function call

    Raises:
        Last exception if all retries fail

    Example:
        result = retry_with_backoff(
            call_external_api,
            max_retries=3,
            base_delay=1.0,
            api_key="key123"
        )
    """
    log = logger_instance or logger
    last_exception = None

    for attempt in range(1, max_retries + 1):
        try:
            result = func(*args, **kwargs)
            if attempt > 1:
                log.info(
                    f"Function {func.__name__} succeeded on attempt {attempt}",
                    extra={"function": func.__name__, "attempt": attempt}
                )
            return result

        except exceptions as e:
            last_exception = e

            if attempt < max_retries:
                delay = min(base_delay * (exponential_base ** (attempt - 1)), max_delay)

                log.warning(
                    f"Function {func.__name__} failed on attempt {attempt}/{max_retries}, "
                    f"retrying in {delay:.2f}s",
                    extra={
                        "function": func.__name__,
                        "attempt": attempt,
                        "max_retries": max_retries,
                        "delay_seconds": delay,
                        "error": str(e),
                        "error_type": type(e).__name__
                    }
                )

                time.sleep(delay)
            else:
                log.error(
                    f"Function {func.__name__} failed after {max_retries} attempts",
                    extra={
                        "function": func.__name__,
                        "max_retries": max_retries,
                        "error": str(e),
                        "error_type": type(e).__name__
                    }
                )

    raise last_exception


class RetryableOperation:
    """
    Context manager for retryable operations with exponential backoff.

    Example:
        with RetryableOperation(max_retries=3, base_delay=1.0) as retry:
            for attempt in retry:
                try:
                    result = call_external_api()
                    retry.success(result)
                    break
                except Exception as e:
                    retry.handle_error(e)
    """

    def __init__(
        self,
        max_retries: int = 3,
        base_delay: float = 1.0,
        max_delay: float = 60.0,
        exponential_base: float = 2.0,
        exceptions: Tuple[Type[Exception], ...] = (Exception,),
        logger_instance: Optional[Logger] = None
    ):
        """
        Initialize retryable operation.

        Args:
            max_retries: Maximum number of retry attempts
            base_delay: Base delay in seconds
            max_delay: Maximum delay in seconds
            exponential_base: Base for exponential calculation
            exceptions: Tuple of exception types to catch and retry
            logger_instance: Optional logger instance
        """
        self.max_retries = max_retries
        self.base_delay = base_delay
        self.max_delay = max_delay
        self.exponential_base = exponential_base
        self.exceptions = exceptions
        self.logger = logger_instance or logger
        self.current_attempt = 0
        self.last_exception = None
        self.result = None
        self.succeeded = False

    def __enter__(self):
        """Enter context manager."""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Exit context manager."""
        if not self.succeeded and self.last_exception:
            # Re-raise the last exception if operation never succeeded
            raise self.last_exception
        return True  # Suppress exception if succeeded

    def __iter__(self):
        """Iterate through retry attempts."""
        for attempt in range(1, self.max_retries + 1):
            self.current_attempt = attempt
            yield attempt

    def handle_error(self, exception: Exception) -> None:
        """
        Handle an error during a retry attempt.

        Args:
            exception: Exception that occurred
        """
        self.last_exception = exception

        if not isinstance(exception, self.exceptions):
            # Not a retryable exception, raise immediately
            raise exception

        if self.current_attempt < self.max_retries:
            delay = min(
                self.base_delay * (self.exponential_base ** (self.current_attempt - 1)),
                self.max_delay
            )

            self.logger.warning(
                f"Operation failed on attempt {self.current_attempt}/{self.max_retries}, "
                f"retrying in {delay:.2f}s",
                extra={
                    "attempt": self.current_attempt,
                    "max_retries": self.max_retries,
                    "delay_seconds": delay,
                    "error": str(exception),
                    "error_type": type(exception).__name__
                }
            )

            time.sleep(delay)
        else:
            self.logger.error(
                f"Operation failed after {self.max_retries} attempts",
                extra={
                    "max_retries": self.max_retries,
                    "error": str(exception),
                    "error_type": type(exception).__name__
                }
            )

    def success(self, result: Any = None) -> None:
        """
        Mark the operation as successful.

        Args:
            result: Optional result value
        """
        self.succeeded = True
        self.result = result

        if self.current_attempt > 1:
            self.logger.info(
                f"Operation succeeded on attempt {self.current_attempt}",
                extra={"attempt": self.current_attempt}
            )


def process_stream_batch_with_isolation(
    records: list,
    process_func: Callable[[Any], None],
    logger_instance: Optional[Logger] = None
) -> list:
    """
    Process a batch of DynamoDB Stream records with error isolation.

    Ensures that failures in individual records don't fail the entire batch.
    Returns a list of failed record identifiers for partial batch failure handling.

    Args:
        records: List of DynamoDB Stream records
        process_func: Function to process each record
        logger_instance: Optional logger instance

    Returns:
        List of batch item failures (dicts with "itemIdentifier" key)

    Example:
        def process_record(record):
            # Process single record
            pass

        batch_item_failures = process_stream_batch_with_isolation(
            event.get("Records", []),
            process_record
        )

        return {"batchItemFailures": batch_item_failures}
    """
    log = logger_instance or logger
    batch_item_failures = []

    for record in records:
        try:
            process_func(record)
        except Exception as e:
            # Log error and add to batch item failures for retry
            sequence_number = record.get("dynamodb", {}).get("SequenceNumber")

            log.error(
                "Failed to process stream record",
                extra={
                    "sequence_number": sequence_number,
                    "error": str(e),
                    "error_type": type(e).__name__
                }
            )

            if sequence_number:
                batch_item_failures.append({"itemIdentifier": sequence_number})

    return batch_item_failures
