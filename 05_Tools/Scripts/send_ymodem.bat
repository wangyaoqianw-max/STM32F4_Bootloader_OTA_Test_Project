@echo off
setlocal EnableExtensions DisableDelayedExpansion

REM ============================================================
REM Tera Term 5 YMODEM Sender Entry
REM
REM Usage:
REM   05_Tools\Scripts\send_ymodem.bat <COMx> <baud> <firmware.img>
REM
REM The local Tera Term path is loaded from the ignored
REM 05_Tools\Config\toolchain.local.bat file.
REM ============================================================

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"

set "LOCAL_CONFIG=%REPO_ROOT%\05_Tools\Config\toolchain.local.bat"
set "MACRO_FILE=%REPO_ROOT%\05_Tools\TeraTerm\send_ymodem.ttl"

if not exist "%LOCAL_CONFIG%" (
    echo [YMODEM][ERROR] Local toolchain configuration not found.
    echo [YMODEM][ERROR] Expected: %LOCAL_CONFIG%
    echo [YMODEM][INFO] Copy 05_Tools\Config\toolchain.local.example.bat to toolchain.local.bat and update local paths.
    exit /b 10
)

call "%LOCAL_CONFIG%"

if not defined TERA_TERM_EXE (
    echo [YMODEM][ERROR] TERA_TERM_EXE is not defined in toolchain.local.bat.
    exit /b 11
)

if not exist "%TERA_TERM_EXE%" (
    echo [YMODEM][ERROR] Tera Term executable not found: %TERA_TERM_EXE%
    exit /b 12
)

for %%I in ("%TERA_TERM_EXE%") do set "TTPMACRO_EXE=%%~dpIttpmacro.exe"
if not exist "%TTPMACRO_EXE%" (
    echo [YMODEM][ERROR] Tera Term macro executable not found: %TTPMACRO_EXE%
    exit /b 13
)

if not exist "%MACRO_FILE%" (
    echo [YMODEM][ERROR] YMODEM macro not found: %MACRO_FILE%
    exit /b 14
)

if "%~1"=="" goto usage
if "%~2"=="" goto usage
if "%~3"=="" goto usage

set "COM_INPUT=%~1"
if /I "%COM_INPUT:~0,3%"=="COM" set "COM_INPUT=%COM_INPUT:~3%"
if not defined COM_INPUT goto invalid_com
set "COM_INVALID="
for /f "delims=0123456789" %%A in ("%COM_INPUT%") do set "COM_INVALID=1"
if defined COM_INVALID goto invalid_com
if "%COM_INPUT%"=="0" goto invalid_com

set "BAUD_INPUT=%~2"
set "BAUD_INVALID="
for /f "delims=0123456789" %%A in ("%BAUD_INPUT%") do set "BAUD_INVALID=1"
if defined BAUD_INVALID goto invalid_baud
if "%BAUD_INPUT%"=="" goto invalid_baud
if "%BAUD_INPUT%"=="0" goto invalid_baud

for %%I in ("%~3") do set "FIRMWARE_FILE=%%~fI"
if not exist "%FIRMWARE_FILE%" (
    echo [YMODEM][ERROR] Firmware image not found: %FIRMWARE_FILE%
    exit /b 16
)

echo ============================================================
echo [YMODEM] Serial port : COM%COM_INPUT%
echo [YMODEM] Baud rate   : %BAUD_INPUT%
echo [YMODEM] Firmware    : %FIRMWARE_FILE%
echo [YMODEM] Macro       : %MACRO_FILE%
echo ============================================================

"%TTPMACRO_EXE%" /V "%MACRO_FILE%" "%COM_INPUT%" "%BAUD_INPUT%" "%FIRMWARE_FILE%"
set "MACRO_RESULT=%ERRORLEVEL%"

if "%MACRO_RESULT%"=="0" (
    echo [YMODEM][PASS] Tera Term reported a successful YMODEM transfer.
) else (
    echo [YMODEM][FAIL] Tera Term reported YMODEM failure. ERRORLEVEL=%MACRO_RESULT%
)

exit /b %MACRO_RESULT%

:invalid_com
echo [YMODEM][ERROR] COM port must be a positive number or COMx: %~1
exit /b 15

:invalid_baud
echo [YMODEM][ERROR] Baud rate must be a positive integer: %~2
exit /b 15

:usage
echo Usage: %~nx0 ^<COMx^> ^<baud^> ^<firmware.img^>
exit /b 15
