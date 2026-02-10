"""
Sensor validation utilities.

Provides functions for:
- Physically possible range validation
- Staleness detection (6 identical consecutive readings)
- Noise detection (>50% change between readings)
- Missing sensor detection (no reporting for 2 hours)
- Sensor status determination
"""
