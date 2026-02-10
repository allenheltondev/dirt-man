"""
Unit tests for event detection logic.
"""

import pytest
from unittest.mock import patch, MagicMock
import os

os.environ["POWERTOOLS_TRACE_DISABLED"] = "true"

from shared.event_detection import (
    detect_watering_event,
    detect_drying_cycle,
    detect_temperature_stress,
    detect_humidity_anomaly,
    detect_environmental_change,
    get_cooldown_period,
    check_cooldown
)
from shared.models import EventType


class TestWateringEventDetection:
    """Tests for watering event detection."""

    def test_rapid_spike_detection_above_threshold(self):
        """Test rapid spike detection when increase exceeds 15 percent."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "soil_moisture": 30.0,
                "soil_moisture_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (30 * 60 * 1000),
            "soil_moisture": 46.5,
            "soil_moisture_status": "ok"
        }

        event = detect_watering_event(readings, current_reading)

        assert event is not None
        assert event.event_type == EventType.WATERING_EVENT
        assert event.detection_metadata["detection_mode"] == "rapid_spike"

    def test_rapid_spike_boundary_exactly_15_percent(self):
        """Test boundary at exactly 15% increase should not trigger."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "soil_moisture": 30.0,
                "soil_moisture_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (30 * 60 * 1000),
            "soil_moisture": 45.0,  # exactly 15% increase
            "soil_moisture_status": "ok"
        }

        event = detect_watering_event(readings, current_reading)
        assert event is None

    def test_rapid_spike_just_above_threshold(self):
        """Test rapid spike just above 15% threshold triggers detection."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "soil_moisture": 30.0,
                "soil_moisture_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (30 * 60 * 1000),
            "soil_moisture": 45.1,  # 15.1% increase
            "soil_moisture_status": "ok"
        }

        event = detect_watering_event(readings, current_reading)
        assert event is not None
        assert event.detection_metadata["detection_mode"] == "rapid_spike"

    def test_gradual_rise_boundary_exactly_10_percent(self):
        """Test gradual rise at exactly 10% increase should trigger (>= 10% per requirements)."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "soil_moisture": 30.0,
                "soil_moisture_status": "ok"
            },
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000 + (30 * 60 * 1000),
                "soil_moisture": 35.0,
                "soil_moisture_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (60 * 60 * 1000),
            "soil_moisture": 40.0,  # exactly 10% total increase
            "soil_moisture_status": "ok"
        }

        event = detect_watering_event(readings, current_reading)
        # Requirement 4.2 says "≥10%" so exactly 10% should trigger
        assert event is not None
        assert event.detection_metadata["detection_mode"] == "gradual_rise"

    def test_gradual_rise_just_above_threshold(self):
        """Test gradual rise just above 10% threshold with positive slopes."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "soil_moisture": 30.0,
                "soil_moisture_status": "ok"
            },
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000 + (30 * 60 * 1000),
                "soil_moisture": 35.0,
                "soil_moisture_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (60 * 60 * 1000),
            "soil_moisture": 40.1,  # 10.1% total increase
            "soil_moisture_status": "ok"
        }

        event = detect_watering_event(readings, current_reading)
        assert event is not None
        assert event.detection_metadata["detection_mode"] == "gradual_rise"

    def test_requires_at_least_2_valid_samples(self):
        """Test that watering detection requires at least 2 valid samples."""
        readings = []
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000,
            "soil_moisture": 50.0,
            "soil_moisture_status": "ok"
        }

        event = detect_watering_event(readings, current_reading)
        assert event is None

    def test_excludes_non_ok_sensor_readings(self):
        """Test that non-ok sensor readings are excluded from detection."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "soil_moisture": 30.0,
                "soil_moisture_status": "noisy"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (30 * 60 * 1000),
            "soil_moisture": 50.0,
            "soil_moisture_status": "ok"
        }

        event = detect_watering_event(readings, current_reading)
        assert event is None

    def test_filters_out_stale_sensor_readings(self):
        """Test that stale sensor readings are excluded."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "soil_moisture": 30.0,
                "soil_moisture_status": "stale"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (30 * 60 * 1000),
            "soil_moisture": 50.0,
            "soil_moisture_status": "ok"
        }

        event = detect_watering_event(readings, current_reading)
        assert event is None

    def test_filters_out_out_of_range_sensor_readings(self):
        """Test that out_of_range sensor readings are excluded."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "soil_moisture": 30.0,
                "soil_moisture_status": "out_of_range"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (30 * 60 * 1000),
            "soil_moisture": 50.0,
            "soil_moisture_status": "ok"
        }

        event = detect_watering_event(readings, current_reading)
        assert event is None

    def test_current_reading_non_ok_returns_none(self):
        """Test that non-ok current reading returns None."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "soil_moisture": 30.0,
                "soil_moisture_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (30 * 60 * 1000),
            "soil_moisture": 50.0,
            "soil_moisture_status": "noisy"
        }

        event = detect_watering_event(readings, current_reading)
        assert event is None



class TestDryingCycleDetection:
    """Tests for drying cycle detection."""

    def test_drying_cycle_boundary_exactly_10_percent(self):
        """Test boundary at exactly 10% drop should not trigger."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "soil_moisture": 50.0,
                "soil_moisture_status": "ok"
            },
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000 + (3 * 60 * 60 * 1000),
                "soil_moisture": 45.0,
                "soil_moisture_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (6 * 60 * 60 * 1000),
            "soil_moisture": 40.0,  # exactly 10% drop
            "soil_moisture_status": "ok"
        }

        event = detect_drying_cycle(readings, current_reading)
        assert event is None

    def test_drying_cycle_just_above_threshold(self):
        """Test drying cycle just above 10% threshold triggers detection."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "soil_moisture": 50.0,
                "soil_moisture_status": "ok"
            },
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000 + (3 * 60 * 60 * 1000),
                "soil_moisture": 45.0,
                "soil_moisture_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (6 * 60 * 60 * 1000),
            "soil_moisture": 39.9,  # 10.1% drop
            "soil_moisture_status": "ok"
        }

        event = detect_drying_cycle(readings, current_reading)
        assert event is not None
        assert event.event_type == EventType.DRYING_CYCLE

    def test_drying_cycle_filters_non_ok_readings(self):
        """Test that non-ok readings are filtered from drying cycle detection."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "soil_moisture": 50.0,
                "soil_moisture_status": "noisy"
            },
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000 + (2 * 60 * 60 * 1000),
                "soil_moisture": 45.0,
                "soil_moisture_status": "ok"
            },
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000 + (4 * 60 * 60 * 1000),
                "soil_moisture": 40.0,
                "soil_moisture_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (6 * 60 * 60 * 1000),
            "soil_moisture": 30.0,
            "soil_moisture_status": "ok"
        }

        event = detect_drying_cycle(readings, current_reading)
        # Should still detect based on valid readings (3 ok readings total)
        assert event is not None

    def test_drying_cycle_requires_minimum_samples(self):
        """Test that drying cycle requires at least 3 samples."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "soil_moisture": 50.0,
                "soil_moisture_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (6 * 60 * 60 * 1000),
            "soil_moisture": 30.0,
            "soil_moisture_status": "ok"
        }

        event = detect_drying_cycle(readings, current_reading)
        assert event is None


class TestTemperatureStressDetection:
    """Tests for temperature stress detection."""

    def test_high_temperature_stress(self):
        """Test detection of high temperature stress."""
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000,
            "temperature": 36.5,
            "temperature_status": "ok"
        }

        event = detect_temperature_stress(current_reading)

        assert event is not None
        assert event.event_type == EventType.TEMPERATURE_STRESS
        assert event.sensor_values["stress_type"] == "high"

    def test_temperature_boundary_35C(self):
        """Test boundary at exactly 35C should not trigger."""
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000,
            "temperature": 35.0,
            "temperature_status": "ok"
        }

        event = detect_temperature_stress(current_reading)
        assert event is None

    def test_low_temperature_stress(self):
        """Test detection of low temperature stress."""
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000,
            "temperature": 4.5,
            "temperature_status": "ok"
        }

        event = detect_temperature_stress(current_reading)

        assert event is not None
        assert event.event_type == EventType.TEMPERATURE_STRESS
        assert event.sensor_values["stress_type"] == "low"

    def test_temperature_boundary_5C(self):
        """Test boundary at exactly 5C should not trigger."""
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000,
            "temperature": 5.0,
            "temperature_status": "ok"
        }

        event = detect_temperature_stress(current_reading)
        assert event is None

    def test_temperature_just_below_5C(self):
        """Test temperature just below 5C triggers low stress."""
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000,
            "temperature": 4.9,
            "temperature_status": "ok"
        }

        event = detect_temperature_stress(current_reading)
        assert event is not None
        assert event.sensor_values["stress_type"] == "low"

    def test_temperature_just_above_35C(self):
        """Test temperature just above 35C triggers high stress."""
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000,
            "temperature": 35.1,
            "temperature_status": "ok"
        }

        event = detect_temperature_stress(current_reading)
        assert event is not None
        assert event.sensor_values["stress_type"] == "high"

    def test_temperature_stress_filters_non_ok_status(self):
        """Test that non-ok temperature status is filtered."""
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000,
            "temperature": 40.0,
            "temperature_status": "noisy"
        }

        event = detect_temperature_stress(current_reading)
        assert event is None

    def test_temperature_stress_missing_temperature(self):
        """Test that missing temperature returns None."""
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000,
            "temperature": None,
            "temperature_status": "ok"
        }

        event = detect_temperature_stress(current_reading)
        assert event is None


class TestHumidityAnomalyDetection:
    """Tests for humidity anomaly detection."""

    def test_humidity_anomaly_boundary_exactly_20_percent(self):
        """Test boundary at exactly 20% change should not trigger."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "humidity": 50.0,
                "humidity_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (60 * 60 * 1000),
            "humidity": 70.0,  # exactly 20% change
            "humidity_status": "ok"
        }

        event = detect_humidity_anomaly(readings, current_reading)
        assert event is None

    def test_humidity_anomaly_just_above_threshold(self):
        """Test humidity anomaly just above 20% threshold."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "humidity": 50.0,
                "humidity_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (60 * 60 * 1000),
            "humidity": 70.1,  # 20.1% change
            "humidity_status": "ok"
        }

        event = detect_humidity_anomaly(readings, current_reading)
        assert event is not None
        assert event.event_type == EventType.HUMIDITY_ANOMALY

    def test_humidity_anomaly_filters_non_ok_readings(self):
        """Test that non-ok humidity readings are filtered."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "humidity": 50.0,
                "humidity_status": "stale"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (60 * 60 * 1000),
            "humidity": 80.0,
            "humidity_status": "ok"
        }

        event = detect_humidity_anomaly(readings, current_reading)
        assert event is None

    def test_humidity_anomaly_current_reading_non_ok(self):
        """Test that non-ok current reading returns None."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "humidity": 50.0,
                "humidity_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (60 * 60 * 1000),
            "humidity": 80.0,
            "humidity_status": "noisy"
        }

        event = detect_humidity_anomaly(readings, current_reading)
        assert event is None


class TestEnvironmentalChangeDetection:
    """Tests for environmental change detection."""

    def test_environmental_change_all_thresholds_met(self):
        """Test environmental change when all thresholds are met."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "temperature": 20.0,
                "humidity": 50.0,
                "pressure": 1000.0,
                "temperature_status": "ok",
                "humidity_status": "ok",
                "pressure_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (2 * 60 * 60 * 1000),
            "temperature": 31.0,  # 11°C change
            "humidity": 66.0,    # 16% change
            "pressure": 1011.0,  # 11 hPa change
            "temperature_status": "ok",
            "humidity_status": "ok",
            "pressure_status": "ok"
        }

        event = detect_environmental_change(readings, current_reading)
        assert event is not None
        assert event.event_type == EventType.ENVIRONMENTAL_CHANGE

    def test_environmental_change_temperature_boundary(self):
        """Test environmental change at temperature boundary (exactly 10°C)."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "temperature": 20.0,
                "humidity": 50.0,
                "pressure": 1000.0,
                "temperature_status": "ok",
                "humidity_status": "ok",
                "pressure_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (2 * 60 * 60 * 1000),
            "temperature": 30.0,  # exactly 10°C change
            "humidity": 66.0,    # 16% change
            "pressure": 1011.0,  # 11 hPa change
            "temperature_status": "ok",
            "humidity_status": "ok",
            "pressure_status": "ok"
        }

        event = detect_environmental_change(readings, current_reading)
        assert event is None

    def test_environmental_change_humidity_boundary(self):
        """Test environmental change at humidity boundary (exactly 15%)."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "temperature": 20.0,
                "humidity": 50.0,
                "pressure": 1000.0,
                "temperature_status": "ok",
                "humidity_status": "ok",
                "pressure_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (2 * 60 * 60 * 1000),
            "temperature": 31.0,  # 11°C change
            "humidity": 65.0,    # exactly 15% change
            "pressure": 1011.0,  # 11 hPa change
            "temperature_status": "ok",
            "humidity_status": "ok",
            "pressure_status": "ok"
        }

        event = detect_environmental_change(readings, current_reading)
        assert event is None

    def test_environmental_change_pressure_boundary(self):
        """Test environmental change at pressure boundary (exactly 10 hPa)."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "temperature": 20.0,
                "humidity": 50.0,
                "pressure": 1000.0,
                "temperature_status": "ok",
                "humidity_status": "ok",
                "pressure_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (2 * 60 * 60 * 1000),
            "temperature": 31.0,  # 11°C change
            "humidity": 66.0,    # 16% change
            "pressure": 1010.0,  # exactly 10 hPa change
            "temperature_status": "ok",
            "humidity_status": "ok",
            "pressure_status": "ok"
        }

        event = detect_environmental_change(readings, current_reading)
        assert event is None

    def test_environmental_change_filters_non_ok_temperature(self):
        """Test that non-ok temperature readings are filtered."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "temperature": 20.0,
                "humidity": 50.0,
                "pressure": 1000.0,
                "temperature_status": "noisy",
                "humidity_status": "ok",
                "pressure_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (2 * 60 * 60 * 1000),
            "temperature": 31.0,
            "humidity": 66.0,
            "pressure": 1011.0,
            "temperature_status": "ok",
            "humidity_status": "ok",
            "pressure_status": "ok"
        }

        event = detect_environmental_change(readings, current_reading)
        assert event is None

    def test_environmental_change_current_reading_non_ok(self):
        """Test that non-ok current reading returns None."""
        readings = [
            {
                "hardware_id": "device-001",
                "timestamp_ms": 1000000,
                "temperature": 20.0,
                "humidity": 50.0,
                "pressure": 1000.0,
                "temperature_status": "ok",
                "humidity_status": "ok",
                "pressure_status": "ok"
            }
        ]
        current_reading = {
            "hardware_id": "device-001",
            "timestamp_ms": 1000000 + (2 * 60 * 60 * 1000),
            "temperature": 31.0,
            "humidity": 66.0,
            "pressure": 1011.0,
            "temperature_status": "ok",
            "humidity_status": "noisy",
            "pressure_status": "ok"
        }

        event = detect_environmental_change(readings, current_reading)
        assert event is None


class TestCooldownEnforcement:
    """Tests for cooldown period enforcement."""

    def test_get_cooldown_period_watering(self):
        """Test cooldown period for watering events is 60 minutes."""
        assert get_cooldown_period(EventType.WATERING_EVENT) == 60 * 60 * 1000

    def test_get_cooldown_period_temperature_stress(self):
        """Test cooldown period for temperature stress is 30 minutes."""
        assert get_cooldown_period(EventType.TEMPERATURE_STRESS) == 30 * 60 * 1000

    def test_get_cooldown_period_humidity_anomaly(self):
        """Test cooldown period for humidity anomaly is 30 minutes."""
        assert get_cooldown_period(EventType.HUMIDITY_ANOMALY) == 30 * 60 * 1000

    def test_get_cooldown_period_environmental_change(self):
        """Test cooldown period for environmental change is 2 hours."""
        assert get_cooldown_period(EventType.ENVIRONMENTAL_CHANGE) == 2 * 60 * 60 * 1000

    def test_get_cooldown_period_drying_cycle(self):
        """Test cooldown period for drying cycle is 0 (no cooldown)."""
        assert get_cooldown_period(EventType.DRYING_CYCLE) == 0

    @patch("shared.event_detection.dynamodb_resource")
    def test_check_cooldown_in_cooldown(self, mock_dynamodb):
        """Test cooldown check when recent event exists."""
        mock_table = MagicMock()
        mock_table.query.return_value = {
            "Items": [{"hardware_id": "device-001", "event_type": "Watering_Event"}]
        }
        mock_dynamodb.Table.return_value = mock_table

        current_time = 1000000 + (30 * 60 * 1000)
        in_cooldown = check_cooldown("device-001", EventType.WATERING_EVENT, current_time)

        assert in_cooldown is True

    @patch("shared.event_detection.dynamodb_resource")
    def test_check_cooldown_not_in_cooldown(self, mock_dynamodb):
        """Test cooldown check when no recent event exists."""
        mock_table = MagicMock()
        mock_table.query.return_value = {"Items": []}
        mock_dynamodb.Table.return_value = mock_table

        current_time = 1000000 + (70 * 60 * 1000)
        in_cooldown = check_cooldown("device-001", EventType.WATERING_EVENT, current_time)

        assert in_cooldown is False

    @patch("shared.event_detection.dynamodb_resource")
    def test_check_cooldown_zero_cooldown_returns_false(self, mock_dynamodb):
        """Test that events with zero cooldown always return False."""
        # Should not even query DynamoDB
        in_cooldown = check_cooldown("device-001", EventType.DRYING_CYCLE, 1000000)
        assert in_cooldown is False
        mock_dynamodb.Table.assert_not_called()

    @patch("shared.event_detection.dynamodb_resource")
    def test_check_cooldown_temperature_stress_30_minutes(self, mock_dynamodb):
        """Test temperature stress cooldown is enforced for 30 minutes."""
        mock_table = MagicMock()
        mock_table.query.return_value = {
            "Items": [{"hardware_id": "device-001", "event_type": "Temperature_Stress"}]
        }
        mock_dynamodb.Table.return_value = mock_table

        current_time = 1000000 + (25 * 60 * 1000)  # 25 minutes later
        in_cooldown = check_cooldown("device-001", EventType.TEMPERATURE_STRESS, current_time)

        assert in_cooldown is True

    @patch("shared.event_detection.dynamodb_resource")
    def test_check_cooldown_environmental_change_2_hours(self, mock_dynamodb):
        """Test environmental change cooldown is enforced for 2 hours."""
        mock_table = MagicMock()
        mock_table.query.return_value = {
            "Items": [{"hardware_id": "device-001", "event_type": "Environmental_Change"}]
        }
        mock_dynamodb.Table.return_value = mock_table

        current_time = 1000000 + (90 * 60 * 1000)  # 90 minutes later
        in_cooldown = check_cooldown("device-001", EventType.ENVIRONMENTAL_CHANGE, current_time)

        assert in_cooldown is True
