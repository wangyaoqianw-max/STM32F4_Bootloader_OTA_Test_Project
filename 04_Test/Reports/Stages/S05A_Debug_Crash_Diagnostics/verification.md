# S05A GDB Debug & Crash Diagnostics Verification

## 1. Verification Metadata

- Stage: `S05A_Debug_Crash_Diagnostics`
- Date: `2026-09-15`
- Branch: `main`
- Scope: GDB automation, CmBacktrace integration, and staged board verification
- Status: `READY_FOR_VERIFICATION`
- CmBacktrace: integrated; controlled Fault board verification is pending
- CmBacktrace Integration Commit: `1c27c8e`

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
| CmBacktrace integration contract test | PASS | `05_Tools/Debug/CmBacktrace/test_cm_backtrace_integration.ps1` |
| Keil full rebuild | PASS | `OTA_APP_rebuild.log`, 0 errors, 14 existing warnings |

The full Keil rebuild used ARMCC V5.06 update 7 (build 960). The warnings are
from existing platform and EasyLogger files; the new CmBacktrace and project
adapter files produced no compiler warnings. The resulting AXF was:

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

## 6. CmBacktrace Integration Verification

### 6.1 Code integration

The Keil project now compiles and links:

```text
cm_backtrace.c
CmBacktrace/fault_handler/keil/cmb_fault.S
cmbacktrace_port.c
cmbacktrace_fault_handlers.S
```

The map file resolves `cm_backtrace_fault`, `cmbacktrace_port_init`,
`vTaskStackAddr`, `vTaskStackSize`, `vTaskName`, and the `STACK` block symbols.
The FreeRTOS compatibility patch is guarded by `CMB_USER_CFG` and derives the
task stack size from `configRECORD_STACK_HIGH_ADDRESS`.

### 6.2 Normal board run

```text
Flash application                 PASS
RTT capture for 5 seconds         PASS
Normal EasyLogger/Application log PASS
GDB runtime snapshot resume       PASS
```

The normal RTT log contains successful log initialization, Application startup,
Storage SPI construction/init/start, and `Application init result: 0`. No reset,
fault loop, or runtime regression was observed.

### 6.3 Fault board test

```text
Controlled Fault injection        PENDING
CmBacktrace Fault RTT output      PENDING
GDB/CmBacktrace cross-validation  PENDING
```

This integration change does not add a destructive Fault trigger. The pending
items require a separately reviewed controlled Fault injection entry and must
not be inferred from a successful build or normal boot.

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
Fault injection and Cortex-M fault context capture
RTT fault output workflow
GDB Fault Capture automation
GDB/CmBacktrace cross-validation
S04 Reset Persistence / Power-cycle Persistence regression
```

Next action: implement and verify the controlled Fault trigger and Fault Capture
workflow, then complete S05A review before entering S06 RTOS Runtime design.
