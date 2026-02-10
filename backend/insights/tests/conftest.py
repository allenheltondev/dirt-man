"""
Pytest configuration and shared fixtures for Plant Insights tests.
"""

import pytest


@pytest.fixture
def sample_reading():
    """Fixture providing a sample sensor reading for tests."""
    return {
        "hardware_id": "test-device-001",
        "timestamp_ms": 1704067200000,  # 2024-01-01 00:00:00 UTC
        "batch_id": "batch-123",
        "temperature": 22.5,
        "humidity": 65.0,
        "pressure": 1013.25,
        "soil_moisture": 45.0,
    }


@pytest.fixture
def sample_event():
    """Fixture providing a sample event for tests."""
    return {
        "hardware_id": "test-device-001",
        "start_time_ms": 1704067200000,
        "end_time_ms": 1704067500000,
        "event_type": "Watering_Event",
        "sensor_values": {
            "soil_moisture_before": 30.0,
            "soil_moisture_after": 75.0,
        },
        "detection_metadata": {
            "detection_mode": "rapid_spike",
        },
    }


@pytest.fixture
def sample_aggregate():
    """Fixture providing a sample aggregate for tests."""
    return {
        "device_window": "test-device-001#hourly",
        "window_type": "hourly",
        "window_start_ms": 1704067200000,
        "window_end_ms": 1704070800000,
        "temperature_stats": {
            "min": 20.0,
            "max": 25.0,
            "avg": 22.5,
            "stddev": 1.2,
            "valid_count": 12,
            "total_count": 12,
        },
    }
