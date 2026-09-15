@echo off
setlocal EnableExtensions

REM ============================================================
REM OTA_APP GDB Runtime Snapshot Entry
REM
REM Usage:
REM   05_Tools\Scripts\gdb_runtime_snapshot.bat resume
REM   05_Tools\Scripts\gdb_runtime_snapshot.bat halt
REM
REM The PowerShell wrapper owns the J-Link GDB Server PID and
REM never executes GDB load.
REM ============================================================

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"

set "LOCAL_CONFIG=%REPO_ROOT%\05_Tools\Config\toolchain.local.bat"
set "GDB_PS1=%SCRIPT_DIR%gdb_runtime_snapshot.ps1"
set "GDB_ROOT=%REPO_ROOT%\05_Tools\Debug\GDB"
set "AXF=%REPO_ROOT%\03_Firmware\Application\OTA_APP\MDK-ARM\Objects\OTA_APP.axf"
set "LOG_DIR=%REPO_ROOT%\06_Output\Logs"
set "SERVER_LOG=%LOG_DIR%\OTA_APP_gdb_server.log"
set "SNAPSHOT_LOG=%LOG_DIR%\OTA_APP_gdb_snapshot.log"

if not exist "%LOCAL_CONFIG%" (
    echo [GDB][ERROR] Local toolchain configuration not found.
    echo [GDB][ERROR] Expected: %LOCAL_CONFIG%
    call :write_failure_logs 40 "Local toolchain configuration not found: %LOCAL_CONFIG%"
    exit /b 40
)

call "%LOCAL_CONFIG%"

if /I not "%~1"=="resume" if /I not "%~1"=="halt" (
    echo [GDB][ERROR] Mode must be resume or halt.
    call :write_failure_logs 43 "Mode must be resume or halt."
    exit /b 43
)

set "GDB_MODE=%~1"
set "GDB_SCRIPT=%GDB_ROOT%\runtime_snapshot_%GDB_MODE%.gdb"

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

if not exist "%AXF%" (
    echo [GDB][ERROR] AXF not found: %AXF%
    echo [GDB][INFO] Run 05_Tools\Scripts\build_app.bat first.
    call :write_failure_logs 45 "AXF not found: %AXF%"
    exit /b 45
)

if not exist "%GDB_SCRIPT%" (
    echo [GDB][ERROR] GDB command script not found: %GDB_SCRIPT%
    call :write_failure_logs 46 "GDB command script not found: %GDB_SCRIPT%"
    exit /b 46
)

if not defined JLINK_DEVICE set "JLINK_DEVICE=STM32F411CE"
if not defined JLINK_IF set "JLINK_IF=SWD"
if not defined JLINK_SPEED set "JLINK_SPEED=4000"
if not defined GDB_PORT set "GDB_PORT=2331"
if not defined GDB_START_TIMEOUT_SECONDS set "GDB_START_TIMEOUT_SECONDS=10"

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

for /f "delims=0123456789" %%A in ("%GDB_START_TIMEOUT_SECONDS%") do (
    echo [GDB][ERROR] GDB_START_TIMEOUT_SECONDS must be a positive integer.
    call :write_failure_logs 48 "GDB_START_TIMEOUT_SECONDS must be a positive integer."
    exit /b 48
)
if "%GDB_START_TIMEOUT_SECONDS%"=="0" (
    echo [GDB][ERROR] GDB_START_TIMEOUT_SECONDS must be a positive integer.
    call :write_failure_logs 48 "GDB_START_TIMEOUT_SECONDS must be a positive integer."
    exit /b 48
)

if not exist "%LOG_DIR%" (
    mkdir "%LOG_DIR%"
    if errorlevel 1 (
        echo [GDB][ERROR] Failed to create log directory: %LOG_DIR%
        exit /b 49
    )
)

echo ============================================================
echo [GDB] Mode     : %GDB_MODE%
echo [GDB] Device   : %JLINK_DEVICE%
echo [GDB] Interface: %JLINK_IF%
echo [GDB] Speed    : %JLINK_SPEED% kHz
echo [GDB] Port     : %GDB_PORT%
echo [GDB] AXF      : %AXF%
echo [GDB] ServerLog: %SERVER_LOG%
echo [GDB] Snapshot : %SNAPSHOT_LOG%
echo ============================================================

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%GDB_PS1%" ^
    -Gdb "%ARM_GDB%" ^
    -JLinkGdbServer "%JLINK_GDB_SERVER%" ^
    -Axf "%AXF%" ^
    -Device "%JLINK_DEVICE%" ^
    -Interface "%JLINK_IF%" ^
    -Speed %JLINK_SPEED% ^
    -Port %GDB_PORT% ^
    -StartTimeoutSeconds %GDB_START_TIMEOUT_SECONDS% ^
    -Mode "%GDB_MODE%" ^
    -GdbScript "%GDB_SCRIPT%" ^
    -ServerLog "%SERVER_LOG%" ^
    -SnapshotLog "%SNAPSHOT_LOG%"

set "GDB_RESULT=%ERRORLEVEL%"

echo.
if "%GDB_RESULT%"=="0" (
    echo [GDB][PASS] Runtime snapshot completed.
    echo [GDB] Server log: %SERVER_LOG%
    echo [GDB] Snapshot log: %SNAPSHOT_LOG%
) else (
    echo [GDB][FAIL] Runtime snapshot returned ERRORLEVEL=%GDB_RESULT%.
    echo [GDB] Server log: %SERVER_LOG%
    echo [GDB] Snapshot log: %SNAPSHOT_LOG%
)

exit /b %GDB_RESULT%

:write_failure_logs
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%" >nul 2>&1
if exist "%LOG_DIR%" (
    >"%SERVER_LOG%" echo [GDB][FAIL] ERRORLEVEL=%~1 %~2
    >"%SNAPSHOT_LOG%" echo [GDB][FAIL] ERRORLEVEL=%~1 %~2
)
exit /b 0
