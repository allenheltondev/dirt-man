# Linting and Testing Setup

This document provides a quick overview of the linting and testing configuration for the ESP32 sensor firmware project.

## Overview

The project now includes:
- **Code formatting** with clang-format
- **Static analysis** with clang-tidy
- **Easy test execution** with batch scripts
- **Compiler warnings** enabled (-Wall, -Wextra, -Wpedantic)

## Quick Start

### Install Tools

1. **clang-format** (required for formatting)
   - Download LLVM: https://releases.llvm.org/download.html
   - Or install with Visual Studio C++ tools
   - Verify: `clang-format --version`

2. **PlatformIO** (already installed if you can build)
   - Tests run in native environment (no ESP32 hardware needed)

### Daily Workflow

```bash
# Format your code before committing
scripts\lint.bat fix

# Run relevant tests
scripts\test.bat test_config_manager
scripts\test.bat test_data_manager

# Build for ESP32
pio run -e esp32dev
```

## Linting

### Check Formatting
```bash
scripts\lint.bat check
```
Checks if code follows formatting standards without making changes.

### Auto-Fix Formatting
```bash
scripts\lint.bat fix
```
Automatically formats all C++ code according to `.clang-format` configuration.

### Configuration Files
- `.clang-format` - Formatting rules (Google style with 4-space indentation)
- `.clang-tidy` - Static analysis rules (bugprone, modernize, performance, readability)

### Formatting Rules
- Indentation: 4 spaces (no tabs)
- Line length: 100 characters max
- Braces: Attach style (K&R)
- Naming:
  - Classes/Structs: `CamelCase`
  - Functions: `camelBack`
  - Variables: `camelBack`
  - Constants: `UPPER_CASE`

## Testing

### Run Individual Tests
```bash
# Run a specific test
scripts\test.bat test_config_manager

# With verbose output
scripts\test.bat test_config_manager -v
```

**Note:** Tests must be run individually due to the current test structure.

### Available Tests
- `test_config_manager` - Configuration management
- `test_config_manager_properties` - Property-based config tests
- `test_data_manager` - Data processing
- `test_data_manager_properties` - Property-based data tests
- `test_data_manager_ring_buffer` - Ring buffer functionality
- `test_data_manager_ring_buffer_properties` - Property-based ring buffer tests
- `test_data_manager_display_buffer` - Display buffer functionality
- `test_data_manager_display_buffer_properties` - Property-based display buffer tests
- `test_time_manager` - Time management
- `test_time_manager_properties` - Property-based time tests

### Test Structure
```
test/
├── test_*.cpp              # Unit tests
├── test_*_properties.cpp   # Property-based tests
├── mocks/                  # Arduino mocks for native testing
└── README.md              # Testing guide
```

## Build Configuration

### ESP32 Environment
```ini
[env:esp32dev]
build_flags =
    -Wall                  # Enable all warnings
    -Wextra                # Enable extra warnings
    -Wpedantic             # Strict ISO C++ compliance
check_tool = clangtidy     # Static analysis tool
```

### Native Test Environment
```ini
[env:native]
build_flags =
    -D UNIT_TEST           # Enable mock sensor mode
    -std=c++17             # C++17 standard
    -Wall -Wextra -Wpedantic
    -I test/mocks          # Include Arduino mocks
test_build_src = no        # Tests are self-contained
```

## CI/CD Integration

The linting and testing scripts are designed for easy CI/CD integration:

```yaml
# Example GitHub Actions workflow
- name: Check Formatting
  run: scripts\lint.bat check

- name: Run Tests
  run: |
    scripts\test.bat test_config_manager
    scripts\test.bat test_data_manager
    scripts\test.bat test_time_manager

- name: Build Firmware
  run: pio run -e esp32dev
```

## Troubleshooting

### clang-format not found
- Install LLVM or Visual Studio C++ tools
- Add to PATH: `C:\Program Files\LLVM\bin`
- Verify: `clang-format --version`

### Test build errors
- Ensure PlatformIO is up to date: `pio upgrade`
- Clean build: `pio run -e native --target clean`
- Run tests individually: `scripts\test.bat test_config_manager`

### Formatting issues
- Check `.clang-format` file exists in project root
- Run `scripts\lint.bat fix` to auto-fix
- Verify changes with `scripts\lint.bat check`

## Resources

- [Clang-Format Documentation](https://clang.llvm.org/docs/ClangFormat.html)
- [Clang-Tidy Documentation](https://clang.llvm.org/extra/clang-tidy/)
- [PlatformIO Testing](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)
- [Project Documentation](docs/README.md)
- [Development Guide](DEVELOPMENT.md)
- [Script Documentation](scripts/README.md)

## Summary

You now have:
- ✅ Code formatting with clang-format
- ✅ Static analysis with clang-tidy
- ✅ Easy test execution with `scripts\test.bat`
- ✅ Easy linting with `scripts\lint.bat`
- ✅ Compiler warnings enabled
- ✅ CI/CD-ready scripts

Run `scripts\lint.bat fix` before committing and `scripts\test.bat <test_name>` to verify your changes!
