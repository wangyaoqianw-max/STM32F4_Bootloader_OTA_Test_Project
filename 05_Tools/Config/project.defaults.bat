@echo off

REM ============================================================
REM Committed project facts for the reusable toolkit.
REM Paths are relative to the repository root.
REM ============================================================

set "PROJECT_KEIL_PROJECT_FILE=03_Firmware\Application\OTA_APP\MDK-ARM\OTA_APP.uvprojx"
set "PROJECT_KEIL_TARGET=OTA_APP"
set "PROJECT_OUTPUT_DIR=03_Firmware\Application\OTA_APP\MDK-ARM\Objects"
set "PROJECT_APP_AXF=03_Firmware\Application\OTA_APP\MDK-ARM\Objects\OTA_APP.axf"
set "PROJECT_APP_HEX=03_Firmware\Application\OTA_APP\MDK-ARM\Objects\OTA_APP.hex"
set "PROJECT_APP_BIN=03_Firmware\Application\OTA_APP\MDK-ARM\Objects\OTA_APP.bin"
set "PROJECT_LOG_DIR=06_Output\Logs"
set "JLINK_DEVICE=STM32F411CE"
set "JLINK_IF=SWD"
