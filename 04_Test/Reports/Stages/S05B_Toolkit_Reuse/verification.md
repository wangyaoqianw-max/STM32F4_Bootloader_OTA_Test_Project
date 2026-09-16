# S05B Toolkit Reuse Verification Evidence

## 1. Verification Metadata

- Stage: `S05B_Toolkit_Reuse`
- Date: `2026-09-16`
- Branch: `main`
- Status: `READY_FOR_REVIEW`
- Current role: `Verification Role`
- Scope: reusable Toolkit Core/Adapters/Workflows, unified Router, Legacy compatibility, S04 project-test isolation, Firmware/YMODEM routing, and cross-project reuse
- Code validation: `PASS`
- Hardware validation: `PASS` for current-project public-entry smoke, real YMODEM/Tera Term transfer, S04 Reset/Power-cycle Persistence, Fault trigger/capture, and second-project Flash/RTT
- Review: `Not created yet`

## 2. Implementation Commits

```text
499df29 refactor(tools): establish reusable configuration core
00cfbc7 refactor(tools): extract process and lock primitives
ab4da98 refactor(tools): add application workflows and adapters
ac1cc4b refactor(tools): separate gdb adapter from debug workflows
e34e005 refactor(tools): add unified toolkit router
0719f83 refactor(tools): isolate s04 persistence extension
92cf50a docs(tools): expose reusable firmware and transport entries
98efc77 test(s05b): complete remaining board verification
```

## 3. Code and Host Verification

All 13 full-regression groups returned `EXIT=0`:

| Area | Result | Evidence |
|---|---|---|
| Core contract | PASS | `05_Tools/Contracts/Core/test_core.ps1` |
| Application workflow contract | PASS | `05_Tools/Contracts/Application/test_application_workflows.ps1` |
| Debug workflow contract | PASS | `05_Tools/Contracts/Debug/test_debug_workflows.ps1` |
| Legacy compatibility | PASS | `05_Tools/Contracts/Compatibility/test_legacy_entries.ps1` |
| Firmware/transport compatibility | PASS | `05_Tools/Contracts/Compatibility/test_firmware_transport_entries.ps1` |
| S04 isolation | PASS | `05_Tools/Contracts/ProjectTests/test_s04_isolation.ps1` |
| GDB automation | PASS | `05_Tools/Debug/GDB/test_gdb_automation.ps1` |
| Tool sequence | PASS | `05_Tools/Debug/test_tool_sequence.ps1` |
| CmBacktrace integration | PASS | `05_Tools/Debug/CmBacktrace/test_cm_backtrace_integration.ps1` |
| Fault diagnostics | PASS | `05_Tools/Debug/CmBacktrace/test_fault_diagnostics.ps1` |
| Firmware unit tests | PASS | `2 tests OK` |
| YMODEM unit tests | PASS | `24 tests OK` |
| S04 Host unit tests | PASS | `15 tests OK` |

Additional checks:

- `git diff --check`: `PASS`;
- Core / Adapters / Workflows hard-code scan for `OTA_APP`, `COM9`, `S04_PERSISTENCE`, `wangyaoqian`, and `Program Files`: `0 matches`;
- public Router and Legacy BAT smoke, including Firmware pack and `ymodem python --help`: `PASS`;
- temporary reuse directories and generated test caches: cleaned; no generated artifacts are part of the handoff.
- selected 10-item Host/Contract regression rerun after the Fault workflow fix: all `PASS`;
- normal Application final build completed with warnings (`ExitCode=1`, `TimedOut=False`), with no compiler error; final Flash and RTT startup check passed.

## 4. Current Project Board Smoke

The connected and powered board was tested through the public entry:

```text
cmd /d /c ".\05_Tools\toolkit.bat build"             PASS
cmd /d /c ".\05_Tools\toolkit.bat flash run"         PASS
cmd /d /c ".\05_Tools\toolkit.bat rtt 10"            PASS
cmd /d /c ".\05_Tools\toolkit.bat snapshot resume"   PASS
```

Evidence:

- `06_Output/Logs/OTA_APP_build.log`: wrapper `ExitCode=1`, `TimedOut=False`; the log reports warnings but no compiler errors;
- `06_Output/Logs/OTA_APP_flash.log`: J-Link load-file and successful flash evidence;
- `06_Output/Logs/OTA_APP_rtt.log`: 415 bytes, containing EasyLogger/service initialization, Application startup, Storage SPI initialization and `Application init result: 0`;
- GDB snapshot log: PC/SP/backtrace and `resume-and-disconnect`;
- independent GDB adapter check read `uwTick = 0x23adc`, then continued and disconnected successfully;
- after the smoke, no `JLinkGDBServerCL`, `JLinkGDBServer`, `arm-none-eabi-gdb`, `JLinkRTTLogger` or `JLink` process remained.

This is a public-entry/toolchain smoke result. Stage-specific acceptance evidence is recorded below; Review Role sign-off is still required.

### 4.1 Real YMODEM / Tera Term Transfer

The S05 board-test entry was temporarily enabled in the Keil project by adding the existing files under `04_Test/Board/S05_UART_Ymodem` and defining `PROJECT_ENABLE_S05_YMODEM_BOARD_TEST=1`. After the test, the project definition, include path, and source references were restored; the normal Application was rebuilt and reflashed successfully.

The detected CH340 port was `COM9`. Tera Term `ttermpro`/`ttpmacro` were opened first and entered the YMODEM wait state before `flash run` reset the board. The real Tera Term macro exited with code `0`. The sent image was `OTA_APP_s04_v1.1.0.img` with 55884 bytes.

The board RTT log showed `YMODEM_READY` and progress through `16384/55884`, `32768/55884`, `49152/55884`, and `55884/55884`. Because the RTT up-channel buffer was already occupied by startup/progress output, the final application lines were independently checked through GDB while the test firmware was still running:

```text
receiver_state       = 6 (FINISHED)
receiver_error       = 0
received_size        = 55884
expected_size        = 55884
payload_written      = 55820
header_committed     = 1
file_started         = 0
packets_received     = 57
packets_accepted     = 57
retries              = 0
```

Conclusion: real YMODEM/Tera Term transfer, Slot B payload/header commit, and receiver completion passed. The board was then returned to the normal Application, whose RTT startup log showed `Application init result: 0`.

### 4.2 Fault Trigger / Capture

The Fault test was run with a temporary Keil definition `DIAG_FAULT_TEST_ENABLE=1`; the normal project definition was restored after the test. The required sequence was used: `flash prepare` first, then the GDB trigger entry so the test firmware was not allowed to Fault before the breakpoint was installed.

```text
cmd /d /c ".\05_Tools\toolkit.bat flash prepare"  PASS
cmd /d /c ".\05_Tools\toolkit.bat fault trigger"    PASS
```

GDB evidence in `06_Output/Logs/OTA_APP_fault_gdb.log` contains `GDB FAULT CAPTURE`, `[FAULT_CONTEXT]`, `[SCB_FAULT_REGISTERS]`, `[BACKTRACE]`, `[STACK]` and `halt-and-detach`. The captured values include:

```text
FAULT PC = 0x08004FEE
CFSR     = 0x00000400
R0       = 0xFFFFFFF0
R1       = 0xDEADBEEF
```

RTT evidence in `06_Output/Logs/OTA_APP_fault_rtt.log` contains `[DIAG_FAULT_CONTEXT]`, the same Fault PC, and the CmBacktrace thread/stack output. The GDB session left the MCU halted. During this verification the trigger script was corrected to halt after reset and use blocking `continue`; the capture workflow was corrected to select the existing `fault_capture.gdb` file in capture mode.

Conclusion: controlled Fault trigger, GDB context capture, retained RTT Fault output and Probe release ordering passed.

### 4.3 S04 Reset / Power-cycle Persistence

The S04 read-only board test was temporarily integrated into the Keil project and built as a separate AXF. The temporary `app_main.c`, project definition, IncludePath and source reference were restored and the AXF was removed after testing.

Reset mode passed with unchanged persistent fields. The baseline and post-reset snapshots both reported:

```text
image_validation=2
image_size=55820
image_crc=0x3ED1C72E
metadata_a_valid=1 metadata_b_valid=0
selected_copy=1 sequence=0
active_slot=255 confirmed_slot=255
slot_a_state=0 slot_b_state=1
confirmed_version=1.1.0
writes=0
```

Power-cycle mode started the RTT listener before the manual power action. After the operator power-cycled the board, the log contained `[S04-PERSIST] POWER_CYCLE_BOOT_CONFIRMED` and a post-boot snapshot with the same fields. The parser returned:

```text
[S04-PERSIST][PASS] all persistent fields are unchanged
[S04-PERSIST][PASS] Persistence parser passed.
```

Evidence: `06_Output/Logs/S04_reset_persistence.log` and `06_Output/Logs/S04_power_cycle_persistence.log`.

Conclusion: Reset and physical Power-cycle did not alter the read-only firmware image or Metadata persistence state.

### 4.4 Second Project Board Flash / RTT

The local source repository was used without modification:
`E:\my_project_2026\Git_test\stm32f4_DMA_UART_ring_RTOS`.

An isolated temporary copy changed only `05_Tools/Config/project.defaults.bat` to point at:

```text
RTT_elog_DMA_UART_ring_project\MDK-ARM\RTT_elog_DMA_UART_ring_project.uvprojx
Target: RTT_elog_DMA_UART_ring_project
Device: STM32F411CE
Output: Objects\RTT_elog_DMA_UART_ring_project.axf
```

The public Toolkit entry results were:

```text
toolkit.bat build      PASS (without errors or warnings)
toolkit.bat flash run  PASS
toolkit.bat rtt 15     PASS
```

RTT evidence in `06_Output/Logs/S05B_second_project_rtt.log` contains EasyLogger initialization, `service_log` initialization, system composition initialization and communication runtime startup. Flash evidence is in `06_Output/Logs/S05B_second_project_flash.log`. The second project has no CMake migration and no CmBacktrace; those capabilities are not part of this reuse target.

Conclusion: the same generic Toolkit Core/Adapter/Workflow/Router was reused on the second Keil + J-Link STM32 project by configuration-only substitution, and real board Flash/RTT passed.

## 5. Listener and Probe Ownership Ordering

For serial/Tera Term/YMODEM tests, open the listener first and wait for the receiver's `C` before any Flash/reset action that can emit early startup information. This ordering is required to avoid losing boot logs and transfer negotiation data.

RTT Logger is a J-Link client and cannot share the Probe with J-Link Flash, GDB Server/client, or RTT Viewer. The Fault workflow therefore completes GDB `detach`/`disconnect`/`quit`, releases the Probe, and only then starts RTT capture.

## 6. Second Project Reuse

Source used directly:
`E:\my_project_2026\Git_test\stm32f4_DMA_UART_ring_RTOS`

The nested Keil project was:

```text
RTT_elog_DMA_UART_ring_project\MDK-ARM\RTT_elog_DMA_UART_ring_project.uvprojx
Target: RTT_elog_DMA_UART_ring_project
Device: STM32F411CEUx
Output: Objects\RTT_elog_DMA_UART_ring_project.axf
```

A temporary copy was configured by changing only `project.defaults.bat` and reusing the ignored local toolchain configuration. Its `toolkit.bat build` returned `PASS`. Aggregate SHA-256 comparison for `Core`, `Adapters`, `Workflows`, and `toolkit.ps1` was equal between the current project and the temporary copy, proving no project-specific edits were needed in generic Toolkit code.

The source repository was not modified by this task. Its pre-existing deleted files and untracked `codex_build.log` were observed and preserved. The reuse copy was removed after the test. The second project has no CMake migration and no CmBacktrace; CmBacktrace verification is therefore not applicable to that reuse target.

## 7. Verification Boundary

All planned S05B verification items are complete. No production firmware, Flash/Memory Layout, RTOS interface, Firmware Image contract, MCU-side YMODEM code, or second-project source code was changed in S05B. Temporary board-test integration files and AXF were restored/removed; the only persistent changes in this verification pass are the Fault workflow fix, its contracts, and synchronized documentation.

Stage status is `READY_FOR_REVIEW`. Review Role must still inspect the final diff and evidence before any `CLOSED` transition.
