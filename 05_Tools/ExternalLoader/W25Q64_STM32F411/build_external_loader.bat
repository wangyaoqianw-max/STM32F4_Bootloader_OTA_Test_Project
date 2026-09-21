@echo off
setlocal EnableExtensions

set "PROJECT_ROOT=%~dp0..\..\.."
call "%PROJECT_ROOT%\05_Tools\Config\toolchain.local.bat"
if not defined KEIL_UV4 (
    echo [LOADER][ERROR] KEIL_UV4 is not configured.
    exit /b 10
)
if not exist "%KEIL_UV4%" (
    echo [LOADER][ERROR] Keil executable not found: %KEIL_UV4%
    exit /b 10
)

set "PROJECT_FILE=%~dp0MDK-ARM\W25Q64_STM32F411.uvprojx"
set "OBJECT_FILE=%~dp0MDK-ARM\Objects\W25Q64_STM32F411.axf"
set "OUTPUT_DIR=%PROJECT_ROOT%\06_Output\Packages\ExternalLoader"
set "LOG_DIR=%PROJECT_ROOT%\06_Output\Logs\ExternalLoader"
set "STLDR_FILE=%OUTPUT_DIR%\W25Q64_STM32F411.stldr"

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

echo [LOADER] Building %PROJECT_FILE%
"%KEIL_UV4%" -b "%PROJECT_FILE%" -t W25Q64_STM32F411 -o "%LOG_DIR%\W25Q64_STM32F411_build.log"
if errorlevel 1 (
    echo [LOADER][FAIL] Keil build failed. See %LOG_DIR%\W25Q64_STM32F411_build.log
    exit /b 20
)
if not exist "%OBJECT_FILE%" (
    echo [LOADER][FAIL] AXF output not found: %OBJECT_FILE%
    exit /b 20
)

copy /Y "%OBJECT_FILE%" "%STLDR_FILE%" >nul
if errorlevel 1 (
    echo [LOADER][FAIL] Could not create %STLDR_FILE%
    exit /b 20
)

echo [LOADER][PASS] %STLDR_FILE%
exit /b 0
