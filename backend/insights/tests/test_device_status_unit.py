"""
Unit tests for DeviceStatus classification and error retention.

Tests Requirements 23.14-23.24:
- Health category boundaries
- Error list truncation
- Error message truncation
"""

import pytest
from datetime import datetime

from shared.models import DeviceStatus, HealthCategory, ErrorRecord
from shared.device_status import (
    derive_health_category,
    truncate_error_message,
    append_error_to_list,
    update_device_status_with_error
)


class TestHealthCategoryDerivation:
    """Tests for health category derivation."""

    def test_healthy_within_two_hours(self):
        """Device is healthy when last_seen_ingest_time is within 2 hours."""
        current_ms = int(datetime.utcnow().timestamp() * 1000)
        last_seen_ms = current_ms - (1 * 3600 * 1000)

        category = derive_health_category(last_seen_ms, None, current_ms)

        assert category == HealthCategory.HEALTHY

    def test_healthy_exactly_two_hours(self):
        """Device is healthy at exactly 2 hours boundary."""
        current_ms = int(datetime.utcnow().timestamp() * 1000)
        last_seen_ms = current_ms - (2 * 3600 * 1000)

        category = derive_health_category(last_seen_ms, None, current_ms)

        assert category == HealthCategory.HEALTHY

    def test_stale_between_two_and_six_hours(self):
        """Device is stale when between 2 and 6 hours ago."""
        current_ms = int(datetime.utcnow().timestamp() * 1000)
        last_seen_ms = current_ms - (4 * 3600 * 1000)

        category = derive_health_category(last_seen_ms, None, current_ms)

        assert category == HealthCategory.STALE

    def test_stale_just_after_two_hours(self):
        """Device is stale just after 2 hours boundary."""
        current_ms = int(datetime.utcnow().timestamp() * 1000)
        last_seen_ms = current_ms - (2 * 3600 * 1000 + 1000)

        category = derive_health_category(last_seen_ms, None, current_ms)

        assert category == HealthCategory.STALE

    def test_stale_exactly_six_hours(self):
        """Device is stale at exactly 6 hours boundary."""
        current_ms = int(datetime.utcnow().timestamp() * 1000)
        last_seen_ms = current_ms - (6 * 3600 * 1000)

        category = derive_health_category(last_seen_ms, None, current_ms)

        assert category == HealthCategory.STALE

    def test_missing_after_six_hours(self):
        """Device is missing when more than 6 hours ago."""
        current_ms = int(datetime.utcnow().timestamp() * 1000)
        last_seen_ms = current_ms - (7 * 3600 * 1000)

        category = derive_health_category(last_seen_ms, None, current_ms)

        assert category == HealthCategory.MISSING

    def test_missing_when_no_last_seen(self):
        """Device is missing when last_seen_ingest_time is None."""
        current_ms = int(datetime.utcnow().timestamp() * 1000)

        category = derive_health_category(None, None, current_ms)

        assert category == HealthCategory.MISSING

    def test_failing_takes_precedence(self):
        """Device is failing when error within 24 hours."""
        current_ms = int(datetime.utcnow().timestamp() * 1000)
        last_seen_ms = current_ms - (1 * 3600 * 1000)
        last_error_ms = current_ms - (2 * 3600 * 1000)

        category = derive_health_category(last_seen_ms, last_error_ms, current_ms)

        assert category == HealthCategory.FAILING

    def test_failing_exactly_twentyfour_hours(self):
        """Device is failing at exactly 24 hours error boundary."""
        current_ms = int(datetime.utcnow().timestamp() * 1000)
        last_seen_ms = current_ms - (1 * 3600 * 1000)
        last_error_ms = current_ms - (24 * 3600 * 1000)

        category = derive_health_category(last_seen_ms, last_error_ms, current_ms)

        assert category == HealthCategory.FAILING

    def test_not_failing_after_twentyfour_hours(self):
        """Device is not failing when error more than 24 hours ago."""
        current_ms = int(datetime.utcnow().timestamp() * 1000)
        last_seen_ms = current_ms - (1 * 3600 * 1000)
        last_error_ms = current_ms - (25 * 3600 * 1000)

        category = derive_health_category(last_seen_ms, last_error_ms, current_ms)

        assert category == HealthCategory.HEALTHY


class TestErrorMessageTruncation:
    """Tests for error message truncation."""

    def test_truncate_short_message(self):
        """Short messages are not truncated."""
        message = "Short error"
        result = truncate_error_message(message)
        assert result == message

    def test_truncate_exactly_256_chars(self):
        """Messages exactly 256 chars are not truncated."""
        message = "x" * 256
        result = truncate_error_message(message)
        assert len(result) == 256

    def test_truncate_over_256_chars(self):
        """Messages over 256 chars are truncated."""
        message = "x" * 300
        result = truncate_error_message(message)
        assert len(result) == 256


class TestErrorListManagement:
    """Tests for error list management."""

    def test_append_error_to_empty_list(self):
        """Appending error to empty list works."""
        errors = []
        result = append_error_to_list(errors, 1000, "TEST", "Test message")
        assert len(result) == 1

    def test_append_error_truncates_message(self):
        """Appending error truncates long messages."""
        errors = []
        long_msg = "x" * 300
        result = append_error_to_list(errors, 1000, "LONG", long_msg)
        assert len(result[0].error_message) == 256

    def test_append_maintains_max_ten(self):
        """Error list is truncated to max 10 entries."""
        errors = []
        for i in range(15):
            errors = append_error_to_list(errors, 1000 + i, f"E{i}", f"Msg {i}")
        assert len(errors) == 10
        assert errors[0].error_code == "E5"


class TestDeviceStatusErrorUpdate:
    """Tests for updating DeviceStatus with errors."""

    def test_update_with_first_error(self):
        """First error updates all error fields."""
        status = DeviceStatus(hardware_id="dev1")
        result = update_device_status_with_error(
            status, "ERR1", "First error", 1000
        )
        assert result.last_error_at_ms == 1000
        assert result.last_error_code == "ERR1"
        assert len(result.last_errors) == 1

    def test_update_maintains_max_ten_errors(self):
        """DeviceStatus maintains max 10 errors."""
        status = DeviceStatus(hardware_id="dev1")
        for i in range(15):
            status = update_device_status_with_error(
                status, f"E{i}", f"Error {i}", 1000 + i
            )
        assert len(status.last_errors) == 10
