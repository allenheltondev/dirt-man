"""
Unit tests for Event Detector Lambda handler.
"""

import pytest
from unittest.mock import Mock, patch, MagicMock
import os

# Set environment variable to disable tracing for tests
os.environ["POWERTOOLS_TRACE_DISABLED"] = "true"

from functions.event_detector import (
    lambda_handler,
    process_stream_record,
    detect_events_for_reading
)


class TestEventDetectorHandler:
    """Tests for Event Detector Lambda handler."""

    def test_lambda_handler_empty_records(self):
        """Test handler with no records."""
        event = {"Records": []}
        context = Mock()

        result = lambda_handler(event, context)

        assert result == {"batchItemFailures": []}

    @patch("functions.event_detector.process_stream_record")
    def test_lambda_handler_processes_records(self, mock_process):
        """Test handler processes all records."""
        event = {
            "Records": [
                {"eventName": "INSERT", "dynamodb": {"SequenceNumber": "1"}},
                {"eventName": "INSERT", "dynamodb": {"SequenceNumber": "2"}}
            ]
        }
        context = Mock()

        result = lambda_handler(event, context)

        assert mock_process.call_count == 2
        assert result == {"batchItemFailures": []}

    @patch("functions.event_detector.process_stream_record")
    def test_lambda_handler_handles_failures(self, mock_process):
        """Test handler handles processing failures."""
        mock_process.side_effect = [None, Exception("Test error")]

        event = {
            "Records": [
                {"eventName": "INSERT", "dynamodb": {"SequenceNumber": "1"}},
                {"eventName": "INSERT", "dynamodb": {"SequenceNumber": "2"}}
            ]
        }
        context = Mock()

        result = lambda_handler(event, context)

        assert len(result["batchItemFailures"]) == 1
        assert result["batchItemFailures"][0]["itemIdentifier"] == "2"

    def test_process_stream_record_skips_remove_events(self):
        """Test that REMOVE events are skipped."""
        record = {
            "eventName": "REMOVE",
            "dynamodb": {}
        }

        # Should not raise an error
        process_stream_record(record)

    @patch("functions.event_detector.extract_reading_from_stream_record")
    @patch("functions.event_detector.is_event_processed")
    @patch("functions.event_detector.detect_events_for_reading")
    def test_process_stream_record_skips_already_processed(
        self, mock_detect, mock_is_processed, mock_extract
    ):
        """Test that already processed readings are skipped."""
        mock_extract.return_value = {
            "hardware_id": "device-001",
            "batch_id": "batch-123",
            "timestamp_ms": 1704067200000
        }
        mock_is_processed.return_value = True

        record = {
            "eventName": "INSERT",
            "dynamodb": {
                "NewImage": {"hardware_id": {"S": "device-001"}}
            }
        }

        process_stream_record(record)

        # Should not call detect_events_for_reading
        mock_detect.assert_not_called()

    @patch("functions.event_detector.extract_reading_from_stream_record")
    @patch("functions.event_detector.is_event_processed")
    @patch("functions.event_detector.detect_events_for_reading")
    def test_process_stream_record_processes_new_reading(
        self, mock_detect, mock_is_processed, mock_extract
    ):
        """Test that new readings are processed."""
        reading = {
            "hardware_id": "device-001",
            "batch_id": "batch-123",
            "timestamp_ms": 1704067200000,
            "soil_moisture": 45.0
        }
        mock_extract.return_value = reading
        mock_is_processed.return_value = False

        record = {
            "eventName": "INSERT",
            "dynamodb": {
                "NewImage": {"hardware_id": {"S": "device-001"}}
            }
        }

        process_stream_record(record)

        # Should call detect_events_for_reading
        mock_detect.assert_called_once()
        call_args = mock_detect.call_args[0]
        assert call_args[0] == reading
        assert call_args[1] == "batch-123#1704067200000"

    @patch("functions.event_detector.mark_event_processed_if_absent")
    def test_detect_events_marks_as_processed(self, mock_mark):
        """Test that readings are marked as processed."""
        reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1704067200000,
            "soil_moisture": 45.0
        }
        reading_id = "batch-123#1704067200000"

        detect_events_for_reading(reading, reading_id)

        mock_mark.assert_called_once_with(reading_id, "device-001")
