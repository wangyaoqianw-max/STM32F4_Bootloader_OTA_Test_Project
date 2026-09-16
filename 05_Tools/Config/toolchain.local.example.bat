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

REM GNU Arm GDB
set "ARM_GDB="

REM Temporary S04 board-test AXF, built outside the formal Keil target.
REM Leave empty until the temporary test target is built for a board rerun.
set "S04_PERSISTENCE_AXF="

REM SEGGER J-Link GDB Server
set "JLINK_GDB_SERVER="

REM Stable target connection settings verified for this project.
set "JLINK_DEVICE=STM32F411CE"
set "JLINK_IF=SWD"
set "JLINK_SPEED=4000"

REM GDB connection settings
set "GDB_PORT=2331"
set "GDB_START_TIMEOUT_SECONDS=10"

REM RTT capture defaults.
set "JLINK_RTT_CHANNEL=0"
set "RTT_CAPTURE_SECONDS=10"
set "S04_PERSISTENCE_CAPTURE_SECONDS=90"

REM External serial module. S04 persistence evidence uses RTT instead.
set "SERIAL_PORT=COM9"

REM Python host-side tools. Leave empty to use python from PATH.
set "PYTHON_EXE="

REM Tera Term 5
set "TERA_TERM_EXE="
