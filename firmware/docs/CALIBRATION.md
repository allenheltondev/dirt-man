# ESP32 Sensor Firmware - Calibration Procedure

## Overview

This document provides detailed procedures for calibrating the soil moisture sensor. Proper calibration ensures accurate percentage readings that reflect actual soil moisture conditions.

## Why Calibration is Necessary

### The Problem

Capacitive soil moisture sensors output analog voltage (0-3.3V) that varies with soil moisture. However:

1. **Raw ADC values are not standardized** - Different sensors have different output ranges
2. **Soil comp
```
percentage = (rawAdc - dryAdc) / (wetAdc - dryAdc) × 100
```

## Calibration Requirements

### Equipment Needed

- ESP32 with firmware installed and running
- Soil moisture sensor (already wired)
- USB cable for serial console connection
- Computer with serial terminal (115200 baud)
- Container with dry soil or sand
- Container with water or saturated soil
- Paper towels or cloth for cleaning sensor

### Time Required

- **Quick calibration:** 5 minutes (using dry air and water)
- **Accurate calibration:** 30 minutes (using actual soil conditions)
- **Optimal calibration:** 24 hours (allowing soil to stabilize)

### Environmental Conditions

- **Temperature:** Calibrate at typical operating temperature (15-30°C)
- **Humidity:** Indoor conditions acceptable
- **Soil type:** Use soil from actual deployment location if possible

## Calibration Methods

### Method 1: Quick Calibration (Air and Water)

**Best for:** Initial testing, development, approximate readings

**Procedure:**

1. **Prepare sensor:**
   - Clean sensor with dry cloth
   - Ensure sensor is at room temperature
   - Verify sensor is connected and reading values

2. **Dry calibration (in air):**
   - Remove sensor from any soil or water
   - Hold sensor in open air
   - Wait 30 seconds for reading to stabilize
   - Note the ADC value from serial console
   - This is your **Dry ADC** value

3. **Wet calibration (in water):**
   - Submerge sensor in clean water (not distilled)
   - Only submerge the sensor probe, not the electronics
   - Wait 30 seconds for reading to stabilize
   - Note the ADC value from serial console
   - This is your **Wet ADC** value

4. **Enter calibration values:**
   - Use serial console `calibrate` command
   - Enter Dry ADC and Wet ADC values
   - Save configuration

**Typical Values:**
- Dry ADC (air): 2800-3500
- Wet ADC (water): 1000-1500

**Limitations:**
- Air is drier than dry soil
- Water is wetter than saturated soil
- May overestimate or underestimate actual soil moisture

### Method 2: Soil-Based Calibration (Recommended)

**Best for:** Accurate readings, production deployment

**Procedure:**

1. **Prepare dry soil:**
   - Fill container with soil from deployment location
   - Spread soil thin on baking sheet
   - Dry in oven at 105°C (220°F) for 2 hours
   - Or leave in sun for 24 hours
   - Let cool to room temperature
   - Place dried soil in container

2. **Dry calibration:**
   - Insert sensor into dry soil
   - Ensure good contact (press firmly)
   - Wait 2-3 minutes for reading to stabilize
   - Note the ADC value from serial console
   - This is your **Dry ADC** value

3. **Prepare saturated soil:**
   - Add water to soil gradually
   - Mix thoroughly
   - Continue until water pools on surface
   - This is field capacity (100% moisture)

4. **Wet calibration:**
   - Insert sensor into saturated soil
   - Ensure good contact (press firmly)
   - Wait 2-3 minutes for reading to stabilize
   - Note the ADC value from serial console
   - This is your **Wet ADC** value

5. **Enter calibration values:**
   - Use serial console `calibrate` command
   - Enter Dry ADC and Wet ADC values
   - Save configuration

**Typical Values:**
- Dry ADC (dry soil): 2500-3200
- Wet ADC (saturated soil): 1200-1800

**Advantages:**
- Accurate for actual soil conditions
- Accounts for soil composition
- Reflects real-world moisture range

### Method 3: In-Situ Calibration (Most Accurate)

**Best for:** Long-term monitoring, research applications

**Procedure:**

1. **Install sensor in deployment location:**
   - Insert sensor at desired depth
   - Ensure good soil contact
   - Connect to ESP32 and power on

2. **Dry calibration:**
   - Wait for natural dry period (no rain for 7+ days)
   - Or manually dry soil around sensor
   - Monitor readings for 24 hours
   - Use lowest stable ADC value as **Dry ADC**

3. **Wet calibration:**
   - Wait for heavy rain or irrigation
   - Or manually saturate soil around sensor
   - Monitor readings for 24 hours
   - Use lowest stable ADC value as **Wet ADC**

4. **Enter calibration values:**
   - Use serial console `calibrate` command
   - Enter Dry ADC and Wet ADC values
   - Save configuration

**Advantages:**
- Most accurate for specific location
- Accounts for installation depth and soil compaction
- Reflects actual operating conditions

**Disadvantages:**
- Requires extended time period
- Depends on weather conditions
- May need to wait for extreme conditions

## Step-by-Step Serial Console Calibration

### Step 1: Monitor Current Readings

Before calibration, observe current sensor readings:

```
[SENSOR] Soil Moisture: 0.0% (Raw ADC: 2850)
```

The percentage will be incorrect before calibration, but the raw ADC value is what we need.

### Step 2: Start Calibration Command

Connect to serial console (115200 baud) and type:

```
> calibrate
```

You'll see:

```
=== Update Soil Moisture Calibration ===
Place sensor in DRY soil and press Enter
```

### Step 3: Dry Calibration

1. **Prepare dry condition** (choose method above)
2. **Wait for reading to stabilize** (watch serial output)
3. **Press Enter** when ready
4. **Note the current ADC value** from serial output
5. **Enter the Dry ADC value:**

```
Enter Dry ADC value (0-4095): 3000
```

### Step 4: Wet Calibration

You'll see:

```
Place sensor in WET soil and press Enter
```

1. **Prepare wet condition** (choose method above)
2. **Wait for reading to stabilize** (watch serial output)
3. **Press Enter** when ready
4. **Note the current ADC value** from serial output
5. **Enter the Wet ADC value:**

```
Enter Wet ADC value (0-4095): 1500
```

### Step 5: Verify Calibration

The firmware will confirm:

```
Calibration updated
Dry ADC: 3000
Wet ADC: 1500
```

### Step 6: Save Configuration

**IMPORTANT:** Calibration is not saved until you run:

```
> save
Configuration saved successfully
```

### Step 7: Verify Readings

Monitor sensor readings to verify calibration:

```
[SENSOR] Soil Moisture: 62.3% (Raw ADC: 2100)
```

The percentage should now reflect actual moisture conditions.

## Validation and Testing

### Verify Calibration Formula

The firmware uses this formula:

```
percentage = (rawAdc - dryAdc) / (wetAdc - dryAdc) × 100
```

**Example:**
- Dry ADC: 3000
- Wet ADC: 1500
- Current ADC: 2100

```
percentage = (2100 - 3000) / (1500 - 3000) × 100
           = (-900) / (-1500) × 100
           = 0.6 × 100
           = 60%
```

### Test Calibration Points

1. **Test dry point:**
   - Place sensor in dry condition
   - Reading should be close to 0%
   - Acceptable range: 0-10%

2. **Test wet point:**
   - Place sensor in wet condition
   - Reading should be close to 100%
   - Acceptable range: 90-100%

3. **Test mid-point:**
   - Place sensor in moderately moist soil
   - Reading should be 40-60%
   - Should feel intuitively correct

### Common Issues

**Problem:** Readings are negative

**Cause:** Current ADC value is higher than Dry ADC

**Solution:**
- Recalibrate with drier conditions
- Ensure Dry ADC is the highest expected value

---

**Problem:** Readings exceed 100%

**Cause:** Current ADC value is lower than Wet ADC

**Solution:**
- Recalibrate with wetter conditions
- Ensure Wet ADC is the lowest expected value

---

**Problem:** Readings don't change much

**Cause:** Dry ADC and Wet ADC are too close together

**Solution:**
- Ensure dry condition is truly dry
- Ensure wet condition is truly saturated
- Difference should be at least 500 ADC counts

---

**Problem:** Readings are inverted (dry shows 100%, wet shows 0%)

**Cause:** Dry ADC and Wet ADC are swapped

**Solution:**
- Dry ADC must be greater than Wet ADC
- Recalibrate with correct values

## Advanced Calibration Topics

### Temperature Compensation

Capacitive sensors are affected by temperature:
- Higher temperature → lower ADC reading
- Lower temperature → higher ADC reading
- Effect: ~0.5% per 10°C

**Mitigation:**
- Calibrate at typical operating temperature
- For critical applications, implement temperature compensation in firmware
- Use BME280 temperature for compensation

### Soil-Specific Calibration

Different soil types have different electrical properties:

| Soil Type | Typical Dry ADC | Typical Wet ADC | Range |
|-----------|-----------------|-----------------|-------|
| Sand | 2800-3200 | 1400-1800 | 1000-1800 |
| Loam | 2600-3000 | 1200-1600 | 1000-1800 |
| Clay | 2400-2800 | 1000-1400 | 1000-1800 |

**Recommendation:** Always calibrate with soil from deployment location.

### Multi-Point Calibration

For highest accuracy, consider 3-point calibration:
- 0% (dry)
- 50% (field capacity)
- 100% (saturated)

**Note:** Current firmware uses 2-point linear calibration. Multi-point requires firmware modification.

### Calibration Drift

Sensor calibration may drift over time due to:
- Sensor aging
- Soil composition changes
- Mineral buildup on sensor
- Temperature cycling

**Recommendation:**
- Recalibrate every 6-12 months
- Clean sensor periodically
- Monitor for unexpected reading changes

## Calibration Best Practices

### Do's

✓ **Use soil from deployment location** for most accurate calibration

✓ **Wait for readings to stabilize** before recording ADC values

✓ **Calibrate at operating temperature** (not in cold garage or hot sun)

✓ **Ensure good sensor contact** with soil during calibration

✓ **Save configuration** after calibration

✓ **Verify calibration** with known moisture conditions

✓ **Document calibration values** for future reference

✓ **Recalibrate periodically** (every 6-12 months)

### Don'ts

✗ **Don't use distilled water** for wet calibration (use tap water or soil)

✗ **Don't submerge electronics** - only the sensor probe

✗ **Don't rush** - allow time for readings to stabilize

✗ **Don't calibrate in extreme temperatures** (< 0°C or > 40°C)

✗ **Don't forget to save** - calibration is lost without saving

✗ **Don't assume calibration is permanent** - sensors drift over time

✗ **Don't use same calibration for different sensors** - each sensor is unique

## Troubleshooting Calibration

### Readings Don't Make Sense

**Check:**
1. Verify calibration values are saved (`show` command)
2. Ensure Dry ADC > Wet ADC
3. Check sensor is properly connected
4. Verify sensor is reading ADC values (not stuck at 0 or 4095)
5. Try recalibration with more extreme conditions

### Calibration Won't Save

**Check:**
1. Dry ADC must be greater than Wet ADC
2. Both values must be 0-4095
3. Run `save` command after `calibrate`
4. Check for error messages in serial output

### Readings Are Unstable

**Check:**
1. Ensure sensor has good soil contact
2. Wait longer for readings to stabilize
3. Check for loose wiring
4. Verify power supply is stable
5. Consider averaging more samples in firmware

### Sensor Reads 0% or 100% Constantly

**Check:**
1. Verify sensor is connected (not open circuit)
2. Check for short circuit in wiring
3. Test sensor with multimeter (should read 0-3.3V)
4. Try different ADC pin
5. Sensor may be damaged - replace if necessary

## Calibration Record Template

Keep a record of calibration for each device:

```
Device ID: esp32-sensor-001
Calibration Date: 2024-01-15
Calibrated By: John Doe

Soil Type: Sandy loam
Location: Garden bed #3
Sensor Depth: 10 cm

Dry Calibration:
  Method: Oven-dried soil
  ADC Value: 3000
  Temperature: 22°C

Wet Calibration:
  Method: Saturated soil
  ADC Value: 1500
  Temperature: 22°C

Verification:
  Dry test: 2% (acceptable)
  Wet test: 98% (acceptable)
  Mid test: 55% (acceptable)

Notes:
  - Sensor cleaned before calibration
  - Soil from deployment location
  - Readings stable and consistent

Next Calibration Due: 2024-07-15
```

## Quick Reference

### Calibration Command Sequence

```
> calibrate
[Place sensor in DRY condition]
[Press Enter]
Enter Dry ADC value (0-4095): 3000
[Place sensor in WET condition]
[Press Enter]
Enter Wet ADC value (0-4095): 1500
> save
```

### Typical ADC Values

| Condition | Typical ADC | Range |
|-----------|-------------|-------|
| Air (very dry) | 3200 | 2800-3500 |
| Dry soil | 2800 | 2500-3200 |
| Moist soil | 2100 | 1800-2500 |
| Wet soil | 1500 | 1200-1800 |
| Water | 1200 | 1000-1500 |

### Validation Checklist

- [ ] Dry ADC > Wet ADC
- [ ] Difference > 500 ADC counts
- [ ] Both values 0-4095
- [ ] Dry test reads 0-10%
- [ ] Wet test reads 90-100%
- [ ] Mid test reads 40-60%
- [ ] Configuration saved
- [ ] Readings make intuitive sense

## Next Steps

After successful calibration:

1. **Monitor readings** for 24 hours to ensure stability
2. **Deploy sensor** in final location
3. **Set thresholds** for low/high moisture alerts (optional)
4. **Document calibration** for future reference
5. **Schedule recalibration** in 6-12 months

For threshold configuration, see [SERIAL_CONSOLE.md](SERIAL_CONSOLE.md)

For deployment procedures, see [DEPLOYMENT.md](DEPLOYMENT.md)
