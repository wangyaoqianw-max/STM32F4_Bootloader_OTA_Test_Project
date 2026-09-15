@echo off
setlocal EnableExtensions

REM ============================================================
REM OTA_APP GDB Fault Capture Entry
REM
REM Usage:
REM   05_Tools\Scripts\gdb_fault_capture.bat capture
REM   05_Tools\Scripts\gdb_fault_capture.bat trigger
REM
REM The target must already be stopped in the project Fault handler.
REM This flow captures GDB and RTT evidence and never resumes the MCU.
REM ============================================================

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"

set "LOCAL_CONFIG=%REPO_ROOT%\05_Tools\Config\toolchain.local.bat"
set "FAULT_PS1=%SCRIPT_DIR%gdb_fault_capture.ps1"
set "FAULT_MODE=%~1"
if not defined FAULT_MODE set "FAULT_MODE=capture"
if /I not "%FAULT_MODE%"=="capture" if /I not "%FAULT_MODE%"=="trigger" (
    echo [GDB][ERROR] Mode must be capture or trigger.
    exit /b 47
)
if /I "%FAULT_MODE%"=="trigger" (
    set "FAULT_GDB=%REPO_ROOT%\05_Tools\Debug\GDB\fault_trigger_capture.gdb"
) else (
    set "FAULT_GDB=%REPO_ROOT%\05_Tools\Debug\GDB\fault_capture.gdb"
)
set "AXF=%REPO_ROOT%\03_Firmware\Application\OTA_APP\MDK-ARM\Objects\OTA_APP.axf"
set "LOG_DIR=%REPO_ROOT%\06_Output\Logs"
set "SERVER_LOG=%LOG_DIR%\OTA_APP_gdb_server.log"
set "GDB_LOG=%LOG_DIR%\OTA_APP_fault_gdb.log"
set "RTT_LOG=%LOG_DIR%\OTA_APP_fault_rtt.log"

if not exist "%LOCAL_CONFIG%" (
    echo [GDB][ERROR] Local toolchain configuration not found.
    echo [GDB][ERROR] Expected: %LOCAL_CONFIG%
    call :write_failure_logs 40 "Local toolchain configuration not found: %LOCAL_CONFIG%"
    exit /b 40
)

call "%LOCAL_CONFIG%"

if not defined ARM_GDB (
    echo [GDB][ERROR] ARM_GDB is not defined in toolchain.local.bat.
    call :write_failure_logs 41 "ARM_GDB is not defined in toolchain.local.bat."
    exit /b 41
)
if not exist "%ARM_GDB%" (
    echo [GDB][ERROR] ARM GDB not found: %ARM_GDB%
    call :write_failure_logs 44 "ARM GDB not found: %ARM_GDB%"
    exit /b 44
)

if not defined JLINK_GDB_SERVER (
    echo [GDB][ERROR] JLINK_GDB_SERVER is not defined in toolchain.local.bat.
    call :write_failure_logs 41 "JLINK_GDB_SERVER is not defined in toolchain.local.bat."
    exit /b 41
)
if not exist "%JLINK_GDB_SERVER%" (
    echo [GDB][ERROR] J-Link GDB Server not found: %JLINK_GDB_SERVER%
    call :write_failure_logs 42 "J-Link GDB Server not found: %JLINK_GDB_SERVER%"
    exit /b 42
)

if not defined JLINK_RTT_LOGGER (
    echo [GDB][ERROR] JLINK_RTT_LOGGER is not defined in toolchain.local.bat.
    call :write_failure_logs 41 "JLINK_RTT_LOGGER is not defined in toolchain.local.bat."
    exit /b 41
)
if not exist "%JLINK_RTT_LOGGER%" (
    echo [GDB][ERROR] J-Link RTT Logger not found: %JLINK_RTT_LOGGER%
    call :write_failure_logs 43 "J-Link RTT Logger not found: %JLINK_RTT_LOGGER%"
    exit /b 43
)

if not exist "%AXF%" (
    echo [GDB][ERROR] AXF not found: %AXF%
    call :write_failure_logs 45 "AXF not found: %AXF%"
    exit /b 45
)
if not exist "%FAULT_GDB%" (
    echo [GDB][ERROR] Fault GDB script not found: %FAULT_GDB%
    call :write_failure_logs 46 "Fault GDB script not found: %FAULT_GDB%"
    exit /b 46
)

if not defined JLINK_DEVICE set "JLINK_DEVICE=STM32F411CE"
if not defined JLINK_IF set "JLINK_IF=SWD"
if not defined JLINK_SPEED set "JLINK_SPEED=4000"
if not defined GDB_PORT set "GDB_PORT=2331"
if not defined GDB_START_TIMEOUT_SECONDS set "GDB_START_TIMEOUT_SECONDS=10"
if not defined JLINK_RTT_CHANNEL set "JLINK_RTT_CHANNEL=0"

for /f "delims=0123456789" %%A in ("%GDB_PORT%") do (
    echo [GDB][ERROR] GDB_PORT must be a positive integer.
    call :write_failure_logs 47 "GDB_PORT must be a positive integer."
    exit /b 47
)
if "%GDB_PORT%"=="0" (
    echo [GDB][ERROR] GDB_PORT must be a positive integer.
    call :write_failure_logs 47 "GDB_PORT must be a positive integer."
    exit /b 47
)

if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"
if not exist "%LOG_DIR%" (
    echo [GDB][ERROR] Failed to create log directory: %LOG_DIR%
    exit /b 49
)

echo ============================================================
echo [GDB] Fault capture target: %JLINK_DEVICE%
echo [GDB] GDB log           : %GDB_LOG%
echo [GDB] RTT log           : %RTT_LOG%
echo ============================================================

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%FAULT_PS1%" ^
    -Gdb "%ARM_GDB%" ^
    -JLinkGdbServer "%JLINK_GDB_SERVER%" ^
    -RttLogger "%JLINK_RTT_LOGGER%" ^
    -Axf "%AXF%" ^
    -Device "%JLINK_DEVICE%" ^
    -Interface "%JLINK_IF%" ^
    -Speed %JLINK_SPEED% ^
    -Port %GDB_PORT% ^
    -StartTimeoutSeconds %GDB_START_TIMEOUT_SECONDS% ^
    -RttChannel %JLINK_RTT_CHANNEL% ^
    -Mode "%FAULT_MODE%" ^
    -GdbScript "%FAULT_GDB%" ^
    -ServerLog "%SERVER_LOG%" ^
    -GdbLog "%GDB_LOG%" ^
    -RttLog "%RTT_LOG%"

set "GDB_RESULT=%ERRORLEVEL%"
echo.
if "%GDB_RESULT%"=="0" (
    echo [GDB][PASS] Fault capture completed; MCU remains halted.
) else (
    echo [GDB][FAIL] Fault capture returned ERRORLEVEL=%GDB_RESULT%.
)
echo [GDB] GDB log: %GDB_LOG%
echo [GDB] RTT log: %RTT_LOG%
echo [GDB] Server log: %SERVER_LOG%

exit /b %GDB_RESULT%

:write_failure_logs
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%" >nul 2>&1
if exist "%LOG_DIR%" (
    >"%SERVER_LOG%" echo [GDB][FAIL] ERRORLEVEL=%~1 %~2
    >"%GDB_LOG%" echo [GDB][FAIL] ERRORLEVEL=%~1 %~2
    >"%RTT_LOG%" echo [RTT][FAIL] ERRORLEVEL=%~1 %~2
)
exit /b 0
