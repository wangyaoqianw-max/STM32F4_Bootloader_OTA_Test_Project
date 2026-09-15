@echo off
setlocal EnableExtensions

REM ============================================================
REM OTA_APP J-Link GDB Server Entry
REM
REM Usage:
REM   05_Tools\Scripts\start_gdb_server.bat
REM
REM This is a foreground helper for manual GDB sessions. The
REM runtime snapshot entrypoint owns and cleans up its own server.
REM ============================================================

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"

set "LOCAL_CONFIG=%REPO_ROOT%\05_Tools\Config\toolchain.local.bat"

if not exist "%LOCAL_CONFIG%" (
    echo [GDB][ERROR] Local toolchain configuration not found.
    echo [GDB][ERROR] Expected: %LOCAL_CONFIG%
    exit /b 40
)

call "%LOCAL_CONFIG%"

if not defined JLINK_GDB_SERVER (
    echo [GDB][ERROR] JLINK_GDB_SERVER is not defined in toolchain.local.bat.
    exit /b 41
)

if not exist "%JLINK_GDB_SERVER%" (
    echo [GDB][ERROR] J-Link GDB Server not found: %JLINK_GDB_SERVER%
    exit /b 42
)

if not defined JLINK_DEVICE set "JLINK_DEVICE=STM32F411CE"
if not defined JLINK_IF set "JLINK_IF=SWD"
if not defined JLINK_SPEED set "JLINK_SPEED=4000"
if not defined GDB_PORT set "GDB_PORT=2331"

echo ============================================================
echo [GDB] Server   : %JLINK_GDB_SERVER%
echo [GDB] Device   : %JLINK_DEVICE%
echo [GDB] Interface: %JLINK_IF%
echo [GDB] Speed    : %JLINK_SPEED% kHz
echo [GDB] Port     : %GDB_PORT%
echo ============================================================

"%JLINK_GDB_SERVER%" ^
    -device "%JLINK_DEVICE%" ^
    -if "%JLINK_IF%" ^
    -speed "%JLINK_SPEED%" ^
    -port "%GDB_PORT%" ^
    -swoport 2332 ^
    -telnetport 2333 ^
    -nogui

set "SERVER_RESULT=%ERRORLEVEL%"
if "%SERVER_RESULT%"=="0" (
    echo [GDB][PASS] J-Link GDB Server exited normally.
) else (
    echo [GDB][FAIL] J-Link GDB Server returned ERRORLEVEL=%SERVER_RESULT%.
)

exit /b %SERVER_RESULT%
