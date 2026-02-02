try:
    Import("env")
    is_platformio = True
except:
    is_platformio = False

import datetime
import os

# Read current version from file or use default
version_file = "version.txt"
if os.path.exists(version_file):
    with open(version_file, 'r') as f:
        parts = f.read().strip().split('.')
        major, minor, patch = int(parts[0]), int(parts[1]), int(parts[2])
else:
    major, minor, patch = 1, 0, 0

# Auto-increment patch version
patch += 1

# Write back
with open(version_file, 'w') as f:
    f.write(f"{major}.{minor}.{patch}")

# Generate build timestamp
build_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
build_number = int(datetime.datetime.now().timestamp())

# Generate version header
version_header = f"""// Auto-generated version file - DO NOT EDIT
#ifndef VERSION_H
#define VERSION_H

#define FIRMWARE_VERSION "{major}.{minor}.{patch}"
#define BUILD_TIMESTAMP "{build_time}"
#define BUILD_NUMBER {build_number}

#endif // VERSION_H
"""

with open("include/Version.h", 'w') as f:
    f.write(version_header)

print(f"Generated version: {major}.{minor}.{patch}")
