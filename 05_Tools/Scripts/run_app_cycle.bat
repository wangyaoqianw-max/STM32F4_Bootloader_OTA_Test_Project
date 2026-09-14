@echo off
setlocal EnableExtensions

REM ============================================================
REM OTA_APP Local Development Cycle
REM
REM Build -> Flash -> RTT capture
REM
REM This command validates tool execution only. It does not replace
REM stage-specific board-test assertions or verification reports.
REM ============================================================

set "SCRIPT_DIR=%~dp0"

echo [CYCLE] Step 1/3: Build
call "%SCRIPT_DIR%build_app.bat"
set "BUILD_RESULT=%ERRORLEVEL%"

if %BUILD_RESULT% GEQ 2 (
    echo [CYCLE][FAIL] Build failed. Stop.
    exit /b %BUILD_RESULT%
)

echo.
echo [CYCLE] Step 2/3: Flash
call "%SCRIPT_DIR%flash_app.bat"
set "FLASH_RESULT=%ERRORLEVEL%"

if not "%FLASH_RESULT%"=="0" (
    echo [CYCLE][FAIL] Flash failed. Stop.
    exit /b %FLASH_RESULT%
)

echo.
echo [CYCLE] Step 3/3: RTT capture
call "%SCRIPT_DIR%rtt_capture.bat" %*
set "RTT_RESULT=%ERRORLEVEL%"

if not "%RTT_RESULT%"=="0" (
    echo [CYCLE][FAIL] RTT capture failed.
    exit /b %RTT_RESULT%
)

echo.
if "%BUILD_RESULT%"=="1" (
    echo [CYCLE][WARN] Build, flash, and RTT capture completed, but Keil reported warnings.
    exit /b 1
)

echo [CYCLE][PASS] Build, flash, and RTT capture commands completed.
echo [CYCLE][INFO] This is not a stage-level hardware verification result.
exit /b 0
