"""
Unit tests for profile learning logic.

Tests:
- Watering interval calculation
- Baseline moisture range calculation
- Stress condition detection
"""

import pytest
from backend.insights.shared.profile_learning import (
    calculate_watering_interval,
    update_profile_with_watering_event,
    calculate_baseline_moisture_range,
    check_stress_condition
)
from backend.insights.shared.models import DeviceProfile


class TestWateringIntervalCalculation:
    """Tests for watering interval calculation."""

    def test_insufficient_data_returns_none(self):
        """Test that insufficient data returns None."""
        # No events
        assert calculate_watering_interval([]) is None

        # Single event
        assert calculate_watering_interval([1000000]) is None

    def test_two_events_calculates_interval(self):
        """Test interval calculation with two events."""
        # Events 1 hour apart (3600 seconds = 3,600,000 ms)
        events = [1000000, 1000000 + 3600000]
        interval = calculate_watering_interval(events)
        assert interval == 3600

    def test_multiple_events_calculates_average(self):
        """Test average interval with multiple events."""
        # Events at 0h, 24h, 48h, 72h (24 hour intervals)
        events = [
            1000000,
            1000000 + (24 * 3600 * 1000),
            1000000 + (48 * 3600 * 1000),
            1000000 + (72 * 3600 * 1000)
        ]
        interval = calculate_watering_interval(events)
        # Average of three 24-hour intervals
        assert interval == 24 * 3600

    def test_unordered_events_are_sorted(self):
        """Test that unordered events are sorted before calculation."""
        # Events out of order
        events = [
            1000000 + (48 * 3600 * 1000),
            1000000,
            1000000 + (24 * 3600 * 1000)
        ]
        interval = calculate_watering_interval(events)
        # Should still calculate 24-hour average
        assert interval == 24 * 3600

    def test_varying_intervals_calculates_average(self):
        """Test average with varying intervals."""
        # Events at 0h, 12h, 36h (12h and 24h intervals)
        events = [
            1000000,
            1000000 + (12 * 3600 * 1000),
            1000000 + (36 * 3600 * 1000)
        ]
        interval = calculate_watering_interval(events)
        # Average of 12h and 24h = 18h
        assert interval == 18 * 3600


class TestProfileUpdateWithWateringEvent:
    """Tests for profile update with watering events."""

    def test_adds_event_to_empty_profile(self):
        """Test adding first event to profile."""
        profile = DeviceProfile(hardware_id="test-device")
        event_time = 1000000

        updated = update_profile_with_watering_event(profile, event_time)

        assert len(updated.last_watering_events) == 1
        assert updated.last_watering_events[0] == event_time
        assert updated.typical_watering_interval_sec is None  # Not enough data

    def test_calculates_interval_with_two_events(self):
        """Test interval calculation after second event."""
        profile = DeviceProfile(
            hardware_id="test-device",
            last_watering_events=[1000000]
        )
        event_time = 1000000 + (24 * 3600 * 1000)  # 24 hours later

        updated = update_profile_with_watering_event(profile, event_time)

        assert len(updated.last_watering_events) == 2
        assert updated.typical_watering_interval_sec == 24 * 3600

    def test_maintains_rolling_window(self):
        """Test that old events are dropped when max is exceeded."""
        # Create profile with max_events_tracked events
        initial_events = [1000000 + (i * 3600 * 1000) for i in range(20)]
        profile = DeviceProfile(
            hardware_id="test-device",
            last_watering_events=initial_events
        )

        # Add one more event
        new_event = 1000000 + (21 * 3600 * 1000)
        updated = update_profile_with_watering_event(profile, new_event, max_events_tracked=20)

        # Should still have 20 events
        assert len(updated.last_watering_events) == 20
        # Oldest event should be dropped
        assert initial_events[0] not in updated.last_watering_events
        # Newest event should be present
        assert new_event in updated.last_watering_events

    def test_recalculates_interval_with_each_event(self):
        """Test that interval is recalculated as events are added."""
        profile = DeviceProfile(
            hardware_id="test-device",
            last_watering_events=[1000000, 1000000 + (24 * 3600 * 1000)]
        )
        profile.typical_watering_interval_sec = 24 * 3600

        # Add event 48 hours after first (24h after second)
        new_event = 1000000 + (48 * 3600 * 1000)
        updated = update_profile_with_watering_event(profile, new_event)

        # Average should still be 24 hours
        assert updated.typical_watering_interval_sec == 24 * 3600


class TestBaselineMoistureRangeCalculation:
    """Tests for baseline moisture range calculation."""

    def test_insufficient_data_returns_none(self):
        """Test that insufficient data returns None."""
        # Empty list
        assert calculate_baseline_moisture_range([]) is None

        # Too few data points
        aggregates = [
            {"soil_moisture_stats": {"avg": 50.0, "valid_count": 1}}
            for _ in range(5)
        ]
        assert calculate_baseline_moisture_range(aggregates) is None

    def test_calculates_range_from_sufficient_data(self):
        """Test range calculation with sufficient data."""
        # Create 20 aggregates with moisture values from 40-60%
        aggregates = [
            {"soil_moisture_stats": {"avg": 40.0 + i, "valid_count": 1}}
            for i in range(20)
        ]

        result = calculate_baseline_moisture_range(aggregates)

        assert result is not None
        assert "min" in result
        assert "max" in result
        # 10th percentile of 40-59 should be around 41-42
        assert 40 <= result["min"] <= 43
        # 90th percentile should be around 57-58
        assert 56 <= result["max"] <= 60

    def test_excludes_invalid_readings(self):
        """Test that invalid readings are excluded."""
        aggregates = [
            {"soil_moisture_stats": {"avg": 50.0, "valid_count": 1}}
            for _ in range(10)
        ]
        # Add some invalid readings
        aggregates.extend([
            {"soil_moisture_stats": {"avg": None, "valid_count": 0}},
            {"soil_moisture_stats": {"valid_count": 0}},
            {}
        ])

        result = calculate_baseline_moisture_range(aggregates)

        # Should still calculate from the 10 valid readings
        assert result is not None

    def test_handles_missing_soil_stats(self):
        """Test handling of aggregates without soil moisture stats."""
        aggregates = [{"temperature_stats": {"avg": 20.0}}]  # No soil stats
        aggregates.extend([
            {"soil_moisture_stats": {"avg": 50.0, "valid_count": 1}}
            for _ in range(10)
        ])

        result = calculate_baseline_moisture_range(aggregates)

        # Should calculate from valid entries only
        assert result is not None

    def test_percentile_boundaries(self):
        """Test that percentile calculation respects boundaries."""
        # Create exactly 10 values: 10, 20, 30, ..., 100
        aggregates = [
            {"soil_moisture_stats": {"avg": float(10 * (i + 1)), "valid_count": 1}}
            for i in range(10)
        ]

        result = calculate_baseline_moisture_range(aggregates)

        assert result is not None
        # 10th percentile (index 1) = 20
        assert result["min"] == 20.0
        # 90th percentile (index 9) = 100
        assert result["max"] == 100.0


class TestStressConditionDetection:
    """Tests for stress condition detection."""

    def test_no_stress_when_moisture_above_threshold(self):
        """Test no stress when moisture is above 30%."""
        profile = DeviceProfile(hardware_id="test-device")

        # Moisture at 35% - above threshold
        is_stress = check_stress_condition(
            profile=profile,
            current_moisture_pct=35.0,
            last_watering_event_ms=1000000,
            current_time_ms=1000000 + (50 * 3600 * 1000)  # 50 hours later
        )

        assert not is_stress

    def test_stress_when_moisture_low_and_no_watering_history(self):
        """Test stress when moisture is low and no watering history."""
        profile = DeviceProfile(hardware_id="test-device")

        # Moisture at 25%, no watering history
        is_stress = check_stress_condition(
            profile=profile,
            current_moisture_pct=25.0,
            last_watering_event_ms=None,
            current_time_ms=1000000
        )

        assert is_stress

    def test_stress_when_moisture_low_and_48_hours_since_watering(self):
        """Test stress when moisture is low and 48+ hours since watering."""
        profile = DeviceProfile(hardware_id="test-device")

        # Moisture at 25%, 48 hours since watering
        is_stress = check_stress_condition(
            profile=profile,
            current_moisture_pct=25.0,
            last_watering_event_ms=1000000,
            current_time_ms=1000000 + (48 * 3600 * 1000)
        )

        assert is_stress

    def test_no_stress_when_recent_watering(self):
        """Test no stress when watering was recent."""
        profile = DeviceProfile(hardware_id="test-device")

        # Moisture at 25%, but watered 24 hours ago
        is_stress = check_stress_condition(
            profile=profile,
            current_moisture_pct=25.0,
            last_watering_event_ms=1000000,
            current_time_ms=1000000 + (24 * 3600 * 1000)
        )

        assert not is_stress

    def test_boundary_at_30_percent_moisture(self):
        """Test boundary condition at exactly 30% moisture."""
        profile = DeviceProfile(hardware_id="test-device")

        # Exactly 30% moisture
        is_stress = check_stress_condition(
            profile=profile,
            current_moisture_pct=30.0,
            last_watering_event_ms=1000000,
            current_time_ms=1000000 + (50 * 3600 * 1000)
        )

        # At threshold, not below - no stress
        assert not is_stress

    def test_boundary_at_48_hours(self):
        """Test boundary condition at exactly 48 hours."""
        profile = DeviceProfile(hardware_id="test-device")

        # Exactly 48 hours since watering
        is_stress = check_stress_condition(
            profile=profile,
            current_moisture_pct=25.0,
            last_watering_event_ms=1000000,
            current_time_ms=1000000 + (48 * 3600 * 1000)
        )

        # At 48 hours - should be stress
        assert is_stress

    def test_stress_when_moisture_at_29_percent(self):
        """Test stress detection just below moisture threshold."""
        profile = DeviceProfile(hardware_id="test-device")

        # 29% moisture (just below threshold)
        is_stress = check_stress_condition(
            profile=profile,
            current_moisture_pct=29.0,
            last_watering_event_ms=1000000,
            current_time_ms=1000000 + (50 * 3600 * 1000)
        )

        assert is_stress
