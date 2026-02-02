# Development Guide

Quick reference for developing the ESP32 sensor firmware.

## Setup

### Install Tools

1. **PlatformIO**
   ```bash
   pip install platformio
   ```

2. **clang-format** (for code formatting)
   - Download LLVM: https://releases.llvm.org/download.html
   - Or install with Visual Studio C++ tools
   - Verify: `clang-format --version`

3. **clang-tidy** (optional, for static analysis)
   - Included with LLVM
   - Verify: `clang-tidy --version`

## Daily Workflow

### Before Committing Code

```bash
# 1. Format your code
scripts\lint.bat fix

# 2. Run relevant tests
scripts\test.bat test_config_manager
scripts\test.bat test_data_manager

# 3. Build for ESP32
pio run -e esp32dev
```

### Code Quality Checks

```bash
# Check formatting (no changes)
scripts\lint.bat check

# Run static analysis
pio check -e native

# Run specific test
scripts\test.bat test_data_manager

# Verbose test output
scripts\test.bat test_data_manager -v
```

## Build Commands

### ESP32 Target

```bash
# Build firmware
pio run -e esp32dev

# Upload to device
pio run -e esp32dev --target upload

# Monitor serial output
pio device monitor -e esp32dev

# Build + Upload + Monitor
pio run -e esp32dev --target upload && pio device monitor -e esp32dev
```

### Native Testing

```bash
# Build tests
pio test -e native --without-testing

# Run all tests
scripts\test.bat

# Run specific test
scripts\test.bat test_config_manager
```

## Testing

### Test Structure

```
test/
├── test_*.cpp              # Unit tests
├── test_*_properties.cpp   # Property-based tests
├── mocks/                  # Arduino mocks
└── README.md              # Testing guide
```

### Writing Tests

```cpp
#include <unity.h>
#include "YourModule.h"

void test_your_feature() {
    // Arrange
    YourModule module;

    // Act
    bool result = module.doSomething();

    // Assert
    TEST_ASSERT_TRUE(result);
}

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_your_feature);
    return UNITY_END();
}
```

### Running Tests

```bash
# All tests (note: must be run individually)
scripts\test.bat test_config_manager
scripts\test.bat test_data_manager
scripts\test.bat test_time_manager

# Specific test file
scripts\test.bat test_config_manager

# With verbose output
scripts\test.bat test_config_manager -v

# Direct PlatformIO (alternative)
pio test -e native -f test_config_manager -v
```

## Code Style

### Formatting Rules

- **Indentation**: 4 spaces (no tabs)
- **Line length**: 100 characters max
- **Braces**: Attach style (K&R)
- **Naming**:
  - Classes/Structs: `CamelCase`
  - Functions: `camelBack`
  - Variables: `camelBack`
  - Constants: `UPPER_CASE`

### Auto-Format

```bash
# Fix all formatting issues
scripts\lint.bat fix

# Check without modifying
scripts\lint.bat check
```

### Configuration Files

- `.clang-format` - Formatting rules
- `.clang-tidy` - Static analysis rules
- `platformio.ini` - Build configuration

## Project Structure

```
esp32-sensor-firmware/
├── src/                    # Implementation files (.cpp)
├── include/                # Header files (.h)
│   └── models/            # Data structures
├── test/                   # Unit tests
│   └── mocks/             # Test mocks
├── scripts/                # Development scripts
│   ├── test.bat           # Test runner
│   ├── lint.bat           # Linting script
│   └── README.md          # Script documentation
├── docs/                   # Documentation
├── .kiro/specs/           # Design specifications
├── .clang-format          # Formatting config
├── .clang-tidy            # Linting config
└── platformio.ini         # Build config
```

## Common Tasks

### Add New Feature

1. Write tests first (TDD)
   ```bash
   # Create test/test_new_feature.cpp
   scripts\test.bat test_new_feature
   ```

2. Implement feature
   ```cpp
   // include/NewFeature.h
   // src/NewFeature.cpp
   ```

3. Format and test
   ```bash
   scripts\lint.bat fix
   scripts\test.bat
   ```

4. Build for ESP32
   ```bash
   pio run -e esp32dev
   ```

### Debug on Hardware

```bash
# Upload with monitor
pio run -e esp32dev --target upload && pio device monitor -e esp32dev

# Use serial console commands
> diag          # System diagnostics
> status        # Current status
> config        # Show configuration
```

### Update Dependencies

```bash
# Update all libraries
pio pkg update

# Update specific library
pio pkg update -e esp32dev "bblanchon/ArduinoJson"
```

## Troubleshooting

### Build Errors

```bash
# Clean build
pio run -e esp32dev --target clean

# Rebuild from scratch
pio run -e esp32dev --target clean && pio run -e esp32dev
```

### Test Failures

```bash
# Run with verbose output
scripts\test.bat test_failing_test -v

# Check test file directly
pio test -e native -f test_failing_test -v
```

### Formatting Issues

```bash
# See what will change
scripts\lint.bat check

# Apply fixes
scripts\lint.bat fix
```

## CI/CD Integration

### GitHub Actions Example

```yaml
name: CI

on: [push, pull_request]

jobs:
  test:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3

      - name: Setup Python
        uses: actions/setup-python@v4
        with:
          python-version: '3.x'

      - name: Install PlatformIO
        run: pip install platformio

      - name: Check Formatting
        run: scripts\lint.bat check

      - name: Run Tests
        run: scripts\test.bat

      - name: Build Firmware
        run: pio run -e esp32dev
```

## Resources

- [PlatformIO Documentation](https://docs.platformio.org/)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [Unity Testing Framework](https://github.com/ThrowTheSwitch/Unity)
- [Clang-Format Documentation](https://clang.llvm.org/docs/ClangFormat.html)
- [Project Documentation](docs/README.md)

## Quick Reference

| Task | Command |
|------|---------|
| Format code | `scripts\lint.bat fix` |
| Check formatting | `scripts\lint.bat check` |
| Run specific test | `scripts\test.bat <name>` |
| Build firmware | `pio run -e esp32dev` |
| Upload firmware | `pio run -e esp32dev --target upload` |
| Monitor serial | `pio device monitor -e esp32dev` |
| Clean build | `pio run -e esp32dev --target clean` |
| Static analysis | `pio check -e esp32dev` |
