"""
Unit tests for aggregation invariants.

Tests that aggregation logic maintains critical invariants:
- valid_count <= total_count
- min <= avg <= max for derived stats
- day window contains all hour windows
- week window contains all day windows

Requirements: 6.1-6.2, 6.10-6.11
"""

import pytest
import sys
import os

# Add shared and functions directories to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'shared'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'functions'))

from aggregator import compute_statistics_from_readings, combine_hourly_aggregates


class TestStatisticsInvariants:
    """Test that statistics computation maintains invariants."""

    def test_valid_count_less_than_or_equal_total_count(self):
        """Test that valid_count <= total_count for all sensors."""
        readings = [
            {
                "temperature": 20.0,
                "temperature_status": "ok",
                "humidity": 50.0,
                "humidity_status": "noisy",  # Invalid
                "pressure": 1013.0,
                "pressure_status": "ok",
                "soil_moisture": 30.0,
                "soil_moisture_status": "stale"  # Invalid
            },
            {
                "temperature": 22.0,
                "temperature_status": "out_of_range",  # Invalid
                "humidity": 55.0,
                "humidity_status": "ok",
                "pressure": 1015.0,
                "pressure_status": "missing",  # Invalid
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

        # Verify invariant for all sensors
        for sensor in ["temperature", "humidity", "pressure", "soil_moisture"]:
            sensor_stats = stats[sensor]
            assert sensor_stats["valid_count"] <= sensor_stats["total_count"], \
                f"{sensor}: valid_count ({sensor_stats['valid_count']}) > total_count ({sensor_stats['total_count']})"

    def test_min_avg_max_ordering_with_valid_data(self):
        """Test that min <= avg <= max when valid data exists."""
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
            },
            {
                "temperature": 20.0,
                "temperature_status": "ok",
                "humidity": 50.0,
                "humidity_status": "ok",
                "pressure": 1015.0,
                "pressure_status": "ok",
                "soil_moisture": 30.0,
                "soil_moisture_status": "ok"
            }
        ]

        stats = compute_statistics_from_readings(readings)

        # Verify min <= avg <= max for all sensors with valid data
        for sensor in ["temperature", "humidity", "pressure", "soil_moisture"]:
            sensor_stats = stats[sensor]
            if sensor_stats["valid_count"] > 0:
                assert sensor_stats["min"] <= sensor_stats["avg"] <= sensor_stats["max"], \
                    f"{sensor}: min ({sensor_stats['min']}) <= avg ({sensor_stats['avg']}) <= max ({sensor_stats['max']}) violated"

    def test_min_avg_max_with_single_reading(self):
        """Test that min == avg == max with a single valid reading."""
        readings = [
            {
                "temperature": 22.5,
                "temperature_status": "ok",
                "humidity": 55.0,
                "humidity_status": "ok",
                "pressure": 1013.25,
                "pressure_status": "ok",
                "soil_moisture": 45.0,
                "soil_moisture_status": "ok"
            }
        ]

        stats = compute_statistics_from_readings(readings)

        for sensor in ["temperature", "humidity", "pressure", "soil_moisture"]:
            sensor_stats = stats[sensor]
            assert sensor_stats["valid_count"] == 1
            assert sensor_stats["min"] == sensor_stats["avg"] == sensor_stats["max"], \
                f"{sensor}: min, avg, max should be equal with single reading"

    def test_no_min_max_avg_when_no_valid_data(self):
        """Test that min, max, avg are not present when valid_count is 0."""
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

        for sensor in ["temperature", "humidity", "pressure", "soil_moisture"]:
            sensor_stats = stats[sensor]
            assert sensor_stats["valid_count"] == 0
            assert sensor_stats["total_count"] == 1
            assert "min" not in sensor_stats
            assert "max" not in sensor_stats
            assert "avg" not in sensor_stats

    def test_invariants_with_extreme_values(self):
        """Test invariants hold with extreme but valid sensor values."""
        readings = [
            {
                "temperature": -40.0,  # Min valid temperature
                "temperature_status": "ok",
                "humidity": 0.0,  # Min valid humidity
                "humidity_status": "ok",
                "pressure": 300.0,  # Min valid pressure
                "pressure_status": "ok",
                "soil_moisture": 0.0,  # Min valid soil moisture
                "soil_moisture_status": "ok"
            },
            {
                "temperature": 85.0,  # Max valid temperature
                "temperature_status": "ok",
                "humidity": 100.0,  # Max valid humidity
                "humidity_status": "ok",
                "pressure": 1100.0,  # Max valid pressure
                "pressure_status": "ok",
                "soil_moisture": 100.0,  # Max valid soil moisture
                "soil_moisture_status": "ok"
            }
        ]

        stats = compute_statistics_from_readings(readings)

        for sensor in ["temperature", "humidity", "pressure", "soil_moisture"]:
            sensor_stats = stats[sensor]
            assert sensor_stats["valid_count"] == 2
            assert sensor_stats["valid_count"] <= sensor_stats["total_count"]
            assert sensor_stats["min"] <= sensor_stats["avg"] <= sensor_stats["max"]


class TestCombinedAggregateInvariants:
    """Test that combining aggregates maintains invariants."""

    def test_combined_valid_count_less_than_or_equal_total_count(self):
        """Test that valid_count <= total_count when combining hourly aggregates."""
        hourly_items = [
            {
                "temperature_stats": {
                    "M": {
                        "min": {"N": "18.0"},
                        "max": {"N": "22.0"},
                        "sum": {"N": "60.0"},
                        "sumsq": {"N": "1204.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "5"}  # Some invalid readings
                    }
                },
                "humidity_stats": {
                    "M": {
                        "min": {"N": "45.0"},
                        "max": {"N": "55.0"},
                        "sum": {"N": "100.0"},
                        "sumsq": {"N": "5050.0"},
                        "valid_count": {"N": "2"},
                        "total_count": {"N": "5"}
                    }
                },
                "pressure_stats": {
                    "M": {
                        "valid_count": {"N": "0"},
                        "total_count": {"N": "5"},
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
                        "total_count": {"N": "5"}
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
                        "total_count": {"N": "4"}
                    }
                },
                "humidity_stats": {
                    "M": {
                        "min": {"N": "50.0"},
                        "max": {"N": "60.0"},
                        "sum": {"N": "110.0"},
                        "sumsq": {"N": "6100.0"},
                        "valid_count": {"N": "2"},
                        "total_count": {"N": "4"}
                    }
                },
                "pressure_stats": {
                    "M": {
                        "valid_count": {"N": "1"},
                        "total_count": {"N": "4"},
                        "min": {"N": "1013.0"},
                        "max": {"N": "1013.0"},
                        "sum": {"N": "1013.0"},
                        "sumsq": {"N": "1026169.0"}
                    }
                },
                "soil_moisture_stats": {
                    "M": {
                        "min": {"N": "30.0"},
                        "max": {"N": "34.0"},
                        "sum": {"N": "96.0"},
                        "sumsq": {"N": "3088.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "4"}
                    }
                }
            }
        ]

        combined = combine_hourly_aggregates(hourly_items)

        # Verify invariant for all sensors
        for sensor in ["temperature", "humidity", "pressure", "soil_moisture"]:
            sensor_stats = combined[sensor]
            assert sensor_stats["valid_count"] <= sensor_stats["total_count"], \
                f"{sensor}: valid_count ({sensor_stats['valid_count']}) > total_count ({sensor_stats['total_count']})"

    def test_combined_min_avg_max_ordering(self):
        """Test that min <= avg <= max when combining hourly aggregates."""
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
                        "min": {"N": "1010.0"},
                        "max": {"N": "1015.0"},
                        "sum": {"N": "3038.0"},
                        "sumsq": {"N": "3076444.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
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
                        "min": {"N": "1012.0"},
                        "max": {"N": "1018.0"},
                        "sum": {"N": "3045.0"},
                        "sumsq": {"N": "3091225.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
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

        # Verify min <= avg <= max for all sensors with valid data
        for sensor in ["temperature", "humidity", "pressure", "soil_moisture"]:
            sensor_stats = combined[sensor]
            if sensor_stats["valid_count"] > 0:
                assert sensor_stats["min"] <= sensor_stats["avg"] <= sensor_stats["max"], \
                    f"{sensor}: min ({sensor_stats['min']}) <= avg ({sensor_stats['avg']}) <= max ({sensor_stats['max']}) violated"

    def test_combined_min_is_minimum_of_all_hourly_mins(self):
        """Test that combined min is the minimum of all hourly minimums."""
        hourly_items = [
            {
                "temperature_stats": {
                    "M": {
                        "min": {"N": "15.0"},  # Lowest temperature
                        "max": {"N": "20.0"},
                        "sum": {"N": "52.5"},
                        "sumsq": {"N": "920.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                }
            },
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
                }
            },
            {
                "temperature_stats": {
                    "M": {
                        "min": {"N": "20.0"},
                        "max": {"N": "25.0"},
                        "sum": {"N": "67.5"},
                        "sumsq": {"N": "1525.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                }
            }
        ]

        combined = combine_hourly_aggregates(hourly_items)

        # Combined min should be 15.0 (the minimum across all hours)
        assert combined["temperature"]["min"] == 15.0

    def test_combined_max_is_maximum_of_all_hourly_maxs(self):
        """Test that combined max is the maximum of all hourly maximums."""
        hourly_items = [
            {
                "humidity_stats": {
                    "M": {
                        "min": {"N": "40.0"},
                        "max": {"N": "50.0"},
                        "sum": {"N": "135.0"},
                        "sumsq": {"N": "6125.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                }
            },
            {
                "humidity_stats": {
                    "M": {
                        "min": {"N": "45.0"},
                        "max": {"N": "55.0"},
                        "sum": {"N": "150.0"},
                        "sumsq": {"N": "7550.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                }
            },
            {
                "humidity_stats": {
                    "M": {
                        "min": {"N": "50.0"},
                        "max": {"N": "65.0"},  # Highest humidity
                        "sum": {"N": "172.5"},
                        "sumsq": {"N": "9962.5"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                }
            }
        ]

        combined = combine_hourly_aggregates(hourly_items)

        # Combined max should be 65.0 (the maximum across all hours)
        assert combined["humidity"]["max"] == 65.0


class TestWindowContainmentInvariants:
    """Test that larger windows contain all smaller windows."""

    def test_daily_aggregate_contains_all_hourly_data(self):
        """Test that daily aggregate total_count equals sum of hourly total_counts."""
        # Simulate 24 hours of data (simplified to 3 hours for testing)
        hourly_items = [
            {
                "temperature_stats": {
                    "M": {
                        "min": {"N": "18.0"},
                        "max": {"N": "22.0"},
                        "sum": {"N": "60.0"},
                        "sumsq": {"N": "1204.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "5"}
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
                        "total_count": {"N": "4"}
                    }
                }
            },
            {
                "temperature_stats": {
                    "M": {
                        "min": {"N": "19.0"},
                        "max": {"N": "23.0"},
                        "sum": {"N": "63.0"},
                        "sumsq": {"N": "1327.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "6"}
                    }
                }
            }
        ]

        combined = combine_hourly_aggregates(hourly_items)

        # Daily total_count should equal sum of hourly total_counts
        expected_total_count = 5 + 4 + 6  # 15
        assert combined["temperature"]["total_count"] == expected_total_count

        # Daily valid_count should equal sum of hourly valid_counts
        expected_valid_count = 3 + 3 + 3  # 9
        assert combined["temperature"]["valid_count"] == expected_valid_count

    def test_weekly_aggregate_contains_all_daily_data(self):
        """Test that weekly aggregate contains all daily data (using same combine function)."""
        # Simulate 7 days of data (simplified to 3 days for testing)
        daily_items = [
            {
                "soil_moisture_stats": {
                    "M": {
                        "min": {"N": "25.0"},
                        "max": {"N": "35.0"},
                        "sum": {"N": "720.0"},
                        "sumsq": {"N": "21800.0"},
                        "valid_count": {"N": "24"},
                        "total_count": {"N": "30"}
                    }
                }
            },
            {
                "soil_moisture_stats": {
                    "M": {
                        "min": {"N": "28.0"},
                        "max": {"N": "38.0"},
                        "sum": {"N": "792.0"},
                        "sumsq": {"N": "26400.0"},
                        "valid_count": {"N": "24"},
                        "total_count": {"N": "28"}
                    }
                }
            },
            {
                "soil_moisture_stats": {
                    "M": {
                        "min": {"N": "26.0"},
                        "max": {"N": "36.0"},
                        "sum": {"N": "744.0"},
                        "sumsq": {"N": "23200.0"},
                        "valid_count": {"N": "24"},
                        "total_count": {"N": "32"}
                    }
                }
            }
        ]

        combined = combine_hourly_aggregates(daily_items)  # Same function works for daily->weekly

        # Weekly total_count should equal sum of daily total_counts
        expected_total_count = 30 + 28 + 32  # 90
        assert combined["soil_moisture"]["total_count"] == expected_total_count

        # Weekly valid_count should equal sum of daily valid_counts
        expected_valid_count = 24 + 24 + 24  # 72
        assert combined["soil_moisture"]["valid_count"] == expected_valid_count

    def test_combined_aggregate_min_not_greater_than_any_hourly_min(self):
        """Test that combined min is not greater than any hourly min."""
        hourly_items = [
            {
                "pressure_stats": {
                    "M": {
                        "min": {"N": "1010.0"},
                        "max": {"N": "1015.0"},
                        "sum": {"N": "3038.0"},
                        "sumsq": {"N": "3076444.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                }
            },
            {
                "pressure_stats": {
                    "M": {
                        "min": {"N": "1012.0"},
                        "max": {"N": "1018.0"},
                        "sum": {"N": "3045.0"},
                        "sumsq": {"N": "3091225.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                }
            },
            {
                "pressure_stats": {
                    "M": {
                        "min": {"N": "1008.0"},  # Lowest min
                        "max": {"N": "1013.0"},
                        "sum": {"N": "3031.5"},
                        "sumsq": {"N": "3063150.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                }
            }
        ]

        combined = combine_hourly_aggregates(hourly_items)

        # Extract all hourly mins
        hourly_mins = [1010.0, 1012.0, 1008.0]

        # Combined min should be the minimum of all hourly mins
        assert combined["pressure"]["min"] == min(hourly_mins)
        assert combined["pressure"]["min"] <= min(hourly_mins)

    def test_combined_aggregate_max_not_less_than_any_hourly_max(self):
        """Test that combined max is not less than any hourly max."""
        hourly_items = [
            {
                "pressure_stats": {
                    "M": {
                        "min": {"N": "1010.0"},
                        "max": {"N": "1015.0"},
                        "sum": {"N": "3038.0"},
                        "sumsq": {"N": "3076444.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                }
            },
            {
                "pressure_stats": {
                    "M": {
                        "min": {"N": "1012.0"},
                        "max": {"N": "1022.0"},  # Highest max
                        "sum": {"N": "3051.0"},
                        "sumsq": {"N": "3103725.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                }
            },
            {
                "pressure_stats": {
                    "M": {
                        "min": {"N": "1008.0"},
                        "max": {"N": "1013.0"},
                        "sum": {"N": "3031.5"},
                        "sumsq": {"N": "3063150.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                }
            }
        ]

        combined = combine_hourly_aggregates(hourly_items)

        # Extract all hourly maxs
        hourly_maxs = [1015.0, 1022.0, 1013.0]

        # Combined max should be the maximum of all hourly maxs
        assert combined["pressure"]["max"] == max(hourly_maxs)
        assert combined["pressure"]["max"] >= max(hourly_maxs)


class TestEmptyDataInvariants:
    """Test invariants with empty or missing data."""

    def test_empty_readings_list(self):
        """Test that empty readings list produces zero counts."""
        readings = []

        stats = compute_statistics_from_readings(readings)

        for sensor in ["temperature", "humidity", "pressure", "soil_moisture"]:
            sensor_stats = stats[sensor]
            assert sensor_stats["valid_count"] == 0
            assert sensor_stats["total_count"] == 0
            assert "min" not in sensor_stats
            assert "max" not in sensor_stats
            assert "avg" not in sensor_stats

    def test_empty_hourly_aggregates_list(self):
        """Test that empty hourly list produces zero counts."""
        hourly_items = []

        combined = combine_hourly_aggregates(hourly_items)

        for sensor in ["temperature", "humidity", "pressure", "soil_moisture"]:
            sensor_stats = combined[sensor]
            assert sensor_stats["valid_count"] == 0
            assert sensor_stats["total_count"] == 0

    def test_mixed_valid_and_invalid_across_hours(self):
        """Test invariants when some hours have valid data and others don't."""
        hourly_items = [
            {
                "temperature_stats": {
                    "M": {
                        "min": {"N": "20.0"},
                        "max": {"N": "22.0"},
                        "sum": {"N": "63.0"},
                        "sumsq": {"N": "1327.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "3"}
                    }
                }
            },
            {
                "temperature_stats": {
                    "M": {
                        "valid_count": {"N": "0"},  # No valid data this hour
                        "total_count": {"N": "5"},
                        "sum": {"N": "0.0"},
                        "sumsq": {"N": "0.0"}
                    }
                }
            },
            {
                "temperature_stats": {
                    "M": {
                        "min": {"N": "19.0"},
                        "max": {"N": "21.0"},
                        "sum": {"N": "60.0"},
                        "sumsq": {"N": "1204.0"},
                        "valid_count": {"N": "3"},
                        "total_count": {"N": "4"}
                    }
                }
            }
        ]

        combined = combine_hourly_aggregates(hourly_items)

        # Verify invariants still hold
        assert combined["temperature"]["valid_count"] == 6  # 3 + 0 + 3
        assert combined["temperature"]["total_count"] == 12  # 3 + 5 + 4
        assert combined["temperature"]["valid_count"] <= combined["temperature"]["total_count"]
        assert combined["temperature"]["min"] <= combined["temperature"]["avg"] <= combined["temperature"]["max"]


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
