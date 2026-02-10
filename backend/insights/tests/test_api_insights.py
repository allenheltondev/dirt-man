"""Unit tests for Insight Query API endpoints."""

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
def sample_insights():
    """Sample insights for testing."""
    return [
        {
            "hardware_id": "device-001",
            "timestamp_ms": 1704074400000,
            "summary": "Your plant is doing well with stable moisture levels.",
            "recommendations": [
                {
                    "action": "Continue current watering schedule",
                    "reason": "Moisture levels are optimal",
                    "urgency": "low"
                }
            ],
            "confidence": "high",
            "trend": "stable",
            "growth_stage_suggestion": "vegetative",
            "evidence": {
                "aggregate_refs": ["device-001#hourly#1704070800000"],
                "event_refs": [],
                "profile_snapshot": {"plant_type": "tomato"}
            },
            "llm_model": "gpt-4",
            "generation_duration_ms": 1500
        },
        {
            "hardware_id": "device-001",
            "timestamp_ms": 1704060000000,
            "summary": "Soil moisture is declining steadily.",
            "recommendations": [
                {
                    "action": "Water your plant soon",
                    "reason": "Moisture has dropped below 30%",
                    "urgency": "medium"
                }
            ],
            "confidence": "medium",
            "trend": "declining",
            "llm_model": "gpt-4",
            "generation_duration_ms": 1200
        }
    ]


@pytest.fixture
def latest_insight_event():
    """Sample API Gateway event for latest insight endpoint."""
    return {
        "resource": "/devices/{hardware_id}/insights/latest",
        "path": "/devices/device-001/insights/latest",
        "httpMethod": "GET",
        "headers": {},
        "queryStringParameters": None,
        "pathParameters": {"hardware_id": "device-001"},
        "body": None
    }


@pytest.fixture
def insights_list_event():
    """Sample API Gateway event for insights list endpoint."""
    return {
        "resource": "/devices/{hardware_id}/insights",
        "path": "/devices/device-001/insights",
        "httpMethod": "GET",
        "headers": {},
        "queryStringParameters": None,
        "pathParameters": {"hardware_id": "device-001"},
        "body": None
    }


class TestGetDeviceLatestInsight:
    """Tests for GET /devices/{hardware_id}/insights/latest endpoint."""

    @patch('functions.api.insights_table')
    def test_get_latest_insight_success(self, mock_table, latest_insight_event, sample_insights):
        """Test successful retrieval of latest insight."""
        mock_table.query.return_value = {'Items': [sample_insights[0]]}

        from functions.api import app
        response = app.resolve(latest_insight_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'insight' in body
        assert body['insight']['timestamp_ms'] == 1704074400000
        assert body['insight']['summary'] == "Your plant is doing well with stable moisture levels."
        assert body['insight']['confidence'] == "high"
        assert body['insight']['trend'] == "stable"

    @patch('functions.api.insights_table')
    def test_get_latest_insight_not_found(self, mock_table, latest_insight_event):
        """Test 404 when no insights exist for device."""
        mock_table.query.return_value = {'Items': []}

        from functions.api import app
        response = app.resolve(latest_insight_event, Mock())

        assert response['statusCode'] == 404
        body = json.loads(response['body'])
        assert 'No insights found' in body['message']

    @patch('functions.api.insights_table')
    def test_get_latest_insight_with_recommendations(self, mock_table, latest_insight_event, sample_insights):
        """Test that recommendations are included in response."""
        mock_table.query.return_value = {'Items': [sample_insights[0]]}

        from functions.api import app
        response = app.resolve(latest_insight_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'recommendations' in body['insight']
        assert len(body['insight']['recommendations']) == 1
        assert body['insight']['recommendations'][0]['action'] == "Continue current watering schedule"
        assert body['insight']['recommendations'][0]['urgency'] == "low"

    @patch('functions.api.insights_table')
    def test_get_latest_insight_with_evidence(self, mock_table, latest_insight_event, sample_insights):
        """Test that evidence is included in response."""
        mock_table.query.return_value = {'Items': [sample_insights[0]]}

        from functions.api import app
        response = app.resolve(latest_insight_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'evidence' in body['insight']
        assert 'aggregate_refs' in body['insight']['evidence']
        assert 'profile_snapshot' in body['insight']['evidence']


class TestGetDeviceInsights:
    """Tests for GET /devices/{hardware_id}/insights endpoint."""

    @patch('functions.api.insights_table')
    def test_get_insights_basic(self, mock_table, insights_list_event, sample_insights):
        """Test basic insights retrieval."""
        mock_table.query.return_value = {'Items': sample_insights}

        from functions.api import app
        response = app.resolve(insights_list_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'insights' in body
        assert len(body['insights']) == 2
        # Verify sorted by timestamp descending
        assert body['insights'][0]['timestamp_ms'] == 1704074400000
        assert body['insights'][1]['timestamp_ms'] == 1704060000000

    @patch('functions.api.insights_table')
    def test_get_insights_empty(self, mock_table, insights_list_event):
        """Test empty results return 200 with empty array."""
        mock_table.query.return_value = {'Items': []}

        from functions.api import app
        response = app.resolve(insights_list_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert body['insights'] == []

    @patch('functions.api.insights_table')
    def test_get_insights_with_time_range(self, mock_table, insights_list_event, sample_insights):
        """Test insights retrieval with time range filtering."""
        insights_list_event['queryStringParameters'] = {
            'start_time': '1704060000000',
            'end_time': '1704080000000'
        }
        mock_table.query.return_value = {'Items': sample_insights}

        from functions.api import app
        response = app.resolve(insights_list_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'insights' in body
        assert len(body['insights']) == 2

    @patch('functions.api.insights_table')
    def test_get_insights_with_start_time_only(self, mock_table, insights_list_event, sample_insights):
        """Test insights retrieval with only start_time filter."""
        insights_list_event['queryStringParameters'] = {
            'start_time': '1704060000000'
        }
        mock_table.query.return_value = {'Items': sample_insights}

        from functions.api import app
        response = app.resolve(insights_list_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'insights' in body
        assert len(body['insights']) == 2

    @patch('functions.api.insights_table')
    def test_get_insights_with_end_time_only(self, mock_table, insights_list_event, sample_insights):
        """Test insights retrieval with only end_time filter."""
        insights_list_event['queryStringParameters'] = {
            'end_time': '1704080000000'
        }
        mock_table.query.return_value = {'Items': sample_insights}

        from functions.api import app
        response = app.resolve(insights_list_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'insights' in body
        assert len(body['insights']) == 2

    @patch('functions.api.insights_table')
    def test_get_insights_with_pagination(self, mock_table, insights_list_event, sample_insights):
        """Test insights retrieval with pagination."""
        last_key = {
            'hardware_id': 'device-001',
            'timestamp_ms': 1704060000000
        }
        mock_table.query.return_value = {
            'Items': [sample_insights[0]],
            'LastEvaluatedKey': last_key
        }

        from functions.api import app
        response = app.resolve(insights_list_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'insights' in body
        assert 'next_token' in body

        # Verify next_token is a valid base64 string
        decoded = base64.b64decode(body['next_token'])
        assert decoded

    @patch('functions.api.insights_table')
    def test_get_insights_with_next_token(self, mock_table, insights_list_event, sample_insights):
        """Test insights retrieval using pagination token."""
        # Create a pagination token
        last_key = {
            'hardware_id': 'device-001',
            'timestamp_ms': 1704074400000
        }
        next_token = base64.b64encode(json.dumps(last_key).encode()).decode()

        insights_list_event['queryStringParameters'] = {
            'next_token': next_token
        }
        mock_table.query.return_value = {'Items': [sample_insights[1]]}

        from functions.api import app
        response = app.resolve(insights_list_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'insights' in body
        assert len(body['insights']) == 1

    @patch('functions.api.insights_table')
    def test_get_insights_with_limit(self, mock_table, insights_list_event, sample_insights):
        """Test insights retrieval with custom limit."""
        insights_list_event['queryStringParameters'] = {
            'limit': '1'
        }
        mock_table.query.return_value = {'Items': [sample_insights[0]]}

        from functions.api import app
        response = app.resolve(insights_list_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        assert 'insights' in body
        assert len(body['insights']) == 1

    @patch('functions.api.insights_table')
    def test_get_insights_limit_max_100(self, mock_table, insights_list_event, sample_insights):
        """Test that limit is capped at 100."""
        insights_list_event['queryStringParameters'] = {
            'limit': '200'
        }
        mock_table.query.return_value = {'Items': sample_insights}

        from functions.api import app
        response = app.resolve(insights_list_event, Mock())

        # Verify the query was called with limit=100 (max)
        call_args = mock_table.query.call_args
        assert call_args[1]['Limit'] == 100

    @patch('functions.api.insights_table')
    def test_get_insights_sorted_descending(self, mock_table, insights_list_event, sample_insights):
        """Test that insights are sorted by timestamp descending."""
        mock_table.query.return_value = {'Items': sample_insights}

        from functions.api import app
        response = app.resolve(insights_list_event, Mock())

        # Verify the query was called with ScanIndexForward=False
        call_args = mock_table.query.call_args
        assert call_args[1]['ScanIndexForward'] is False

    @patch('functions.api.insights_table')
    def test_get_insights_all_fields_present(self, mock_table, insights_list_event, sample_insights):
        """Test that all insight fields are present in response."""
        mock_table.query.return_value = {'Items': [sample_insights[0]]}

        from functions.api import app
        response = app.resolve(insights_list_event, Mock())

        assert response['statusCode'] == 200
        body = json.loads(response['body'])
        insight = body['insights'][0]

        # Verify all expected fields are present
        assert 'hardware_id' in insight
        assert 'timestamp_ms' in insight
        assert 'summary' in insight
        assert 'recommendations' in insight
        assert 'confidence' in insight
        assert 'trend' in insight
        assert 'growth_stage_suggestion' in insight
        assert 'evidence' in insight
        assert 'llm_model' in insight
        assert 'generation_duration_ms' in insight
