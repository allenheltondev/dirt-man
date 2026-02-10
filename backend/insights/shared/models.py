"""
Data models for Plant Insights and Events.

Contains domain models and persistence models:
- Reading
- SensorStatus
- Event
- Aggregate
- Insight
- DeviceProfile
- DeviceStatus
- Rollup
- InsightRequest
"""

from dataclasses import dataclass, field
from typing import Optional, List, Dict, Any
from enum import Enum
from datetime import datetime


class SensorStatus(str, Enum):
    """Sensor operational status."""
    OK = "ok"
    MISSING = "missing"
    STALE = "stale"
    OUT_OF_RANGE = "out_of_range"
    NOISY = "noisy"


class HealthCategory(str, Enum):
    """Device health category based on last_seen_ingest_time."""
    HEALTHY = "healthy"  # within 2 hours
    STALE = "stale"      # 2-6 hours
    MISSING = "missing"  # > 6 hours
    FAILING = "failing"  # last_error_at within 24 hours


class SensorStatusSummary(str, Enum):
    """Overall sensor status summary."""
    OK = "ok"
    DEGRADED = "degraded"
    MISSING = "missing"


@dataclass
class ErrorRecord:
    """Individual error record in Device Status."""
    timestamp_ms: int
    error_code: str
    error_message: str  # truncated to 256 chars


@dataclass
class DeviceStatus:
    """
    Per-device health status record.

    Tracks processing state, data quality, and health metrics.
    """
    hardware_id: str
    last_seen_event_time_ms: Optional[int] = None
    last_seen_ingest_time_ms: Optional[int] = None
    expected_interval_sec: int = 300
    last_processed_event_time_ms: Optional[int] = None
    ingest_event_skew_seconds: Optional[float] = None
    pipeline_lag_seconds: Optional[float] = None
    coverage_pct_last_hour: Optional[float] = None
    sensor_status_summary: SensorStatusSummary = SensorStatusSummary.OK
    last_event_detected_at_ms: Optional[int] = None
    last_aggregate_computed_at_ms: Optional[int] = None
    last_insight_generated_at_ms: Optional[int] = None
    last_error_at_ms: Optional[int] = None
    last_error_code: Optional[str] = None
    last_errors: List[ErrorRecord] = field(default_factory=list)
    updated_at_ms: Optional[int] = None

    def to_dynamodb_item(self) -> Dict[str, Any]:
        """Convert to DynamoDB item format."""
        item = {
            "hardware_id": self.hardware_id,
            "expected_interval_sec": self.expected_interval_sec,
            "sensor_status_summary": self.sensor_status_summary.value,
        }

        if self.last_seen_event_time_ms is not None:
            item["last_seen_event_time_ms"] = self.last_seen_event_time_ms
        if self.last_seen_ingest_time_ms is not None:
            item["last_seen_ingest_time_ms"] = self.last_seen_ingest_time_ms
        if self.last_processed_event_time_ms is not None:
            item["last_processed_event_time_ms"] = self.last_processed_event_time_ms
        if self.ingest_event_skew_seconds is not None:
            item["ingest_event_skew_seconds"] = self.ingest_event_skew_seconds
        if self.pipeline_lag_seconds is not None:
            item["pipeline_lag_seconds"] = self.pipeline_lag_seconds
        if self.coverage_pct_last_hour is not None:
            item["coverage_pct_last_hour"] = self.coverage_pct_last_hour
        if self.last_event_detected_at_ms is not None:
            item["last_event_detected_at_ms"] = self.last_event_detected_at_ms
        if self.last_aggregate_computed_at_ms is not None:
            item["last_aggregate_computed_at_ms"] = self.last_aggregate_computed_at_ms
        if self.last_insight_generated_at_ms is not None:
            item["last_insight_generated_at_ms"] = self.last_insight_generated_at_ms
        if self.last_error_at_ms is not None:
            item["last_error_at_ms"] = self.last_error_at_ms
        if self.last_error_code is not None:
            item["last_error_code"] = self.last_error_code
        if self.last_errors:
            item["last_errors"] = [
                {
                    "timestamp_ms": err.timestamp_ms,
                    "error_code": err.error_code,
                    "error_message": err.error_message
                }
                for err in self.last_errors
            ]
        if self.updated_at_ms is not None:
            item["updated_at_ms"] = self.updated_at_ms

        return item


@dataclass
class Reading:
    """Sensor reading from a device."""
    hardware_id: str
    batch_id: str
    timestamp_ms: int
    ingest_time_ms: int
    temperature: Optional[float] = None
    humidity: Optional[float] = None
    pressure: Optional[float] = None
    soil_moisture: Optional[float] = None
    temperature_status: SensorStatus = SensorStatus.OK
    humidity_status: SensorStatus = SensorStatus.OK
    pressure_status: SensorStatus = SensorStatus.OK
    soil_moisture_status: SensorStatus = SensorStatus.OK


class EventType(str, Enum):
    """Event types detected by the system."""
    WATERING_EVENT = "Watering_Event"
    DRYING_CYCLE = "Drying_Cycle"
    TEMPERATURE_STRESS = "Temperature_Stress"
    HUMIDITY_ANOMALY = "Humidity_Anomaly"
    ENVIRONMENTAL_CHANGE = "Environmental_Change"


@dataclass
class SensorStats:
    """Statistical summary for a sensor over a time window."""
    min: Optional[float] = None
    max: Optional[float] = None
    avg: Optional[float] = None
    stddev: Optional[float] = None
    valid_count: int = 0
    total_count: int = 0
    # Accumulator fields for incremental updates
    sum: float = 0.0
    sumsq: float = 0.0  # sum of squares for stddev calculation

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dict for DynamoDB storage."""
        result = {
            "valid_count": self.valid_count,
            "total_count": self.total_count,
            "sum": self.sum,
            "sumsq": self.sumsq
        }
        if self.min is not None:
            result["min"] = self.min
        if self.max is not None:
            result["max"] = self.max
        if self.avg is not None:
            result["avg"] = self.avg
        if self.stddev is not None:
            result["stddev"] = self.stddev
        return result

    @staticmethod
    def from_dict(data: Dict[str, Any]) -> 'SensorStats':
        """Create from dict loaded from DynamoDB."""
        return SensorStats(
            min=data.get("min"),
            max=data.get("max"),
            avg=data.get("avg"),
            stddev=data.get("stddev"),
            valid_count=data.get("valid_count", 0),
            total_count=data.get("total_count", 0),
            sum=data.get("sum", 0.0),
            sumsq=data.get("sumsq", 0.0)
        )


@dataclass
class Aggregate:
    """Time-series aggregate for a device and time window."""
    hardware_id: str
    window_type: str  # "hourly", "daily", "weekly"
    window_start_ms: int
    window_end_ms: int
    temperature_stats: SensorStats = field(default_factory=SensorStats)
    humidity_stats: SensorStats = field(default_factory=SensorStats)
    pressure_stats: SensorStats = field(default_factory=SensorStats)
    soil_moisture_stats: SensorStats = field(default_factory=SensorStats)
    computed_at_ms: Optional[int] = None
    is_complete: bool = False

    def get_device_window_key(self) -> str:
        """Get the composite partition key for DynamoDB."""
        return f"{self.hardware_id}#{self.window_type}"

    def to_dynamodb_item(self) -> Dict[str, Any]:
        """Convert to DynamoDB item format."""
        item = {
            "device_window": self.get_device_window_key(),
            "window_start_ms": self.window_start_ms,
            "window_end_ms": self.window_end_ms,
            "window_type": self.window_type,
            "hardware_id": self.hardware_id,
            "temperature_stats": self.temperature_stats.to_dict(),
            "humidity_stats": self.humidity_stats.to_dict(),
            "pressure_stats": self.pressure_stats.to_dict(),
            "soil_moisture_stats": self.soil_moisture_stats.to_dict(),
            "is_complete": self.is_complete
        }
        if self.computed_at_ms is not None:
            item["computed_at_ms"] = self.computed_at_ms
        return item


class EventType(str, Enum):
    """Event types detected by the system."""
    WATERING_EVENT = "Watering_Event"
    DRYING_CYCLE = "Drying_Cycle"
    TEMPERATURE_STRESS = "Temperature_Stress"
    HUMIDITY_ANOMALY = "Humidity_Anomaly"
    ENVIRONMENTAL_CHANGE = "Environmental_Change"


@dataclass
class Event:
    """Detected event from sensor data."""
    hardware_id: str
    event_type: EventType
    start_time_ms: int
    end_time_ms: int
    sensor_values: Dict[str, Any]
    detection_metadata: Dict[str, Any]
    created_at_ms: Optional[int] = None

    def to_dynamodb_item(self) -> Dict[str, Any]:
        """Convert to DynamoDB item format."""
        item = {
            "hardware_id": self.hardware_id,
            "start_time_ms": self.start_time_ms,
            "end_time_ms": self.end_time_ms,
            "event_type": self.event_type.value,
            "sensor_values": self.sensor_values,
            "detection_metadata": self.detection_metadata,
        }
        if self.created_at_ms is not None:
            item["created_at_ms"] = self.created_at_ms
        return item


@dataclass
class DeviceProfile:
    """
    Per-device learned patterns and configuration.

    Stores plant-specific information and learned behavioral patterns.
    """
    hardware_id: str
    plant_type: Optional[str] = None
    soil_type: Optional[str] = None
    pot_size_liters: Optional[float] = None
    expected_interval_sec: int = 300
    baseline_moisture_range: Optional[Dict[str, float]] = None  # {"min": x, "max": y}
    typical_watering_interval_sec: Optional[int] = None
    last_watering_events: List[int] = field(default_factory=list)  # List of timestamp_ms
    updated_at_ms: Optional[int] = None

    def to_dynamodb_item(self) -> Dict[str, Any]:
        """Convert to DynamoDB item format."""
        item = {
            "hardware_id": self.hardware_id,
            "expected_interval_sec": self.expected_interval_sec,
        }
        if self.plant_type is not None:
            item["plant_type"] = self.plant_type
        if self.soil_type is not None:
            item["soil_type"] = self.soil_type
        if self.pot_size_liters is not None:
            item["pot_size_liters"] = self.pot_size_liters
        if self.baseline_moisture_range is not None:
            item["baseline_moisture_range"] = self.baseline_moisture_range
        if self.typical_watering_interval_sec is not None:
            item["typical_watering_interval_sec"] = self.typical_watering_interval_sec
        if self.last_watering_events:
            item["last_watering_events"] = self.last_watering_events
        if self.updated_at_ms is not None:
            item["updated_at_ms"] = self.updated_at_ms
        return item

    @staticmethod
    def from_dynamodb_item(item: Dict[str, Any]) -> 'DeviceProfile':
        """Create from DynamoDB item."""
        return DeviceProfile(
            hardware_id=item["hardware_id"],
            plant_type=item.get("plant_type"),
            soil_type=item.get("soil_type"),
            pot_size_liters=item.get("pot_size_liters"),
            expected_interval_sec=item.get("expected_interval_sec", 300),
            baseline_moisture_range=item.get("baseline_moisture_range"),
            typical_watering_interval_sec=item.get("typical_watering_interval_sec"),
            last_watering_events=item.get("last_watering_events", []),
            updated_at_ms=item.get("updated_at_ms")
        )


class ConfidenceLevel(str, Enum):
    """Insight confidence level."""
    LOW = "low"
    MEDIUM = "medium"
    HIGH = "high"


class TrendClassification(str, Enum):
    """Overall trend classification."""
    IMPROVING = "improving"
    DECLINING = "declining"
    STABLE = "stable"


@dataclass
class Recommendation:
    """Individual recommendation in an insight."""
    action: str
    reason: str
    urgency: str  # "low", "medium", "high"

    def to_dict(self) -> Dict[str, str]:
        """Convert to dict for storage."""
        return {
            "action": self.action,
            "reason": self.reason,
            "urgency": self.urgency
        }

    @staticmethod
    def from_dict(data: Dict[str, str]) -> 'Recommendation':
        """Create from dict."""
        return Recommendation(
            action=data["action"],
            reason=data["reason"],
            urgency=data["urgency"]
        )


@dataclass
class Insight:
    """
    LLM-generated natural language insight.

    Contains analysis, recommendations, and evidence references.
    """
    hardware_id: str
    timestamp_ms: int
    summary: str
    recommendations: List[Recommendation]
    confidence: ConfidenceLevel
    trend: TrendClassification
    growth_stage_suggestion: Optional[str] = None
    evidence: Optional[Dict[str, Any]] = None  # aggregate_refs, event_refs, profile_snapshot
    llm_model: Optional[str] = None
    generation_duration_ms: Optional[int] = None

    def to_dynamodb_item(self) -> Dict[str, Any]:
        """Convert to DynamoDB item format."""
        item = {
            "hardware_id": self.hardware_id,
            "timestamp_ms": self.timestamp_ms,
            "summary": self.summary,
            "recommendations": [rec.to_dict() for rec in self.recommendations],
            "confidence": self.confidence.value,
            "trend": self.trend.value,
        }
        if self.growth_stage_suggestion is not None:
            item["growth_stage_suggestion"] = self.growth_stage_suggestion
        if self.evidence is not None:
            item["evidence"] = self.evidence
        if self.llm_model is not None:
            item["llm_model"] = self.llm_model
        if self.generation_duration_ms is not None:
            item["generation_duration_ms"] = self.generation_duration_ms
        return item

    @staticmethod
    def from_dynamodb_item(item: Dict[str, Any]) -> 'Insight':
        """Create from DynamoDB item."""
        return Insight(
            hardware_id=item["hardware_id"],
            timestamp_ms=item["timestamp_ms"],
            summary=item["summary"],
            recommendations=[Recommendation.from_dict(rec) for rec in item["recommendations"]],
            confidence=ConfidenceLevel(item["confidence"]),
            trend=TrendClassification(item["trend"]),
            growth_stage_suggestion=item.get("growth_stage_suggestion"),
            evidence=item.get("evidence"),
            llm_model=item.get("llm_model"),
            generation_duration_ms=item.get("generation_duration_ms")
        )


class InsightRequestType(str, Enum):
    """Type of insight request."""
    SCHEDULED = "scheduled"
    EVENT = "event"


class InsightRequestStatus(str, Enum):
    """Status of insight request."""
    PENDING = "pending"
    PROCESSING = "processing"
    DONE = "done"
    FAILED = "failed"


@dataclass
class InsightRequest:
    """
    Request for insight generation.

    Used for rate limiting and batching insight generation.
    """
    hardware_id: str
    request_time_ms: int
    request_type: InsightRequestType
    status: InsightRequestStatus
    event_type: Optional[str] = None
    processed_at_ms: Optional[int] = None
    error_message: Optional[str] = None

    def to_dynamodb_item(self) -> Dict[str, Any]:
        """Convert to DynamoDB item format."""
        item = {
            "hardware_id": self.hardware_id,
            "request_time_ms": self.request_time_ms,
            "request_type": self.request_type.value,
            "status": self.status.value,
        }
        if self.event_type is not None:
            item["event_type"] = self.event_type
        if self.processed_at_ms is not None:
            item["processed_at_ms"] = self.processed_at_ms
        if self.error_message is not None:
            item["error_message"] = self.error_message
        return item

    @staticmethod
    def from_dynamodb_item(item: Dict[str, Any]) -> 'InsightRequest':
        """Create from DynamoDB item."""
        return InsightRequest(
            hardware_id=item["hardware_id"],
            request_time_ms=item["request_time_ms"],
            request_type=InsightRequestType(item["request_type"]),
            status=InsightRequestStatus(item["status"]),
            event_type=item.get("event_type"),
            processed_at_ms=item.get("processed_at_ms"),
            error_message=item.get("error_message")
        )
