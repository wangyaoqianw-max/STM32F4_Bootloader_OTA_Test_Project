@echo off

REM ============================================================
REM Local Toolchain Configuration Template
REM
REM Copy this file to:
REM     05_Tools\Config\toolchain.local.bat
REM
REM toolchain.local.bat is machine-local and must not be committed.
REM Fill in tool paths for the current development machine.
REM ============================================================

REM Keil MDK / uVision 5
set "KEIL_UV4="

REM SEGGER J-Link tools
set "JLINK_EXE="
set "JLINK_RTT_LOGGER="

REM Stable target connection settings verified for this project.
set "JLINK_DEVICE=STM32F411CE"
set "JLINK_IF=SWD"
set "JLINK_SPEED=4000"

REM RTT capture defaults.
set "JLINK_RTT_CHANNEL=0"
set "RTT_CAPTURE_SECONDS=10"

REM Reserved for later host-side tools.
REM set "PYTHON_EXE=python"

REM Tera Term 5
set "TERA_TERM_EXE="
