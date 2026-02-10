"""Unit tests for Cross-Device Administrative Query API endpoints."""

import json
import base64
import os
import pytest
from unittest.mock import Mock, patch

# Set environment variables before importing the module
os.environ['PLANT_EVENTS_TABLE'] = 'test-events-table'
os.environ['PLANT_AGGREGATES_TABLE'] = 'test-aggregates-table'
os.environ['PLANT_INSIGHTS_TABLE'] = 'test-insights-table'
os.environ['PLANT_DEVICE_PROFILES_TABLE'] = 'test-profiles-table'
os.environ['PLANT_DEVICE_STATUS_TABLE'] = 'test-status-table'
os.environ['PLANT_ROLLUPS_TABLE'] = 'test-rollups-table'
os.environ['POWERTOOLS_SERVICE_NAME'] = 'test-service'


@pytest.fixture
def sample_cross_device_events():
    """Sample events from multiple devices for testing."""
    return [
        {
            "hardware_id": "device-001",
            "start_time_ms": 1704070800000,
            "end_time_ms": 1704071100000,
            "event_type": "Temperature_Stress",
            "sensor_values": {"temperature": 38.5},
            "detection_metadata": {"threshold": 35.0}
        },
        {
            "hardware_id": "device-002",
            "start_time_ms": 1704070900000,
            "end_time_ms": 1704071200000,
            "event_type": "Temperature_Stress",
            "sensor_values": {"temperature": 39.2},
            "detection_metadata": {"threshold": 35.0}
        }
    ]


@pytest.fixture
def sample_stale_devices():
    """Sample stale device status records for testing."""
    return [
        {
            "hardware_id": "device-003",
            "last_seen_ingest_time_ms": 1704060000000,
            "health_category": "stale",
            "expected_interval_sec": 300,
            "sensor_status_summary": "ok"
        },
        {
            "hardware_id": "device-004",
            "last_seen_ingest_time_ms": 1704050000000,
            "health_category": "stale",
            "expected_interval_sec": 300,
            "sensor_status_summary": "degraded"
        }
    ]


@pytest.fixture
def sample_device_status():
    """Sample device status for testing."""
    return {
        "hardware_id": "device-001",
        "last_seen_ingest_time_ms": 1704070000000,
        "expected_interval_sec": 300,
        "sensor_status_summary": "ok"
    }


@pytest.fixture
def sample_aggregates_low_moisture():
    """Sample aggregates with low moisture for testing."""
    return [
        {
            "hardware_id": "device-001",
            "device_window": "device-001#hourly",
            "window_type": "hourly",
            "window_start_ms": 1704070800000,
            "window_end_ms": 1704074400000,
            "is_complete": True,
            "soil_moisture_stats": {
                "min": 20.0,
                "max": 25.0,
                "avg": 22.5,
                "stddev": 1.5,
                "valid_count": 12,
                "total_count": 12
            }
        }
    ]


class TestGetCrossDeviceEvents:
    """Tests for GET /admin/events endpoint."""

    def test_missing_event_type(self):
        """Test missing event_type parameter."""
        event = {
            "resource": "/admin/events",
            "path": "/admin/events",
            "httpMethod": "GET",
            "headers": {},
            "queryStringParameters": None,
            "pathParameters": {},
            "body": None
        }

        from functions.api import app
        response = app.resolve(event, Mock())

        assert response['statusCode'] == 400
        body = json.loads(response['body'])
        assert 'event_type parameter is required' in body['message']

    def test_invalid_event_type(self):
        """Test invalid event_type parameter."""
        event = {
            "resource": "/admin/events",
            "path": "/admin/events",
            "httpMethod": "GET",
            "headers": {},
            "queryStringParameters": {"event_type": "Invalid_Event"},
            "pathParameters": {},
            "body": None
        }

        from functions.api import app
        response = app.resolve(event, Mock())

        assert response['statusCode'] == 400
        body = json.loads(response['body'])
        assert 'event_type must be one of' in body['message']

    @patch('functions.api.events_table')
    def test_basic_retrieval(self, mock_table, sample_cross_device_events):
        """Test basic cross-device event retrieval."""
        event = {
            "resource": "/admin/events",
            "path": "/admin/events",
            "httpMethod": "GET",
            "headers": {},
            "queryStringParameters": {"event_type": "Temperature_Stress"},
            "pathParameters": {},
            "body": None
        }

        mock_table.query.return_value = {'Items': sample_cross_device_events}

        from functions.api import app
        response = app.resolve(event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'events' in body
        assert len(body['events']) == 2


class TestGetStaleDevices:
    """Tests for GET /admin/devices/stale endpoint."""

    @patch('functions.api.device_status_table')
    def test_basic_retrieval(self, mock_table, sample_stale_devices):
        """Test basic stale device retrieval."""
        event = {
            "resource": "/admin/devices/stale",
            "path": "/admin/devices/stale",
            "httpMethod": "GET",
            "headers": {},
            "queryStringParameters": None,
            "pathParameters": {},
            "body": None
        }

        mock_table.query.return_value = {'Items': sample_stale_devices}

        from functions.api import app
        response = app.resolve(event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'devices' in body
        assert len(body['devices']) == 2


class TestGetLowMoistureDevices:
    """Tests for GET /admin/devices/low-moisture endpoint."""

    @patch('functions.api.events_table')
    @patch('functions.api.aggregates_table')
    @patch('functions.api.device_status_table')
    def test_basic_retrieval(
        self, mock_status_table, mock_agg_table, mock_events_table,
        sample_device_status, sample_aggregates_low_moisture
    ):
        """Test basic low moisture device retrieval."""
        event = {
            "resource": "/admin/devices/low-moisture",
            "path": "/admin/devices/low-moisture",
            "httpMethod": "GET",
            "headers": {},
            "queryStringParameters": None,
            "pathParameters": {},
            "body": None
        }

        mock_status_table.scan.return_value = {'Items': [sample_device_status]}
        mock_agg_table.query.return_value = {'Items': sample_aggregates_low_moisture}
        mock_events_table.query.side_effect = [
            {'Items': []},
            {'Items': []}
        ]

        from functions.api import app
        response = app.resolve(event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'devices' in body
        assert 'moisture_threshold' in body
