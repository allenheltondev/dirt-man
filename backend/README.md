# Serverless Backend API

This directory contains the serverless backend API for ESP32 environmental monitoring devices. The system consists of two AWS Lambda functions written in Rust, deployed using AWS SAM.

## Architecture

- **Data Plane API**: Handles device registration and sensor data ingestion
- **Control Plane API**: Provides administrative operations (API key management, device queries)
- **DynamoDB Tables**: Stores devices, API keys, sensor readings, and processed batches
- **Function URLs**: Public
          # This file
```

## Prerequisites

### Required Tools

1. **Rust** (1.70 or later)
   ```bash
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   ```

2. **cargo-lambda** (for building Lambda functions)
   ```bash
   # Via pip
   pip install cargo-lambda

   # Or via Homebrew (macOS)
   brew install cargo-lambda
   ```

3. **AWS CLI** (2.x)
   ```bash
   # macOS
   brew install awscli

   # Or download from: https://aws.amazon.com/cli/
   ```

4. **AWS SAM CLI**
   ```bash
   # macOS
   brew install aws-sam-cli

   # Or follow: https://docs.aws.amazon.com/serverless-application-model/latest/developerguide/install-sam-cli.html
   ```

### AWS Configuration

1. Configure AWS credentials:
   ```bash
   aws configure
   ```

2. Set your AWS region (optional, defaults to us-east-1):
   ```bash
   export AWS_REGION=us-east-1
   ```

## Building

### Build Both Lambda Functions

```bash
cd backend
./build.sh
```

This script:
- Builds the shared crate
- Builds the Data Plane Lambda (ARM64)
- Builds the Control Plane Lambda (ARM64)
- Outputs binaries to `target/lambda/` directories

### Build Individual Functions

```bash
# Data Plane only
cd data
cargo lambda build --release --arm64

# Control Plane only
cd control
cargo lambda build --release --arm64
```

## Deployment

### Environment-Specific Deployment

The system supports three environments: `dev`, `staging`, and `prod`.

#### 1. Set Required Secrets

```bash
# Admin token for Control Plane API authentication
export ADMIN_TOKEN="your-secure-admin-token-here"

# Pepper for API key hashing (keep this secret!)
export API_KEY_PEPPER="your-secure-pepper-here"

# Optional: CORS origin for Control Plane (defaults to *)
export CORS_ALLOWED_ORIGIN="https://your-domain.com"
```

**Security Note**: Use strong, randomly generated values for production. Example:
```bash
export ADMIN_TOKEN=$(openssl rand -hex 32)
export API_KEY_PEPPER=$(openssl rand -hex 32)
```

#### 2. Deploy to Environment

```bash
cd backend

# Deploy to dev (default)
./deploy.sh dev

# Deploy to staging
./deploy.sh staging

# Deploy to prod
./deploy.sh prod
```

The deployment script:
- Validates environment and credentials
- Builds Lambda functions
- Runs SAM build
- Deploys the CloudFormation stack
- Displays stack outputs (Function URLs, table names)

### Manual Deployment

If you prefer manual control:

```bash
# Build
./build.sh

# SAM build
sam build

# SAM deploy (guided)
sam deploy --guided

# Or deploy with parameters
sam deploy \
  --stack-name esp32-backend-dev \
  --region us-east-1 \
  --capabilities CAPABILITY_IAM \
  --parameter-overrides \
      Environment=dev \
      AdminToken="$ADMIN_TOKEN" \
      ApiKeyPepper="$API_KEY_PEPPER" \
      CorsAllowedOrigin="*" \
      LogRetentionDays=7 \
  --resolve-s3
```

## Environment-Specific Configuration

### Development (dev)
- Log retention: 7 days
- CORS: Allow all origins (*)
- Recommended for testing and development

### Staging (staging)
- Log retention: 14 days
- CORS: Allow all origins (*)
- Recommended for pre-production testing

### Production (prod)
- Log retention: 30 days
- CORS: Specific origin (set via CORS_ALLOWED_ORIGIN)
- Point-in-time recovery enabled for critical tables
- Use strong, unique secrets

## Stack Outputs

After deployment, the stack provides these outputs:

- **DataPlaneFunctionUrl**: HTTPS endpoint for device communication
- **ControlPlaneFunctionUrl**: HTTPS endpoint for administrative operations
- **DevicesTableName**: DynamoDB table name for devices
- **ApiKeysTableName**: DynamoDB table name for API keys
- **ProcessedBatchesTableName**: DynamoDB table name for batch tracking
- **DeviceReadingsTableName**: DynamoDB table name for sensor readings

## Testing the Deployment

### 1. Create an API Key

```bash
# Get the Control Plane URL from stack outputs
CONTROL_URL="<your-control-plane-url>"

# Create an API key
curl -X POST "$CONTROL_URL/api-keys" \
  -H "Authorization: Bearer $ADMIN_TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"description": "Test API key"}'

# Save the returned api_key value (shown only once!)
export API_KEY="<returned-api-key>"
```

### 2. Register a Device

```bash
# Get the Data Plane URL from stack outputs
DATA_URL="<your-data-plane-url>"

# Register a device
curl -X POST "$DATA_URL/register" \
  -H "X-API-Key: $API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "hardware_id": "AA:BB:CC:DD:EE:FF",
    "boot_id": "550e8400-e29b-41d4-a716-446655440000",
    "firmware_version": "1.0.0",
    "capabilities": {
      "sensors": ["bme280"],
      "features": {}
    }
  }'
```

### 3. Submit Sensor Data

```bash
curl -X POST "$DATA_URL/data" \
  -H "X-API-Key: $API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "readings": [{
      "batch_id": "AA:BB:CC:DD:EE:FF_550e8400-e29b-41d4-a716-446655440000_1704067200000_1704067800000",
      "hardware_id": "AA:BB:CC:DD:EE:FF",
      "boot_id": "550e8400-e29b-41d4-a716-446655440000",
      "firmware_version": "1.0.0",
      "timestamp_ms": 1704067800000,
      "sensors": {
        "bme280_temp_c": 22.5,
        "humidity_pct": 45.2
      },
      "sensor_status": {
        "bme280": "ok"
      }
    }]
  }'
```

### 4. Query Devices

```bash
# List all devices
curl "$CONTROL_URL/devices" \
  -H "Authorization: Bearer $ADMIN_TOKEN"

# Get device details
curl "$CONTROL_URL/devices/AA:BB:CC:DD:EE:FF" \
  -H "Authorization: Bearer $ADMIN_TOKEN"

# Get device readings
curl "$CONTROL_URL/devices/AA:BB:CC:DD:EE:FF/readings?limit=10" \
  -H "Authorization: Bearer $ADMIN_TOKEN"
```

## Monitoring

### CloudWatch Logs

Logs are available in CloudWatch:
- Data Plane: `/aws/lambda/<environment>-data-plane-api`
- Control Plane: `/aws/lambda/<environment>-control-plane-api`

### View Logs

```bash
# Data Plane logs
aws logs tail /aws/lambda/dev-data-plane-api --follow

# Control Plane logs
aws logs tail /aws/lambda/dev-control-plane-api --follow
```

### CloudWatch Metrics

Lambda automatically publishes metrics:
- Invocations
- Duration
- Errors
- Throttles
- Concurrent executions

## Updating the Stack

To update an existing deployment:

```bash
# Make your code changes
# Rebuild
./build.sh

# Redeploy
./deploy.sh <environment>
```

SAM will create a CloudFormation change set and apply only the necessary updates.

## Deleting the Stack

To remove all resources:

```bash
aws cloudformation delete-stack \
  --stack-name esp32-backend-<environment> \
  --region <region>
```

**Warning**: This will delete all DynamoDB tables and their data. Ensure you have backups if needed.

## Troubleshooting

### Build Failures

1. **cargo-lambda not found**
   ```bash
   pip install cargo-lambda
   ```

2. **Rust compilation errors**
   - Ensure Rust 1.70+ is installed
   - Run `cargo clean` and rebuild

### Deployment Failures

1. **AWS credentials not configured**
   ```bash
   aws configure
   ```

2. **Missing secrets**
   - Ensure `ADMIN_TOKEN` and `API_KEY_PEPPER` are set
   - Check with: `echo $ADMIN_TOKEN`

3. **Stack already exists**
   - Use `sam deploy` without `--guided` to update
   - Or delete the existing stack first

4. **Insufficient permissions**
   - Ensure your IAM user/role has permissions for:
     - Lambda
     - DynamoDB
     - CloudFormation
     - IAM (for creating roles)
     - S3 (for SAM artifacts)

### Runtime Errors

1. **Check CloudWatch Logs**
   ```bash
   aws logs tail /aws/lambda/<environment>-data-plane-api --follow
   ```

2. **Enable X-Ray tracing** (already enabled in template)
   - View traces in AWS X-Ray console

3. **Test locally with SAM**
   ```bash
   sam local start-api
   ```

## Development Workflow

1. Make code changes in `data/`, `control/`, or `shared/`
2. Run tests: `cargo test`
3. Build: `./build.sh`
4. Deploy to dev: `./deploy.sh dev`
5. Test with curl or Postman
6. Review CloudWatch logs
7. Iterate

## Security Best Practices

1. **Use strong secrets**: Generate with `openssl rand -hex 32`
2. **Rotate secrets regularly**: Update stack parameters
3. **Restrict CORS in production**: Set specific origin
4. **Monitor API usage**: Set up CloudWatch alarms
5. **Enable point-in-time recovery**: Already enabled for critical tables
6. **Use least-privilege IAM roles**: Already configured in template
7. **Store secrets in AWS Secrets Manager**: Consider for production

## Cost Optimization

- **ARM64 architecture**: ~20% cost savings vs x86
- **On-demand DynamoDB**: Pay only for what you use
- **Log retention**: Shorter retention in dev (7 days)
- **Lambda memory**: Optimized (512MB data, 256MB control)

## Additional Resources

- [AWS SAM Documentation](https://docs.aws.amazon.com/serverless-application-model/)
- [cargo-lambda Documentation](https://www.cargo-lambda.info/)
- [DynamoDB Best Practices](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/best-practices.html)
- [Lambda Best Practices](https://docs.aws.amazon.com/lambda/latest/dg/best-practices.html)

## Support

For issues or questions:
1. Check CloudWatch logs
2. Review the design document: `.kiro/specs/serverless-backend-api/design.md`
3. Review the requirements: `.kiro/specs/serverless-backend-api/requirements.md`
