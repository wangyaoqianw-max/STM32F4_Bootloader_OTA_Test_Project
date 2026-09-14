@echo off
setlocal EnableExtensions

REM ============================================================
REM OTA_APP RTT Capture Entry
REM
REM Usage:
REM   05_Tools\Scripts\rtt_capture.bat [duration_seconds]
REM
REM Default duration comes from RTT_CAPTURE_SECONDS.
REM ============================================================

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"

set "LOCAL_CONFIG=%REPO_ROOT%\05_Tools\Config\toolchain.local.bat"
set "LOG_DIR=%REPO_ROOT%\06_Output\Logs"
set "RTT_LOG=%LOG_DIR%\OTA_APP_rtt.log"
set "RTT_DIAG_LOG=%LOG_DIR%\OTA_APP_rtt_logger.log"
set "RTT_PS1=%SCRIPT_DIR%rtt_capture.ps1"

if not exist "%LOCAL_CONFIG%" (
    echo [RTT][ERROR] Local toolchain configuration not found.
    echo [RTT][ERROR] Expected: %LOCAL_CONFIG%
    exit /b 30
)

call "%LOCAL_CONFIG%"

if not defined JLINK_RTT_LOGGER (
    echo [RTT][ERROR] JLINK_RTT_LOGGER is not defined in toolchain.local.bat.
    exit /b 31
)

if not exist "%JLINK_RTT_LOGGER%" (
    echo [RTT][ERROR] J-Link RTT Logger not found: %JLINK_RTT_LOGGER%
    exit /b 32
)

if not defined JLINK_DEVICE set "JLINK_DEVICE=STM32F411CE"
if not defined JLINK_IF set "JLINK_IF=SWD"
if not defined JLINK_SPEED set "JLINK_SPEED=4000"
if not defined JLINK_RTT_CHANNEL set "JLINK_RTT_CHANNEL=0"
if not defined RTT_CAPTURE_SECONDS set "RTT_CAPTURE_SECONDS=10"

if not "%~1"=="" set "RTT_CAPTURE_SECONDS=%~1"

for /f "delims=0123456789" %%A in ("%RTT_CAPTURE_SECONDS%") do (
    echo [RTT][ERROR] Duration must be a positive integer number of seconds.
    exit /b 33
)

if "%RTT_CAPTURE_SECONDS%"=="0" (
    echo [RTT][ERROR] Duration must be greater than zero.
    exit /b 33
)

if not exist "%LOG_DIR%" (
    mkdir "%LOG_DIR%"
    if errorlevel 1 (
        echo [RTT][ERROR] Failed to create log directory: %LOG_DIR%
        exit /b 34
    )
)

echo ============================================================
echo [RTT] Device   : %JLINK_DEVICE%
echo [RTT] Interface: %JLINK_IF%
echo [RTT] Speed    : %JLINK_SPEED% kHz
echo [RTT] Channel  : %JLINK_RTT_CHANNEL%
echo [RTT] Duration : %RTT_CAPTURE_SECONDS% s
echo [RTT] Output   : %RTT_LOG%
echo ============================================================

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%RTT_PS1%" ^
    -Exe "%JLINK_RTT_LOGGER%" ^
    -Device "%JLINK_DEVICE%" ^
    -Interface "%JLINK_IF%" ^
    -Speed %JLINK_SPEED% ^
    -Channel %JLINK_RTT_CHANNEL% ^
    -DurationSeconds %RTT_CAPTURE_SECONDS% ^
    -Output "%RTT_LOG%" ^
    -Diagnostic "%RTT_DIAG_LOG%"

set "RTT_RESULT=%ERRORLEVEL%"

echo.
if "%RTT_RESULT%"=="0" (
    echo [RTT][PASS] RTT data captured.
    echo [RTT] Log: %RTT_LOG%
) else if "%RTT_RESULT%"=="32" (
    echo [RTT][FAIL] RTT logger ran but no RTT payload was captured.
    echo [RTT][INFO] Check firmware RTT initialization, channel number, target execution state, and J-Link ownership.
) else (
    echo [RTT][FAIL] RTT capture returned ERRORLEVEL=%RTT_RESULT%.
    echo [RTT][INFO] Diagnostic log: %RTT_DIAG_LOG%
)

exit /b %RTT_RESULT%
