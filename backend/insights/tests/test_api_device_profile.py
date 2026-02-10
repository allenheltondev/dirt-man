"""Unit tests for Device Profile API endpoints."""

import json
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
def sample_profile():
    """Sample device profile for testing."""
    return {
        "hardware_id": "device-001",
        "plant_type": "pothos",
        "soil_type": "potting_mix",
        "pot_size_liters": 5.0,
        "expected_interval_sec": 300,
        "baseline_moisture_range": {"min": 30.0, "max": 60.0},
        "typical_watering_interval_sec": 172800,
        "last_watering_events": [1704070800000, 1704243600000],
        "updated_at_ms": 1704330000000
    }


@pytest.fixture
def get_profile_event():
    """Sample API Gateway event for GET profile."""
    return {
        "resource": "/devices/{hardware_id}/profile",
        "path": "/devices/device-001/profile",
        "httpMethod": "GET",
        "headers": {},
        "queryStringParameters": None,
        "pathParameters": {"hardware_id": "device-001"},
        "body": None
    }


@pytest.fixture
def put_profile_event():
    """Sample API Gateway event for PUT profile."""
    return {
        "resource": "/devices/{hardware_id}/profile",
        "path": "/devices/device-001/profile",
        "httpMethod": "PUT",
        "headers": {"Content-Type": "application/json"},
        "queryStringParameters": None,
        "pathParameters": {"hardware_id": "device-001"},
        "body": json.dumps({
            "plant_type": "snake_plant",
            "soil_type": "coco_coir",
            "pot_size": 3.5,
            "expected_interval_sec": 600
        })
    }


class TestGetDeviceProfile:
    """Tests for GET /devices/{hardware_id}/profile endpoint."""

    @patch('functions.api.device_profiles_table')
    def test_get_profile_success(self, mock_table, get_profile_event, sample_profile):
        """Test successful profile retrieval."""
        mock_table.get_item.return_value = {'Item': sample_profile}

        from functions.api import app
        response = app.resolve(get_profile_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'profile' in body
        assert body['profile']['hardware_id'] == 'device-001'
        assert body['profile']['plant_type'] == 'pothos'
        assert body['profile']['typical_watering_interval_sec'] == 172800

    @patch('functions.api.device_profiles_table')
    def test_get_profile_not_found(self, mock_table, get_profile_event):
        """Test profile not found returns 404."""
        mock_table.get_item.return_value = {}

        from functions.api import app
        response = app.resolve(get_profile_event, Mock())

        assert response['statusCode'] == 404
        body = json.loads(response['body'])
        assert 'No profile found' in body['message']


class TestUpdateDeviceProfile:
    """Tests for PUT /devices/{hardware_id}/profile endpoint."""

    @patch('functions.api.device_profiles_table')
    def test_update_profile_new(self, mock_table, put_profile_event):
        """Test creating a new profile."""
        mock_table.get_item.return_value = {}
        mock_table.put_item.return_value = {}

        from functions.api import app
        response = app.resolve(put_profile_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'profile' in body
        assert body['profile']['hardware_id'] == 'device-001'
        assert body['profile']['plant_type'] == 'snake_plant'
        assert body['profile']['soil_type'] == 'coco_coir'
        assert body['profile']['pot_size_liters'] == 3.5
        assert body['profile']['expected_interval_sec'] == 600

    @patch('functions.api.device_profiles_table')
    def test_update_profile_existing_preserves_learned(self, mock_table, put_profile_event, sample_profile):
        """Test updating existing profile preserves learned patterns."""
        mock_table.get_item.return_value = {'Item': sample_profile}
        mock_table.put_item.return_value = {}

        from functions.api import app
        response = app.resolve(put_profile_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'profile' in body

        # Verify updated fields
        assert body['profile']['plant_type'] == 'snake_plant'
        assert body['profile']['soil_type'] == 'coco_coir'

        # Verify learned patterns are preserved
        assert body['profile']['typical_watering_interval_sec'] == 172800
        assert body['profile']['baseline_moisture_range'] == {"min": 30.0, "max": 60.0}
        assert body['profile']['last_watering_events'] == [1704070800000, 1704243600000]

    def test_update_profile_invalid_plant_type(self, put_profile_event):
        """Test invalid plant_type returns 400."""
        put_profile_event['body'] = json.dumps({
            "plant_type": "invalid_plant_type"
        })

        from functions.api import app
        response = app.resolve(put_profile_event, Mock())

        assert response['statusCode'] == 400
        body = json.loads(response['body'])
        assert 'not supported' in body['message']

    def test_update_profile_invalid_soil_type(self, put_profile_event):
        """Test invalid soil_type returns 400."""
        put_profile_event['body'] = json.dumps({
            "soil_type": "invalid_soil"
        })

        from functions.api import app
        response = app.resolve(put_profile_event, Mock())

        assert response['statusCode'] == 400
        body = json.loads(response['body'])
        assert 'soil_type must be one of' in body['message']

    def test_update_profile_negative_pot_size(self, put_profile_event):
        """Test negative pot_size returns 400."""
        put_profile_event['body'] = json.dumps({
            "pot_size": -5.0
        })

        from functions.api import app
        response = app.resolve(put_profile_event, Mock())

        assert response['statusCode'] == 400
        body = json.loads(response['body'])
        assert 'pot_size must be a positive number' in body['message']

    def test_update_profile_zero_pot_size(self, put_profile_event):
        """Test zero pot_size returns 400."""
        put_profile_event['body'] = json.dumps({
            "pot_size": 0
        })

        from functions.api import app
        response = app.resolve(put_profile_event, Mock())

        assert response['statusCode'] == 400
        body = json.loads(response['body'])
        assert 'pot_size must be a positive number' in body['message']

    def test_update_profile_invalid_expected_interval(self, put_profile_event):
        """Test invalid expected_interval_sec returns 400."""
        put_profile_event['body'] = json.dumps({
            "expected_interval_sec": -100
        })

        from functions.api import app
        response = app.resolve(put_profile_event, Mock())

        assert response['statusCode'] == 400
        body = json.loads(response['body'])
        assert 'expected_interval_sec must be a positive integer' in body['message']

    @patch('functions.api.device_profiles_table')
    def test_update_profile_partial_update(self, mock_table, put_profile_event, sample_profile):
        """Test partial profile update only changes specified fields."""
        mock_table.get_item.return_value = {'Item': sample_profile}
        mock_table.put_item.return_value = {}

        # Update only plant_type
        put_profile_event['body'] = json.dumps({
            "plant_type": "monstera"
        })

        from functions.api import app
        response = app.resolve(put_profile_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])

        # Verify only plant_type changed
        assert body['profile']['plant_type'] == 'monstera'
        assert body['profile']['soil_type'] == 'potting_mix'  # unchanged
        assert body['profile']['pot_size_liters'] == 5.0  # unchanged
        assert body['profile']['expected_interval_sec'] == 300  # unchanged
