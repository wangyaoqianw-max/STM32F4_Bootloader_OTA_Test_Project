# S10 Trial Confirm Rollback Verification Matrix

## Metadata

- Stage: `S10_Trial_Confirm_Rollback`
- Status: `READY_FOR_VERIFICATION`
- Execution Branch: `main`
- Plan Baseline: `651b3001b4c23cf4162e3367a91ae43307207bce`
- Actual Clean Execution Baseline: `79b95d1f4681c2f7b5f961785079a112a3c62492`
- Remote Check at implementation start: `HEAD == origin/main`, ahead/behind `0/0`
- Date: `2026-09-20`

## Baseline Evidence

| Check | Command / Scope | Result |
|---|---|---|
| Application build | `05_Tools\toolkit.bat build application` | PASS; no errors or warnings |
| Bootloader build | `05_Tools\toolkit.bat build bootloader` | PASS; no errors or warnings |
| Firmware/tool contracts | `05_Tools\Firmware`, `05_Tools\Ymodem\tests` | PASS; 2/2 and 24/24 |
| GDB automation contract | `05_Tools\Debug\GDB\test_gdb_automation.ps1` | PASS |
| Compatibility/core contracts | `05_Tools\Contracts\Compatibility`, `05_Tools\Contracts\Core` | PASS |
| S04/S05/S07A Host tests | format, CRC, storage, YMODEM, receiver, startup | PASS |
| S09 Host contracts | prevalidate, installer, metadata commit | PASS |
| Factory Restore | destructive board operation | BLOCKED; failed batch wrote Slot A Payload but timed out before Header-last commit; F0 not established |
| Ymodem board transport | real target v1.1 transfer | PASS for transfer/READY_TO_INSTALL; rollback transport pending |
| RTT capture | real target capture | PARTIAL; post-power-cycle Application startup and YMODEM evidence captured; pre-Confirm Rollback RTT pending |
| GDB snapshot/fault on target | real target session | PARTIAL; Application idle and IWDG evidence captured; pre-Confirm Rollback breakpoint pending |

The current baseline contains no identified S10 production test hook or fault-injection macro. Temporary S10 hooks, if required later, must be isolated, marked `TEST ONLY`, compile-time disabled by default, and removed before the final clean build.

## Latest Verification Pause

2026-09-20 按用户要求暂停人工 S10 板测。临时 Trial 测试开关已移除，恢复脚本已删除，正式 Application 已重新构建、烧录并通过 RTT 启动冒烟：

```text
build application       PASS; 0 error / 0 warning
flash application run   PASS
rtt application 8       PASS
OTA/Application/Display init and initial render/backlight  PASS
```

这只证明正式固件恢复到可正常启动状态，不替代 Trial 期间断电、Rollback、Rollback 中断恢复或 LED/LCD 肉眼验收。下一次新对话继续这些未完成项目；S10 硬件验证仍为 `PARTIAL / PENDING`。

## Real Target Evidence Collected

- Formal Application `NONE` startup after the Event Flags mapping fix: startup result fields are `PLATFORM_ERR_OK`, system state is `RUNNING`, Health is `STABLE`, and `trial=0`; GDB stopped at the FreeRTOS idle path without the previous controlled-reset loop.
- `app_v1.1.img` was rebuilt as `84680 bytes` (`84616-byte payload`, `83` YMODEM blocks). The real target transfer completed with `84680 bytes`, `retries=2`, sender exit code `0`; RTT reached `READY_TO_INSTALL` with `84680/84680` bytes.
- After the second PA0 action, GDB read `g_appHealthContext.readyMask=0x7`, `state=APP_HEALTH_STATE_STABLE`, `trial=1`, `g_appHealthConfirmResult=PLATFORM_ERR_OK`, and `g_appStartupContext.systemState=APP_SYSTEM_STATE_RUNNING`. This proves the runtime Ready/strict Confirm handshake, but does not by itself prove persisted Metadata after a later power-cycle.
- The latest S09 Factory Restore retry opened Sender before flash but the target still reported `Soft-I2C init FAIL: BUSY` / `BOOT halt: external device init`; Sender then timed out waiting for the initial `C`. It is not counted as a new Factory Restore PASS. The earlier S09 baseline evidence remains S09-owned.
- The 2026-09-20 Factory Restore workflow follow-up is committed as `0ee7f5d`; it now flashes the temporary receiver before starting YMODEM, waits for a fresh RTT readiness marker, bounds the sender timeout below the toolkit process limit, and reaches recovery on failure. The target still did not reach Application because Bootloader startup reported `Soft-I2C init FAIL: BUSY` and halted.
- A clean Bootloader flash followed by GDB snapshot stopped in `diagnostics_fault_entry`; the backtrace reaches `boot_soft_i2c_init()` line 287. The current clean configuration has `DIAG_FAULT_TEST_ENABLE=0`, so the observation is a board startup fault and not a valid S10 fault-injection result.
- After the user power-cycled the board, fresh RTT reached Application initialization with all observed initialization results equal to `0`; GDB then stopped in `prvIdleTask`. The earlier Soft-I2C startup observation is retained as historical failure evidence and is not treated as the current board state.
- A later Factory Restore attempt reproduced the startup boundary: fresh RTT captured `[BOOT][E] Soft-I2C init FAIL: BUSY` and `[BOOT][E] BOOT halt: external device init`; no YMODEM `C` was emitted, so the Sender timeout is a board-startup blocker, not a protocol PASS/FAIL result.

## S10-01 Root Cause and Revised Physical Checkpoints

The root cause was isolated in `06_Output/Logs/S10/20260920-continue-root-cause/`:

- A direct W25Q64 erase/write/read test passed immediately and after reset, so the physical write path and retention are usable.
- The failed Factory Restore `ymodem.log` acknowledged data blocks 0–80 and then stopped during the EOT/end-Header phase without a JSON result.
- Independent readback after recovery found Slot A Header all `0xFF`, Slot A `+0x1000` containing the v1.0 payload vector (`0x2000E690, 0x08010281, ...`), and Slot B Header all `0xFF`.
- `factory_restore.ps1` passes Sender `--timeout 120`, while the outer `Invoke-ToolkitProcess` default is `60000 ms`; the outer timeout kills Sender before `ota_firmware_sink_end()` can commit the Header. This is the direct cause of the invalid pre-burn baseline.

The revised S10 plan adds independent physical checkpoints C0–C5. C1 is required after YMODEM/end-Header commit and before formal Application flash; C2 is required immediately after formal Application flash/reset; C3–C5 protect Slot A across B receive, Bootloader PENDING→TRIAL, and pre-Confirm B runtime. No Trial/Rollback test may start without C1 and C2.

## S10-04 Execution Result (Run `20260921-121213`)

- F0 Known-Good：`PASS`。新的 External Loader 完成 Slot A v1.0 Header/Payload 写入和 SHA256 读回；Slot B Header 擦空；AT24C02 Metadata 基线报告 `baseline PASS`。
- First PA0 / YMODEM：`PASS`。COM9/115200 完成 `84680` bytes、83 blocks、retries 2、exit 0；实时 RTT 达到 `OTA state=4`、`84680/84680`、`error=0`。
- Second PA0 / Trial start：`OBSERVED`。实时 RTT 重新出现 Application 初始化，用户确认 v1.1 LED 运行窗口。
- Trial power-cycle visual result：`PASS for observed behavior`。用户在 v1.1 LED 闪动后断电上电，肉眼观察到 LED 频率恢复 v1.0。
- Final internal image：`PASS`。`06_Output/Logs/S10/20260921-121213/F3/validation.txt` 证明从 `0x08010000` 读回 `81348` bytes 与 v1.0 payload 逐字节相同，`FIRST_MISMATCH=-1`；上电后 Application RTT 初始化结果均为 `0`。
- Bootloader chain：`MISSING`。Application RTT 控制块为 `0x2000DE04`，Bootloader 为 `0x200000E0`；固定地址 Logger 未能跨掉电自动切换，所以没有 `TRIAL → ROLLBACK → restore → NONE` 的连续 Bootloader 证据。

本轮整体分类：`PARTIAL`。硬件最终行为支持已回滚到 v1.0，但不能仅凭最终镜像和 LED 现象替代 Bootloader 中间状态证据。下一轮需先实现双 RTT 地址的自动监听/归档，再重复 S10-04。

## Task 3 Evidence

- Strict Lifecycle API: `firmware_lifecycle_confirm(firmware_storage_t *)`; it reloads latest Metadata, validates the Trial/pending invariants, performs full pending-image validation, reuses the existing atomic Metadata commit, and reloads Metadata for field verification.
- Runtime boundary: `app_ota_runtime_confirm_trial()` is the only Application runtime entry; `appMainTask` and `app_health` do not receive Firmware Storage or Raw Driver pointers.
- Host Test: `04_Test/Host/S10_Trial_Confirm_Rollback/s10_lifecycle_host_test.c` — PASS; strict state/slot gates, Header/size/Payload CRC/Version gates, commit body/marker failure propagation, reload mismatch, normal `TRIAL → NONE`, and confirmed Slot/Version update.
- Application project XML parse — PASS.
- Application Build: `05_Tools\toolkit.bat build application` — PASS; no errors or warnings.
- Real target Confirm transaction, RTT and GDB evidence — PASS for the runtime handshake; direct no-feed IWDG reset is PASS, while Trial reset/power-cycle evidence remains pending.

## Task 4 Evidence

- Runtime integration contract: `05_Tools\Contracts\Application\test_s10_runtime_health_integration.ps1` — PASS; Health bootstrap, three Runtime Ready reports, single Confirm request gate, otaWorker execution, Confirm result handoff, and narrow lifecycle status API are present. The contract also rejects Raw Storage references in Health, appMainTask and appSystem.
- Health Host Test: `04_Test/Host/S10_Trial_Confirm_Rollback/s10_health_host_test.c` — PASS; Ready deadline, Observation Window, Confirm success/failure states, feed permission and stable `NONE` behavior.
- Application Build: `05_Tools\toolkit.bat build application` — PASS; `OTA_APP_build.log` reports `ExitCode=0`, no errors or warnings.
- Runtime task changes: appMainTask is the only long-term Watchdog Feed owner; otaWorker remains the Confirm Storage transaction owner; display/OTA Ready reports occur after the startup decision.
- Real Runtime Ready/Confirm RTT and GDB evidence — PASS for `readyMask=0x7`, stable Trial health and `PLATFORM_ERR_OK` Confirm result; Trial reset/power-cycle and Rollback evidence remain pending.

## Task 5 Evidence

- TDD red baseline: the new Metadata Host Test initially accepted `pendingSlot == confirmedSlot` before the production validation change.
- Metadata invariant Host Test: `04_Test/Host/S10_Trial_Confirm_Rollback/s10_metadata_invariant_host_test.c` — PASS; Application and Bootloader encode/decode both reject same-slot pending, missing/invalid confirmed baseline, and invalid pending Slot for PENDING/TRIAL/ROLLBACK. Factory `confirmedSlot=NONE + pendingSlot=NONE + upgradeState=NONE` and stable known-good `NONE` records remain accepted.
- Application/Bootloader raw compatibility: valid records encoded by both implementations are byte-identical; S09 fixed Metadata V2 vector remains accepted.
- Regression Host Tests — PASS: S04 format, S07 Metadata, S09 contract, S09 prevalidation, and S09 atomic Metadata commit.
- S09 prevalidation Host Fixture was corrected from the now-invalid `confirmedSlot=A / pendingSlot=A` combination to the normal `confirmedSlot=A / pendingSlot=B` combination; no production prevalidation behavior changed.
- Metadata V2 raw layout, sequence comparison, CRC calculation and commit-marker ordering were not changed.
- Application Build: `05_Tools\toolkit.bat build application` — PASS; no errors or warnings.
- Bootloader Build: `05_Tools\toolkit.bat build bootloader` — PASS; no errors or warnings. Map reports `Total ROM Size = 21140 bytes (20.64 KiB)`, below 64 KiB.

## Task 6 Evidence

- TDD red baseline: the new Bootloader recovery Host Test initially failed to compile because the Confirmed prevalidate and the two narrow Installer APIs did not yet exist.
- Prevalidate split: `boot_prevalidate_candidate()` remains the PENDING-only entry; `boot_prevalidate_confirmed()` accepts only TRIAL/ROLLBACK, selects `confirmedSlot`, checks `confirmedVersion`, and both use the private common Header/size/vector/Payload CRC validation.
- Installer split: `boot_installer_install_pending()` and `boot_installer_restore_confirmed()` select their sources through the narrow prevalidate entries and share one private erase/copy/read-back/Internal CRC/vector install core. No arbitrary Slot install API was added.
- S10 Bootloader recovery Host Test: `04_Test/Host/S10_Trial_Confirm_Rollback/s10_boot_recovery_host_test.c` — PASS; Pending B, Confirmed A, wrong state, same Slot, version mismatch, invalid Payload CRC/vector, common source selection, and prevalidate-before-erase gate.
- S09 regression Host Tests — PASS: prevalidate, installer, fixed Header/Metadata contract, and atomic Metadata commit.
- Bootloader Clean Build: `05_Tools\toolkit.bat build bootloader` — PASS; no errors or warnings. Map reports `Total ROM Size = 21288 bytes (20.79 KiB)`, below 64 KiB.
- Real Bootloader rollback/restore, RTT, GDB and board evidence — rollback restore remains pending; formal NONE and Trial/Confirm target evidence is recorded below.

## Task 7 Evidence

- TDD red baseline: the rollback transaction Host Test initially failed to compile because the two rollback Metadata APIs did not yet exist; after implementation it passed.
- Rollback transaction Host Test: `04_Test/Host/S10_Trial_Confirm_Rollback/s10_rollback_transaction_host_test.c` — PASS; verifies `TRIAL → ROLLBACK`, `ROLLBACK → NONE`, preserved confirmed Slot/Version and Slot states, pending clearing only at completion, wrong-state rejection, body/marker atomic boundaries and restart-readable power-loss outcomes.
- Boot decision contract: `05_Tools\Contracts\Bootloader\test_s10_boot_decision_contract.ps1` — PASS; verifies PENDING/TRIAL/ROLLBACK ordering, confirmed prevalidation before destructive restore, persisted ROLLBACK before erase, restart-from-zero ROLLBACK path, and diagnostic-only Reset Cause.
- S09 Metadata commit Host Test — PASS; existing `PENDING → TRIAL` atomic marker ordering and compatibility remain green after private core extraction.
- Bootloader Clean Build: `05_Tools\toolkit.bat build bootloader` — PASS; no errors or warnings. Map reports `Total ROM Size = 22372 bytes (21.85 KiB)`, below 64 KiB.
- Reset Cause snapshot/log includes BOR, POR, PIN, Software and IWDG flags; direct no-feed IWDG reset is PASS, while target rollback restore and Trial power-cycle evidence remain pending; Debug Freeze/resume and recovery power-cycle evidence are recorded separately.

## Task 8 Evidence

- Host regression matrix — PASS: S02 SPI chunking; S04 CRC, firmware format and storage; S05 storage write, YMODEM parser/receiver and flash sink; S07A startup; S07 IRQ, Key, Worker, Display, Metadata, sink and OTA service; S09 contract, prevalidate, installer and Metadata commit; S10 health, lifecycle, Metadata invariant, boot recovery and rollback transaction.
- Python tests — PASS: `05_Tools/Firmware/test_pack_firmware.py` 2/2; `05_Tools/Ymodem/tests` 24/24; `04_Test/Host/S04_Firmware_Image_Storage/test_s04_persistence_log.py` 15/15.
- Static contracts — PASS: all 12 scripts under `05_Tools/Contracts`; GDB automation, CmBacktrace integration/fault diagnostics, tool sequence and S05C parser fixture checks also PASS.
- Host fixture maintenance — commit `54c9827`; S04 test stubs now expose the current AT24C02/W25Q64 initializer contracts and the Metadata recovery fixture uses a distinct valid confirmed/pending Slot pair. The S07 UART test double uses independent test-owned TX storage instead of removed production struct fields. No production API or behavior was changed by this fixture commit.
- Application Clean Build — `05_Tools\toolkit.bat build application` PASS; 0 errors, 0 warnings. Current map reports `Total ROM Size = 84616 bytes (82.63 KiB)`; observed heap-4 `.bss` 24576 bytes and startup stack 1024 bytes.
- Bootloader Clean Build — `05_Tools\toolkit.bat build bootloader` PASS; 0 errors, 0 warnings. Map reports `Total ROM Size = 22372 bytes (21.85 KiB)`, below the 64 KiB limit.
- Real-board automation — PARTIAL/PENDING: formal NONE startup, repeated v1.1 YMODEM transfer, Runtime Ready/strict Confirm state, IWDG Debug Freeze/resume and direct no-feed IWDG reset evidence were captured; Trial power-cycle before Confirm and Rollback evidence remain pending.
- Automated follow-up on 2026-09-20 — PASS: S10 Host Tests, Python regressions, static contracts, Application build and Bootloader build were rerun; hardware claims were not upgraded from the existing partial evidence.
- S05C result-file checks — fixture-level SPI/I²C project assertions PASS after parser normalization; no real target capture result file is available, so board capture remains `PENDING / NOT_EXECUTED`.
- Temporary production test code — none added; all Task 8 checks use existing Host/Contract test assets. Generated build outputs and Python caches are not part of the commit; local untracked cache cleanup remains pending safe user-approved cleanup.

## Task 9 Evidence

- Consolidated manual board checklist prepared for the Verification Role: Factory baseline, PA0/OTA transfer, Trial runtime/Confirm, Trial software reset, real power-cycle during Trial, rollback interruption recovery, LED/LCD observation and Reset Cause RTT evidence.
- Hardware execution status — `PARTIAL / PENDING`: formal NONE GDB evidence, repeated v1.1 transfer through Runtime Ready/strict Confirm, recovery power-cycle, IWDG Debug Freeze and direct no-feed IWDG reset evidence are recorded; Trial power-cycle before Confirm, Rollback and visual board evidence remain pending. Historical S04/S07/S09 logs are not reused as S10 evidence.
- S05C real I2C/SPI result-file checks remain `PENDING / NOT_EXECUTED`; parser and fixture-level project assertions are complete.

## Task 10 Exit Evidence

- Production warning cleanup — commit `a5295c2`; explicit enum initialization and signed range comparisons remove compiler diagnostics without changing public APIs or lifecycle semantics.
- Final Application Clean Build — `05_Tools\toolkit.bat build application` PASS; raw UV4 clean-build log reports `0 Error(s), 0 Warning(s)`. Current map Total ROM is `84616 bytes (82.63 KiB)`; configured FreeRTOS heap remains `24576 bytes`, startup stack remains `1024 bytes`.
- Final Bootloader Clean Build — `05_Tools\toolkit.bat build bootloader` PASS; `0 Error(s), 0 Warning(s)`. Total ROM remains `22372 bytes (21.85 KiB)`, below 64 KiB.
- Final static, Python and Host evidence is recorded in Task 8; the nonblocking Event Flags mapping fix is committed as `5ca2d14`, and the final formal build was rerun afterward.
- Verification follow-up commit `0ee7f5d` fixes Factory Restore sequencing/recovery; its real-board retry exposed the historical Bootloader Soft-I2C startup fault described above.
- Verification follow-up captured IWDG `PR/RLR/DBGMCU` values, a 12-second halted target with successful resume, and a direct no-feed GDB reset with `IWDG_RESET_HANDLER_HIT` / `RCC_CSR=0x24000000`. The observed reset-to-`trial=0` sequence is not counted as Rollback because the pre-Confirm boundary and Bootloader decision log were not captured.
- Code verification — `PASS`; hardware verification — `PENDING`.
- Temporary production test code — `NONE`; no test-only production hook or forced-failure behavior remains in the final build.
- S09 Deferred Fault Injection — not executed in S10; remains S09-owned and is not counted in this stage.

## Acceptance Matrix

The category is the primary evidence path. A later board result never replaces a required host contract or static/build check.

| ID | Category | Evidence target |
|---:|---|---|
| 1 | HOST_AUTO | Lifecycle states remain `NONE/PENDING/TRIAL/ROLLBACK`; no `CONFIRMED` |
| 2 | HOST_AUTO | Stable firmware is represented by `NONE + confirmedSlot + confirmedVersion` |
| 3 | HOST_AUTO | Unconfirmed Trial boot enters rollback without a failure counter |
| 4 | HOST_AUTO | IWDG integration does not use CubeMX regeneration |
| 5 | HOST_AUTO | Watchdog HAL is isolated behind Platform/Impl |
| 6 | BOARD_AUTO | IWDG starts after `HAL_Init()`, before clock and scheduler |
| 7 | BOARD_AUTO | Approximately 10 s timeout; HAL module and Keil source are present |
| 8 | BOARD_AUTO | Pre-RTOS/startup feed uses bounded checkpoints, not a feed task |
| 9 | BOARD_AUTO | `appMainTask` is the runtime feed owner |
| 10 | HOST_AUTO | Feed occurs only after a valid work period and permitted health state |
| 11 | BOARD_AUTO | Debug halt freezes IWDG |
| 12 | BOARD_AUTO | Continue resumes IWDG |
| 13 | HOST_AUTO | Trial Confirm requires `APP_SYSTEM_STATE_RUNNING` |
| 14 | HOST_AUTO | Trial DEGRADED cannot Confirm |
| 15 | HOST_AUTO | MAIN/OTA/DISPLAY ready events occur after SYSTEM_RUN |
| 16 | HOST_AUTO | Runtime Ready evidence matches main cycle, OTA command wait, and display queue wait |
| 17 | HOST_AUTO | No duplicate component enum is introduced |
| 18 | HOST_AUTO | Runtime Ready deadline is 5 s and timeout prevents permanent Trial |
| 19 | HOST_AUTO | Observation Window is 5 s |
| 20 | HOST_AUTO | Observation is not the only health condition |
| 21 | HOST_AUTO | Feed policies for WAIT/OBSERVING/CONFIRMING/STABLE are explicit |
| 22 | HOST_AUTO | `firmware_confirm()` does not reuse `service_ota_confirm_install()` |
| 23 | HOST_AUTO | `firmware_confirm()` belongs to Firmware Lifecycle |
| 24 | HOST_AUTO | otaWorker owns Firmware Storage I/O; health/main do not access storage directly |
| 25 | HOST_AUTO | Confirm reloads latest Metadata |
| 26 | HOST_AUTO | Confirm validates Trial, A/B identity, distinct pending slot, and VALID pending slot |
| 27 | HOST_AUTO | Confirm validates header, size, payload CRC, and version |
| 28 | HOST_AUTO | Confirm atomically updates confirmed/pending fields and upgrade state |
| 29 | HOST_AUTO | Commit marker is last and metadata is reloaded for verification |
| 30 | HOST_AUTO | Confirm power-loss outcomes are complete Trial or complete NONE |
| 31 | HOST_AUTO | Destructive gate preserves a distinct Known-Good confirmed slot |
| 32 | BOARD_AUTO | Bootloader prevalidates confirmed image before rollback erase |
| 33 | HOST_AUTO | Confirmed image version matches `confirmedVersion` |
| 34 | BOARD_AUTO | Invalid confirmed image prevents Internal APP erase |
| 35 | BOARD_AUTO | `TRIAL -> ROLLBACK` is persisted before Internal erase |
| 36 | HOST_AUTO | Rollback source is the read-only External confirmed slot |
| 37 | HOST_AUTO | PENDING install and ROLLBACK restore share validation/install core |
| 38 | HOST_AUTO | No second Internal Flash installer exists |
| 39 | BOARD_AUTO | Rollback restart begins from zero after interruption |
| 40 | BOARD_AUTO | `ROLLBACK -> NONE` waits for Internal CRC/vector PASS |
| 41 | HOST_AUTO | Rollback ends with `pendingSlot = NONE` |
| 42 | HOST_AUTO | Rollback preserves confirmed slot/version Known-Good baseline |
| 43 | HOST_AUTO | Runtime Trial failure does not change static Slot VALID state |
| 44 | HOST_AUTO | Reset cause is diagnostic only and does not decide rollback |
| 45 | BOARD_AUTO | Stable NONE firmware keeps the watchdog running |
| 46 | HOST_AUTO | S05A/S07A/S09 contracts have no unplanned regression |
| 47 | HOST_AUTO | Application and Bootloader clean builds pass |
| 48 | HOST_AUTO | Bootloader remains below 64 KiB |
| 49 | BOARD_MANUAL | Real-board Trial Confirm and IWDG/Rollback chain |
| 50 | BOARD_AUTO | Rollback reset/power-loss fault injection produces readable evidence; real power cut may require manual fallback |
| 51 | BOARD_AUTO | Any S09 Deferred Fault Injection result is written back to S09 evidence only |

## S09 Deferred Boundary

The following remain S09-owned until actually executed: erase-after checkpoints, approximately 25%/50%/complete programming checkpoints, Internal CRC before/after, metadata body/commit-marker interruption, and power-loss/retry. If executed during the S10 board window, the evidence must be appended to `04_Test/Reports/Stages/S09_Firmware_Installation/verification.md` and must not be counted as new S10 functionality.
