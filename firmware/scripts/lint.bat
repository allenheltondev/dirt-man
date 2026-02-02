@echo off
REM Lint C++ code using clang-format and clang-tidy
REM Usage: scripts\lint.bat [check|fix]

setlocal enabledelayedexpansion

set MODE=%1
if "%MODE%"=="" set MODE=check

echo ========================================
echo C++ Linting for ESP32 Sensor Firmware
echo ========================================
echo.

if "%MODE%"=="fix" (
    echo Running clang-format to FIX formatting issues...
    echo.
    for %%f in (src\*.cpp include\*.h include\models\*.h test\*.cpp test\mocks\*.cpp test\mocks\*.h) do (
        if exist "%%f" clang-format -i -style=file "%%f"
    )
    echo.
    echo Formatting applied!
) else if "%MODE%"=="check" (
    echo Running clang-format to CHECK formatting...
    echo.
    set FOUND_ISSUES=0
    for %%f in (src\*.cpp include\*.h include\models\*.h test\*.cpp test\mocks\*.cpp test\mocks\*.h) do (
        if exist "%%f" (
            clang-format --dry-run --Werror -style=file "%%f" 2>nul
            if errorlevel 1 set FOUND_ISSUES=1
        )
    )
    if !FOUND_ISSUES!==1 (
        echo.
        echo Formatting issues found! Run 'scripts\lint.bat fix' to auto-fix.
        exit /b 1
    )
    echo.
    echo No formatting issues found!
) else (
    echo Invalid mode: %MODE%
    echo Usage: scripts\lint.bat [check^|fix]
    exit /b 1
)

echo.
echo ========================================
echo Linting complete!
echo ========================================

endlocal
