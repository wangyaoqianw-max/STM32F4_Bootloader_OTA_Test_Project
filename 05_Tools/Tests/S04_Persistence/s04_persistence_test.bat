@echo off
setlocal EnableExtensions

REM ============================================================
REM S04 Reset / Power-cycle Persistence Test Entry
REM
REM Usage:
REM   05_Tools\Scripts\s04_persistence_test.bat reset
REM   05_Tools\Scripts\s04_persistence_test.bat power-cycle
REM
REM reset      : GDB reads a baseline, executes reset and continue&,
REM              reads the post-reset snapshot, then disconnects.
REM power-cycle: RTT listener starts first; operator power-cycles the
REM              target after the READY message.
REM ============================================================

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..\..") do set "REPO_ROOT=%%~fI"

set "LOCAL_CONFIG=%REPO_ROOT%\05_Tools\Config\toolchain.local.bat"
set "PERSISTENCE_PY=%SCRIPT_DIR%s04_persistence_test.py"
set "PERSISTENCE_ROOT=%REPO_ROOT%\04_Test\Host\S04_Firmware_Image_Storage"
set "PERSISTENCE_PARSER=%PERSISTENCE_ROOT%\s04_persistence_log.py"
set "GDB_ROOT=%REPO_ROOT%\05_Tools\Tests\S04_Persistence"
set "GDB_SCRIPT=%GDB_ROOT%\s04_reset_persistence.gdb"
set "AXF="
set "LOG_DIR=%REPO_ROOT%\06_Output\Logs"
set "SERVER_LOG=%LOG_DIR%\S04_persistence_gdb_server.log"
set "RESET_LOG=%LOG_DIR%\S04_reset_persistence.log"
set "POWER_LOG=%LOG_DIR%\S04_power_cycle_persistence.log"
set "MODE=%~1"

if /I not "%MODE%"=="reset" if /I not "%MODE%"=="power-cycle" (
    echo [S04-PERSIST][ERROR] Mode must be reset or power-cycle.
    exit /b 40
)

if not exist "%LOCAL_CONFIG%" (
    echo [S04-PERSIST][ERROR] Local toolchain configuration not found.
    echo [S04-PERSIST][ERROR] Expected: %LOCAL_CONFIG%
    exit /b 41
)

call "%LOCAL_CONFIG%"

if not defined S04_PERSISTENCE_AXF (
    echo [S04-PERSIST][ERROR] S04_PERSISTENCE_AXF is not defined.
    echo [S04-PERSIST][INFO] Set it to a separately built S04 persistence test AXF.
    exit /b 48
)
set "AXF=%S04_PERSISTENCE_AXF%"

if not defined PYTHON_EXE set "PYTHON_EXE=python"
if not defined JLINK_DEVICE set "JLINK_DEVICE=STM32F411CE"
if not defined JLINK_IF set "JLINK_IF=SWD"
if not defined JLINK_SPEED set "JLINK_SPEED=4000"
if not defined GDB_PORT set "GDB_PORT=2331"
if not defined GDB_START_TIMEOUT_SECONDS set "GDB_START_TIMEOUT_SECONDS=10"
if not defined JLINK_RTT_CHANNEL set "JLINK_RTT_CHANNEL=0"
if not defined S04_PERSISTENCE_CAPTURE_SECONDS set "S04_PERSISTENCE_CAPTURE_SECONDS=90"
if not defined SERIAL_PORT set "SERIAL_PORT=COM9"

if not exist "%PERSISTENCE_PY%" (
    echo [S04-PERSIST][ERROR] Python test runner not found: %PERSISTENCE_PY%
    exit /b 42
)
if not exist "%PERSISTENCE_PARSER%" (
    echo [S04-PERSIST][ERROR] Host parser not found: %PERSISTENCE_PARSER%
    exit /b 43
)
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%" >nul 2>&1

if /I "%MODE%"=="reset" (
    if not defined ARM_GDB (
        echo [S04-PERSIST][ERROR] ARM_GDB is not defined in toolchain.local.bat.
        exit /b 44
    )
    if not exist "%ARM_GDB%" (
        echo [S04-PERSIST][ERROR] ARM GDB not found: %ARM_GDB%
        exit /b 45
    )
    if not defined JLINK_GDB_SERVER (
        echo [S04-PERSIST][ERROR] JLINK_GDB_SERVER is not defined in toolchain.local.bat.
        exit /b 46
    )
    if not exist "%JLINK_GDB_SERVER%" (
        echo [S04-PERSIST][ERROR] J-Link GDB Server not found: %JLINK_GDB_SERVER%
        exit /b 47
    )
    if not exist "%AXF%" (
        echo [S04-PERSIST][ERROR] AXF not found: %AXF%
        echo [S04-PERSIST][INFO] Run 05_Tools\Scripts\build_app.bat first.
        exit /b 48
    )
    if not exist "%GDB_SCRIPT%" (
        echo [S04-PERSIST][ERROR] GDB command script not found: %GDB_SCRIPT%
        exit /b 49
    )

    echo ============================================================
    echo [S04-PERSIST] Mode     : reset
    echo [S04-PERSIST] Device   : %JLINK_DEVICE%
    echo [S04-PERSIST] Interface: %JLINK_IF%
    echo [S04-PERSIST] Speed    : %JLINK_SPEED% kHz
    echo [S04-PERSIST] Output   : %RESET_LOG%
    echo ============================================================

    "%PYTHON_EXE%" "%PERSISTENCE_PY%" ^
        --mode reset ^
        --gdb "%ARM_GDB%" ^
        --server "%JLINK_GDB_SERVER%" ^
        --rtt-logger "%JLINK_RTT_LOGGER%" ^
        --axf "%AXF%" ^
        --gdb-script "%GDB_SCRIPT%" ^
        --parser "%PERSISTENCE_PARSER%" ^
        --device "%JLINK_DEVICE%" ^
        --interface "%JLINK_IF%" ^
        --speed %JLINK_SPEED% ^
        --port %GDB_PORT% ^
        --channel %JLINK_RTT_CHANNEL% ^
        --start-timeout %GDB_START_TIMEOUT_SECONDS% ^
        --capture-seconds %S04_PERSISTENCE_CAPTURE_SECONDS% ^
        --server-log "%SERVER_LOG%" ^
        --output-log "%RESET_LOG%" ^
        --log-directory "%LOG_DIR%" ^
        --serial-port "%SERIAL_PORT%"
    exit /b %ERRORLEVEL%
)

if not defined JLINK_RTT_LOGGER (
    echo [S04-PERSIST][ERROR] JLINK_RTT_LOGGER is not defined in toolchain.local.bat.
    exit /b 50
)
if not exist "%JLINK_RTT_LOGGER%" (
    echo [S04-PERSIST][ERROR] J-Link RTT Logger not found: %JLINK_RTT_LOGGER%
    exit /b 51
)
if not defined ARM_GDB (
    echo [S04-PERSIST][ERROR] ARM_GDB is required to resolve the test AXF symbols.
    exit /b 52
)
if not exist "%ARM_GDB%" (
    echo [S04-PERSIST][ERROR] ARM GDB not found: %ARM_GDB%
    exit /b 53
)
if not exist "%AXF%" (
    echo [S04-PERSIST][ERROR] S04 persistence test AXF not found: %AXF%
    echo [S04-PERSIST][INFO] Set S04_PERSISTENCE_AXF to a separately built test image.
    exit /b 54
)

echo ============================================================
echo [S04-PERSIST] Mode     : power-cycle
echo [S04-PERSIST] Device   : %JLINK_DEVICE%
echo [S04-PERSIST] Interface: %JLINK_IF%
echo [S04-PERSIST] Speed    : %JLINK_SPEED% kHz
echo [S04-PERSIST] Channel  : %JLINK_RTT_CHANNEL%
echo [S04-PERSIST] Duration : %S04_PERSISTENCE_CAPTURE_SECONDS% s
echo [S04-PERSIST] Serial   : %SERIAL_PORT% (informational only)
echo [S04-PERSIST] Output   : %POWER_LOG%
echo ============================================================

"%PYTHON_EXE%" "%PERSISTENCE_PY%" ^
    --mode power-cycle ^
    --gdb "%ARM_GDB%" ^
    --server "%JLINK_GDB_SERVER%" ^
    --rtt-logger "%JLINK_RTT_LOGGER%" ^
    --axf "%AXF%" ^
    --gdb-script "%GDB_SCRIPT%" ^
    --parser "%PERSISTENCE_PARSER%" ^
    --device "%JLINK_DEVICE%" ^
    --interface "%JLINK_IF%" ^
    --speed %JLINK_SPEED% ^
    --port %GDB_PORT% ^
    --channel %JLINK_RTT_CHANNEL% ^
    --start-timeout %GDB_START_TIMEOUT_SECONDS% ^
    --capture-seconds %S04_PERSISTENCE_CAPTURE_SECONDS% ^
    --server-log "%SERVER_LOG%" ^
    --output-log "%POWER_LOG%" ^
    --log-directory "%LOG_DIR%" ^
    --serial-port "%SERIAL_PORT%"

exit /b %ERRORLEVEL%
