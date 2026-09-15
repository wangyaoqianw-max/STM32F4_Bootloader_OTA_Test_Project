# S05A Debug & Crash Diagnostics Handoff

## Metadata

- Stage: `S05A_Debug_Crash_Diagnostics`
- Status: `READY_FOR_IMPLEMENTATION`
- Branch: `main`
- Baseline Commit: `ec6119f64306d027c86208329823cf42b77ceebf`
- Current Scope: GDB automation and real-board verification
- Design / Implementation Plan Input: Project Owner supplied S05A plan, 2026-09-15
- Implementation Commit: `Not created yet`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Updated At: `2026-09-15`

## Position In Roadmap

S05A is a new small stage inserted after the closed `S05_UART_Ymodem` stage and before `S06_RTOS_Runtime`. It does not reopen S05 and does not change the S05 Ymodem or Firmware Storage contract.

The immediate S05A delivery is limited to:

```text
Keil AXF
  ↓
J-Link GDB Server
  ↓
GDB runtime snapshot automation
  ↓
Real-board verification
```

CmBacktrace source has not yet been downloaded. Fault injection, Cortex-M fault context integration, CmBacktrace integration, and the related fault-capture workflow are deferred and must not be reported as implemented in this checkpoint.

S04 Reset Persistence and Power-cycle Persistence remain `PENDING / DEFERRED`; they are not part of the current GDB automation checkpoint.

## Completed Manual GDB Baseline

The following was verified on the real STM32F411CE board without executing GDB `load`:

```text
J-Link GDB Server V7.92             PASS
J-Link V9.70 / target voltage 3.28V  PASS
STM32F411CE + SWD @ 4000 kHz        PASS
Keil OTA_APP.axf symbol loading     PASS
Source line mapping                 PASS
Breakpoint / Continue               PASS
Next / Step                         PASS
Backtrace                           PASS
Memory read                         PASS
Variable read                       PASS
```

The tested AXF is:

```text
03_Firmware/Application/OTA_APP/MDK-ARM/Objects/OTA_APP.axf
```

The tested tool executables are configured locally and must not be committed as machine-specific paths:

```text
JLinkGDBServerCL.exe
arm-none-eabi-gdb.exe
```

## Frozen Debug Behavior

```text
Target:     STM32F411CE
Interface:  SWD
Speed:      4000 kHz
GDB port:   2331
```

Use:

```gdb
monitor reset
```

Do not use `monitor reset halt`; J-Link GDB Server V7.92 rejects that form.

The exit behavior is:

```text
Halt state
→ detach
→ MCU remains halted
```

```text
Running state
→ continue&
→ disconnect
→ quit
→ MCU continues running
```

`continue&` followed by `disconnect` was verified by reconnecting after two seconds: `uwTick` advanced from `0` to `0x19E544`, and PC was in FreeRTOS `prvIdleTask`. Resume sessions must not use `-batch`, and must not execute `detach` after `continue&`.

GDB runtime automation must not execute `load`; Flash programming remains the responsibility of the existing `flash_app.bat` flow.

## Implementation Inputs

The next implementation should add the GDB-only tooling described in the S05A plan:

```text
05_Tools/Config/toolchain.local.example.bat
05_Tools/Debug/README.md
05_Tools/Debug/GDB/
05_Tools/Scripts/start_gdb_server.bat
05_Tools/Scripts/gdb_runtime_snapshot.bat
05_Tools/Scripts/gdb_runtime_snapshot.ps1
```

Required runtime modes:

```text
resume: snapshot → continue& → disconnect → quit
halt:   snapshot → detach → quit
```

The PowerShell wrapper may terminate only the J-Link GDB Server PID that it created. It must not use a global image-name kill. Every configuration, startup, GDB, timeout, and cleanup failure must return a non-zero error code and write a diagnostic log.

## Verification Handoff

Before S05A can close, verify on the real board:

```text
GDB runtime snapshot resume mode       PASS
GDB runtime snapshot halt mode         PASS
Resume mode leaves MCU running        PASS
Halt mode leaves MCU halted           PASS
No implicit Flash programming         PASS
Server PID cleanup                    PASS
J-Link released                       PASS
Non-zero failure paths                PASS
Existing Flash / RTT reuse             PASS
```

After the GDB-only scope is closed, reassess whether the remaining Fault/CmBacktrace work should continue in S05A or be split into a later diagnostics stage before entering S06.

## Next Action

Implement and board-test the GDB runtime snapshot automation. Do not start CmBacktrace integration until its upstream source is available and its vendor integration scope is explicitly confirmed.
