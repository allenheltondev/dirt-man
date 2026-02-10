"""
Unit tests for retry utilities.

Tests the exponential backoff retry logic and stream batch processing.
"""

import pytest
import time
from unittest.mock import Mock, patch
from aws_lambda_powertools import Logger

# Add shared directory to path
import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'shared'))

from retry_utils import (
    exponential_backoff_retry,
    retry_with_backoff,
    RetryableOperation,
    process_stream_batch_with_isolation
)


class TestExponentialBackoffRetry:
    """Test exponential backoff retry decorator."""

    def test_successful_first_attempt(self):
        """Test that successful operations don't retry."""
        call_count = 0

        @exponential_backoff_retry(max_retries=3)
        def successful_operation():
            nonlocal call_count
            call_count += 1
            return "success"

        result = successful_operation()

        assert result == "success"
        assert call_count == 1

    def test_retry_on_failure(self):
        """Test that failed operations are retried."""
        call_count = 0

        @exponential_backoff_retry(max_retries=3, base_delay=0.1)
        def failing_operation():
            nonlocal call_count
            call_count += 1
            if call_count < 3:
                raise ValueError("Temporary failure")
            return "success"

        result = failing_operation()

        assert result == "success"
        assert call_count == 3

    def test_max_retries_exhausted(self):
        """Test that exception is raised after max retries."""
        call_count = 0

        @exponential_backoff_retry(max_retries=3, base_delay=0.1)
        def always_failing_operation():
            nonlocal call_count
            call_count += 1
            raise ValueError("Permanent failure")

        with pytest.raises(ValueError, match="Permanent failure"):
            always_failing_operation()

        assert call_count == 3

    def test_exponential_backoff_timing(self):
        """Test that backoff delays increase exponentially."""
        call_times = []

        @exponential_backoff_retry(max_retries=3, base_delay=0.1, exponential_base=2.0)
        def timed_operation():
            call_times.append(time.time())
            if len(call_times) < 3:
                raise ValueError("Retry")
            return "success"

        result = timed_operation()

        assert result == "success"
        assert len(call_times) == 3

        # Check that delays are approximately exponential
        # First retry: ~0.1s delay
        # Second retry: ~0.2s delay
        if len(call_times) >= 2:
            delay1 = call_times[1] - call_times[0]
            assert 0.08 < delay1 < 0.15  # Allow some tolerance

        if len(call_times) >= 3:
            delay2 = call_times[2] - call_times[1]
            assert 0.15 < delay2 < 0.30  # Allow some tolerance

    def test_specific_exception_types(self):
        """Test that only specified exceptions are retried."""
        call_count = 0

        @exponential_backoff_retry(max_retries=3, base_delay=0.1, exceptions=(ValueError,))
        def selective_retry():
            nonlocal call_count
            call_count += 1
            if call_count == 1:
                raise ValueError("Retryable")
            elif call_count == 2:
                raise TypeError("Not retryable")
            return "success"

        with pytest.raises(TypeError, match="Not retryable"):
            selective_retry()

        assert call_count == 2  # ValueError was retried, TypeError was not


class TestRetryWithBackoff:
    """Test retry_with_backoff function."""

    def test_successful_operation(self):
        """Test successful operation without retries."""
        def successful_func(value):
            return value * 2

        result = retry_with_backoff(
            successful_func,
            max_retries=3,
            base_delay=0.1,
            value=5
        )

        assert result == 10

    def test_retry_on_failure(self):
        """Test retry on failure."""
        call_count = [0]

        def failing_func():
            call_count[0] += 1
            if call_count[0] < 2:
                raise ValueError("Retry")
            return "success"

        result = retry_with_backoff(
            failing_func,
            max_retries=3,
            base_delay=0.1
        )

        assert result == "success"
        assert call_count[0] == 2


class TestRetryableOperation:
    """Test RetryableOperation context manager."""

    def test_successful_operation(self):
        """Test successful operation in context manager."""
        with RetryableOperation(max_retries=3, base_delay=0.1) as retry:
            for attempt in retry:
                try:
                    result = "success"
                    retry.success(result)
                    break
                except Exception as e:
                    retry.handle_error(e)

        assert retry.succeeded
        assert retry.result == "success"

    def test_retry_on_failure(self):
        """Test retry on failure in context manager."""
        call_count = [0]

        with RetryableOperation(max_retries=3, base_delay=0.1) as retry:
            for attempt in retry:
                try:
                    call_count[0] += 1
                    if call_count[0] < 2:
                        raise ValueError("Retry")
                    retry.success("success")
                    break
                except Exception as e:
                    retry.handle_error(e)

        assert retry.succeeded
        assert call_count[0] == 2

    def test_max_retries_exhausted(self):
        """Test that exception is raised after max retries."""
        with pytest.raises(ValueError, match="Always fails"):
            with RetryableOperation(max_retries=3, base_delay=0.1) as retry:
                for attempt in retry:
                    try:
                        raise ValueError("Always fails")
                    except Exception as e:
                        retry.handle_error(e)


class TestProcessStreamBatchWithIsolation:
    """Test stream batch processing with error isolation."""

    def test_all_records_succeed(self):
        """Test processing when all records succeed."""
        processed_records = []

        def process_func(record):
            processed_records.append(record['id'])

        records = [
            {'id': 1, 'dynamodb': {'SequenceNumber': 'seq1'}},
            {'id': 2, 'dynamodb': {'SequenceNumber': 'seq2'}},
            {'id': 3, 'dynamodb': {'SequenceNumber': 'seq3'}}
        ]

        failures = process_stream_batch_with_isolation(records, process_func)

        assert len(failures) == 0
        assert processed_records == [1, 2, 3]

    def test_partial_failures(self):
        """Test processing when some records fail."""
        processed_records = []

        def process_func(record):
            if record['id'] == 2:
                raise ValueError("Processing failed")
            processed_records.append(record['id'])

        records = [
            {'id': 1, 'dynamodb': {'SequenceNumber': 'seq1'}},
            {'id': 2, 'dynamodb': {'SequenceNumber': 'seq2'}},
            {'id': 3, 'dynamodb': {'SequenceNumber': 'seq3'}}
        ]

        failures = process_stream_batch_with_isolation(records, process_func)

        assert len(failures) == 1
        assert failures[0]['itemIdentifier'] == 'seq2'
        assert processed_records == [1, 3]

    def test_all_records_fail(self):
        """Test processing when all records fail."""
        def process_func(record):
            raise ValueError("Always fails")

        records = [
            {'id': 1, 'dynamodb': {'SequenceNumber': 'seq1'}},
            {'id': 2, 'dynamodb': {'SequenceNumber': 'seq2'}}
        ]

        failures = process_stream_batch_with_isolation(records, process_func)

        assert len(failures) == 2
        assert failures[0]['itemIdentifier'] == 'seq1'
        assert failures[1]['itemIdentifier'] == 'seq2'

    def test_missing_sequence_number(self):
        """Test handling of records without sequence numbers."""
        def process_func(record):
            raise ValueError("Fails")

        records = [
            {'id': 1, 'dynamodb': {}},  # Missing SequenceNumber
            {'id': 2, 'dynamodb': {'SequenceNumber': 'seq2'}}
        ]

        failures = process_stream_batch_with_isolation(records, process_func)

        # Only record with sequence number should be in failures
        assert len(failures) == 1
        assert failures[0]['itemIdentifier'] == 'seq2'


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
