# S05B Toolkit Reuse Verification Evidence

## 1. Verification Metadata

- Stage: `S05B_Toolkit_Reuse`
- Date: `2026-09-16`
- Branch: `main`
- Status: `READY_FOR_VERIFICATION`
- Current role: `S05B Implementation Role → Verification Role`
- Scope: reusable Toolkit Core/Adapters/Workflows, unified Router, Legacy compatibility, S04 project-test isolation, Firmware/YMODEM routing, and cross-project reuse
- Code validation: `PASS`
- Hardware validation: `PASS` for current-project public-entry smoke; stage-specific items below remain `PENDING`
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

## 4. Current Project Board Smoke

The connected and powered board was tested through the public entry:

```text
cmd /d /c ".\05_Tools\toolkit.bat build"             PASS
cmd /d /c ".\05_Tools\toolkit.bat flash run"         PASS
cmd /d /c ".\05_Tools\toolkit.bat rtt 10"            PASS
cmd /d /c ".\05_Tools\toolkit.bat snapshot resume"   PASS
```

Evidence:

- `06_Output/Logs/OTA_APP_build.log`: `ExitCode=0`, `TimedOut=False`;
- `06_Output/Logs/OTA_APP_flash.log`: J-Link load-file and successful flash evidence;
- `06_Output/Logs/OTA_APP_rtt.log`: 415 bytes, containing EasyLogger/service initialization, Application startup, Storage SPI initialization and `Application init result: 0`;
- GDB snapshot log: PC/SP/backtrace and `resume-and-disconnect`;
- independent GDB adapter check read `uwTick = 0x23adc`, then continued and disconnected successfully;
- after the smoke, no `JLinkGDBServerCL`, `JLinkGDBServer`, `arm-none-eabi-gdb`, `JLinkRTTLogger` or `JLink` process remained.

This is a public-entry/toolchain smoke result. It does not by itself close the stage-specific YMODEM, Fault, S04, or second-project board acceptance items.

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

## 7. Pending Items and Verification Boundary

- `PENDING`: real YMODEM/Tera Term serial transfer, including receiver `C` negotiation and firmware validation;
- `PENDING`: S04 Reset / Power-cycle Persistence board test with the dedicated S04 image;
- `PENDING`: Fault `trigger/capture` board test with controlled fault injection and RTT/GDB evidence;
- `PENDING`: second-project board Flash/RTT smoke.

No production firmware, Flash/Memory Layout, RTOS interface, Firmware Image contract, MCU-side YMODEM code, or second-project source code was changed in S05B. Stage status remains `READY_FOR_VERIFICATION`; independent Verification Role review is required before `READY_FOR_REVIEW` or `CLOSED`.
