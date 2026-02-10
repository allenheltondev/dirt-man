"""Event detection logic."""
import os
import time
from typing import List, Dict, Any, Optional
import boto3
from botocore.exceptions import ClientError
from aws_lambda_powertools import Logger
from .models import Event, EventType

logger = Logger(child=True)
dynamodb = boto3.client("dynamodb")
dynamodb_resource = boto3.resource("dynamodb")

READINGS_TABLE = os.environ.get("READINGS_TABLE", "plant_readings")
EVENTS_TABLE = os.environ.get("EVENTS_TABLE", "plant_events")
DEVICE_STATUS_TABLE = os.environ.get("DEVICE_STATUS_TABLE", "plant_device_status")
INSIGHT_REQUESTS_TABLE = os.environ.get("INSIGHT_REQUESTS_TABLE", "plant_insight_requests")

COOLDOWN_WATERING = 60 * 60 * 1000
COOLDOWN_TEMPERATURE_STRESS = 30 * 60 * 1000
COOLDOWN_HUMIDITY_ANOMALY = 30 * 60 * 1000
COOLDOWN_ENVIRONMENTAL_CHANGE = 2 * 60 * 60 * 1000


def get_recent_readings(hardware_id: str, since_ms: int, limit: int = 100) -> List[Dict[str, Any]]:
    """Fetch recent readings."""
    try:
        table = dynamodb_resource.Table(READINGS_TABLE)
        response = table.query(
            KeyConditionExpression="hardware_id = :hw_id AND timestamp_ms >= :since",
            ExpressionAttributeValues={":hw_id": hardware_id, ":since": since_ms},
            Limit=limit,
            ScanIndexForward=True
        )
        return response.get("Items", [])
    except ClientError as e:
        logger.error("Error fetching readings", extra={"error": str(e)})
        return []



def detect_watering_event(readings: List[Dict[str, Any]], current_reading: Dict[str, Any]) -> Optional[Event]:
    """Detect watering events."""
    hardware_id = current_reading.get("hardware_id")
    current_timestamp = current_reading.get("timestamp_ms")
    current_moisture = current_reading.get("soil_moisture")
    current_status = current_reading.get("soil_moisture_status", "ok")

    if current_moisture is None or current_status != "ok":
        return None

    valid_readings = [
        r for r in readings
        if r.get("soil_moisture") is not None
        and r.get("soil_moisture_status", "ok") == "ok"
        and r.get("timestamp_ms") < current_timestamp
    ]

    if not valid_readings:
        return None

    thirty_min_ago = current_timestamp - (30 * 60 * 1000)
    recent_30min = [r for r in valid_readings if r.get("timestamp_ms") >= thirty_min_ago]

    if recent_30min:
        min_moisture = min(r.get("soil_moisture") for r in recent_30min)
        increase = current_moisture - min_moisture

        if increase > 15.0:
            start_time = min(r.get("timestamp_ms") for r in recent_30min)
            return Event(
                hardware_id=hardware_id,
                event_type=EventType.WATERING_EVENT,
                start_time_ms=start_time,
                end_time_ms=current_timestamp,
                sensor_values={"soil_moisture_before": min_moisture, "soil_moisture_after": current_moisture, "increase_pct": increase},
                detection_metadata={"detection_mode": "rapid_spike", "window_minutes": 30},
                created_at_ms=int(time.time() * 1000)
            )

    sixty_min_ago = current_timestamp - (60 * 60 * 1000)
    recent_60min = [r for r in valid_readings if r.get("timestamp_ms") >= sixty_min_ago]

    if len(recent_60min) >= 2:
        min_moisture = min(r.get("soil_moisture") for r in recent_60min)
        increase = current_moisture - min_moisture

        if increase >= 10.0:
            all_samples = recent_60min + [current_reading]
            all_samples.sort(key=lambda x: x.get("timestamp_ms"))

            positive_slopes = sum(1 for i in range(len(all_samples) - 1) if all_samples[i + 1].get("soil_moisture") > all_samples[i].get("soil_moisture"))

            if positive_slopes >= 2:
                start_time = min(r.get("timestamp_ms") for r in recent_60min)
                return Event(
                    hardware_id=hardware_id,
                    event_type=EventType.WATERING_EVENT,
                    start_time_ms=start_time,
                    end_time_ms=current_timestamp,
                    sensor_values={"soil_moisture_before": min_moisture, "soil_moisture_after": current_moisture, "increase_pct": increase},
                    detection_metadata={"detection_mode": "gradual_rise", "window_minutes": 60},
                    created_at_ms=int(time.time() * 1000)
                )

    return None



def detect_drying_cycle(readings: List[Dict[str, Any]], current_reading: Dict[str, Any]) -> Optional[Event]:
    """Detect drying cycle."""
    hardware_id = current_reading.get("hardware_id")
    current_timestamp = current_reading.get("timestamp_ms")
    current_moisture = current_reading.get("soil_moisture")

    if current_moisture is None or current_reading.get("soil_moisture_status", "ok") != "ok":
        return None

    six_hours_ago = current_timestamp - (6 * 60 * 60 * 1000)
    valid_readings = [
        r for r in readings
        if r.get("soil_moisture") is not None
        and r.get("soil_moisture_status", "ok") == "ok"
        and r.get("timestamp_ms") >= six_hours_ago
        and r.get("timestamp_ms") < current_timestamp
    ]

    if not valid_readings:
        return None

    all_samples = valid_readings + [current_reading]
    all_samples.sort(key=lambda x: x.get("timestamp_ms"))

    if len(all_samples) < 3:
        return None

    max_moisture = max(r.get("soil_moisture") for r in all_samples)
    total_drop = max_moisture - current_moisture

    if total_drop > 10.0:
        declining_count = sum(1 for i in range(len(all_samples) - 1) if all_samples[i + 1].get("soil_moisture") < all_samples[i].get("soil_moisture"))

        if declining_count >= (len(all_samples) - 1) * 0.7:
            return Event(
                hardware_id=hardware_id,
                event_type=EventType.DRYING_CYCLE,
                start_time_ms=all_samples[0].get("timestamp_ms"),
                end_time_ms=current_timestamp,
                sensor_values={"soil_moisture_start": max_moisture, "soil_moisture_end": current_moisture, "total_drop_pct": total_drop},
                detection_metadata={"window_hours": 6},
                created_at_ms=int(time.time() * 1000)
            )

    return None


def detect_temperature_stress(current_reading: Dict[str, Any]) -> Optional[Event]:
    """Detect temperature stress."""
    temperature = current_reading.get("temperature")

    if temperature is None or current_reading.get("temperature_status", "ok") != "ok":
        return None

    if temperature > 35.0 or temperature < 5.0:
        stress_type = "high" if temperature > 35.0 else "low"
        return Event(
            hardware_id=current_reading.get("hardware_id"),
            event_type=EventType.TEMPERATURE_STRESS,
            start_time_ms=current_reading.get("timestamp_ms"),
            end_time_ms=current_reading.get("timestamp_ms"),
            sensor_values={"temperature": temperature, "stress_type": stress_type},
            detection_metadata={},
            created_at_ms=int(time.time() * 1000)
        )

    return None


def detect_humidity_anomaly(readings: List[Dict[str, Any]], current_reading: Dict[str, Any]) -> Optional[Event]:
    """Detect humidity anomaly."""
    current_humidity = current_reading.get("humidity")

    if current_humidity is None or current_reading.get("humidity_status", "ok") != "ok":
        return None

    one_hour_ago = current_reading.get("timestamp_ms") - (60 * 60 * 1000)
    valid_readings = [
        r for r in readings
        if r.get("humidity") is not None
        and r.get("humidity_status", "ok") == "ok"
        and r.get("timestamp_ms") >= one_hour_ago
        and r.get("timestamp_ms") < current_reading.get("timestamp_ms")
    ]

    if not valid_readings:
        return None

    all_humidity = [r.get("humidity") for r in valid_readings] + [current_humidity]
    humidity_change = max(all_humidity) - min(all_humidity)

    if humidity_change > 20.0:
        return Event(
            hardware_id=current_reading.get("hardware_id"),
            event_type=EventType.HUMIDITY_ANOMALY,
            start_time_ms=one_hour_ago,
            end_time_ms=current_reading.get("timestamp_ms"),
            sensor_values={"change_pct": humidity_change},
            detection_metadata={},
            created_at_ms=int(time.time() * 1000)
        )

    return None


def detect_environmental_change(readings: List[Dict[str, Any]], current_reading: Dict[str, Any]) -> Optional[Event]:
    """Detect environmental change."""
    current_temp = current_reading.get("temperature")
    current_humidity = current_reading.get("humidity")
    current_pressure = current_reading.get("pressure")

    if (current_temp is None or current_reading.get("temperature_status", "ok") != "ok" or
        current_humidity is None or current_reading.get("humidity_status", "ok") != "ok" or
        current_pressure is None or current_reading.get("pressure_status", "ok") != "ok"):
        return None

    two_hours_ago = current_reading.get("timestamp_ms") - (2 * 60 * 60 * 1000)
    valid_readings = [
        r for r in readings
        if (r.get("temperature") is not None and r.get("temperature_status", "ok") == "ok" and
            r.get("humidity") is not None and r.get("humidity_status", "ok") == "ok" and
            r.get("pressure") is not None and r.get("pressure_status", "ok") == "ok" and
            r.get("timestamp_ms") >= two_hours_ago and
            r.get("timestamp_ms") < current_reading.get("timestamp_ms"))
    ]

    if not valid_readings:
        return None

    all_temps = [r.get("temperature") for r in valid_readings] + [current_temp]
    all_humidity = [r.get("humidity") for r in valid_readings] + [current_humidity]
    all_pressure = [r.get("pressure") for r in valid_readings] + [current_pressure]

    temp_change = max(all_temps) - min(all_temps)
    humidity_change = max(all_humidity) - min(all_humidity)
    pressure_change = max(all_pressure) - min(all_pressure)

    if temp_change > 10.0 and humidity_change > 15.0 and pressure_change > 10.0:
        return Event(
            hardware_id=current_reading.get("hardware_id"),
            event_type=EventType.ENVIRONMENTAL_CHANGE,
            start_time_ms=two_hours_ago,
            end_time_ms=current_reading.get("timestamp_ms"),
            sensor_values={"temperature_change": temp_change, "humidity_change": humidity_change, "pressure_change": pressure_change},
            detection_metadata={},
            created_at_ms=int(time.time() * 1000)
        )

    return None



def get_cooldown_period(event_type: EventType) -> int:
    """Get cooldown period."""
    cooldowns = {
        EventType.WATERING_EVENT: COOLDOWN_WATERING,
        EventType.TEMPERATURE_STRESS: COOLDOWN_TEMPERATURE_STRESS,
        EventType.HUMIDITY_ANOMALY: COOLDOWN_HUMIDITY_ANOMALY,
        EventType.ENVIRONMENTAL_CHANGE: COOLDOWN_ENVIRONMENTAL_CHANGE,
        EventType.DRYING_CYCLE: 0
    }
    return cooldowns.get(event_type, 0)


def check_cooldown(hardware_id: str, event_type: EventType, current_time_ms: int) -> bool:
    """Check if event type is in cooldown."""
    cooldown_ms = get_cooldown_period(event_type)
    if cooldown_ms == 0:
        return False

    try:
        table = dynamodb_resource.Table(EVENTS_TABLE)
        since_ms = current_time_ms - cooldown_ms

        response = table.query(
            KeyConditionExpression="hardware_id = :hw_id AND start_time_ms >= :since",
            FilterExpression="event_type = :event_type",
            ExpressionAttributeValues={":hw_id": hardware_id, ":since": since_ms, ":event_type": event_type.value},
            Limit=1
        )

        return len(response.get("Items", [])) > 0
    except ClientError as e:
        logger.error("Error checking cooldown", extra={"error": str(e)})
        return False


def persist_event(event: Event) -> bool:
    """Persist event."""
    try:
        table = dynamodb_resource.Table(EVENTS_TABLE)
        table.put_item(
            Item=event.to_dynamodb_item(),
            ConditionExpression="attribute_not_exists(hardware_id) AND attribute_not_exists(start_time_ms)"
        )
        return True
    except ClientError as e:
        if e.response["Error"]["Code"] == "ConditionalCheckFailedException":
            return False
        logger.error("Error persisting event", extra={"error": str(e)})
        raise


def update_device_status_after_event(hardware_id: str, event_time_ms: int) -> None:
    """Update device status."""
    try:
        now_ms = int(time.time() * 1000)
        dynamodb.update_item(
            TableName=DEVICE_STATUS_TABLE,
            Key={"hardware_id": {"S": hardware_id}},
            UpdateExpression="SET last_event_detected_at_ms = :now, last_processed_event_time_ms = :event_time, updated_at_ms = :now",
            ExpressionAttributeValues={":now": {"N": str(now_ms)}, ":event_time": {"N": str(event_time_ms)}}
        )
    except ClientError as e:
        logger.error("Error updating device status", extra={"error": str(e)})


def create_insight_request_for_critical_event(hardware_id: str, event_type: EventType) -> None:
    """Create insight request for critical events."""
    if event_type != EventType.TEMPERATURE_STRESS:
        return

    try:
        now_ms = int(time.time() * 1000)
        one_hour_ago = now_ms - (60 * 60 * 1000)
        one_day_ago = now_ms - (24 * 60 * 60 * 1000)

        table = dynamodb_resource.Table(INSIGHT_REQUESTS_TABLE)

        response = table.query(
            KeyConditionExpression="hardware_id = :hw_id AND request_time_ms >= :since",
            FilterExpression="#status = :pending",
            ExpressionAttributeNames={"#status": "status"},
            ExpressionAttributeValues={":hw_id": hardware_id, ":since": one_hour_ago, ":pending": "pending"},
            Limit=1
        )

        if response.get("Items"):
            return

        response = table.query(
            KeyConditionExpression="hardware_id = :hw_id AND request_time_ms >= :since",
            FilterExpression="request_type = :event_type",
            ExpressionAttributeValues={":hw_id": hardware_id, ":since": one_day_ago, ":event_type": "event"}
        )

        if len(response.get("Items", [])) >= 6:
            return

        table.put_item(Item={"hardware_id": hardware_id, "request_time_ms": now_ms, "request_type": "event", "event_type": event_type.value, "status": "pending"})
    except ClientError as e:
        logger.error("Error creating insight request", extra={"error": str(e)})
