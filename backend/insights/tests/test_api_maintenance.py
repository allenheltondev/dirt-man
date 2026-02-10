"""Unit tests for Maintenance and Reprocessing API endpoints."""

import json
import os
import pytest
from unittest.mock import Mock, patch, MagicMock

# Set environment variables before importing the module
os.environ['PLANT_EVENTS_TABLE'] = 'test-events-table'
os.environ['PLANT_AGGREGATES_TABLE'] = 'test-aggregates-table'
os.environ['PLANT_INSIGHTS_TABLE'] = 'test-insights-table'
os.environ['PLANT_DEVICE_PROFILES_TABLE'] = 'test-profiles-table'
os.environ['PLANT_DEVICE_STATUS_TABLE'] = 'test-status-table'
os.environ['PLANT_ROLLUPS_TABLE'] = 'test-rollups-table'
os.environ['READINGS_TABLE'] = 'test-readings-table'
os.environ['INSIGHT_REQUESTS_TABLE'] = 'test-insight-requests-table'
os.environ['POWERTOOLS_SERVICE_NAME'] = 'test-service'


class TestRecomputeAggregates:
    """Tests for POST /admin/recompute-aggregates endpoint."""

    def test_missing_hardware_id(self):
        """Test missing hardware_id parameter."""
        event = {
            "resource": "/admin/recompute-aggregates",
            "path": "/admin/recompute-aggregates",
            "httpMethod": "POST",
            "headers": {},
            "queryStringParameters": None,
            "pathParameters": {},
            "body": json.dumps({
                "start_time": 1704070800000,
                "end_time": 1704074400000
            })
        }

        from functions.api import app
        response = app.resolve(event, Mock())

        assert response['statusCode'] == 400
        body = json.loads(response['body'])
        assert 'hardware_id is required' in body['message']

    def test_invalid_time_range(self):
        """Test invalid time range."""
        event = {
            "resource": "/admin/recompute-aggregates",
            "path": "/admin/recompute-aggregates",
            "httpMethod": "POST",
            "headers": {},
            "queryStringParameters": None,
            "pathParameters": {},
            "body": json.dumps({
                "hardware_id": "device-001",
                "start_time": 1704074400000,
                "end_time": 1704070800000
            })
        }

        from functions.api import app
        response = app.resolve(event, Mock())

        assert response['statusCode'] == 400
        body = json.loads(response['body'])
        assert 'start_time must be less than end_time' in body['message']


class TestRerunEventDetection:
    """Tests for POST /admin/rerun-event-detection endpoint."""

    def test_missing_hardware_id(self):
        """Test missing hardware_id parameter."""
        event = {
            "resource": "/admin/rerun-event-detection",
            "path": "/admin/rerun-event-detection",
            "httpMethod": "POST",
            "headers": {},
            "queryStringParameters": None,
            "pathParameters": {},
            "body": json.dumps({
                "start_time": 1704070800000,
                "end_time": 1704074400000
            })
        }

        from functions.api import app
        response = app.resolve(event, Mock())

        assert response['statusCode'] == 400
        body = json.loads(response['body'])
        assert 'hardware_id is required' in body['message']


class TestRegenerateInsight:
    """Tests for POST /admin/regenerate-insight endpoint."""

    def test_missing_hardware_id(self):
        """Test missing hardware_id parameter."""
        event = {
            "resource": "/admin/regenerate-insight",
            "path": "/admin/regenerate-insight",
            "httpMethod": "POST",
            "headers": {},
            "queryStringParameters": None,
            "pathParameters": {},
            "body": json.dumps({})
        }

        from functions.api import app
        response = app.resolve(event, Mock())

        assert response['statusCode'] == 400
        body = json.loads(response['body'])
        assert 'hardware_id is required' in body['message']
