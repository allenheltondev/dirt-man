"""
Unit tests for Aggregator Lambda.

Tests aggregation logic, time window calculations, and statistics computation.
"""

import pytest
from datetime import datetime, timezone
import sys
import os

# Add shared and functions directories to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'shared'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'functions'))

from aggregator import compute_statistics_from_readings, combine_hourly_aggregates
from time_utils import align_to_hour, align_to_day, align_to_week, get_hour_window


class TestTimeAlignment:
    """Test time window alignment functions."""

    def test_align_to_hour(self):
        """Test hour alignment."""
        # 2024-01-15 14:35:22 UTC
        timestamp_ms = 1705330522000
        aligned = align_to_hour(timestamp_ms)

        # Should align to 2024-01-15 14:00:00 UTC
        # Verify it's aligned to the hour
        dt = datetime.fromtimestamp(aligned / 1000, tz=timezone.utc)
        assert dt.minute == 0
        assert dt.second == 0
        assert dt.microsecond == 0

    def test_align_to_day(self):
        """Test day alignment."""
        # 2024-01-15 14:35:22 UTC
        timestamp_ms = 1705330522000
        aligned = align_to_day(timestamp_ms)

        # Should align to 2024-01-15 00:00:00 UTC
        expected = 1705276800000
        assert aligned == expected

    def test_align_to_week(self):
        """Test week alignment to Monday."""
        # 2024-01-17 (Wednesday) 14:35:22 UTC
        timestamp_ms = 1705503322000
        aligned = align_to_week(timestamp_ms)

        # Should align to 2024-01-15 (Monday) 00:00:00 UTC
        expected = 1705276800000
        assert aligned == expected

    def test_get_hour_window(self):
        """Test hour window boundaries."""
        # 2024-01-15 14:35:22 UTC
        timestamp_ms = 1705330522000
        start, end = get_hour_window(timestamp_ms)

        # Verify window is exactly 1 hour
        assert end - start == 3600000  # 1 hour in ms

        # Verify start is aligned to hour
        dt_start = datetime.fromtimestamp(start / 1000, tz=timezone.utc)
        assert dt_start.minute == 0
        assert dt_start.second == 0


class TestStatisticsComputation:
    """Test statistics computation from readings."""

    def test_compute_statistics_with_valid_readings(self):
        """Test statistics computation with all valid readings."""
        readings = [
            {
                "temperature": 20.0,
                "temperature_status": "ok",
                "humidity": 50.0,
                "humidity_status": "ok",
                "pressure": 1013.0,
                "pressure_status": "ok",
                "soil_moisture": 30.0,
                "soil_moisture_status": "ok"
            },
            {
                "temperature": 22.0,
                "temperature_status": "ok",
                "humidity": 55.0,
                "humidity_status": "ok",
                "pressure": 1015.0,
                "pressure_status": "ok",
                "soil_moisture": 32.0,
                "soil_moisture_status": "ok"
            },
            {
                "temperature": 21.0,
                "temperature_status": "ok",
                "humidity": 52.0,
                "humidity_status": "ok",
                "pressure": 1014.0,
                "pressure_status": "ok",
                "soil_moisture": 31.0,
                "soil_moisture_status": "ok"
            }
        ]

        stats = compute_statistics_from_readings(readings)

        # Check temperature stats
        assert stats["temperature"]["min"] == 20.0
        assert stats["temperature"]["max"] == 22.0
        assert stats["temperature"]["avg"] == 21.0
        assert stats["temperature"]["valid_count"] == 3
        assert stats["temperature"]["total_count"] == 3

        # Check humidity stats
        assert stats["humidity"]["min"] == 50.0
        assert stats["humidity"]["max"] == 55.0
        assert stats["humidity"]["valid_count"] == 3

    def test_compute_statistics_with_invalid_readings(self):
        """Test that invalid readings are excluded from statistics."""
        readings = [
            {
                "temperature": 20.0,
                "temperature_status": "ok",
                "humidity": 50.0,
                "humidity_status": "noisy",  # Invalid
                "pressure": 1013.0,
                "pressure_status": "ok",
                "soil_moisture": 30.0,
                "soil_moisture_status": "ok"
            },
            {
                "temperature": 22.0,
                "temperature_status": "ok",
                "humidity": 55.0,
                "humidity_status": "ok",
                "pressure": 1015.0,
                "pressure_status": "out_of_range",  # Invalid
                "soil_moisture": 32.0,
                "soil_moisture_status": "ok"
            }
        ]

        stats = compute_statistics_from_readings(readings)

        # Temperature should have 2 valid readings
        assert stats["temperature"]["valid_count"] == 2
        assert stats["temperature"]["total_count"] == 2

        # Humidity should have 1 valid reading (second one)
        assert stats["humidity"]["valid_count"] == 1
        assert stats["humidity"]["total_count"] == 2

        # Pressure should have 1 valid reading (first one)
        assert stats["pressure"]["valid_count"] == 1
        assert stats["pressure"]["total_count"] == 2

    def test_compute_statistics_with_no_valid_readings(self):
        """Test statistics with no valid readings."""
        readings = [
            {
                "temperature": 20.0,
                "temperature_status": "noisy",
                "humidity": 50.0,
                "humidity_status": "stale",
                "pressure": 1013.0,
                "pressure_status": "out_of_range",
                "soil_moisture": 30.0,
                "soil_moisture_status": "missing"
            }
        ]

        stats = compute_statistics_from_readings(readings)

        # All sensors should have 0 valid count but 1 total count
        for sensor in ["temperature", "humidity", "pressure", "soil_moisture"]:
            assert stats[sensor]["valid_count"] == 0
            assert stats[sensor]["total_count"] == 1
            assert "min" not in stats[sensor]
            assert "max" not in stats[sensor]

    def test_compute_statistics_invariants(self):
        """Test that statistics maintain invariants."""
        readings = [
            {
                "temperature": 15.0,
                "temperature_status": "ok",
                "humidity": 40.0,
                "humidity_status": "ok",
                "pressure": 1010.0,
                "pressure_status": "ok",
                "soil_moisture": 25.0,
                "soil_moisture_status": "ok"
            },
            {
                "temperature": 25.0,
                "temperature_status": "ok",
                "humidity": 60.0,
                "humidity_status": "ok",
                "pressure": 1020.0,
                "pressure_status": "ok",
                "soil_moisture": 35.0,
                "soil_moisture_status": "ok"
            }
        ]

        stats = compute_statistics_from_readings(readings)

        # Test invariants for each sensor
        for sensor in ["temperature", "humidity", "pressure", "soil_moisture"]:
            sensor_stats = stats[sensor]

            # valid_count <= total_count
            assert sensor_stats["valid_count"] <= sensor_stats["total_count"]

            # min <= avg <= max
            if sensor_stats["valid_count"] > 0:
                assert sensor_stats["min"] <= sensor_stats["avg"] <= sensor_stats["max"]


class TestAggregationCombination:
    """Test combining hourly aggregates into daily/weekly."""

    def test_combine_hourly_aggregates(self):
        """Test combining multiple hourly aggregates."""
        hourly_items = [
            {
                "temperature_stats": {
                    "M": {
                        "min": {"N": "18.0"},
                        "max": {"N": "22.0"},
                        "sum": {"N": "60.0"},
                        "sumsq": {"N": "1204.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                },
                "humidity_stats": {
                    "M": {
                        "min": {"N": "45.0"},
                        "max": {"N": "55.0"},
                        "sum": {"N": "150.0"},
                        "sumsq": {"N": "7550.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                },
                "pressure_stats": {
                    "M": {
                        "valid_count": {"N": "0"},
                        "total_count": {"N": "3"},
                        "sum": {"N": "0.0"},
                        "sumsq": {"N": "0.0"}
                    }
                },
                "soil_moisture_stats": {
                    "M": {
                        "min": {"N": "28.0"},
                        "max": {"N": "32.0"},
                        "sum": {"N": "90.0"},
                        "sumsq": {"N": "2708.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                }
            },
            {
                "temperature_stats": {
                    "M": {
                        "min": {"N": "20.0"},
                        "max": {"N": "24.0"},
                        "sum": {"N": "66.0"},
                        "sumsq": {"N": "1460.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                },
                "humidity_stats": {
                    "M": {
                        "min": {"N": "50.0"},
                        "max": {"N": "60.0"},
                        "sum": {"N": "165.0"},
                        "sumsq": {"N": "9125.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                },
                "pressure_stats": {
                    "M": {
                        "valid_count": {"N": "0"},
                        "total_count": {"N": "3"},
                        "sum": {"N": "0.0"},
                        "sumsq": {"N": "0.0"}
                    }
                },
                "soil_moisture_stats": {
                    "M": {
                        "min": {"N": "30.0"},
                        "max": {"N": "34.0"},
                        "sum": {"N": "96.0"},
                        "sumsq": {"N": "3088.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                }
            }
        ]

        combined = combine_hourly_aggregates(hourly_items)

        # Check temperature combined stats
        assert combined["temperature"]["min"] == 18.0
        assert combined["temperature"]["max"] == 24.0
        assert combined["temperature"]["valid_count"] == 6
        assert combined["temperature"]["total_count"] == 6
        assert combined["temperature"]["sum"] == 126.0

        # Check humidity combined stats
        assert combined["humidity"]["min"] == 45.0
        assert combined["humidity"]["max"] == 60.0
        assert combined["humidity"]["valid_count"] == 6

        # Check pressure has no valid readings
        assert combined["pressure"]["valid_count"] == 0
        assert combined["pressure"]["total_count"] == 6

    def test_combine_empty_hourly_aggregates(self):
        """Test combining with no hourly aggregates."""
        combined = combine_hourly_aggregates([])

        # Should return empty stats for all sensors
        for sensor in ["temperature", "humidity", "pressure", "soil_moisture"]:
            assert combined[sensor]["valid_count"] == 0
            assert combined[sensor]["total_count"] == 0


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
