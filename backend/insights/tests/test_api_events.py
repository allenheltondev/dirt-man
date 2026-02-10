"""Unit tests for Event Query API endpoint."""

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
def sample_events():
    """Sample events for testing."""
    return [
        {
            "hardware_id": "device-001",
            "start_time_ms": 1704070800000,
            "end_time_ms": 1704071100000,
            "event_type": "Watering_Event",
            "sensor_values": {"soil_moisture_before": 30.0},
            "detection_metadata": {"detection_mode": "rapid_spike"}
        }
    ]


@pytest.fixture
def sample_aggregates():
    """Sample aggregates for testing."""
    return [
        {
            "hardware_id": "device-001",
            "device_window": "device-001#hourly",
            "window_type": "hourly",
            "window_start_ms": 1704070800000,
            "window_end_ms": 1704074400000,
            "is_complete": True,
            "computed_at_ms": 1704074500000,
            "temperature_stats": {
                "min": 20.5,
                "max": 25.3,
                "avg": 22.8,
                "stddev": 1.2,
                "valid_count": 12,
                "total_count": 12
            },
            "humidity_stats": {
                "min": 45.0,
                "max": 55.0,
                "avg": 50.0,
                "stddev": 2.5,
                "valid_count": 12,
                "total_count": 12
            },
            "pressure_stats": {
                "min": 1010.0,
                "max": 1015.0,
                "avg": 1012.5,
                "stddev": 1.5,
                "valid_count": 12,
                "total_count": 12
            },
            "soil_moisture_stats": {
                "min": 30.0,
                "max": 35.0,
                "avg": 32.5,
                "stddev": 1.8,
                "valid_count": 12,
                "total_count": 12
            }
        }
    ]


@pytest.fixture
def api_gateway_event():
    """Sample API Gateway event."""
    return {
        "resource": "/devices/{hardware_id}/events",
        "path": "/devices/device-001/events",
        "httpMethod": "GET",
        "headers": {},
        "queryStringParameters": None,
        "pathParameters": {"hardware_id": "device-001"},
        "body": None
    }


@pytest.fixture
def aggregates_api_gateway_event():
    """Sample API Gateway event for aggregates endpoint."""
    return {
        "resource": "/devices/{hardware_id}/aggregates",
        "path": "/devices/device-001/aggregates",
        "httpMethod": "GET",
        "headers": {},
        "queryStringParameters": {"window_type": "hourly"},
        "pathParameters": {"hardware_id": "device-001"},
        "body": None
    }


class TestGetDeviceEvents:
    """Tests for GET /devices/{hardware_id}/events endpoint."""

    @patch('functions.api.events_table')
    def test_get_events_basic(self, mock_table, api_gateway_event, sample_events):
        """Test basic event retrieval."""
        mock_table.query.return_value = {'Items': sample_events}

        from functions.api import app
        response = app.resolve(api_gateway_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'events' in body
        assert len(body['events']) == 1

    @patch('functions.api.events_table')
    def test_get_events_empty(self, mock_table, api_gateway_event):
        """Test empty results."""
        mock_table.query.return_value = {'Items': []}

        from functions.api import app
        response = app.resolve(api_gateway_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert body['events'] == []


class TestGetDeviceAggregates:
    """Tests for GET /devices/{hardware_id}/aggregates endpoint."""

    @patch('functions.api.aggregates_table')
    def test_get_aggregates_basic(self, mock_table, aggregates_api_gateway_event, sample_aggregates):
        """Test basic aggregate retrieval."""
        mock_table.query.return_value = {'Items': sample_aggregates}

        from functions.api import app
        response = app.resolve(aggregates_api_gateway_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'aggregates' in body
        assert len(body['aggregates']) == 1

        # Verify derived stats are included
        agg = body['aggregates'][0]
        assert 'temperature_stats' in agg
        assert agg['temperature_stats']['min'] == 20.5
        assert agg['temperature_stats']['max'] == 25.3
        assert agg['temperature_stats']['avg'] == 22.8
        assert agg['temperature_stats']['stddev'] == 1.2
        assert agg['temperature_stats']['valid_count'] == 12
        assert agg['temperature_stats']['total_count'] == 12

    @patch('functions.api.aggregates_table')
    def test_get_aggregates_empty(self, mock_table, aggregates_api_gateway_event):
        """Test empty results."""
        mock_table.query.return_value = {'Items': []}

        from functions.api import app
        response = app.resolve(aggregates_api_gateway_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert body['aggregates'] == []

    def test_get_aggregates_missing_window_type(self, aggregates_api_gateway_event):
        """Test missing window_type parameter."""
        aggregates_api_gateway_event['queryStringParameters'] = None

        from functions.api import app
        response = app.resolve(aggregates_api_gateway_event, Mock())

        # BadRequestError is caught by the resolver and returns 400
        assert response['statusCode'] == 400
        body = json.loads(response['body'])
        assert 'window_type parameter is required' in body['message']

    def test_get_aggregates_invalid_window_type(self, aggregates_api_gateway_event):
        """Test invalid window_type parameter."""
        aggregates_api_gateway_event['queryStringParameters'] = {'window_type': 'invalid'}

        from functions.api import app
        response = app.resolve(aggregates_api_gateway_event, Mock())

        # BadRequestError is caught by the resolver and returns 400
        assert response['statusCode'] == 400
        body = json.loads(response['body'])
        assert 'window_type must be one of' in body['message']

    @patch('functions.api.aggregates_table')
    def test_get_aggregates_with_time_range(self, mock_table, aggregates_api_gateway_event, sample_aggregates):
        """Test aggregate retrieval with time range filtering."""
        aggregates_api_gateway_event['queryStringParameters'] = {
            'window_type': 'hourly',
            'start_time': '1704070000000',
            'end_time': '1704080000000'
        }
        mock_table.query.return_value = {'Items': sample_aggregates}

        from functions.api import app
        response = app.resolve(aggregates_api_gateway_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'aggregates' in body
        assert len(body['aggregates']) == 1

    @patch('functions.api.aggregates_table')
    def test_get_aggregates_with_pagination(self, mock_table, aggregates_api_gateway_event, sample_aggregates):
        """Test aggregate retrieval with pagination."""
        last_key = {
            'device_window': 'device-001#hourly',
            'window_start_ms': 1704070800000
        }
        mock_table.query.return_value = {
            'Items': sample_aggregates,
            'LastEvaluatedKey': last_key
        }

        from functions.api import app
        response = app.resolve(aggregates_api_gateway_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'aggregates' in body
        assert 'next_token' in body

        # Verify next_token is a valid base64 string
        import base64
        decoded = base64.b64decode(body['next_token'])
        assert decoded

    @patch('functions.api.aggregates_table')
    def test_get_aggregates_daily_window(self, mock_table, aggregates_api_gateway_event, sample_aggregates):
        """Test aggregate retrieval with daily window type."""
        aggregates_api_gateway_event['queryStringParameters'] = {'window_type': 'daily'}
        sample_aggregates[0]['window_type'] = 'daily'
        sample_aggregates[0]['device_window'] = 'device-001#daily'
        mock_table.query.return_value = {'Items': sample_aggregates}

        from functions.api import app
        response = app.resolve(aggregates_api_gateway_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'aggregates' in body
        assert body['aggregates'][0]['window_type'] == 'daily'

    @patch('functions.api.aggregates_table')
    def test_get_aggregates_weekly_window(self, mock_table, aggregates_api_gateway_event, sample_aggregates):
        """Test aggregate retrieval with weekly window type."""
        aggregates_api_gateway_event['queryStringParameters'] = {'window_type': 'weekly'}
        sample_aggregates[0]['window_type'] = 'weekly'
        sample_aggregates[0]['device_window'] = 'device-001#weekly'
        mock_table.query.return_value = {'Items': sample_aggregates}

        from functions.api import app
        response = app.resolve(aggregates_api_gateway_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'aggregates' in body
        assert body['aggregates'][0]['window_type'] == 'weekly'

