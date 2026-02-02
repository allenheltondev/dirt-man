@echo off
REM Run unit tests for ESP32 sensor firmware
REM Usage: scripts\test.bat [unit|integration|all] [test_name] [-v]

setlocal

set TEST_TYPE=%1
set TEST_FILTER=%2
set VERBOSE=%3

REM Default to unit tests if no type specified
if "%TEST_TYPE%"=="" set TEST_TYPE=unit
if "%TEST_TYPE%"=="-v" (
    set TEST_TYPE=unit
    set VERBOSE=-v
)

echo ========================================
echo Running Tests: %TEST_TYPE%
echo ========================================
echo.

REM Handle unit tests
if "%TEST_TYPE%"=="unit" goto run_unit
REM Handle integration tests
if "%TEST_TYPE%"=="integration" goto run_integration
REM Handle all tests
if "%TEST_TYPE%"=="all" goto run_all
REM Invalid type
goto invalid_type

:run_unit
echo Running unit tests in native environment...
echo.
if "%TEST_FILTER%"=="" goto unit_no_filter
if "%TEST_FILTER%"=="-v" goto unit_verbose
if "%VERBOSE%"=="-v" goto unit_filter_verbose
pio test -e native -f %TEST_FILTER%
goto check_result

:unit_no_filter
pio test -e native
goto check_result

:unit_verbose
pio test -e native -v
goto check_result

:unit_filter_verbose
pio test -e native -f %TEST_FILTER% -v
goto check_result

:run_integration
echo Running integration tests on ESP32...
echo NOTE: Requires ESP32 hardware connected
echo.
if "%TEST_FILTER%"=="" goto integration_no_filter
if "%TEST_FILTER%"=="-v" goto integration_verbose
if "%VERBOSE%"=="-v" goto integration_filter_verbose
pio test -e esp32test -f %TEST_FILTER%
goto check_result

:integration_no_filter
pio test -e esp32test
goto check_result

:integration_verbose
pio test -e esp32test -v
goto check_result

:integration_filter_verbose
pio test -e esp32test -f %TEST_FILTER% -v
goto check_result

:run_all
echo Running all tests...
echo.
echo [1/2] Unit tests (native)...
pio test -e native
if errorlevel 1 (
    echo.
    echo Unit tests FAILED!
    exit /b 1
)
echo.
echo [2/2] Integration tests (ESP32)...
echo NOTE: Requires ESP32 hardware connected
pio test -e esp32test
if errorlevel 1 (
    echo.
    echo Integration tests FAILED!
    exit /b 1
)
goto success

:invalid_type
echo Invalid test type: %TEST_TYPE%
echo Usage: scripts\test.bat [unit^|integration^|all] [test_name] [-v]
exit /b 1

:check_result
if errorlevel 1 (
    echo.
    echo ========================================
    echo Tests FAILED!
    echo ========================================
    exit /b 1
)
goto success

:success
echo.
echo ========================================
echo All tests PASSED!
echo ========================================

endlocal
