@echo off
setlocal

REM ============================================================
REM OTA_APP Keil Build Entry
REM
REM Purpose:
REM   Provide one stable build command for developers and AI agents.
REM
REM Usage:
REM   05_Tools\Scripts\build_app.bat
REM ============================================================

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"

set "LOCAL_CONFIG=%REPO_ROOT%\05_Tools\Config\toolchain.local.bat"
set "PROJECT_FILE=%REPO_ROOT%\03_Firmware\Application\OTA_APP\MDK-ARM\OTA_APP.uvprojx"
set "TARGET_NAME=OTA_APP"
set "BUILD_LOG_DIR=%REPO_ROOT%\06_Output\Logs"
set "BUILD_LOG=%BUILD_LOG_DIR%\OTA_APP_build.log"

if not exist "%LOCAL_CONFIG%" (
    echo [BUILD][ERROR] Local toolchain configuration not found.
    echo [BUILD][ERROR] Expected: %LOCAL_CONFIG%
    echo [BUILD][INFO] Copy 05_Tools\Config\toolchain.local.example.bat to toolchain.local.bat and update local paths.
    exit /b 10
)

call "%LOCAL_CONFIG%"

if not defined KEIL_UV4 (
    echo [BUILD][ERROR] KEIL_UV4 is not defined in toolchain.local.bat.
    exit /b 11
)

if not exist "%KEIL_UV4%" (
    echo [BUILD][ERROR] Keil executable not found: %KEIL_UV4%
    exit /b 12
)

if not exist "%PROJECT_FILE%" (
    echo [BUILD][ERROR] Keil project not found: %PROJECT_FILE%
    exit /b 13
)

if not exist "%BUILD_LOG_DIR%" (
    mkdir "%BUILD_LOG_DIR%"
    if errorlevel 1 (
        echo [BUILD][ERROR] Failed to create build log directory: %BUILD_LOG_DIR%
        exit /b 14
    )
)

echo ============================================================
echo [BUILD] Project : OTA_APP
echo [BUILD] Target  : %TARGET_NAME%
echo [BUILD] Tool    : %KEIL_UV4%
echo [BUILD] Log     : %BUILD_LOG%
echo ============================================================

"%KEIL_UV4%" -b "%PROJECT_FILE%" -t "%TARGET_NAME%" -o "%BUILD_LOG%"
set "KEIL_RESULT=%ERRORLEVEL%"

echo.
if exist "%BUILD_LOG%" (
    type "%BUILD_LOG%"
) else (
    echo [BUILD][WARN] Keil build log was not created.
)

echo.
if "%KEIL_RESULT%"=="0" (
    echo [BUILD][PASS] Keil build completed without errors or warnings.
) else if "%KEIL_RESULT%"=="1" (
    echo [BUILD][WARN] Keil build completed with warnings.
) else if "%KEIL_RESULT%"=="2" (
    echo [BUILD][FAIL] Keil build completed with errors.
) else if "%KEIL_RESULT%"=="3" (
    echo [BUILD][FAIL] Keil reported a fatal build error.
) else (
    echo [BUILD][FAIL] Keil returned unexpected ERRORLEVEL=%KEIL_RESULT%.
)

exit /b %KEIL_RESULT%
