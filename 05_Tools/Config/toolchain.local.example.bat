@echo off

REM ============================================================
REM Local Toolchain Configuration Template
REM
REM Copy this file to:
REM     05_Tools\Config\toolchain.local.bat
REM
REM toolchain.local.bat is machine-local and must not be committed.
REM Update the paths if the tools are installed elsewhere.
REM ============================================================

REM Keil MDK / uVision 5
set "KEIL_UV4=E:\APP\ProgramFile\MDK\Core\UV4\UV4.exe"

REM Reserved for future project automation.
REM set "JLINK_EXE=C:\Program Files\SEGGER\JLink\JLink.exe"
REM set "JLINK_RTT_LOGGER=C:\Program Files\SEGGER\JLink\JLinkRTTLogger.exe"
REM set "PYTHON_EXE=python"
