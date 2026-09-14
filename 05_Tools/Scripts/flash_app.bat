@echo off
setlocal EnableExtensions

REM ============================================================
REM OTA_APP J-Link Flash Entry
REM
REM Purpose:
REM   Program the latest Keil-generated OTA_APP.hex and start it.
REM
REM Usage:
REM   05_Tools\Scripts\flash_app.bat
REM ============================================================

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"

set "LOCAL_CONFIG=%REPO_ROOT%\05_Tools\Config\toolchain.local.bat"
set "FIRMWARE_FILE=%REPO_ROOT%\03_Firmware\Application\OTA_APP\MDK-ARM\Objects\OTA_APP.hex"
set "LOG_DIR=%REPO_ROOT%\06_Output\Logs"
set "FLASH_LOG=%LOG_DIR%\OTA_APP_flash.log"
set "JLINK_COMMAND_FILE=%TEMP%\stm32f4_ota_flash_%RANDOM%_%RANDOM%.jlink"

if not exist "%LOCAL_CONFIG%" (
    echo [FLASH][ERROR] Local toolchain configuration not found.
    echo [FLASH][ERROR] Expected: %LOCAL_CONFIG%
    echo [FLASH][INFO] Copy 05_Tools\Config\toolchain.local.example.bat to toolchain.local.bat and update local paths.
    exit /b 20
)

call "%LOCAL_CONFIG%"

if not defined JLINK_EXE (
    echo [FLASH][ERROR] JLINK_EXE is not defined in toolchain.local.bat.
    exit /b 21
)

if not exist "%JLINK_EXE%" (
    echo [FLASH][ERROR] J-Link Commander not found: %JLINK_EXE%
    exit /b 22
)

if not defined JLINK_DEVICE set "JLINK_DEVICE=STM32F411CE"
if not defined JLINK_IF set "JLINK_IF=SWD"
if not defined JLINK_SPEED set "JLINK_SPEED=4000"

if not exist "%FIRMWARE_FILE%" (
    echo [FLASH][ERROR] Firmware image not found:
    echo     %FIRMWARE_FILE%
    echo [FLASH][INFO] Run 05_Tools\Scripts\build_app.bat first.
    exit /b 23
)

if not exist "%LOG_DIR%" (
    mkdir "%LOG_DIR%"
    if errorlevel 1 (
        echo [FLASH][ERROR] Failed to create log directory: %LOG_DIR%
        exit /b 24
    )
)

> "%JLINK_COMMAND_FILE%" echo EoE 1
>>"%JLINK_COMMAND_FILE%" echo r
>>"%JLINK_COMMAND_FILE%" echo h
>>"%JLINK_COMMAND_FILE%" echo loadfile "%FIRMWARE_FILE%"
>>"%JLINK_COMMAND_FILE%" echo r
>>"%JLINK_COMMAND_FILE%" echo h
>>"%JLINK_COMMAND_FILE%" echo g
>>"%JLINK_COMMAND_FILE%" echo exit

echo ============================================================
echo [FLASH] Device   : %JLINK_DEVICE%
echo [FLASH] Interface: %JLINK_IF%
echo [FLASH] Speed    : %JLINK_SPEED% kHz
echo [FLASH] Firmware : %FIRMWARE_FILE%
echo [FLASH] Log      : %FLASH_LOG%
echo ============================================================

"%JLINK_EXE%" ^
    -Device "%JLINK_DEVICE%" ^
    -If "%JLINK_IF%" ^
    -Speed "%JLINK_SPEED%" ^
    -AutoConnect 1 ^
    -ExitOnError 1 ^
    -NoGui 1 ^
    -CommandFile "%JLINK_COMMAND_FILE%" > "%FLASH_LOG%" 2>&1

set "JLINK_RESULT=%ERRORLEVEL%"

if exist "%JLINK_COMMAND_FILE%" del /q "%JLINK_COMMAND_FILE%" >nul 2>&1

echo.
if exist "%FLASH_LOG%" type "%FLASH_LOG%"

echo.
if "%JLINK_RESULT%"=="0" (
    echo [FLASH][PASS] J-Link programming command completed successfully.
) else (
    echo [FLASH][FAIL] J-Link returned ERRORLEVEL=%JLINK_RESULT%.
    echo [FLASH][INFO] Check that Keil, RTT Viewer, RTT Logger, or another debugger is not holding the J-Link.
)

exit /b %JLINK_RESULT%
