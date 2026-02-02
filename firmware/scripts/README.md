# Development Scripts

This directory contains helper scripts for development workflows.

## Linting

### Check Code Formatting
```bash
scripts\lint.bat check
```
Checks if code follows the project's formatting standards without making changes.

### Auto-Fix Formatting
```bash
scripts\lint.bat fix
```
Automatically formats all C++ code according to `.clang-format` configuration.

### Requirements
- **clang-format**: Install via LLVM or Visual Studio
  - Download: https://releases.llvm.org/download.html
  - Or install with Visual Studio C++ tools

## Testing

### Run Unit Tests (Native Environment)
```bash
scripts\test.bat unit
```
Runs unit tests on your host machine (no ESP32 hardware needed).

### Run Integration Tests (ESP32 Hardware)
```bash
scripts\test.bat integration
```
Runs integration tests on connected ESP32 hardware.

### Run All Tests
```bash
scripts\test.bat all
```
Runs both unit and integration tests.

### Run Specific Test
```bash
scripts\test.bat unit test_data_structures
scripts\test.bat integration test_network_manager
```
Runs a specific test by name.

### Verbose Output
```bash
scripts\test.bat unit -v
scripts\test.bat unit test_data_structures -v
```
Runs tests with verbose output for debugging.

### Test Organization

**Unit Tests** (`test/unit/`):
- Run in native environment (no hardware needed)
- Self-contained, no ESP32 dependencies
- Fast execution

**Integration Tests** (`test/integration/`):
- Run on ESP32 hardware
- Test WiFi, Serial, and other ESP32-specific features
- Require connected ESP32 device

**Other Tests** (`test/`):
- Tests that depend on source files
- May need to be run individually

### Requirements
- **PlatformIO**: Already installed if you can build the project
- **ESP32 Hardware**: Only required for integration tests

## Quick Reference

| Command | Description |
|---------|-------------|
| `scripts\lint.bat check` | Check code formatting |
| `scripts\lint.bat fix` | Auto-fix formatting |
| `scripts\test.bat unit` | Run unit tests (native) |
| `scripts\test.bat integration` | Run integration tests (ESP32) |
| `scripts\test.bat all` | Run all tests |
| `scripts\test.bat unit <name>` | Run specific unit test |
| `scripts\test.bat unit <name> -v` | Run test with verbose output |

## CI/CD Integration

These scripts are designed to be easily integrated into CI/CD pipelines:

```yaml
# Example GitHub Actions workflow
- name: Lint Code
  run: scripts\lint.bat check

- name: Run Tests
  run: scripts\test.bat
```
