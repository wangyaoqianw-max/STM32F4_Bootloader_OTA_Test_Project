@echo off
setlocal EnableExtensions DisableDelayedExpansion

REM ============================================================
REM AI-callable Python YMODEM Sender Entry
REM
REM Usage:
REM   05_Tools\Scripts\send_ymodem_python.bat devices --json
REM   05_Tools\Scripts\send_ymodem_python.bat send app.bin --port COM7 --baud 115200
REM
REM PYTHON_EXE may be set in the ignored local toolchain configuration.
REM ============================================================

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
set "SENDER_SCRIPT=%REPO_ROOT%\05_Tools\Ymodem\ymodem_sender.py"
set "PYTHON_EXE=python"
set "LOCAL_CONFIG=%REPO_ROOT%\05_Tools\Config\toolchain.local.bat"

if exist "%LOCAL_CONFIG%" call "%LOCAL_CONFIG%"
if not defined PYTHON_EXE set "PYTHON_EXE=python"

if not exist "%SENDER_SCRIPT%" (
    echo [YMODEM][ERROR] Python sender not found: %SENDER_SCRIPT% 1>&2
    exit /b 10
)

"%PYTHON_EXE%" "%SENDER_SCRIPT%" %*
exit /b %ERRORLEVEL%
