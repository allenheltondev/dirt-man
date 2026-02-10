"""
Unit tests for rollup helper functions.

Tests bucket alignment, key generation, and TTL calculation.
"""

import pytest
from datetime import datetime, timezone, timedelta
from shared.rollup_helpers import (
    align_to_minute,
    align_to_hour_bucket,
    generate_bucket_key,
    generate_metric_key,
    calculate_ttl,
    get_minute_bucket,
    get_hour_bucket,
    MINUTE_BUCKET_TTL_DAYS,
    HOUR_BUCKET_TTL_DAYS
)


class TestBucketAlignment:
    """Test bucket alignment functions."""

    def test_align_to_minute(self):
        """Test minute alignment."""
        # 2024-01-15 10:23:45.123 UTC
        timestamp_ms = 1705318425123
        aligned = align_to_minute(timestamp_ms)

        # Should align to 2024-01-15 10:23:00.000 UTC
        expected = 1705318380000
        assert aligned == expected

    def test_align_to_hour_bucket(self):
        """Test hour alignment."""
        # 2024-01-15 10:23:45.123 UTC
        timestamp_ms = 1705318425123
        aligned = align_to_hour_bucket(timestamp_ms)

        # Should align to 2024-01-15 10:00:00.000 UTC
        expected = 1705316400000
        assert aligned == expected

    def test_get_minute_bucket(self):
        """Test get_minute_bucket returns aligned timestamp."""
        timestamp_ms = 1705318425123
        bucket = get_minute_bucket(timestamp_ms)
        assert bucket == align_to_minute(timestamp_ms)

    def test_get_hour_bucket(self):
        """Test get_hour_bucket returns aligned timestamp."""
        timestamp_ms = 1705318425123
        bucket = get_hour_bucket(timestamp_ms)
        assert bucket == align_to_hour_bucket(timestamp_ms)


class TestKeyGeneration:
    """Test key generation functions."""

    def test_generate_bucket_key(self):
        """Test bucket key generation."""
        bucket_type = "minute"
        bucket_start_ms = 1705318380000

        key = generate_bucket_key(bucket_type, bucket_start_ms)
        assert key == "minute#1705318380000"

    def test_generate_bucket_key_hour(self):
        """Test bucket key generation for hour."""
        bucket_type = "hour"
        bucket_start_ms = 1705316400000

        key = generate_bucket_key(bucket_type, bucket_start_ms)
        assert key == "hour#1705316400000"

    def test_generate_metric_key_no_dimensions(self):
        """Test metric key generation without dimensions."""
        metric_name = "readings_ingested_count"

        key = generate_metric_key(metric_name)
        assert key == "readings_ingested_count#"

    def test_generate_metric_key_with_dimensions(self):
        """Test metric key generation with dimensions."""
        metric_name = "events_detected_count"
        dimensions = {"event_type": "Watering_Event"}

        key = generate_metric_key(metric_name, dimensions)
        assert key == "events_detected_count#event_type=Watering_Event"

    def test_generate_metric_key_sorted_dimensions(self):
        """Test metric key generation sorts dimensions alphabetically."""
        metric_name = "test_metric"
        dimensions = {"z_key": "z_value", "a_key": "a_value", "m_key": "m_value"}

        key = generate_metric_key(metric_name, dimensions)
        # Dimensions should be sorted: a, m, z
        assert key == "test_metric#a_key=a_value,m_key=m_value,z_key=z_value"

    def test_generate_metric_key_dimension_ordering_consistency(self):
        """Test that dimension ordering is consistent regardless of input order."""
        metric_name = "test"
        dims1 = {"b": "2", "a": "1"}
        dims2 = {"a": "1", "b": "2"}

        key1 = generate_metric_key(metric_name, dims1)
        key2 = generate_metric_key(metric_name, dims2)

        assert key1 == key2
        assert key1 == "test#a=1,b=2"


class TestTTLCalculation:
    """Test TTL calculation."""

    def test_calculate_ttl_minute_bucket(self):
        """Test TTL calculation for minute bucket."""
        bucket_start_ms = 1705318380000  # 2024-01-15 10:23:00 UTC
        ttl = calculate_ttl("minute", bucket_start_ms)

        # TTL should be 7 days after bucket start
        bucket_start_dt = datetime.fromtimestamp(bucket_start_ms / 1000, tz=timezone.utc)
        expected_ttl_dt = bucket_start_dt + timedelta(days=MINUTE_BUCKET_TTL_DAYS)
        expected_ttl = int(expected_ttl_dt.timestamp())

        assert ttl == expected_ttl

    def test_calculate_ttl_hour_bucket(self):
        """Test TTL calculation for hour bucket."""
        bucket_start_ms = 1705316400000  # 2024-01-15 10:00:00 UTC
        ttl = calculate_ttl("hour", bucket_start_ms)

        # TTL should be 90 days after bucket start
        bucket_start_dt = datetime.fromtimestamp(bucket_start_ms / 1000, tz=timezone.utc)
        expected_ttl_dt = bucket_start_dt + timedelta(days=HOUR_BUCKET_TTL_DAYS)
        expected_ttl = int(expected_ttl_dt.timestamp())

        assert ttl == expected_ttl

    def test_calculate_ttl_unknown_bucket_type(self):
        """Test TTL calculation for unknown bucket type defaults to 7 days."""
        bucket_start_ms = 1705318380000
        ttl = calculate_ttl("unknown", bucket_start_ms)

        # Should default to 7 days
        bucket_start_dt = datetime.fromtimestamp(bucket_start_ms / 1000, tz=timezone.utc)
        expected_ttl_dt = bucket_start_dt + timedelta(days=7)
        expected_ttl = int(expected_ttl_dt.timestamp())

        assert ttl == expected_ttl

    def test_ttl_returns_seconds_not_milliseconds(self):
        """Test that TTL is returned in seconds (Unix epoch), not milliseconds."""
        bucket_start_ms = 1705318380000
        ttl = calculate_ttl("minute", bucket_start_ms)

        # TTL should be in seconds (10 digits for current timestamps)
        # Milliseconds would be 13 digits
        assert len(str(ttl)) <= 11  # Allow for future timestamps
        assert len(str(ttl)) >= 10  # Current timestamps are 10 digits
