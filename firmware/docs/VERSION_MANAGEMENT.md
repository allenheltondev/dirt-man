# Firmware Version Management

## Overview

The firmware version is automatically incremented on every build using a Python script that runs before compilation.

## How It Works

1. **Version Storage**: `version.txt` stores the current version in `MAJOR.MINOR.PATCH` format
2. **Auto-Increment**: `scripts/version_increment.py` runs before each build and increments the patch version
3. **Header Generation**: The script generates `include/Version.h` with version constants
4. **Build Integration**: PlatformIO's `extra_scripts` configuration triggers the script automatically

## Version Format

- **MAJOR.MINOR.PATCH** (e.g., 1.0.42)
- **BUILD_TIMESTAMP**: Human-readable build date/time
- **BUILD_NUMBER**: Unix timestamp for unique build identification

## Manual Version Control

### Increment Patch Version (Automatic)
Just build the project - patch version increments automatically.

### Increment Minor Version
Edit `version.txt`:
```
1.1.0
```

### Increment Major Version
Edit `version.txt`:
```
2.0.0
```

### Reset Version
Edit `version.txt`:
```
1.0.0
```

## Usage in Code

```cpp
#include "Version.h"

Serial.println(FIRMWARE_VERSION);    // "1.0.42"
Serial.println(BUILD_TIMESTAMP);     // "2026-02-02 14:30:15"
Serial.println(BUILD_NUMBER);        // 1738512615
```

## Version Tracking Options

### Option 1: Track version.txt in Git (Recommended)
- Commit `version.txt` to track version history
- Team members share the same version counter
- Remove `version.txt` from `.gitignore`

### Option 2: Local Version Counter
- Keep `version.txt` in `.gitignore`
- Each developer has their own build counter
- Useful for development builds

## Build Commands

```bash
# Build with auto-increment
pio run

# Upload with auto-increment
pio run --target upload

# Clean build (version still increments)
pio run --target clean
pio run
```

## Version in API Response

The version is included in the `/api/status` endpoint:

```json
{
  "firmware_version": "1.0.42",
  "build_timestamp": "2026-02-02 14:30:15",
  "build_number": 1738512615
}
```

## Disabling Auto-Increment

To disable auto-increment (e.g., for testing), comment out in `platformio.ini`:

```ini
; extra_scripts = pre:scripts/version_increment.py
```

## CI/CD Integration

For automated builds, you can:
1. Set version via environment variable
2. Use git commit count as build number
3. Tag releases with semantic versions

Example modification to `version_increment.py`:
```python
# Use git commit count as patch version
import subprocess
patch = int(subprocess.check_output(['git', 'rev-list', '--count', 'HEAD']))
```
