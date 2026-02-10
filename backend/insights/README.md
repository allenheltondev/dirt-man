# Python Lambda Functions - Insights

This directory contains all Python Lambda functions and shared modules for the Plant Insights system.

## Directory Structure

```
insights/
├── functions/          # Lambda handler files (flat structure)
├── shared/            # Shared utility modules
├── tests/             # Test suite (unit, property, and integration tests)
├── requirements.txt   # Shared dependencies
└── README.md         # This file
```

## Overview

The `insights/` directory follows a flat, simplified structure that improves maintainability and reduces import complexity. Each Lambda function is a single Python file in the `functions/` directory, and shared code is organized as simple modules in the `shared/` directory.

## Lambda Functions

Lambda handler files are located in `functions/` and follow the naming convention `{function_purpose}.py`. Each handler file contains a `lambda_handler` function that serves as the entry point for AWS Lambda.

### Available Functions

- `event_detector.py` - Detects plant events from readings stream
- `device_status_updater.py` - Updates device status from readings stream
- `aggregator.py` - Aggregates readings into time windows
- `insight_generator.py` - Generates LLM-based insights
- `rollup_updater.py` - Updates operational metrics rollups
- `api.py` - API endpoint handler

## Shared Modules

Shared modules are located in `shared/` and can be imported using standard Python imports:

```python
from shared.dynamodb_helpers import parse_dynamodb_item
from shared.idempotency import generate_reading_id
from shared.models import ReadingModel
```

### Available Modules

- `dynamodb_helpers.py` - DynamoDB stream parsing and utilities
- `idempotency.py` - Idempotency key generation and tracking
- `device_status.py` - Device status management utilities
- `models.py` - Pydantic models for data validation
- `sensor_validation.py` - Sensor data validation functions
- `time_utils.py` - Time manipulation utilities

## Testing

Tests are located in the `tests/` directory and follow the naming convention `test_{module}_{type}.py`:

- `test_{module}_unit.py` - Unit tests for specific examples and edge cases
- `test_{module}_properties.py` - Property-based tests for universal properties
- `test_{module}_integration.py` - Integration tests with AWS services

### Running Tests

```bash
# Navigate to insights directory
cd backend/insights

# Run all tests
pytest tests/ -v

# Run only unit tests
pytest tests/ -v -k "unit"

# Run only property tests
pytest tests/ -v -k "properties"

# Run tests for a specific module
pytest tests/test_event_detector_unit.py -v

# Run with coverage report
pytest tests/ --cov=functions --cov=shared --cov-report=html

# Run with verbose output and show print statements
pytest tests/ -v -s
```

### Test Structure

Each test file should follow this structure:

```python
"""Tests for {module_name}."""

import pytest
from functions.my_function import lambda_handler


class TestMyFunction:
    """Test suite for my_function."""

    def test_basic_functionality(self):
        """Test basic functionality works correctly."""
        # Arrange
        event = {"key": "value"}

        # Act
        result = lambda_handler(event, None)

        # Assert
        assert result["statusCode"] == 200

    def test_error_handling(self):
        """Test error handling for invalid input."""
        event = {}

        with pytest.raises(ValueError):
            lambda_handler(event, None)
```

### Property-Based Testing

Property-based tests use Hypothesis to validate universal properties:

```python
"""Property-based tests for {module_name}."""

from hypothesis import given, strategies as st
from functions.my_function import process_data


@given(st.text())
def test_process_data_never_returns_none(input_data):
    """Property: process_data should never return None for any input."""
    result = process_data(input_data)
    assert result is not None
```

## Dependencies

Dependencies are organized into three files:

- `requirements.txt` - Shared dependencies used across multiple Lambda functions
- `functions/requirements.txt` - Function-specific dependencies
- `tests/requirements-test.txt` - Test-specific dependencies

## Adding New Lambda Functions

To add a new Lambda function, follow these steps:

### 1. Create the Handler File

Create a new file in `functions/` following the naming convention: `{function_purpose}.py`

```python
"""
Module docstring describing the Lambda function purpose.
"""

import os
from typing import Dict, Any
from aws_lambda_powertools import Logger
from aws_lambda_powertools.utilities.typing import LambdaContext

# Import shared modules using standard imports
from shared.dynamodb_helpers import parse_dynamodb_item
from shared.models import ReadingModel

logger = Logger()


@logger.inject_lambda_context
def lambda_handler(event: Dict[str, Any], context: LambdaContext) -> Dict[str, Any]:
    """
    Lambda handler function.

    Args:
        event: Lambda event (e.g., DynamoDB Stream record, API Gateway request)
        context: Lambda context object

    Returns:
        Response dictionary
    """
    logger.info("Processing event", extra={"event": event})

    # Your logic here

    return {
        "statusCode": 200,
        "body": "Success"
    }
```

### 2. Add Tests

Create test files in `tests/` directory:

- `test_{function_purpose}_unit.py` - Unit tests for specific examples
- `test_{function_purpose}_properties.py` - Property-based tests (optional)

```python
# tests/test_my_function_unit.py
import pytest
from functions.my_function import lambda_handler


def test_lambda_handler_success():
    """Test successful event processing."""
    event = {"key": "value"}
    context = None

    result = lambda_handler(event, context)

    assert result["statusCode"] == 200
```

### 3. Update SAM Template

Add the function definition to `backend/template.yaml`:

```yaml
MyNewFunction:
  Type: AWS::Serverless::Function
  Properties:
    Runtime: python3.11
    CodeUri: insights/functions/
    Handler: my_function.lambda_handler
    Description: Description of what this function does
    Timeout: 60
    MemorySize: 512
    Environment:
      Variables:
        MY_TABLE: !Ref MyTable
    Policies:
      - AWSLambdaBasicExecutionRole
      - Version: 2012-10-17
        Statement:
          - Effect: Allow
            Action:
              - dynamodb:PutItem
              - dynamodb:GetItem
            Resource: !GetAtt MyTable.Arn
    Events:
      MyEvent:
        Type: DynamoDB  # or Api, Schedule, etc.
        Properties:
          Stream: !GetAtt SourceTable.StreamArn
          StartingPosition: LATEST
          BatchSize: 100
```

### 4. Build and Test

```bash
# Run tests locally
cd backend/insights
pytest tests/test_my_function_unit.py -v

# Build with SAM
cd backend
sam build

# Test locally with SAM (optional)
sam local invoke MyNewFunction -e events/test-event.json
```

## Deployment

Lambda functions are deployed using AWS SAM (Serverless Application Model).

### Prerequisites

- AWS CLI configured with appropriate credentials
- AWS SAM CLI installed
- Python 3.11 or later

### Build and Deploy

```bash
# Navigate to backend directory
cd backend

# Build the application (compiles and packages Lambda functions)
sam build

# Deploy to AWS (first time - interactive)
sam deploy --guided

# Deploy to AWS (subsequent deployments)
sam deploy
```

### SAM Template Configuration

Each Lambda function is defined in `backend/template.yaml` with the following structure:

```yaml
FunctionName:
  Type: AWS::Serverless::Function
  Properties:
    Runtime: python3.11
    CodeUri: insights/functions/
    Handler: {filename}.lambda_handler
    Description: Function description
    Timeout: 60
    MemorySize: 512
    Environment:
      Variables:
        TABLE_NAME: !Ref TableName
    Policies:
      - AWSLambdaBasicExecutionRole
      - DynamoDBCrudPolicy:
          TableName: !Ref TableName
    Events:
      StreamEvent:
        Type: DynamoDB
        Properties:
          Stream: !GetAtt SourceTable.StreamArn
          StartingPosition: LATEST
          BatchSize: 100
```

### Validating the Template

```bash
# Validate SAM template syntax
sam validate

# Validate template and check for issues
sam validate --lint
```

## Import Pattern

All imports use standard Python module imports without path manipulation:

```python
# ✓ Correct
from shared.dynamodb_helpers import extract_reading_from_stream_record
from shared.idempotency import generate_reading_id

# ✗ Incorrect (do not use)
import sys
sys.path.insert(0, '...')
from ...shared.dynamodb_helpers import extract_reading_from_stream_record
```

## Development Guidelines

- **Keep Lambda handlers focused and single-purpose** - Each handler should do one thing well
- **Extract reusable logic into shared modules** - Don't duplicate code across handlers
- **Write both unit tests and property-based tests** - Unit tests validate specific examples, property tests validate universal behaviors
- **Follow the flat structure** - Avoid creating nested packages or subdirectories
- **Use type hints** - Improves code clarity, enables better IDE support, and catches errors early
- **Use AWS Lambda Powertools** - Provides structured logging, tracing, and metrics
- **Handle errors gracefully** - Always catch and log exceptions, return meaningful error responses
- **Keep functions stateless** - Don't rely on local state between invocations
- **Use environment variables for configuration** - Never hardcode table names, endpoints, or secrets
- **Log important events** - Use structured logging with context for debugging

### Code Style

Follow PEP 8 style guidelines:

```bash
# Format code with black
black functions/ shared/ tests/

# Check with flake8
flake8 functions/ shared/ tests/

# Type check with mypy
mypy functions/ shared/
```

## Troubleshooting

### Import Errors

If you encounter import errors:

1. **Verify directory structure** - Ensure `shared/__init__.py` exists
2. **Check import paths** - Use `from shared.module import function`, not relative imports
3. **Verify PYTHONPATH** - When running locally, ensure the insights directory is in your path
4. **Check for circular imports** - Shared modules should not import from functions

```bash
# Test imports without running code
python -c "from shared.dynamodb_helpers import parse_dynamodb_item"
```

### Test Discovery Issues

If pytest can't find your tests:

1. **Check test file naming** - Must start with `test_`
2. **Check test function naming** - Must start with `test_`
3. **Verify pytest.ini** - Should be in the insights directory
4. **Check for syntax errors** - Run `python -m py_compile tests/test_file.py`

```bash
# List all discovered tests without running them
pytest --collect-only
```

### SAM Build Failures

If `sam build` fails:

1. **Validate template** - Run `sam validate --lint`
2. **Check Python version** - Ensure Python 3.11 is installed
3. **Verify requirements.txt** - Check for dependency conflicts
4. **Check CodeUri paths** - Must be relative to template.yaml location

```bash
# Build with verbose output
sam build --debug

# Build specific function
sam build FunctionName
```

### Lambda Function Errors in AWS

If functions fail in AWS but work locally:

1. **Check CloudWatch Logs** - View detailed error messages
2. **Verify IAM permissions** - Ensure function has required policies
3. **Check environment variables** - Verify all required variables are set
4. **Verify timeout and memory** - Increase if function is timing out
5. **Check layer compatibility** - Ensure any layers are compatible with Python 3.11

```bash
# View recent logs
sam logs -n FunctionName --tail

# Invoke function with test event
sam remote invoke FunctionName --event-file events/test.json
```

### Common Issues

**Issue**: `ModuleNotFoundError: No module named 'shared'`
- **Solution**: Ensure you're running from the correct directory and `shared/__init__.py` exists

**Issue**: Tests pass locally but fail in CI/CD
- **Solution**: Check Python version consistency and install test dependencies

**Issue**: SAM deploy fails with "Template format error"
- **Solution**: Validate YAML syntax and indentation in template.yaml

**Issue**: Function times out in AWS
- **Solution**: Increase Timeout value in SAM template or optimize function code

## Additional Resources

- [AWS Lambda Developer Guide](https://docs.aws.amazon.com/lambda/latest/dg/welcome.html)
- [AWS SAM Documentation](https://docs.aws.amazon.com/serverless-application-model/latest/developerguide/what-is-sam.html)
- [AWS Lambda Powertools Python](https://docs.powertools.aws.dev/lambda/python/latest/)
- [Pytest Documentation](https://docs.pytest.org/)
- [Hypothesis Documentation](https://hypothesis.readthedocs.io/)

## Project Structure Summary

```
backend/
├── template.yaml                           # SAM template defining all resources
├── samconfig.toml                          # SAM deployment configuration
└── insights/                               # Python Lambda code root
    ├── functions/                          # Lambda handlers (flat)
    │   ├── event_detector.py              # Event detection handler
    │   ├── device_status_updater.py       # Device status handler
    │   ├── aggregator.py                  # Aggregation handler
    │   ├── insight_generator.py           # Insight generation handler
    │   ├── rollup_updater.py              # Rollup handler
    │   ├── api.py                         # API handler
    │   └── requirements.txt               # Function-specific dependencies
    ├── shared/                            # Shared modules (flat)
    │   ├── __init__.py                    # Package marker
    │   ├── dynamodb_helpers.py            # DynamoDB utilities
    │   ├── idempotency.py                 # Idempotency utilities
    │   ├── device_status.py               # Device status utilities
    │   ├── models.py                      # Pydantic models
    │   ├── sensor_validation.py           # Validation utilities
    │   └── time_utils.py                  # Time utilities
    ├── tests/                             # Test suite (flat)
    │   ├── conftest.py                    # Shared test fixtures
    │   ├── test_*_unit.py                 # Unit tests
    │   ├── test_*_properties.py           # Property-based tests
    │   ├── test_*_integration.py          # Integration tests
    │   └── requirements-test.txt          # Test dependencies
    ├── requirements.txt                   # Shared dependencies
    ├── pytest.ini                         # Pytest configuration
    └── README.md                          # This file
```
