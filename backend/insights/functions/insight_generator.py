"""
Insight Generator Lambda

Generates LLM-powered natural language insights:
- Scheduled insight generation (1-2x daily)
- Event-driven insight generation (critical events)
- Processes InsightRequest items
"""

import os
import json
import time
from typing import Dict, Any, List, Optional, Tuple
from datetime import datetime, timezone, timedelta
import boto3
from boto3.dynamodb.conditions import Key
from aws_lambda_powertools import Logger
from aws_lambda_powertools.utilities.typing import LambdaContext

# Import shared modules
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'shared'))

from models import (
    DeviceProfile, Insight, InsightRequest, Aggregate, Event,
    ConfidenceLevel, TrendClassification, Recommendation,
    InsightRequestType, InsightRequestStatus, SensorStats, DeviceStatus
)
from device_status import update_device_status_field

logger = Logger()

# DynamoDB clients
dynamodb = boto3.resource('dynamodb')
device_status_table = dynamodb.Table(os.environ.get('DEVICE_STATUS_TABLE', 'plant_device_status'))
device_profiles_table = dynamodb.Table(os.environ.get('DEVICE_PROFILES_TABLE', 'plant_device_profiles'))
aggregates_table = dynamodb.Table(os.environ.get('AGGREGATES_TABLE', 'plant_aggregates'))
events_table = dynamodb.Table(os.environ.get('EVENTS_TABLE', 'plant_events'))
insights_table = dynamodb.Table(os.environ.get('INSIGHTS_TABLE', 'plant_insights'))
insight_requests_table = dynamodb.Table(os.environ.get('INSIGHT_REQUESTS_TABLE', 'plant_insight_requests'))

# LLM Configuration
LLM_API_KEY = os.environ.get('LLM_API_KEY', '')
LLM_MODEL = os.environ.get('LLM_MODEL', 'gpt-3.5-turbo')  # Cost-effective model
MAX_LLM_RETRIES = 3
ACTIVE_DEVICE_THRESHOLD_HOURS = 24
EVENT_DRIVEN_DAILY_CAP = 6
EVENT_BATCHING_WINDOW_HOURS = 1


def get_active_devices(threshold_hours: int = ACTIVE_DEVICE_THRESHOLD_HOURS) -> List[str]:
    """
    Enumerate active devices based on last_seen_ingest_time_ms.

    Args:
        threshold_hours: Hours threshold for considering a device active

    Returns:
        List of hardware_ids for active devices
    """
    now_ms = int(time.time() * 1000)
    threshold_ms = now_ms - (threshold_hours * 60 * 60 * 1000)

    active_devices = []

    try:
        # Scan device status table for active devices
        # Note: In production, consider using a GSI or maintaining an active devices list
        response = device_status_table.scan()

        for item in response.get('Items', []):
            last_seen = item.get('last_seen_ingest_time_ms')
            if last_seen and last_seen >= threshold_ms:
                active_devices.append(item['hardware_id'])

        logger.info(f"Found {len(active_devices)} active devices", extra={
            "threshold_hours": threshold_hours,
            "device_count": len(active_devices)
        })

    except Exception as e:
        logger.error(f"Error enumerating active devices: {e}")
        raise

    return active_devices


def create_insight_requests(hardware_ids: List[str], request_type: InsightRequestType) -> int:
    """
    Create InsightRequest items for devices.

    Args:
        hardware_ids: List of device hardware IDs
        request_type: Type of insight request (scheduled or event)

    Returns:
        Number of requests created
    """
    now_ms = int(time.time() * 1000)
    created_count = 0

    for hardware_id in hardware_ids:
        try:
            request = InsightRequest(
                hardware_id=hardware_id,
                request_time_ms=now_ms,
                request_type=request_type,
                status=InsightRequestStatus.PENDING
            )

            insight_requests_table.put_item(Item=request.to_dynamodb_item())
            created_count += 1

        except Exception as e:
            logger.error(f"Error creating insight request for {hardware_id}: {e}")
            continue

    logger.info(f"Created {created_count} insight requests", extra={
        "request_type": request_type.value,
        "created_count": created_count
    })

    return created_count


def get_pending_insight_requests(batch_size: int = 10) -> List[InsightRequest]:
    """
    Fetch pending InsightRequest items in batches.

    Args:
        batch_size: Maximum number of requests to fetch

    Returns:
        List of pending InsightRequest objects
    """
    pending_requests = []

    try:
        # Scan for pending requests
        # Note: In production, use GSI on status for efficient queries
        response = insight_requests_table.scan(
            FilterExpression='#status = :pending',
            ExpressionAttributeNames={'#status': 'status'},
            ExpressionAttributeValues={':pending': InsightRequestStatus.PENDING.value},
            Limit=batch_size
        )

        for item in response.get('Items', []):
            pending_requests.append(InsightRequest.from_dynamodb_item(item))

        logger.info(f"Found {len(pending_requests)} pending insight requests")

    except Exception as e:
        logger.error(f"Error fetching pending insight requests: {e}")
        raise

    return pending_requests


def mark_request_processing(hardware_id: str, request_time_ms: int) -> None:
    """Mark an InsightRequest as processing."""
    try:
        insight_requests_table.update_item(
            Key={
                'hardware_id': hardware_id,
                'request_time_ms': request_time_ms
            },
            UpdateExpression='SET #status = :processing',
            ExpressionAttributeNames={'#status': 'status'},
            ExpressionAttributeValues={':processing': InsightRequestStatus.PROCESSING.value}
        )
    except Exception as e:
        logger.error(f"Error marking request as processing: {e}")


def mark_request_done(hardware_id: str, request_time_ms: int) -> None:
    """Mark an InsightRequest as done."""
    now_ms = int(time.time() * 1000)
    try:
        insight_requests_table.update_item(
            Key={
                'hardware_id': hardware_id,
                'request_time_ms': request_time_ms
            },
            UpdateExpression='SET #status = :done, processed_at_ms = :now',
            ExpressionAttributeNames={'#status': 'status'},
            ExpressionAttributeValues={
                ':done': InsightRequestStatus.DONE.value,
                ':now': now_ms
            }
        )
    except Exception as e:
        logger.error(f"Error marking request as done: {e}")


def mark_request_failed(hardware_id: str, request_time_ms: int, error_message: str) -> None:
    """Mark an InsightRequest as failed."""
    now_ms = int(time.time() * 1000)
    try:
        insight_requests_table.update_item(
            Key={
                'hardware_id': hardware_id,
                'request_time_ms': request_time_ms
            },
            UpdateExpression='SET #status = :failed, processed_at_ms = :now, error_message = :error',
            ExpressionAttributeNames={'#status': 'status'},
            ExpressionAttributeValues={
                ':failed': InsightRequestStatus.FAILED.value,
                ':now': now_ms,
                ':error': error_message[:256]  # Truncate error message
            }
        )
    except Exception as e:
        logger.error(f"Error marking request as failed: {e}")


def check_event_driven_rate_limit(hardware_id: str) -> bool:
    """
    Check if device has exceeded event-driven insight daily cap.

    Args:
        hardware_id: Device hardware ID

    Returns:
        True if within rate limit, False if exceeded
    """
    now_ms = int(time.time() * 1000)
    day_start_ms = now_ms - (24 * 60 * 60 * 1000)

    try:
        # Query insight requests for this device in last 24 hours
        response = insight_requests_table.query(
            KeyConditionExpression=Key('hardware_id').eq(hardware_id) & Key('request_time_ms').gte(day_start_ms),
            FilterExpression='request_type = :event',
            ExpressionAttributeValues={':event': InsightRequestType.EVENT.value}
        )

        event_driven_count = len(response.get('Items', []))

        if event_driven_count >= EVENT_DRIVEN_DAILY_CAP:
            logger.warning(f"Event-driven rate limit exceeded for {hardware_id}", extra={
                "hardware_id": hardware_id,
                "count": event_driven_count,
                "cap": EVENT_DRIVEN_DAILY_CAP
            })
            return False

        return True

    except Exception as e:
        logger.error(f"Error checking rate limit: {e}")
        return False


def fetch_aggregates_for_insight(hardware_id: str, hours: int = 24) -> List[Aggregate]:
    """
    Fetch aggregates for the last N hours.

    Args:
        hardware_id: Device hardware ID
        hours: Number of hours to fetch

    Returns:
        List of Aggregate objects
    """
    now_ms = int(time.time() * 1000)
    start_ms = now_ms - (hours * 60 * 60 * 1000)

    aggregates = []

    try:
        # Query hourly aggregates
        device_window = f"{hardware_id}#hourly"
        response = aggregates_table.query(
            KeyConditionExpression=Key('device_window').eq(device_window) & Key('window_start_ms').gte(start_ms)
        )

        for item in response.get('Items', []):
            agg = Aggregate(
                hardware_id=item['hardware_id'],
                window_type=item['window_type'],
                window_start_ms=item['window_start_ms'],
                window_end_ms=item['window_end_ms'],
                temperature_stats=SensorStats.from_dict(item.get('temperature_stats', {})),
                humidity_stats=SensorStats.from_dict(item.get('humidity_stats', {})),
                pressure_stats=SensorStats.from_dict(item.get('pressure_stats', {})),
                soil_moisture_stats=SensorStats.from_dict(item.get('soil_moisture_stats', {})),
                computed_at_ms=item.get('computed_at_ms'),
                is_complete=item.get('is_complete', False)
            )
            aggregates.append(agg)

        logger.info(f"Fetched {len(aggregates)} aggregates for {hardware_id}", extra={
            "hardware_id": hardware_id,
            "hours": hours,
            "count": len(aggregates)
        })

    except Exception as e:
        logger.error(f"Error fetching aggregates: {e}")

    return aggregates


def fetch_recent_events(hardware_id: str, hours: int = 24) -> List[Event]:
    """
    Fetch recent events for a device.

    Args:
        hardware_id: Device hardware ID
        hours: Number of hours to fetch

    Returns:
        List of Event objects
    """
    now_ms = int(time.time() * 1000)
    start_ms = now_ms - (hours * 60 * 60 * 1000)

    events = []

    try:
        response = events_table.query(
            KeyConditionExpression=Key('hardware_id').eq(hardware_id) & Key('start_time_ms').gte(start_ms)
        )

        for item in response.get('Items', []):
            from models import EventType
            event = Event(
                hardware_id=item['hardware_id'],
                event_type=EventType(item['event_type']),
                start_time_ms=item['start_time_ms'],
                end_time_ms=item['end_time_ms'],
                sensor_values=item.get('sensor_values', {}),
                detection_metadata=item.get('detection_metadata', {}),
                created_at_ms=item.get('created_at_ms')
            )
            events.append(event)

        logger.info(f"Fetched {len(events)} events for {hardware_id}", extra={
            "hardware_id": hardware_id,
            "hours": hours,
            "count": len(events)
        })

    except Exception as e:
        logger.error(f"Error fetching events: {e}")

    return events


def fetch_device_profile(hardware_id: str) -> Optional[DeviceProfile]:
    """
    Fetch device profile.

    Args:
        hardware_id: Device hardware ID

    Returns:
        DeviceProfile object or None if not found
    """
    try:
        response = device_profiles_table.get_item(Key={'hardware_id': hardware_id})

        if 'Item' in response:
            return DeviceProfile.from_dynamodb_item(response['Item'])
        else:
            # Return default profile
            return DeviceProfile(hardware_id=hardware_id)

    except Exception as e:
        logger.error(f"Error fetching device profile: {e}")
        return DeviceProfile(hardware_id=hardware_id)


def build_insight_prompt(
    aggregates: List[Aggregate],
    events: List[Event],
    profile: DeviceProfile,
    previous_week_aggregates: List[Aggregate]
) -> str:
    """
    Build structured prompt for LLM insight generation.

    Args:
        aggregates: Recent aggregates (last 24 hours)
        events: Recent events
        profile: Device profile
        previous_week_aggregates: Aggregates from previous week for trend comparison

    Returns:
        Prompt string
    """
    # Calculate summary statistics
    if aggregates:
        temp_values = [agg.temperature_stats.avg for agg in aggregates if agg.temperature_stats.avg is not None]
        humidity_values = [agg.humidity_stats.avg for agg in aggregates if agg.humidity_stats.avg is not None]
        moisture_values = [agg.soil_moisture_stats.avg for agg in aggregates if agg.soil_moisture_stats.avg is not None]

        avg_temp = sum(temp_values) / len(temp_values) if temp_values else None
        avg_humidity = sum(humidity_values) / len(humidity_values) if humidity_values else None
        avg_moisture = sum(moisture_values) / len(moisture_values) if moisture_values else None
    else:
        avg_temp = avg_humidity = avg_moisture = None

    # Build current conditions summary
    conditions = []
    if avg_temp is not None:
        conditions.append(f"Temperature: {avg_temp:.1f}°C")
    if avg_humidity is not None:
        conditions.append(f"Humidity: {avg_humidity:.1f}%")
    if avg_moisture is not None:
        conditions.append(f"Soil Moisture: {avg_moisture:.1f}%")

    conditions_str = ", ".join(conditions) if conditions else "No recent data available"

    # Build events summary
    events_str = ""
    if events:
        event_types = {}
        for event in events:
            event_type = event.event_type.value
            event_types[event_type] = event_types.get(event_type, 0) + 1
        events_str = ", ".join([f"{count} {etype}" for etype, count in event_types.items()])
    else:
        events_str = "No recent events"

    # Build plant context
    plant_context = ""
    if profile.plant_type:
        plant_context += f"Plant Type: {profile.plant_type}\n"
    if profile.soil_type:
        plant_context += f"Soil Type: {profile.soil_type}\n"
    if profile.pot_size_liters:
        plant_context += f"Pot Size: {profile.pot_size_liters}L\n"

    if not plant_context:
        plant_context = "Plant information not provided\n"

    # Build prompt
    prompt = f"""You are a plant care assistant analyzing sensor data from an IoT monitoring system.

{plant_context}
Current Conditions (last 24 hours):
{conditions_str}

Recent Events:
{events_str}

Please provide a beginner-friendly analysis in JSON format with the following structure:
{{
  "summary": "Brief summary of current plant conditions (2-3 sentences)",
  "recommendations": [
    {{
      "action": "Specific action to take",
      "reason": "Why this action is recommended",
      "urgency": "low|medium|high"
    }}
  ],
  "confidence": "low|medium|high",
  "trend": "improving|declining|stable",
  "growth_stage_suggestion": "Optional suggestion about growth stage"
}}

Important guidelines:
- Use simple, beginner-friendly language
- Provide actionable recommendations
- NEVER diagnose plant diseases
- If data is insufficient, set confidence to "low" and explain in summary
- Focus on environmental conditions and care practices
- Be encouraging and supportive

Respond ONLY with valid JSON, no additional text."""

    return prompt


def call_llm_api(
    prompt: str,
    hardware_id: str,
    max_retries: int = MAX_LLM_RETRIES,
    request_id: Optional[str] = None
) -> Optional[Dict[str, Any]]:
    """
    Call LLM API with retry logic and exponential backoff.

    Args:
        prompt: Prompt string
        hardware_id: Device hardware ID for logging
        max_retries: Maximum number of retries
        request_id: Optional request ID for correlation

    Returns:
        Parsed JSON response or None on failure
    """
    import requests
    from logging_utils import log_llm_api_call

    if not LLM_API_KEY:
        logger.warning("LLM_API_KEY not configured, returning mock response")
        # Return mock response for testing
        return {
            "summary": "Monitoring data is being collected. Continue regular care routine.",
            "recommendations": [
                {
                    "action": "Continue monitoring",
                    "reason": "System is collecting baseline data",
                    "urgency": "low"
                }
            ],
            "confidence": "low",
            "trend": "stable",
            "growth_stage_suggestion": None
        }

    headers = {
        "Authorization": f"Bearer {LLM_API_KEY}",
        "Content-Type": "application/json"
    }

    payload = {
        "model": LLM_MODEL,
        "messages": [
            {"role": "user", "content": prompt}
        ],
        "temperature": 0.7,
        "max_tokens": 1000
    }

    for attempt in range(1, max_retries + 1):
        call_start_time = time.time()
        try:
            response = requests.post(
                "https://api.openai.com/v1/chat/completions",
                headers=headers,
                json=payload,
                timeout=30
            )

            call_duration_ms = int((time.time() - call_start_time) * 1000)

            if response.status_code == 200:
                result = response.json()
                content = result['choices'][0]['message']['content']

                # Parse JSON response
                parsed = json.loads(content)

                # Log successful API call
                log_llm_api_call(
                    logger=logger,
                    hardware_id=hardware_id,
                    attempt=attempt,
                    max_retries=max_retries,
                    success=True,
                    duration_ms=call_duration_ms,
                    request_id=request_id
                )

                return parsed
            else:
                error_msg = f"LLM API returned status {response.status_code}"

                # Log failed API call
                log_llm_api_call(
                    logger=logger,
                    hardware_id=hardware_id,
                    attempt=attempt,
                    max_retries=max_retries,
                    success=False,
                    duration_ms=call_duration_ms,
                    error_message=error_msg,
                    request_id=request_id
                )

                if attempt < max_retries:
                    # Exponential backoff
                    wait_time = 2 ** (attempt - 1)
                    time.sleep(wait_time)

        except Exception as e:
            call_duration_ms = int((time.time() - call_start_time) * 1000)
            error_msg = str(e)

            # Log failed API call
            log_llm_api_call(
                logger=logger,
                hardware_id=hardware_id,
                attempt=attempt,
                max_retries=max_retries,
                success=False,
                duration_ms=call_duration_ms,
                error_message=error_msg,
                request_id=request_id
            )

            if attempt < max_retries:
                # Exponential backoff
                wait_time = 2 ** (attempt - 1)
                time.sleep(wait_time)

    return None


def validate_and_sanitize_insight(llm_response: Dict[str, Any]) -> Dict[str, Any]:
    """
    Validate and sanitize LLM response.

    Args:
        llm_response: Raw LLM response

    Returns:
        Validated and sanitized response
    """
    # Check for disease diagnosis keywords and replace
    disease_keywords = ['disease', 'infection', 'pathogen', 'fungus', 'bacteria', 'virus', 'blight', 'rot', 'mold']

    summary = llm_response.get('summary', '')
    for keyword in disease_keywords:
        if keyword.lower() in summary.lower():
            summary = summary.replace(keyword, 'condition')
            logger.warning(f"Replaced disease keyword '{keyword}' in summary")

    # Sanitize recommendations
    recommendations = []
    for rec in llm_response.get('recommendations', []):
        action = rec.get('action', '')
        reason = rec.get('reason', '')

        for keyword in disease_keywords:
            if keyword.lower() in action.lower():
                action = action.replace(keyword, 'condition')
            if keyword.lower() in reason.lower():
                reason = reason.replace(keyword, 'condition')

        recommendations.append({
            'action': action,
            'reason': reason,
            'urgency': rec.get('urgency', 'low')
        })

    return {
        'summary': summary,
        'recommendations': recommendations,
        'confidence': llm_response.get('confidence', 'low'),
        'trend': llm_response.get('trend', 'stable'),
        'growth_stage_suggestion': llm_response.get('growth_stage_suggestion')
    }


def generate_insight_for_device(hardware_id: str, request_id: Optional[str] = None) -> Optional[Insight]:
    """
    Generate insight for a single device.

    Args:
        hardware_id: Device hardware ID
        request_id: Optional request ID for correlation

    Returns:
        Generated Insight object or None on failure
    """
    from logging_utils import (
        log_insight_generation_start,
        log_insight_generation_success,
        log_insight_generation_failure
    )

    start_time = time.time()

    # Log start of insight generation
    log_insight_generation_start(
        logger=logger,
        hardware_id=hardware_id,
        request_type="manual" if request_id else "scheduled",
        request_id=request_id
    )

    try:
        # Fetch data
        aggregates = fetch_aggregates_for_insight(hardware_id, hours=24)
        events = fetch_recent_events(hardware_id, hours=24)
        profile = fetch_device_profile(hardware_id)
        previous_week_aggregates = fetch_aggregates_for_insight(hardware_id, hours=24 * 7)

        # Check data sufficiency
        valid_hours = sum(1 for agg in aggregates if agg.temperature_stats.valid_count > 0)

        # Build prompt
        prompt = build_insight_prompt(aggregates, events, profile, previous_week_aggregates)

        # Call LLM with retry logic
        llm_response = call_llm_api(
            prompt=prompt,
            hardware_id=hardware_id,
            max_retries=MAX_LLM_RETRIES,
            request_id=request_id
        )

        if not llm_response:
            generation_duration_ms = int((time.time() - start_time) * 1000)
            log_insight_generation_failure(
                logger=logger,
                hardware_id=hardware_id,
                error_message="LLM API call failed after all retries",
                error_type="LLM_API_FAILURE",
                generation_duration_ms=generation_duration_ms,
                request_id=request_id
            )
            return None

        # Validate and sanitize
        sanitized = validate_and_sanitize_insight(llm_response)

        # Override confidence if insufficient data
        if valid_hours < 12:
            sanitized['confidence'] = 'low'
            sanitized['summary'] = f"Insufficient data (only {valid_hours} hours of valid readings). " + sanitized['summary']

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
            llm_model=LLM_MODEL,
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

        # Log successful insight generation
        log_insight_generation_success(
            logger=logger,
            hardware_id=hardware_id,
            confidence=insight.confidence.value,
            trend=insight.trend.value,
            generation_duration_ms=generation_duration_ms,
            request_id=request_id,
            aggregate_count=len(aggregates),
            event_count=len(events)
        )

        return insight

    except Exception as e:
        generation_duration_ms = int((time.time() - start_time) * 1000)

        # Log failure
        log_insight_generation_failure(
            logger=logger,
            hardware_id=hardware_id,
            error_message=str(e),
            error_type=type(e).__name__,
            generation_duration_ms=generation_duration_ms,
            request_id=request_id
        )

        # Record error in device status
        from device_status import record_device_error
        record_device_error(
            hardware_id=hardware_id,
            error_code='INSIGHT_GENERATION_FAILED',
            error_message=str(e)
        )

        return None


@logger.inject_lambda_context
def lambda_handler(event: Dict[str, Any], context: LambdaContext) -> Dict[str, Any]:
    """
    Lambda handler for generating LLM-powered insights.

    Handles two invocation modes:
    1. Scheduled invocation (EventBridge): Creates InsightRequests for active devices
    2. Request processing: Processes pending InsightRequest items

    Args:
        event: Lambda event containing scheduled trigger or processing request
        context: Lambda context

    Returns:
        Response dict with generation status
    """
    logger.info("Insight Generator Lambda invoked", extra={"event": event})

    # Determine invocation mode
    source = event.get('source', '')

    if source == 'aws.events':
        # Scheduled invocation - create insight requests
        logger.info("Processing scheduled insight generation")

        active_devices = get_active_devices()
        created_count = create_insight_requests(active_devices, InsightRequestType.SCHEDULED)

        return {
            "statusCode": 200,
            "body": json.dumps({
                "message": "Insight requests created",
                "active_devices": len(active_devices),
                "requests_created": created_count
            })
        }

    else:
        # Process pending insight requests
        logger.info("Processing pending insight requests")

        pending_requests = get_pending_insight_requests(batch_size=10)

        results = {
            "processed": 0,
            "succeeded": 0,
            "failed": 0
        }

        for request in pending_requests:
            results["processed"] += 1

            # Mark as processing
            mark_request_processing(request.hardware_id, request.request_time_ms)

            # Generate insight
            insight = generate_insight_for_device(request.hardware_id)

            if insight:
                mark_request_done(request.hardware_id, request.request_time_ms)
                results["succeeded"] += 1
            else:
                mark_request_failed(request.hardware_id, request.request_time_ms, "Insight generation failed")
                results["failed"] += 1

        logger.info("Insight request processing complete", extra=results)

        return {
            "statusCode": 200,
            "body": json.dumps({
                "message": "Insight requests processed",
                **results
            })
        }
