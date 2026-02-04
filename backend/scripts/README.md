# Backend Scripts

This directory contains utility scripts for development, testing, and deployment of the serverless backend API.

## Testing Scripts

### Setup DynamoDB Local

Downloads and installs DynamoDB Local for integration testing:

```bash
./setup-dynamodb-local.sh
```

**What it does:**
- Downloads DynamoDB Local from AWS
- Extracts to `backend/dynamodb-local/`
- Verifies installation

**Requirements:**
- `curl` for downloading
- `tar` for extraction

### Start DynamoDB Local

Starts DynamoDB Local in the background:

```bash
./start-dynamodb-local.sh
```

**What it does:**
- Starts DynamoDB Local on port 8000
- Runs in-memory mode (no persistence)
- Saves PID to `dynamodb-local.pid`
- Logs output to `dynamodb-local.log`

**Requirements:**
- Java 8 or later
- DynamoDB Local installed (run `setup-dynamodb-local.sh` first)

**Configuration:**
- Port: 8000
- Mode: In-memory (data lost on restart)
- Shared DB: Enabled (all clients see same data)

### Stop DynamoDB Local

Stops the running DynamoDB Local instance:

```bash
./stop-dynamodb-local.sh
```

**What it does:**
- Reads PID from `dynamodb-local.pid`
- Gracefully stops the process
- Force kills if necessary
- Removes PID file

### Run All Tests

Runs the complete test suite (unit, property-based, and integration tests):

```bash
./run-all-tests.sh
```

**What it does:**
1. Starts DynamoDB Local if not running
2. Sets environment variables for tests
3. Runs unit tests for all crates
4. Runs property-based tests (100 iterations)
5. Runs integration tests (single-threaded)

**Environment Variables Set:**
- `AWS_ACCESS_KEY_ID=test`
- `AWS_SECRET_ACCESS_KEY=test`
- `AWS_REGION=us-east-1`
- `DYNAMODB_ENDPOINT=http://localhost:8000`

**Requirements:**
- DynamoDB Local installed
- Rust toolchain installed
- All dependencies resolved (`cargo build`)

## Usage Examples

### First-Time Setup

```bash
# 1. Install DynamoDB Local
./setup-dynamodb-local.sh

# 2. Start DynamoDB Local
./start-dynamodb-local.sh

# 3. Run tests
./run-all-tests.sh

# 4. Stop DynamoDB Local when done
./stop-dynamodb-local.sh
```

### Daily Development Workflow

```bash
# Start DynamoDB Local (if not running)
./start-dynamodb-local.sh

# Run tests as you develop
cd ../shared && cargo test
cd ../data && cargo test
cd ../control && cargo test

# Or run all tests at once
./run-all-tests.sh

# Stop DynamoDB Local at end of day
./stop-dynamodb-local.sh
```

### CI/CD Integration

For continuous integration, you can use Docker instead:

```bash
# Start DynamoDB Local in Docker
docker run -d -p 8000:8000 --name dynamodb-local amazon/dynamodb-local

# Run tests
export DYNAMODB_ENDPOINT=http://localhost:8000
cargo test --all

# Stop and remove container
docker stop dynamodb-local
docker rm dynamodb-local
```

## Troubleshooting

### DynamoDB Local won't start

**Problem:** "Error: Java is not installed"

**Solution:** Install Java 8 or later:
```bash
# Ubuntu/Debian
sudo apt-get install openjdk-11-jre

# macOS
brew install openjdk@11

# Windows
# Download from https://adoptium.net/
```

**Problem:** "Address already in use" or port 8000 conflict

**Solution:** Check if another process is using port 8000:
```bash
# Linux/macOS
lsof -i :8000

# Windows
netstat -ano | findstr :8000
```

Kill the conflicting process or change the port in `start-dynamodb-local.sh`.

### Tests fail with connection errors

**Problem:** Tests fail with "Connection refused" or "Unable to connect to DynamoDB"

**Solution:**
1. Verify DynamoDB Local is running:
   ```bash
   curl http://localhost:8000
   ```
   Should return: `{"__type":"com.amazon.coral.service#UnknownOperationException","message":null}`

2. Check the log file:
   ```bash
   cat ../dynamodb-local/dynamodb-local.log
   ```

3. Restart DynamoDB Local:
   ```bash
   ./stop-dynamodb-local.sh
   ./start-dynamodb-local.sh
   ```

### Integration tests fail with table errors

**Problem:** "ResourceNotFoundException: Cannot do operations on a non-existent table"

**Solution:** Integration tests should create tables automatically. If they don't:
1. Check that test setup code is running
2. Verify DynamoDB Local is in shared DB mode
3. Try restarting DynamoDB Local with `-sharedDb` flag

### Stale PID file

**Problem:** "DynamoDB Local is already running" but it's not actually running

**Solution:** Remove the stale PID file:
```bash
rm ../dynamodb-local/dynamodb-local.pid
./start-dynamodb-local.sh
```

## Script Permissions

If you get "Permission denied" errors, make scripts executable:

```bash
chmod +x setup-dynamodb-local.sh
chmod +x start-dynamodb-local.sh
chmod +x stop-dynamodb-local.sh
chmod +x run-all-tests.sh
```

## Additional Resources

- [DynamoDB Local Documentation](https://docs.aws.amazon.com/amazondynamodb/latest/developerguide/DynamoDBLocal.html)
- [AWS SDK for Rust](https://github.com/awslabs/aws-sdk-rust)
- [Cargo Test Documentation](https://doc.rust-lang.org/cargo/commands/cargo-test.html)
