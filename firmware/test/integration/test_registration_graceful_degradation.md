# Integration Test: Registration Failure Graceful Degradation

**Validates: Requirements 2.7, 10.2, 10.3**

## Test Objective

Verify that the device continues normal sensor operation even when registration fails, demonstrating graceful degradation.

## Test Setup

1. Configure the device with an invalid or unreachable backend API endpoint
2. Ensure sensors are properly connected and functional
3. Monitor serial output for registration attempts and sensor readings

## Test Procedure

### Test Case 1: Network Error During Registration

1. Configure device with unreachable API endpoint (e.g., `https://192.168.255.255/api/data`)
2. Power on the device
3. Observe registration attempt fails with network error
4. Verify sensor readings continue at configured intervals
5. Verify data is buffered for later transmission

**Expected Results:**
- Registration fails with network error logged
- Sensor readings continue uninterrupted
- Data buffering occurs normally
- Device does not block or hang

### Test Case 2: HTTP 500 Error During Registration

1. Configure device with API endpoint that returns HTTP 500
2. Power on the device
3. Observe registration attempt fails with server error
4. Verify retry logic engages with exponential backoff
5. Verify sensor readings continue during retry attempts

**Expected Results:**
- Registration fails with HTTP 500 logged
- Retry attempts occur with backoff (1s, 2s, 4s, 8s, 16s)
- Sensor readings continue uninterrupted
- Data transmission continues (with unregistered status)

### Test Case 3: Payload Includes IDs When Unregistered

1. With registration failed, capture sensor data payload
2. Verify payload includes `hardware_id` field
3. Verify payload includes `boot_id` field
4. Verify payload does NOT include `confirmation_id` field

**Expected Payload Structure:**
```json
{
  "hardware_id": "AA:BB:CC:DD:EE:FF",
  "boot_id": "550e8400-e29b-41d4-a716-446655440000",
  "readings": [...]
}
```

## Manual Verification Steps

1. Connect to device via serial console
2. Monitor output during boot and operation
3. Verify log messages indicate:
   - Registration attempt
   - Registration failure
   - Sensor readings continuing
   - Data buffering/transmission continuing
4. Use network packet capture to verify payload structure

## Success Criteria

- [ ] Device boots successfully even with registration failure
- [ ] Sensor readings occur at configured intervals
- [ ] Data transmission continues (buffered if offline)
- [ ] Payload includes hardware_id and boot_id
- [ ] No blocking or hanging behavior observed
- [ ] Retry logic operates in background without affecting sensors

## Notes

This test should be run on actual hardware with real sensors to verify end-to-end graceful degradation behavior.
