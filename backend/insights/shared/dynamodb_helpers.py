"""
DynamoDB Stream record parsing utilities.
"""

from typing import Dict, Any
from boto3.dynamodb.types import TypeDeserializer


# Initialize the deserializer
deserializer = TypeDeserializer()


def parse_dynamodb_item(dynamodb_item: Dict[str, Any]) -> Dict[str, Any]:
    """
    Parse a DynamoDB item from stream format to Python dict.

    Uses boto3's TypeDeserializer for proper type conversion.

    Args:
        dynamodb_item: DynamoDB item in stream format (with type descriptors like 'S', 'N', etc.)

    Returns:
        Parsed item as Python dict with native types
    """
    if not dynamodb_item:
        return {}

    python_dict = {}
    for key, value in dynamodb_item.items():
        python_dict[key] = deserializer.deserialize(value)

    return python_dict


def extract_reading_from_stream_record(record: Dict[str, Any]) -> Dict[str, Any]:
    """
    Extract and parse a reading from a DynamoDB Stream record.

    Args:
        record: DynamoDB Stream record

    Returns:
        Parsed reading as Python dict
    """
    new_image = record.get("dynamodb", {}).get("NewImage", {})
    return parse_dynamodb_item(new_image)
