# S05A GDB Debug & Crash Diagnostics Verification

## 1. Verification Metadata

- Stage: `S05A_Debug_Crash_Diagnostics`
- Date: `2026-09-15`
- Branch: `main`
- Scope: GDB automation and real-board verification
- Status: `READY_FOR_REVIEW`
- CmBacktrace: not included; source has not been downloaded

## 2. Verification Environment

```text
Target        STM32F411CE
Interface     SWD
Speed         4000 kHz
GDB port      2331
Compiler      Keil / ARMCC, existing project flow
Symbols       OTA_APP.axf
GDB           arm-none-eabi-gdb from RT-Thread Studio
Server        SEGGER J-Link GDB Server V7.92
Probe         J-Link V9.70
```

The local executable paths are stored only in the ignored
`05_Tools/Config/toolchain.local.bat` and are not committed.

## 3. Code Verification

| Check | Result | Evidence |
|---|---|---|
| GDB automation contract test | PASS | `05_Tools/Debug/GDB/test_gdb_automation.ps1` |
| PowerShell syntax parse | PASS | Windows PowerShell parser check |
| Invalid mode | PASS | `gdb_runtime_snapshot.bat invalid` returned non-zero, mode rejected |
| Missing GDB / AXF | PASS | Direct wrapper failure-path checks returned non-zero |
| Invalid port | PASS | Port `0` rejected with non-zero result |
| Non-listening server | PASS | Startup failure/timeout path returned non-zero |
| GDB `load` protection | PASS | Both command scripts and wrapper reject `load` |
| `git diff --check` | PASS | No whitespace errors |

No production firmware source was changed, so a new Keil firmware build was
not required for this tooling-only checkpoint. The tested AXF was the existing
Keil output at:

```text
03_Firmware/Application/OTA_APP/MDK-ARM/Objects/OTA_APP.axf
```

## 4. Manual GDB Control Baseline

The existing real-board baseline passed:

```text
Breakpoint       PASS
Continue         PASS
Next             PASS
Step             PASS
Backtrace        PASS
Memory read      PASS
Variable read    PASS
```

## 5. Automated Runtime Snapshot Board Test

### 5.1 Halt mode

Command:

```bat
05_Tools\Scripts\gdb_runtime_snapshot.bat halt
```

Result: `PASS`. The snapshot log contained registers, PC/SP/LR/XPSR, source
mapping, backtrace, stack memory and the `halt-and-detach` exit marker.

After the wrapper exited, two independent GDB connections were made with an
approximately two-second interval. `uwTick` was read as:

```text
0x38A62C -> 0x38A62C
```

Conclusion: `detach` from the halted target leaves the MCU halted.

### 5.2 Resume mode

Command:

```bat
05_Tools\Scripts\gdb_runtime_snapshot.bat resume
```

Result: `PASS`. The snapshot log contained the `resume-and-disconnect` exit
marker. The session used `continue& -> disconnect -> quit`.

With a follow-up GDB server session, `uwTick` was read before and after an
approximately two-second running interval as:

```text
0x396313 -> 0x399082
```

Conclusion: the MCU continues running after the resume exit sequence.

### 5.3 Resource cleanup

The wrapper owns and terminates only the J-Link GDB Server process it started.
After the board tests, no `JLinkGDBServerCL`, `arm-none-eabi-gdb`, `UV4`,
`JLinkRTTViewer` or `JLinkRTTLogger` process remained. The J-Link probe was
released for subsequent sessions.

No GDB `load` command was issued and no Flash programming was performed by
the runtime snapshot flow.

## 6. Logs

The latest local runtime logs are generated at:

```text
06_Output/Logs/OTA_APP_gdb_server.log
06_Output/Logs/OTA_APP_gdb_snapshot.log
```

They are ignored local artifacts and are not committed to Git.

## 7. Deferred Work

The following remain outside this checkpoint:

```text
CmBacktrace source acquisition and port
Fault injection and Cortex-M fault context capture
RTT fault output workflow
S04 Reset Persistence / Power-cycle Persistence regression
```

Next action: complete S05A review. Only after review should the project enter
S06 RTOS Runtime design; CmBacktrace should be handled as a separately scoped
follow-up once its source is available.
