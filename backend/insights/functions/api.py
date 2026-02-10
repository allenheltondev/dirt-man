"""
Control Plane API Lambda handlers

API endpoints for querying and managing Plant Insights data:
- Event queries
- Aggregate queries
- Insight queries
- Device profile management
- Dashboard queries
- Administrative queries
"""

import os
import base64
import json
from typing import Dict, Any, List, Optional
from aws_lambda_powertools import Logger
from aws_lambda_powertools.utilities.typing import LambdaContext
from aws_lambda_powertools.event_handler import APIGatewayRestResolver
import boto3
from boto3.dynamodb.conditions import Key

logger = Logger()
app = APIGatewayRestResolver()

# Initialize DynamoDB client
dynamodb = boto3.resource('dynamodb')
events_table = dynamodb.Table(os.environ['PLANT_EVENTS_TABLE'])
aggregates_table = dynamodb.Table(os.environ['PLANT_AGGREGATES_TABLE'])
insights_table = dynamodb.Table(os.environ['PLANT_INSIGHTS_TABLE'])
device_profiles_table = dynamodb.Table(os.environ['PLANT_DEVICE_PROFILES_TABLE'])
device_status_table = dynamodb.Table(os.environ['PLANT_DEVICE_STATUS_TABLE'])
rollups_table = dynamodb.Table(os.environ['PLANT_ROLLUPS_TABLE'])


def encode_pagination_token(last_key: Dict[str, Any]) -> str:
    """Encode DynamoDB LastEvaluatedKey as a base64 pagination token."""
    return base64.b64encode(json.dumps(last_key).encode()).decode()


def decode_pagination_token(token: str) -> Dict[str, Any]:
    """Decode base64 pagination token to DynamoDB ExclusiveStartKey."""
    try:
        return json.loads(base64.b64decode(token.encode()).decode())
    except Exception as e:
        logger.warning(f"Failed to decode pagination token: {e}")
        return {}


@app.get("/devices/<hardware_id>/events")
def get_device_events(hardware_id: str):
    """
    Query plant events for a specific device.

    Query parameters:
    - event_type: Filter by event type (optional)
    - start_time: Filter events starting after this timestamp in ms (optional)
    - end_time: Filter events starting before this timestamp in ms (optional)
    - limit: Maximum number of events to return (default: 50, max: 100)
    - next_token: Pagination token for retrieving next page (optional)

    Returns:
    - events: List of event objects
    - next_token: Token for next page (if more results available)
    """
    logger.info("Querying events for device", extra={"hardware_id": hardware_id})

    # Parse query parameters
    query_params = app.current_event.query_string_parameters or {}
    event_type_filter = query_params.get('event_type')
    start_time = query_params.get('start_time')
    end_time = query_params.get('end_time')
    limit = min(int(query_params.get('limit', 50)), 100)
    next_token = query_params.get('next_token')

    try:
        # Build query parameters
        query_kwargs = {
            'KeyConditionExpression': Key('hardware_id').eq(hardware_id),
            'ScanIndexForward': False,  # Sort by start_time descending
            'Limit': limit
        }

        # Add time range filter if provided
        if start_time or end_time:
            range_conditions = []
            if start_time:
                range_conditions.append(Key('start_time_ms').gte(int(start_time)))
            if end_time:
                range_conditions.append(Key('start_time_ms').lte(int(end_time)))

            # Combine with existing key condition
            if len(range_conditions) == 1:
                query_kwargs['KeyConditionExpression'] = query_kwargs['KeyConditionExpression'] & range_conditions[0]
            elif len(range_conditions) == 2:
                query_kwargs['KeyConditionExpression'] = query_kwargs['KeyConditionExpression'] & range_conditions[0] & range_conditions[1]

        # Add pagination token if provided
        if next_token:
            exclusive_start_key = decode_pagination_token(next_token)
            if exclusive_start_key:
                query_kwargs['ExclusiveStartKey'] = exclusive_start_key

        # Execute query
        response = events_table.query(**query_kwargs)

        # Filter by event_type if specified (post-query filter)
        events = response.get('Items', [])
        if event_type_filter:
            events = [e for e in events if e.get('event_type') == event_type_filter]

        # Build response
        result = {
            'events': events
        }

        # Add pagination token if more results available
        if 'LastEvaluatedKey' in response:
            result['next_token'] = encode_pagination_token(response['LastEvaluatedKey'])

        logger.info(f"Retrieved {len(events)} events for device {hardware_id}")
        return result

    except Exception as e:
        logger.error(f"Error querying events: {e}", exc_info=True)
        return {
            'statusCode': 500,
            'body': json.dumps({'error': 'Internal server error'})
        }


@app.get("/devices/<hardware_id>/aggregates")
def get_device_aggregates(hardware_id: str):
    """
    Query time-series aggregates for a specific device.

    Query parameters:
    - window_type: Filter by window type - hourly, daily, or weekly (REQUIRED)
    - start_time: Filter aggregates starting after this timestamp in ms (optional)
    - end_time: Filter aggregates starting before this timestamp in ms (optional)
    - limit: Maximum number of aggregates to return (default: 50, max: 100)
    - next_token: Pagination token for retrieving next page (optional)

    Returns:
    - aggregates: List of aggregate objects with derived stats per sensor
    - next_token: Token for next page (if more results available)
    """
    from aws_lambda_powertools.event_handler.exceptions import BadRequestError

    logger.info("Querying aggregates for device", extra={"hardware_id": hardware_id})

    # Parse query parameters
    query_params = app.current_event.query_string_parameters or {}
    window_type = query_params.get('window_type')
    start_time = query_params.get('start_time')
    end_time = query_params.get('end_time')
    limit = min(int(query_params.get('limit', 50)), 100)
    next_token = query_params.get('next_token')

    # Validate required window_type parameter
    if not window_type:
        raise BadRequestError('window_type parameter is required (hourly, daily, or weekly)')

    # Validate window_type value
    if window_type not in ['hourly', 'daily', 'weekly']:
        raise BadRequestError('window_type must be one of: hourly, daily, weekly')

    try:
        # Build composite partition key
        device_window = f"{hardware_id}#{window_type}"

        # Build query parameters
        query_kwargs = {
            'KeyConditionExpression': Key('device_window').eq(device_window),
            'ScanIndexForward': False,  # Sort by window_start_ms descending
            'Limit': limit
        }

        # Add time range filter if provided
        if start_time or end_time:
            range_conditions = []
            if start_time:
                range_conditions.append(Key('window_start_ms').gte(int(start_time)))
            if end_time:
                range_conditions.append(Key('window_start_ms').lte(int(end_time)))

            # Combine with existing key condition
            if len(range_conditions) == 1:
                query_kwargs['KeyConditionExpression'] = query_kwargs['KeyConditionExpression'] & range_conditions[0]
            elif len(range_conditions) == 2:
                query_kwargs['KeyConditionExpression'] = query_kwargs['KeyConditionExpression'] & range_conditions[0] & range_conditions[1]

        # Add pagination token if provided
        if next_token:
            exclusive_start_key = decode_pagination_token(next_token)
            if exclusive_start_key:
                query_kwargs['ExclusiveStartKey'] = exclusive_start_key

        # Execute query
        response = aggregates_table.query(**query_kwargs)

        # Process aggregates to return derived stats
        aggregates = []
        for item in response.get('Items', []):
            aggregate = {
                'hardware_id': item.get('hardware_id'),
                'window_type': item.get('window_type'),
                'window_start_ms': item.get('window_start_ms'),
                'window_end_ms': item.get('window_end_ms'),
                'is_complete': item.get('is_complete', False),
                'computed_at_ms': item.get('computed_at_ms')
            }

            # Extract derived stats for each sensor type
            for sensor_type in ['temperature', 'humidity', 'pressure', 'soil_moisture']:
                stats_key = f"{sensor_type}_stats"
                if stats_key in item:
                    stats = item[stats_key]
                    aggregate[stats_key] = {
                        'min': stats.get('min'),
                        'max': stats.get('max'),
                        'avg': stats.get('avg'),
                        'stddev': stats.get('stddev'),
                        'valid_count': stats.get('valid_count', 0),
                        'total_count': stats.get('total_count', 0)
                    }

            aggregates.append(aggregate)

        # Build response
        result = {
            'aggregates': aggregates
        }

        # Add pagination token if more results available
        if 'LastEvaluatedKey' in response:
            result['next_token'] = encode_pagination_token(response['LastEvaluatedKey'])

        logger.info(f"Retrieved {len(aggregates)} aggregates for device {hardware_id} with window_type {window_type}")
        return result

    except Exception as e:
        logger.error(f"Error querying aggregates: {e}", exc_info=True)
        return {
            'statusCode': 500,
            'body': json.dumps({'error': 'Internal server error'})
        }


@app.get("/devices/<hardware_id>/insights/latest")
def get_device_latest_insight(hardware_id: str):
    """
    Query the most recent insight for a specific device.

    Returns:
    - insight: The most recent insight object
    - 404 if no insights exist for the device

    Requirements: 13.1, 13.6
    """
    from aws_lambda_powertools.event_handler.exceptions import NotFoundError

    logger.info("Querying latest insight for device", extra={"hardware_id": hardware_id})

    try:
        # Query for the most recent insight (limit 1, descending order)
        response = insights_table.query(
            KeyConditionExpression=Key('hardware_id').eq(hardware_id),
            ScanIndexForward=False,  # Sort by timestamp_ms descending
            Limit=1
        )

        items = response.get('Items', [])

        if not items:
            logger.info(f"No insights found for device {hardware_id}")
            raise NotFoundError(f"No insights found for device {hardware_id}")

        insight = items[0]
        logger.info(f"Retrieved latest insight for device {hardware_id}", extra={
            "timestamp_ms": insight.get('timestamp_ms')
        })

        return {"insight": insight}

    except NotFoundError:
        raise
    except Exception as e:
        logger.error(f"Error querying latest insight: {e}", exc_info=True)
        return {
            'statusCode': 500,
            'body': json.dumps({'error': 'Internal server error'})
        }


@app.get("/devices/<hardware_id>/insights")
def get_device_insights(hardware_id: str):
    """
    Query historical insights for a specific device.

    Query parameters:
    - start_time: Filter insights after this timestamp in ms (optional)
    - end_time: Filter insights before this timestamp in ms (optional)
    - limit: Maximum number of insights to return (default: 50, max: 100)
    - next_token: Pagination token for retrieving next page (optional)

    Returns:
    - insights: List of insight objects sorted by timestamp descending
    - next_token: Token for next page (if more results available)
    - Empty array with HTTP 200 if no insights exist

    Requirements: 13.2-13.5, 13.7
    """
    logger.info("Querying insights for device", extra={"hardware_id": hardware_id})

    # Parse query parameters
    query_params = app.current_event.query_string_parameters or {}
    start_time = query_params.get('start_time')
    end_time = query_params.get('end_time')
    limit = min(int(query_params.get('limit', 50)), 100)
    next_token = query_params.get('next_token')

    try:
        # Build query parameters
        query_kwargs = {
            'KeyConditionExpression': Key('hardware_id').eq(hardware_id),
            'ScanIndexForward': False,  # Sort by timestamp_ms descending
            'Limit': limit
        }

        # Add time range filter if provided
        if start_time or end_time:
            range_conditions = []
            if start_time:
                range_conditions.append(Key('timestamp_ms').gte(int(start_time)))
            if end_time:
                range_conditions.append(Key('timestamp_ms').lte(int(end_time)))

            # Combine with existing key condition
            if len(range_conditions) == 1:
                query_kwargs['KeyConditionExpression'] = query_kwargs['KeyConditionExpression'] & range_conditions[0]
            elif len(range_conditions) == 2:
                query_kwargs['KeyConditionExpression'] = query_kwargs['KeyConditionExpression'] & range_conditions[0] & range_conditions[1]

        # Add pagination token if provided
        if next_token:
            exclusive_start_key = decode_pagination_token(next_token)
            if exclusive_start_key:
                query_kwargs['ExclusiveStartKey'] = exclusive_start_key

        # Execute query
        response = insights_table.query(**query_kwargs)

        insights = response.get('Items', [])

        # Build response
        result = {
            'insights': insights
        }

        # Add pagination token if more results available
        if 'LastEvaluatedKey' in response:
            result['next_token'] = encode_pagination_token(response['LastEvaluatedKey'])

        logger.info(f"Retrieved {len(insights)} insights for device {hardware_id}")
        return result

    except Exception as e:
        logger.error(f"Error querying insights: {e}", exc_info=True)
        return {
            'statusCode': 500,
            'body': json.dumps({'error': 'Internal server error'})
        }


@app.post("/devices/<hardware_id>/insights/generate")
def trigger_manual_insight_generation(hardware_id: str):
    """
    Manually trigger insight generation for a device.

    Validates device has at least 6 hours of data, then generates
    and returns an insight synchronously.

    Returns:
    - insight: The generated insight object
    - 400 if insufficient data (< 6 hours)
    - 500 if generation fails
    - 504 if generation times out (> 30 seconds)

    Requirements: 14.1-14.5
    """
    from aws_lambda_powertools.event_handler.exceptions import BadRequestError
    import time
    import sys
    import os

    # Add shared modules to path
    shared_path = os.path.join(os.path.dirname(__file__), '..', 'shared')
    if shared_path not in sys.path:
        sys.path.append(shared_path)

    logger.info("Manual insight generation requested", extra={"hardware_id": hardware_id})

    try:
        # Import insight generation functions from the same module directory
        # insight_generator.py is in the same directory as api.py
        import insight_generator
        from models import Insight, Recommendation, ConfidenceLevel, TrendClassification
        from device_status import update_device_status_field, record_device_error

        start_time = time.time()
        timeout_seconds = 30

        # Fetch aggregates for last 24 hours to check data sufficiency
        aggregates = insight_generator.fetch_aggregates_for_insight(hardware_id, hours=24)

        # Count valid hours (hours with at least some valid data)
        valid_hours = sum(1 for agg in aggregates if agg.temperature_stats.valid_count > 0)

        logger.info(f"Device has {valid_hours} hours of valid data", extra={
            "hardware_id": hardware_id,
            "valid_hours": valid_hours
        })

        # Validate minimum data requirement (6 hours)
        if valid_hours < 6:
            logger.warning(f"Insufficient data for insight generation: {valid_hours} hours", extra={
                "hardware_id": hardware_id,
                "valid_hours": valid_hours
            })
            raise BadRequestError(
                f"Insufficient data for insight generation. Device has only {valid_hours} hours of valid data. "
                f"At least 6 hours of data is required."
            )

        # Check timeout before proceeding
        if time.time() - start_time > timeout_seconds:
            logger.error("Insight generation timed out during data validation")
            return {
                'statusCode': 504,
                'body': json.dumps({'error': 'Insight generation timed out'})
            }

        # Fetch additional data for insight generation
        events = insight_generator.fetch_recent_events(hardware_id, hours=24)
        profile = insight_generator.fetch_device_profile(hardware_id)
        previous_week_aggregates = insight_generator.fetch_aggregates_for_insight(hardware_id, hours=24 * 7)

        # Check timeout before LLM call
        if time.time() - start_time > timeout_seconds:
            logger.error("Insight generation timed out during data fetching")
            return {
                'statusCode': 504,
                'body': json.dumps({'error': 'Insight generation timed out'})
            }

        # Build prompt
        prompt = insight_generator.build_insight_prompt(aggregates, events, profile, previous_week_aggregates)

        # Call LLM with remaining time budget
        remaining_time = timeout_seconds - (time.time() - start_time)
        if remaining_time < 5:
            logger.error("Insufficient time remaining for LLM call")
            return {
                'statusCode': 504,
                'body': json.dumps({'error': 'Insight generation timed out'})
            }

        llm_response = insight_generator.call_llm_api(prompt)

        if not llm_response:
            logger.error(f"LLM API call failed for {hardware_id}")

            # Record error in device status
            record_device_error(
                hardware_id=hardware_id,
                error_code='INSIGHT_GENERATION_FAILED',
                error_message='LLM API call failed'
            )

            return {
                'statusCode': 500,
                'body': json.dumps({'error': 'Insight generation failed. LLM API call unsuccessful.'})
            }

        # Validate and sanitize response
        sanitized = insight_generator.validate_and_sanitize_insight(llm_response)

        # Override confidence if insufficient data (< 12 hours)
        if valid_hours < 12:
            sanitized['confidence'] = 'low'
            sanitized['summary'] = f"Limited data available ({valid_hours} hours of valid readings). " + sanitized['summary']

        # Create Insight object
        now_ms = int(time.time() * 1000)
        generation_duration_ms = int((time.time() - start_time) * 1000)

        recommendations = [
            Recommendation(
                action=rec['action'],
                reason=rec['reason'],
                urgency=rec['urgency']
            )
            for rec in sanitized['recommendations']
        ]

        insight = Insight(
            hardware_id=hardware_id,
            timestamp_ms=now_ms,
            summary=sanitized['summary'],
            recommendations=recommendations,
            confidence=ConfidenceLevel(sanitized['confidence']),
            trend=TrendClassification(sanitized['trend']),
            growth_stage_suggestion=sanitized.get('growth_stage_suggestion'),
            evidence={
                'aggregate_count': len(aggregates),
                'event_count': len(events),
                'profile_snapshot': profile.to_dynamodb_item()
            },
            llm_model=os.environ.get('LLM_MODEL', 'gpt-3.5-turbo'),
            generation_duration_ms=generation_duration_ms
        )

        # Store insight
        insights_table.put_item(Item=insight.to_dynamodb_item())

        # Update device status
        update_device_status_field(
            hardware_id=hardware_id,
            field_name='last_insight_generated_at_ms',
            field_value=now_ms
        )

        logger.info(f"Manual insight generated successfully for {hardware_id}", extra={
            "hardware_id": hardware_id,
            "confidence": insight.confidence.value,
            "duration_ms": generation_duration_ms
        })

        # Return the generated insight
        return {
            "insight": insight.to_dynamodb_item()
        }

    except BadRequestError:
        raise
    except Exception as e:
        logger.error(f"Error generating manual insight: {e}", exc_info=True)

        # Record error in device status
        try:
            from device_status import record_device_error
            record_device_error(
                hardware_id=hardware_id,
                error_code='INSIGHT_GENERATION_FAILED',
                error_message=str(e)
            )
        except:
            pass

        return {
            'statusCode': 500,
            'body': json.dumps({'error': f'Insight generation failed: {str(e)}'})
        }


@app.get("/aggregates")
def get_aggregates():
    """Query time-series aggregates."""
    logger.info("Querying aggregates")
    # TODO: Implement aggregate query logic
    return {"aggregates": []}


@app.get("/devices/<hardware_id>/profile")
def get_device_profile(hardware_id: str):
    """
    Get device profile including learned fields.

    Returns:
    - profile: Device profile object with all fields including learned patterns
    - 404 if profile does not exist

    Requirements: 15.2
    """
    from aws_lambda_powertools.event_handler.exceptions import NotFoundError
    import sys
    import os

    # Add shared modules to path
    shared_path = os.path.join(os.path.dirname(__file__), '..', 'shared')
    if shared_path not in sys.path:
        sys.path.append(shared_path)

    from models import DeviceProfile

    logger.info("Getting device profile", extra={"hardware_id": hardware_id})

    try:
        # Query device profile from DynamoDB
        response = device_profiles_table.get_item(
            Key={'hardware_id': hardware_id}
        )

        if 'Item' not in response:
            logger.info(f"No profile found for device {hardware_id}")
            raise NotFoundError(f"No profile found for device {hardware_id}")

        profile_item = response['Item']

        # Convert to DeviceProfile model for validation
        profile = DeviceProfile.from_dynamodb_item(profile_item)

        logger.info(f"Retrieved profile for device {hardware_id}", extra={
            "plant_type": profile.plant_type,
            "has_learned_patterns": profile.typical_watering_interval_sec is not None
        })

        return {"profile": profile.to_dynamodb_item()}

    except NotFoundError:
        raise
    except Exception as e:
        logger.error(f"Error retrieving device profile: {e}", exc_info=True)
        return {
            'statusCode': 500,
            'body': json.dumps({'error': 'Internal server error'})
        }


@app.put("/devices/<hardware_id>/profile")
def update_device_profile(hardware_id: str):
    """
    Update device profile configuration.

    Accepts:
    - plant_type: Plant species (validated against supported list)
    - soil_type: Soil type (potting_mix, coco_coir, peat, soil, hydroponic)
    - pot_size: Pot size in liters (positive number)
    - expected_interval_sec: Expected time between readings in seconds

    Preserves learned patterns when updating.

    Returns:
    - profile: Updated device profile object
    - 400 if validation fails

    Requirements: 15.1, 15.3-15.6
    """
    from aws_lambda_powertools.event_handler.exceptions import BadRequestError
    import sys
    import os
    import time

    # Add shared modules to path
    shared_path = os.path.join(os.path.dirname(__file__), '..', 'shared')
    if shared_path not in sys.path:
        sys.path.append(shared_path)

    from models import DeviceProfile

    logger.info("Updating device profile", extra={"hardware_id": hardware_id})

    # Supported plant types (common houseplants and garden plants)
    SUPPORTED_PLANT_TYPES = [
        "pothos", "snake_plant", "spider_plant", "peace_lily", "monstera",
        "philodendron", "ficus", "rubber_plant", "aloe_vera", "jade_plant",
        "tomato", "basil", "mint", "rosemary", "lavender", "pepper",
        "lettuce", "spinach", "strawberry", "cucumber", "herb_generic",
        "succulent_generic", "cactus_generic", "houseplant_generic"
    ]

    # Supported soil types
    SUPPORTED_SOIL_TYPES = ["potting_mix", "coco_coir", "peat", "soil", "hydroponic"]

    try:
        # Parse request body
        body = app.current_event.json_body

        # Validate plant_type if provided
        plant_type = body.get('plant_type')
        if plant_type is not None:
            if not isinstance(plant_type, str):
                raise BadRequestError("plant_type must be a string")
            if plant_type not in SUPPORTED_PLANT_TYPES:
                raise BadRequestError(
                    f"plant_type '{plant_type}' is not supported. "
                    f"Supported types: {', '.join(SUPPORTED_PLANT_TYPES)}"
                )

        # Validate soil_type if provided
        soil_type = body.get('soil_type')
        if soil_type is not None:
            if not isinstance(soil_type, str):
                raise BadRequestError("soil_type must be a string")
            if soil_type not in SUPPORTED_SOIL_TYPES:
                raise BadRequestError(
                    f"soil_type must be one of: {', '.join(SUPPORTED_SOIL_TYPES)}"
                )

        # Validate pot_size if provided
        pot_size = body.get('pot_size')
        if pot_size is not None:
            try:
                pot_size_float = float(pot_size)
                if pot_size_float <= 0:
                    raise BadRequestError("pot_size must be a positive number")
                pot_size = pot_size_float
            except (ValueError, TypeError):
                raise BadRequestError("pot_size must be a positive number")

        # Validate expected_interval_sec if provided
        expected_interval_sec = body.get('expected_interval_sec')
        if expected_interval_sec is not None:
            try:
                expected_interval_sec = int(expected_interval_sec)
                if expected_interval_sec <= 0:
                    raise BadRequestError("expected_interval_sec must be a positive integer")
            except (ValueError, TypeError):
                raise BadRequestError("expected_interval_sec must be a positive integer")

        # Fetch existing profile to preserve learned patterns
        response = device_profiles_table.get_item(
            Key={'hardware_id': hardware_id}
        )

        if 'Item' in response:
            # Update existing profile
            existing_profile = DeviceProfile.from_dynamodb_item(response['Item'])
            logger.info(f"Updating existing profile for device {hardware_id}")
        else:
            # Create new profile
            existing_profile = DeviceProfile(hardware_id=hardware_id)
            logger.info(f"Creating new profile for device {hardware_id}")

        # Update only the provided fields, preserving learned patterns
        if plant_type is not None:
            existing_profile.plant_type = plant_type
        if soil_type is not None:
            existing_profile.soil_type = soil_type
        if pot_size is not None:
            existing_profile.pot_size_liters = pot_size
        if expected_interval_sec is not None:
            existing_profile.expected_interval_sec = expected_interval_sec

        # Update timestamp
        existing_profile.updated_at_ms = int(time.time() * 1000)

        # Save to DynamoDB
        device_profiles_table.put_item(Item=existing_profile.to_dynamodb_item())

        logger.info(f"Profile updated successfully for device {hardware_id}", extra={
            "plant_type": existing_profile.plant_type,
            "soil_type": existing_profile.soil_type,
            "pot_size_liters": existing_profile.pot_size_liters,
            "preserved_learned_patterns": existing_profile.typical_watering_interval_sec is not None
        })

        return {"profile": existing_profile.to_dynamodb_item()}

    except BadRequestError:
        raise
    except Exception as e:
        logger.error(f"Error updating device profile: {e}", exc_info=True)
        return {
            'statusCode': 500,
            'body': json.dumps({'error': 'Internal server error'})
        }


@app.get("/dashboard/rollups")
def get_dashboard_rollups():
    """
    Query operational metrics rollups for dashboard.

    Query parameters:
    - bucket_type: Filter by bucket type - minute or hour (REQUIRED)
    - start_time: Filter rollups starting after this timestamp in ms (REQUIRED)
    - end_time: Filter rollups starting before this timestamp in ms (REQUIRED)
    - metric_name: Filter by metric name prefix (optional)
    - event_type: Filter by event_type dimension (optional)
    - window_type: Filter by window_type dimension (optional)
    - status: Filter by status dimension (optional)
    - limit: Maximum number of rollups to return (default: 100, max: 500)
    - next_token: Pagination token for retrieving next page (optional)

    Returns:
    - rollups: List of rollup objects
    - next_token: Token for next page (if more results available)

    Requirements: 24.1-24.2, 24.6
    """
    from aws_lambda_powertools.event_handler.exceptions import BadRequestError

    logger.info("Querying dashboard rollups")

    # Parse query parameters
    query_params = app.current_event.query_string_parameters or {}
    bucket_type = query_params.get('bucket_type')
    start_time = query_params.get('start_time')
    end_time = query_params.get('end_time')
    metric_name_prefix = query_params.get('metric_name')
    event_type_filter = query_params.get('event_type')
    window_type_filter = query_params.get('window_type')
    status_filter = query_params.get('status')
    limit = min(int(query_params.get('limit', 100)), 500)
    next_token = query_params.get('next_token')

    # Validate required parameters
    if not bucket_type:
        raise BadRequestError('bucket_type parameter is required (minute or hour)')
    if bucket_type not in ['minute', 'hour']:
        raise BadRequestError('bucket_type must be one of: minute, hour')
    if not start_time:
        raise BadRequestError('start_time parameter is required (timestamp in ms)')
    if not end_time:
        raise BadRequestError('end_time parameter is required (timestamp in ms)')

    try:
        start_time_ms = int(start_time)
        end_time_ms = int(end_time)
    except (ValueError, TypeError):
        raise BadRequestError('start_time and end_time must be valid timestamps in milliseconds')

    if start_time_ms >= end_time_ms:
        raise BadRequestError('start_time must be less than end_time')

    try:
        # Query rollups by bucket_type and time range
        # We need to query multiple bucket_keys since each bucket_start_ms creates a separate partition
        # For efficiency, we'll query in chunks and aggregate results

        all_rollups = []

        # Generate bucket_keys for the time range
        # Bucket alignment: minute buckets are 60000ms, hour buckets are 3600000ms
        bucket_interval_ms = 60000 if bucket_type == 'minute' else 3600000

        current_bucket_start = (start_time_ms // bucket_interval_ms) * bucket_interval_ms

        while current_bucket_start <= end_time_ms and len(all_rollups) < limit:
            bucket_key = f"{bucket_type}#{current_bucket_start}"

            # Build query parameters
            query_kwargs = {
                'KeyConditionExpression': Key('bucket_key').eq(bucket_key),
                'Limit': limit - len(all_rollups)
            }

            # Add metric_name prefix filter if provided
            if metric_name_prefix:
                query_kwargs['KeyConditionExpression'] = query_kwargs['KeyConditionExpression'] & Key('metric_key').begins_with(metric_name_prefix)

            # Execute query
            response = rollups_table.query(**query_kwargs)

            items = response.get('Items', [])

            # Apply dimension filters (post-query filtering)
            if event_type_filter or window_type_filter or status_filter:
                filtered_items = []
                for item in items:
                    dimensions = item.get('dimensions', {})

                    # Check dimension filters
                    if event_type_filter and dimensions.get('event_type') != event_type_filter:
                        continue
                    if window_type_filter and dimensions.get('window_type') != window_type_filter:
                        continue
                    if status_filter and dimensions.get('status') != status_filter:
                        continue

                    filtered_items.append(item)

                items = filtered_items

            all_rollups.extend(items)

            # Move to next bucket
            current_bucket_start += bucket_interval_ms

            # Stop if we've collected enough rollups
            if len(all_rollups) >= limit:
                break

        # Trim to exact limit
        rollups = all_rollups[:limit]

        # Build response
        result = {
            'rollups': rollups
        }

        # For simplicity, we don't implement pagination tokens for cross-bucket queries
        # In production, this would require more sophisticated cursor management

        logger.info(f"Retrieved {len(rollups)} rollups for bucket_type {bucket_type}")
        return result

    except BadRequestError:
        raise
    except Exception as e:
        logger.error(f"Error querying rollups: {e}", exc_info=True)
        return {
            'statusCode': 500,
            'body': json.dumps({'error': 'Internal server error'})
        }


@app.get("/dashboard/devices/status")
def get_dashboard_devices_status():
    """
    Query device status list for dashboard.

    Query parameters:
    - health_category: Filter by health category - healthy, stale, missing, or failing (optional)
    - limit: Maximum number of devices to return (default: 50, max: 100)
    - next_token: Pagination token for retrieving next page (optional)

    Returns:
    - devices: List of device status objects sorted by last_seen_ingest_time descending
    - next_token: Token for next page (if more results available)

    Requirements: 24.3-24.4, 24.6-24.7
    """
    from aws_lambda_powertools.event_handler.exceptions import BadRequestError
    import time

    logger.info("Querying dashboard device status list")

    # Parse query parameters
    query_params = app.current_event.query_string_parameters or {}
    health_category_filter = query_params.get('health_category')
    limit = min(int(query_params.get('limit', 50)), 100)
    next_token = query_params.get('next_token')

    # Validate health_category if provided
    if health_category_filter and health_category_filter not in ['healthy', 'stale', 'missing', 'failing']:
        raise BadRequestError('health_category must be one of: healthy, stale, missing, failing')

    try:
        if health_category_filter:
            # Query using HealthIndex GSI
            query_kwargs = {
                'IndexName': 'HealthIndex',
                'KeyConditionExpression': Key('health_category').eq(health_category_filter),
                'ScanIndexForward': False,  # Sort by last_seen_ingest_time descending
                'Limit': limit
            }

            # Add pagination token if provided
            if next_token:
                exclusive_start_key = decode_pagination_token(next_token)
                if exclusive_start_key:
                    query_kwargs['ExclusiveStartKey'] = exclusive_start_key

            # Execute query
            response = device_status_table.query(**query_kwargs)
        else:
            # No filter - scan for recent active devices
            # This is not ideal but acceptable for small datasets
            # In production, consider maintaining a "recent devices" index
            scan_kwargs = {
                'Limit': limit
            }

            # Add pagination token if provided
            if next_token:
                exclusive_start_key = decode_pagination_token(next_token)
                if exclusive_start_key:
                    scan_kwargs['ExclusiveStartKey'] = exclusive_start_key

            # Execute scan
            response = device_status_table.scan(**scan_kwargs)

        devices = response.get('Items', [])

        # Derive health_category for each device if not stored
        now_ms = int(time.time() * 1000)
        for device in devices:
            if 'health_category' not in device:
                device['health_category'] = derive_health_category(
                    device.get('last_seen_ingest_time_ms'),
                    device.get('last_error_at_ms'),
                    now_ms
                )

        # Sort by last_seen_ingest_time descending if not using GSI
        if not health_category_filter:
            devices.sort(
                key=lambda d: d.get('last_seen_ingest_time_ms', 0),
                reverse=True
            )

        # Build response
        result = {
            'devices': devices
        }

        # Add pagination token if more results available
        if 'LastEvaluatedKey' in response:
            result['next_token'] = encode_pagination_token(response['LastEvaluatedKey'])

        logger.info(f"Retrieved {len(devices)} device status records")
        return result

    except BadRequestError:
        raise
    except Exception as e:
        logger.error(f"Error querying device status list: {e}", exc_info=True)
        return {
            'statusCode': 500,
            'body': json.dumps({'error': 'Internal server error'})
        }


@app.get("/dashboard/devices/<hardware_id>/status")
def get_dashboard_device_status(hardware_id: str):
    """
    Get device status for a specific device.

    Returns:
    - device: Device status object with derived health_category
    - 404 if device status does not exist

    Requirements: 24.5
    """
    from aws_lambda_powertools.event_handler.exceptions import NotFoundError
    import time

    logger.info("Getting device status", extra={"hardware_id": hardware_id})

    try:
        # Query device status from DynamoDB
        response = device_status_table.get_item(
            Key={'hardware_id': hardware_id}
        )

        if 'Item' not in response:
            logger.info(f"No status found for device {hardware_id}")
            raise NotFoundError(f"No status found for device {hardware_id}")

        device = response['Item']

        # Derive health_category if not stored
        now_ms = int(time.time() * 1000)
        if 'health_category' not in device:
            device['health_category'] = derive_health_category(
                device.get('last_seen_ingest_time_ms'),
                device.get('last_error_at_ms'),
                now_ms
            )

        logger.info(f"Retrieved status for device {hardware_id}", extra={
            "health_category": device.get('health_category'),
            "last_seen_ingest_time_ms": device.get('last_seen_ingest_time_ms')
        })

        return {"device": device}

    except NotFoundError:
        raise
    except Exception as e:
        logger.error(f"Error retrieving device status: {e}", exc_info=True)
        return {
            'statusCode': 500,
            'body': json.dumps({'error': 'Internal server error'})
        }


def derive_health_category(last_seen_ingest_time_ms: Optional[int], last_error_at_ms: Optional[int], now_ms: int) -> str:
    """
    Derive health category from device status fields.

    Health category logic:
    - failing: last_error_at_ms within 24 hours
    - healthy: last_seen_ingest_time_ms within 2 hours
    - stale: last_seen_ingest_time_ms between 2 and 6 hours
    - missing: last_seen_ingest_time_ms more than 6 hours ago or null

    Requirements: 23.20-23.24
    """
    # Check for recent errors first
    if last_error_at_ms and (now_ms - last_error_at_ms) < 24 * 3600 * 1000:
        return 'failing'

    # Check last seen time
    if not last_seen_ingest_time_ms:
        return 'missing'

    time_since_last_seen_ms = now_ms - last_seen_ingest_time_ms

    if time_since_last_seen_ms <= 2 * 3600 * 1000:  # 2 hours
        return 'healthy'
    elif time_since_last_seen_ms <= 6 * 3600 * 1000:  # 6 hours
        return 'stale'
    else:
        return 'missing'


@app.get("/admin/events")
def get_cross_device_events():
    """
    Query events across all devices by event type.

    Query parameters:
    - event_type: Filter by event type (REQUIRED)
    - start_time: Filter events starting after this timestamp in ms (optional)
    - end_time: Filter events starting before this timestamp in ms (optional)
    - limit: Maximum number of events to return (default: 50, max: 100)
    - next_token: Pagination token for retrieving next page (optional)

    Returns:
    - events: List of event objects across all devices
    - next_token: Token for next page (if more results available)

    Requirements: 16.1, 16.4-16.5
    """
    from aws_lambda_powertools.event_handler.exceptions import BadRequestError

    logger.info("Querying cross-device events")

    # Parse query parameters
    query_params = app.current_event.query_string_parameters or {}
    event_type = query_params.get('event_type')
    start_time = query_params.get('start_time')
    end_time = query_params.get('end_time')
    limit = min(int(query_params.get('limit', 50)), 100)
    next_token = query_params.get('next_token')

    # Validate required event_type parameter
    if not event_type:
        raise BadRequestError('event_type parameter is required')

    # Validate event_type value
    valid_event_types = [
        'Watering_Event', 'Drying_Cycle', 'Temperature_Stress',
        'Humidity_Anomaly', 'Environmental_Change'
    ]
    if event_type not in valid_event_types:
        raise BadRequestError(f'event_type must be one of: {", ".join(valid_event_types)}')

    try:
        # Build query parameters for EventTypeIndex GSI
        query_kwargs = {
            'IndexName': 'EventTypeIndex',
            'KeyConditionExpression': Key('event_type').eq(event_type),
            'ScanIndexForward': False,  # Sort by start_time descending
            'Limit': limit
        }

        # Add time range filter if provided
        if start_time or end_time:
            range_conditions = []
            if start_time:
                range_conditions.append(Key('start_time_ms').gte(int(start_time)))
            if end_time:
                range_conditions.append(Key('start_time_ms').lte(int(end_time)))

            # Combine with existing key condition
            if len(range_conditions) == 1:
                query_kwargs['KeyConditionExpression'] = query_kwargs['KeyConditionExpression'] & range_conditions[0]
            elif len(range_conditions) == 2:
                query_kwargs['KeyConditionExpression'] = query_kwargs['KeyConditionExpression'] & range_conditions[0] & range_conditions[1]

        # Add pagination token if provided
        if next_token:
            exclusive_start_key = decode_pagination_token(next_token)
            if exclusive_start_key:
                query_kwargs['ExclusiveStartKey'] = exclusive_start_key

        # Execute query
        response = events_table.query(**query_kwargs)

        events = response.get('Items', [])

        # Build response
        result = {
            'events': events
        }

        # Add pagination token if more results available
        if 'LastEvaluatedKey' in response:
            result['next_token'] = encode_pagination_token(response['LastEvaluatedKey'])

        logger.info(f"Retrieved {len(events)} cross-device events for event_type {event_type}")
        return result

    except BadRequestError:
        raise
    except Exception as e:
        logger.error(f"Error querying cross-device events: {e}", exc_info=True)
        return {
            'statusCode': 500,
            'body': json.dumps({'error': 'Internal server error'})
        }


@app.get("/admin/devices/stale")
def get_stale_devices():
    """
    Query devices that have not reported recently (stale or missing).

    Query parameters:
    - health_category: Filter by health category - stale or missing (default: stale)
    - limit: Maximum number of devices to return (default: 50, max: 100)
    - next_token: Pagination token for retrieving next page (optional)

    Returns:
    - devices: List of device status objects for stale/missing devices
    - next_token: Token for next page (if more results available)

    Requirements: 16.2
    """
    from aws_lambda_powertools.event_handler.exceptions import BadRequestError
    import time

    logger.info("Querying stale devices")

    # Parse query parameters
    query_params = app.current_event.query_string_parameters or {}
    health_category = query_params.get('health_category', 'stale')
    limit = min(int(query_params.get('limit', 50)), 100)
    next_token = query_params.get('next_token')

    # Validate health_category
    if health_category not in ['stale', 'missing']:
        raise BadRequestError('health_category must be one of: stale, missing')

    try:
        # Query using HealthIndex GSI
        query_kwargs = {
            'IndexName': 'HealthIndex',
            'KeyConditionExpression': Key('health_category').eq(health_category),
            'ScanIndexForward': False,  # Sort by last_seen_ingest_time descending
            'Limit': limit
        }

        # Add pagination token if provided
        if next_token:
            exclusive_start_key = decode_pagination_token(next_token)
            if exclusive_start_key:
                query_kwargs['ExclusiveStartKey'] = exclusive_start_key

        # Execute query
        response = device_status_table.query(**query_kwargs)

        devices = response.get('Items', [])

        # Build response
        result = {
            'devices': devices,
            'health_category': health_category
        }

        # Add pagination token if more results available
        if 'LastEvaluatedKey' in response:
            result['next_token'] = encode_pagination_token(response['LastEvaluatedKey'])

        logger.info(f"Retrieved {len(devices)} {health_category} devices")
        return result

    except BadRequestError:
        raise
    except Exception as e:
        logger.error(f"Error querying stale devices: {e}", exc_info=True)
        return {
            'statusCode': 500,
            'body': json.dumps({'error': 'Internal server error'})
        }


@app.get("/admin/devices/low-moisture")
def get_low_moisture_devices():
    """
    Query devices with low moisture and no recent watering.

    Identifies devices where:
    - Latest soil moisture is below 30%
    - No watering event detected in the last 48 hours

    Query parameters:
    - moisture_threshold: Moisture threshold percentage (default: 30)
    - hours_without_watering: Hours without watering to consider (default: 48)
    - limit: Maximum number of devices to return (default: 50, max: 100)

    Returns:
    - devices: List of devices with low moisture and no recent watering
    - Each device includes: hardware_id, latest_moisture, last_watering_time_ms, hours_since_watering

    Requirements: 16.3, 10.7
    """
    from aws_lambda_powertools.event_handler.exceptions import BadRequestError
    import time

    logger.info("Querying devices with low moisture and no recent watering")

    # Parse query parameters
    query_params = app.current_event.query_string_parameters or {}
    moisture_threshold = float(query_params.get('moisture_threshold', 30))
    hours_without_watering = int(query_params.get('hours_without_watering', 48))
    limit = min(int(query_params.get('limit', 50)), 100)

    # Validate parameters
    if moisture_threshold < 0 or moisture_threshold > 100:
        raise BadRequestError('moisture_threshold must be between 0 and 100')
    if hours_without_watering < 0:
        raise BadRequestError('hours_without_watering must be positive')

    try:
        now_ms = int(time.time() * 1000)
        cutoff_time_ms = now_ms - (hours_without_watering * 3600 * 1000)

        # Get all active devices from device status table
        # We'll scan for devices with recent activity (within last 24 hours)
        recent_cutoff_ms = now_ms - (24 * 3600 * 1000)

        scan_response = device_status_table.scan(
            FilterExpression='last_seen_ingest_time_ms > :cutoff',
            ExpressionAttributeValues={':cutoff': recent_cutoff_ms}
        )

        active_devices = scan_response.get('Items', [])
        logger.info(f"Found {len(active_devices)} active devices")

        # For each device, check moisture and watering history
        low_moisture_devices = []

        for device_status in active_devices:
            hardware_id = device_status['hardware_id']

            # Get latest hourly aggregate for moisture reading
            device_window = f"{hardware_id}#hourly"

            # Query most recent hourly aggregate
            agg_response = aggregates_table.query(
                KeyConditionExpression=Key('device_window').eq(device_window),
                ScanIndexForward=False,  # Most recent first
                Limit=1
            )

            aggregates = agg_response.get('Items', [])

            if not aggregates:
                logger.debug(f"No aggregates found for device {hardware_id}")
                continue

            latest_aggregate = aggregates[0]
            soil_moisture_stats = latest_aggregate.get('soil_moisture_stats', {})

            # Check if we have valid moisture data
            if soil_moisture_stats.get('valid_count', 0) == 0:
                logger.debug(f"No valid moisture data for device {hardware_id}")
                continue

            # Use average moisture from latest hour
            latest_moisture = soil_moisture_stats.get('avg')

            if latest_moisture is None or latest_moisture >= moisture_threshold:
                logger.debug(f"Device {hardware_id} moisture {latest_moisture} is above threshold")
                continue

            # Check for recent watering events
            watering_response = events_table.query(
                KeyConditionExpression=Key('hardware_id').eq(hardware_id) & Key('start_time_ms').gte(cutoff_time_ms),
                FilterExpression='event_type = :event_type',
                ExpressionAttributeValues={':event_type': 'Watering_Event'},
                ScanIndexForward=False,  # Most recent first
                Limit=1
            )

            recent_waterings = watering_response.get('Items', [])

            if recent_waterings:
                # Device has been watered recently, skip
                logger.debug(f"Device {hardware_id} has recent watering event")
                continue

            # Get last watering event (if any) for reporting
            all_watering_response = events_table.query(
                KeyConditionExpression=Key('hardware_id').eq(hardware_id),
                FilterExpression='event_type = :event_type',
                ExpressionAttributeValues={':event_type': 'Watering_Event'},
                ScanIndexForward=False,  # Most recent first
                Limit=1
            )

            all_waterings = all_watering_response.get('Items', [])
            last_watering_time_ms = all_waterings[0]['start_time_ms'] if all_waterings else None
            hours_since_watering = (now_ms - last_watering_time_ms) / (3600 * 1000) if last_watering_time_ms else None

            # Add to results
            low_moisture_devices.append({
                'hardware_id': hardware_id,
                'latest_moisture': latest_moisture,
                'moisture_timestamp_ms': latest_aggregate.get('window_start_ms'),
                'last_watering_time_ms': last_watering_time_ms,
                'hours_since_watering': hours_since_watering,
                'last_seen_ingest_time_ms': device_status.get('last_seen_ingest_time_ms')
            })

            # Stop if we've reached the limit
            if len(low_moisture_devices) >= limit:
                break

        # Sort by moisture level (lowest first)
        low_moisture_devices.sort(key=lambda d: d['latest_moisture'])

        logger.info(f"Found {len(low_moisture_devices)} devices with low moisture and no recent watering")

        return {
            'devices': low_moisture_devices,
            'moisture_threshold': moisture_threshold,
            'hours_without_watering': hours_without_watering,
            'count': len(low_moisture_devices)
        }

    except BadRequestError:
        raise
    except Exception as e:
        logger.error(f"Error querying low moisture devices: {e}", exc_info=True)
        return {
            'statusCode': 500,
            'body': json.dumps({'error': 'Internal server error'})
        }


@app.get("/dashboard")
def get_dashboard():
    """Get dashboard summary data."""
    logger.info("Getting dashboard data")
    # TODO: Implement dashboard query logic
    return {"dashboard": {}}


@app.post("/admin/recompute-aggregates")
def recompute_aggregates():
    """
    Recompute aggregates for a device and date range.

    Request body:
    - hardware_id: Device hardware ID (REQUIRED)
    - start_time: Start timestamp in ms (REQUIRED)
    - end_time: End timestamp in ms (REQUIRED)

    Process:
    1. Rebuild hourly aggregates from raw readings for the range
    2. Recompute daily from hourly for impacted days
    3. Recompute weekly from daily for impacted weeks
    4. Mark aggregates as reprocessed with timestamp

    Returns:
    - status: Success message
    - aggregates_recomputed: Count of aggregates recomputed
    - hours_recomputed: Number of hourly aggregates
    - days_recomputed: Number of daily aggregates
    - weeks_recomputed: Number of weekly aggregates

    Requirements: 18.1, 18.4-18.5
    """
    from aws_lambda_powertools.event_handler.exceptions import BadRequestError
    import sys
    import os
    import time

    # Add shared modules to path
    shared_path = os.path.join(os.path.dirname(__file__), '..', 'shared')
    if shared_path not in sys.path:
        sys.path.append(shared_path)

    from time_utils import align_to_hour, align_to_day, align_to_week

    logger.info("Recompute aggregates requested")

    try:
        # Parse request body
        body = app.current_event.json_body

        # Validate required parameters
        hardware_id = body.get('hardware_id')
        start_time = body.get('start_time')
        end_time = body.get('end_time')

        if not hardware_id:
            raise BadRequestError('hardware_id is required')
        if not start_time:
            raise BadRequestError('start_time is required')
        if not end_time:
            raise BadRequestError('end_time is required')

        try:
            start_time_ms = int(start_time)
            end_time_ms = int(end_time)
        except (ValueError, TypeError):
            raise BadRequestError('start_time and end_time must be valid timestamps in milliseconds')

        if start_time_ms >= end_time_ms:
            raise BadRequestError('start_time must be less than end_time')

        logger.info(f"Recomputing aggregates for device {hardware_id} from {start_time_ms} to {end_time_ms}")

        # Import aggregator functions
        import aggregator

        # Step 1: Rebuild hourly aggregates from raw readings
        hours_recomputed = 0
        current_hour_start = align_to_hour(start_time_ms)

        while current_hour_start < end_time_ms:
            hour_end = current_hour_start + 3600000  # 1 hour in ms

            try:
                # Rebuild this hour
                aggregator.rebuild_hourly_aggregate(hardware_id, current_hour_start, hour_end)
                hours_recomputed += 1

                # Mark as reprocessed
                mark_aggregate_reprocessed(hardware_id, 'hourly', current_hour_start)

                logger.info(f"Recomputed hourly aggregate for {hardware_id} at {current_hour_start}")

            except Exception as e:
                logger.error(f"Failed to recompute hourly aggregate: {e}", exc_info=True)
                # Continue with next hour

            current_hour_start = hour_end

        # Step 2: Recompute daily aggregates from hourly
        days_recomputed = 0
        current_day_start = align_to_day(start_time_ms)

        while current_day_start < end_time_ms:
            day_end = current_day_start + 86400000  # 1 day in ms

            try:
                # Recompute this day from hourly aggregates
                aggregator.compute_daily_aggregate_for_device(hardware_id, current_day_start, day_end)
                days_recomputed += 1

                # Mark as reprocessed
                mark_aggregate_reprocessed(hardware_id, 'daily', current_day_start)

                logger.info(f"Recomputed daily aggregate for {hardware_id} at {current_day_start}")

            except Exception as e:
                logger.error(f"Failed to recompute daily aggregate: {e}", exc_info=True)
                # Continue with next day

            current_day_start = day_end

        # Step 3: Recompute weekly aggregates from daily
        weeks_recomputed = 0
        current_week_start = align_to_week(start_time_ms)

        while current_week_start < end_time_ms:
            week_end = current_week_start + 604800000  # 1 week in ms

            try:
                # Recompute this week from daily aggregates
                aggregator.compute_weekly_aggregate_for_device(hardware_id, current_week_start, week_end)
                weeks_recomputed += 1

                # Mark as reprocessed
                mark_aggregate_reprocessed(hardware_id, 'weekly', current_week_start)

                logger.info(f"Recomputed weekly aggregate for {hardware_id} at {current_week_start}")

            except Exception as e:
                logger.error(f"Failed to recompute weekly aggregate: {e}", exc_info=True)
                # Continue with next week

            current_week_start = week_end

        total_recomputed = hours_recomputed + days_recomputed + weeks_recomputed

        logger.info(
            f"Recompute aggregates completed for {hardware_id}",
            extra={
                "hardware_id": hardware_id,
                "hours_recomputed": hours_recomputed,
                "days_recomputed": days_recomputed,
                "weeks_recomputed": weeks_recomputed,
                "total_recomputed": total_recomputed
            }
        )

        return {
            "status": "success",
            "message": f"Recomputed {total_recomputed} aggregates for device {hardware_id}",
            "aggregates_recomputed": total_recomputed,
            "hours_recomputed": hours_recomputed,
            "days_recomputed": days_recomputed,
            "weeks_recomputed": weeks_recomputed,
            "start_time_ms": start_time_ms,
            "end_time_ms": end_time_ms
        }

    except BadRequestError:
        raise
    except Exception as e:
        logger.error(f"Error recomputing aggregates: {e}", exc_info=True)
        return {
            'statusCode': 500,
            'body': json.dumps({'error': f'Failed to recompute aggregates: {str(e)}'})
        }


def mark_aggregate_reprocessed(hardware_id: str, window_type: str, window_start_ms: int) -> None:
    """
    Mark an aggregate as reprocessed by updating its metadata.

    Args:
        hardware_id: Device hardware ID
        window_type: Window type (hourly, daily, weekly)
        window_start_ms: Window start timestamp
    """
    import time

    device_window = f"{hardware_id}#{window_type}"
    now_ms = int(time.time() * 1000)

    try:
        aggregates_table.update_item(
            Key={
                'device_window': device_window,
                'window_start_ms': window_start_ms
            },
            UpdateExpression='SET reprocessed_at_ms = :now, is_reprocessed = :true',
            ExpressionAttributeValues={
                ':now': now_ms,
                ':true': True
            }
        )
        logger.debug(f"Marked {window_type} aggregate as reprocessed for {hardware_id} at {window_start_ms}")
    except Exception as e:
        logger.warning(f"Failed to mark aggregate as reprocessed: {e}")
        # Non-critical, continue


@app.post("/admin/rerun-event-detection")
def rerun_event_detection():
    """
    Re-run event detection for a device and date range.

    Request body:
    - hardware_id: Device hardware ID (REQUIRED)
    - start_time: Start timestamp in ms (REQUIRED)
    - end_time: End timestamp in ms (REQUIRED)

    Process:
    1. Query raw readings for the device and date range
    2. Re-run event detection over each reading
    3. Mark detected events with reprocessed marker
    4. Update device status

    Returns:
    - status: Success message
    - readings_processed: Count of readings processed
    - events_detected: Count of events detected

    Requirements: 18.2, 18.4-18.5
    """
    from aws_lambda_powertools.event_handler.exceptions import BadRequestError
    import sys
    import os
    import time

    # Add shared modules to path
    shared_path = os.path.join(os.path.dirname(__file__), '..', 'shared')
    if shared_path not in sys.path:
        sys.path.append(shared_path)

    logger.info("Rerun event detection requested")

    try:
        # Parse request body
        body = app.current_event.json_body

        # Validate required parameters
        hardware_id = body.get('hardware_id')
        start_time = body.get('start_time')
        end_time = body.get('end_time')

        if not hardware_id:
            raise BadRequestError('hardware_id is required')
        if not start_time:
            raise BadRequestError('start_time is required')
        if not end_time:
            raise BadRequestError('end_time is required')

        try:
            start_time_ms = int(start_time)
            end_time_ms = int(end_time)
        except (ValueError, TypeError):
            raise BadRequestError('start_time and end_time must be valid timestamps in milliseconds')

        if start_time_ms >= end_time_ms:
            raise BadRequestError('start_time must be less than end_time')

        logger.info(f"Rerunning event detection for device {hardware_id} from {start_time_ms} to {end_time_ms}")

        # Query raw readings for the device and date range
        readings_table = dynamodb.Table(os.environ['READINGS_TABLE'])

        all_readings = []
        query_kwargs = {
            'KeyConditionExpression': Key('hardware_id').eq(hardware_id) & Key('timestamp_ms').between(start_time_ms, end_time_ms),
            'ScanIndexForward': True  # Process in chronological order
        }

        response = readings_table.query(**query_kwargs)
        all_readings.extend(response.get('Items', []))

        # Handle pagination
        while 'LastEvaluatedKey' in response:
            query_kwargs['ExclusiveStartKey'] = response['LastEvaluatedKey']
            response = readings_table.query(**query_kwargs)
            all_readings.extend(response.get('Items', []))

        logger.info(f"Found {len(all_readings)} readings to reprocess for {hardware_id}")

        # Import event detection functions
        from event_detection import (
            get_recent_readings,
            detect_watering_event,
            detect_drying_cycle,
            detect_temperature_stress,
            detect_humidity_anomaly,
            detect_environmental_change,
            persist_event
        )

        readings_processed = 0
        events_detected_count = 0

        # Process each reading for event detection
        for reading in all_readings:
            timestamp_ms = reading.get('timestamp_ms')
            batch_id = reading.get('batch_id', 'reprocess')
            reading_id = f"{batch_id}#{timestamp_ms}"

            # Fetch recent readings for context (last 6 hours)
            six_hours_ago = timestamp_ms - (6 * 60 * 60 * 1000)
            recent_readings = get_recent_readings(hardware_id, six_hours_ago, limit=200)

            # Run all detection algorithms
            detectors = [
                ("watering", lambda: detect_watering_event(recent_readings, reading)),
                ("drying", lambda: detect_drying_cycle(recent_readings, reading)),
                ("temperature_stress", lambda: detect_temperature_stress(reading)),
                ("humidity_anomaly", lambda: detect_humidity_anomaly(recent_readings, reading)),
                ("environmental_change", lambda: detect_environmental_change(recent_readings, reading))
            ]

            for detector_name, detector_func in detectors:
                try:
                    event = detector_func()
                    if event:
                        # Persist event with reprocessed marker
                        if persist_event(event):
                            events_detected_count += 1

                            # Mark event as reprocessed
                            mark_event_reprocessed(hardware_id, event.event_type.value, event.start_time_ms)

                            logger.info(
                                f"{detector_name} event detected during reprocessing",
                                extra={
                                    "hardware_id": hardware_id,
                                    "event_type": event.event_type.value,
                                    "start_time_ms": event.start_time_ms
                                }
                            )
                except Exception as e:
                    logger.error(
                        f"Error in {detector_name} detection during reprocessing",
                        extra={
                            "hardware_id": hardware_id,
                            "error": str(e),
                            "error_type": type(e).__name__
                        }
                    )
                    # Continue with other detectors

            readings_processed += 1

        logger.info(
            f"Rerun event detection completed for {hardware_id}",
            extra={
                "hardware_id": hardware_id,
                "readings_processed": readings_processed,
                "events_detected": events_detected_count
            }
        )

        return {
            "status": "success",
            "message": f"Reprocessed {readings_processed} readings for device {hardware_id}",
            "readings_processed": readings_processed,
            "events_detected": events_detected_count,
            "start_time_ms": start_time_ms,
            "end_time_ms": end_time_ms
        }

    except BadRequestError:
        raise
    except Exception as e:
        logger.error(f"Error rerunning event detection: {e}", exc_info=True)
        return {
            'statusCode': 500,
            'body': json.dumps({'error': f'Failed to rerun event detection: {str(e)}'})
        }


def mark_event_reprocessed(hardware_id: str, event_type: str, start_time_ms: int) -> None:
    """
    Mark an event as reprocessed by updating its metadata.

    Args:
        hardware_id: Device hardware ID
        event_type: Event type
        start_time_ms: Event start timestamp
    """
    import time

    now_ms = int(time.time() * 1000)

    try:
        events_table.update_item(
            Key={
                'hardware_id': hardware_id,
                'start_time_ms': start_time_ms
            },
            UpdateExpression='SET reprocessed_at_ms = :now, is_reprocessed = :true',
            ExpressionAttributeValues={
                ':now': now_ms,
                ':true': True
            }
        )
        logger.debug(f"Marked event as reprocessed for {hardware_id} at {start_time_ms}")
    except Exception as e:
        logger.warning(f"Failed to mark event as reprocessed: {e}")
        # Non-critical, continue


@app.post("/admin/regenerate-insight")
def regenerate_insight():
    """
    Regenerate insight for a device.

    Request body:
    - hardware_id: Device hardware ID (REQUIRED)

    Process:
    1. Create InsightRequest with reprocess marker
    2. Force immediate insight generation
    3. Mark insight as reprocessed

    Returns:
    - status: Success message
    - insight: The regenerated insight object

    Requirements: 18.3, 18.5
    """
    from aws_lambda_powertools.event_handler.exceptions import BadRequestError
    import sys
    import os
    import time

    # Add shared modules to path
    shared_path = os.path.join(os.path.dirname(__file__), '..', 'shared')
    if shared_path not in sys.path:
        sys.path.append(shared_path)

    from models import InsightRequest, InsightRequestType, InsightRequestStatus

    logger.info("Regenerate insight requested")

    try:
        # Parse request body
        body = app.current_event.json_body

        # Validate required parameters
        hardware_id = body.get('hardware_id')

        if not hardware_id:
            raise BadRequestError('hardware_id is required')

        logger.info(f"Regenerating insight for device {hardware_id}")

        # Get insight_requests_table
        insight_requests_table = dynamodb.Table(os.environ.get('INSIGHT_REQUESTS_TABLE', 'plant_insight_requests'))

        # Create InsightRequest with reprocess marker
        now_ms = int(time.time() * 1000)

        request = InsightRequest(
            hardware_id=hardware_id,
            request_time_ms=now_ms,
            request_type=InsightRequestType.SCHEDULED,  # Use scheduled type for manual regeneration
            status=InsightRequestStatus.PENDING
        )

        # Store the request
        insight_requests_table.put_item(Item=request.to_dynamodb_item())

        logger.info(f"Created InsightRequest for {hardware_id}")

        # Import insight generation functions
        import insight_generator

        # Process the request immediately (synchronous for this endpoint)
        try:
            # Mark as processing
            insight_requests_table.update_item(
                Key={
                    'hardware_id': hardware_id,
                    'request_time_ms': now_ms
                },
                UpdateExpression='SET #status = :processing',
                ExpressionAttributeNames={'#status': 'status'},
                ExpressionAttributeValues={':processing': InsightRequestStatus.PROCESSING.value}
            )

            # Generate the insight
            insight = insight_generator.generate_insight_for_device(hardware_id)

            if insight:
                # Mark insight as reprocessed
                mark_insight_reprocessed(hardware_id, insight.timestamp_ms)

                # Mark request as done
                insight_requests_table.update_item(
                    Key={
                        'hardware_id': hardware_id,
                        'request_time_ms': now_ms
                    },
                    UpdateExpression='SET #status = :done, processed_at_ms = :processed',
                    ExpressionAttributeNames={'#status': 'status'},
                    ExpressionAttributeValues={
                        ':done': InsightRequestStatus.DONE.value,
                        ':processed': int(time.time() * 1000)
                    }
                )

                logger.info(f"Successfully regenerated insight for {hardware_id}")

                return {
                    "status": "success",
                    "message": f"Regenerated insight for device {hardware_id}",
                    "insight": insight.to_dynamodb_item()
                }
            else:
                # Mark request as failed
                insight_requests_table.update_item(
                    Key={
                        'hardware_id': hardware_id,
                        'request_time_ms': now_ms
                    },
                    UpdateExpression='SET #status = :failed, processed_at_ms = :processed, error_message = :error',
                    ExpressionAttributeNames={'#status': 'status'},
                    ExpressionAttributeValues={
                        ':failed': InsightRequestStatus.FAILED.value,
                        ':processed': int(time.time() * 1000),
                        ':error': 'Insight generation returned None'
                    }
                )

                logger.error(f"Failed to regenerate insight for {hardware_id}: generation returned None")

                return {
                    'statusCode': 500,
                    'body': json.dumps({'error': 'Failed to regenerate insight: generation returned None'})
                }

        except Exception as e:
            # Mark request as failed
            try:
                insight_requests_table.update_item(
                    Key={
                        'hardware_id': hardware_id,
                        'request_time_ms': now_ms
                    },
                    UpdateExpression='SET #status = :failed, processed_at_ms = :processed, error_message = :error',
                    ExpressionAttributeNames={'#status': 'status'},
                    ExpressionAttributeValues={
                        ':failed': InsightRequestStatus.FAILED.value,
                        ':processed': int(time.time() * 1000),
                        ':error': str(e)[:256]
                    }
                )
            except:
                pass

            logger.error(f"Error regenerating insight: {e}", exc_info=True)
            raise

    except BadRequestError:
        raise
    except Exception as e:
        logger.error(f"Error regenerating insight: {e}", exc_info=True)
        return {
            'statusCode': 500,
            'body': json.dumps({'error': f'Failed to regenerate insight: {str(e)}'})
        }


def mark_insight_reprocessed(hardware_id: str, timestamp_ms: int) -> None:
    """
    Mark an insight as reprocessed by updating its metadata.

    Args:
        hardware_id: Device hardware ID
        timestamp_ms: Insight timestamp
    """
    import time

    now_ms = int(time.time() * 1000)

    try:
        insights_table.update_item(
            Key={
                'hardware_id': hardware_id,
                'timestamp_ms': timestamp_ms
            },
            UpdateExpression='SET reprocessed_at_ms = :now, is_reprocessed = :true',
            ExpressionAttributeValues={
                ':now': now_ms,
                ':true': True
            }
        )
        logger.debug(f"Marked insight as reprocessed for {hardware_id} at {timestamp_ms}")
    except Exception as e:
        logger.warning(f"Failed to mark insight as reprocessed: {e}")
        # Non-critical, continue


@logger.inject_lambda_context
def lambda_handler(event: Dict[str, Any], context: LambdaContext) -> Dict[str, Any]:
    """
    Lambda handler for Control Plane API endpoints.

    Args:
        event: API Gateway event
        context: Lambda context

    Returns:
        API Gateway response
    """
    return app.resolve(event, context)
