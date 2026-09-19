# S10 Trial Confirm Rollback Verification Matrix

## Metadata

- Stage: `S10_Trial_Confirm_Rollback`
- Status: `IN_PROGRESS`
- Execution Branch: `main`
- Plan Baseline: `651b3001b4c23cf4162e3367a91ae43307207bce`
- Actual Clean Execution Baseline: `79b95d1f4681c2f7b5f961785079a112a3c62492`
- Remote Check at implementation start: `HEAD == origin/main`, ahead/behind `0/0`
- Date: `2026-09-19`

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
| Factory Restore | destructive board operation | NOT_EXECUTED; BOARD_MANUAL/BOARD_AUTO pending |
| Ymodem board transport | existing host contract passed; real target transport | NOT_EXECUTED; BOARD_AUTO pending |
| RTT capture | real target capture | NOT_EXECUTED; BOARD_AUTO pending |
| GDB snapshot/fault on target | real target session | NOT_EXECUTED; BOARD_AUTO pending |

The current baseline contains no identified S10 production test hook or fault-injection macro. Temporary S10 hooks, if required later, must be isolated, marked `TEST ONLY`, compile-time disabled by default, and removed before the final clean build.

## Task 3 Evidence

- Strict Lifecycle API: `firmware_lifecycle_confirm(firmware_storage_t *)`; it reloads latest Metadata, validates the Trial/pending invariants, performs full pending-image validation, reuses the existing atomic Metadata commit, and reloads Metadata for field verification.
- Runtime boundary: `app_ota_runtime_confirm_trial()` is the only Application runtime entry; `appMainTask` and `app_health` do not receive Firmware Storage or Raw Driver pointers.
- Host Test: `04_Test/Host/S10_Trial_Confirm_Rollback/s10_lifecycle_host_test.c` — PASS; strict state/slot gates, Header/size/Payload CRC/Version gates, commit body/marker failure propagation, reload mismatch, normal `TRIAL → NONE`, and confirmed Slot/Version update.
- Application project XML parse — PASS.
- Application Build: `05_Tools\toolkit.bat build application` — PASS; no errors or warnings.
- Real target Confirm transaction, RTT, GDB, IWDG and board reset/power-cycle evidence — NOT_EXECUTED; pending later Board Auto/Manual verification.

## Task 4 Evidence

- Runtime integration contract: `05_Tools\Contracts\Application\test_s10_runtime_health_integration.ps1` — PASS; Health bootstrap, three Runtime Ready reports, single Confirm request gate, otaWorker execution, Confirm result handoff, and narrow lifecycle status API are present. The contract also rejects Raw Storage references in Health, appMainTask and appSystem.
- Health Host Test: `04_Test/Host/S10_Trial_Confirm_Rollback/s10_health_host_test.c` — PASS; Ready deadline, Observation Window, Confirm success/failure states, feed permission and stable `NONE` behavior.
- Application Build: `05_Tools\toolkit.bat build application` — PASS; `OTA_APP_build.log` reports `ExitCode=0`, no errors or warnings.
- Runtime task changes: appMainTask is the only long-term Watchdog Feed owner; otaWorker remains the Confirm Storage transaction owner; display/OTA Ready reports occur after the startup decision.
- Real Runtime Ready/Confirm RTT, GDB, watchdog and board reset evidence — NOT_EXECUTED; pending later Board Auto/Manual verification.

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
- Real Bootloader rollback/restore, RTT, GDB and board evidence — NOT_EXECUTED; pending Task 7/8/9 board verification.

## Task 7 Evidence

- TDD red baseline: the rollback transaction Host Test initially failed to compile because the two rollback Metadata APIs did not yet exist; after implementation it passed.
- Rollback transaction Host Test: `04_Test/Host/S10_Trial_Confirm_Rollback/s10_rollback_transaction_host_test.c` — PASS; verifies `TRIAL → ROLLBACK`, `ROLLBACK → NONE`, preserved confirmed Slot/Version and Slot states, pending clearing only at completion, wrong-state rejection, body/marker atomic boundaries and restart-readable power-loss outcomes.
- Boot decision contract: `05_Tools\Contracts\Bootloader\test_s10_boot_decision_contract.ps1` — PASS; verifies PENDING/TRIAL/ROLLBACK ordering, confirmed prevalidation before destructive restore, persisted ROLLBACK before erase, restart-from-zero ROLLBACK path, and diagnostic-only Reset Cause.
- S09 Metadata commit Host Test — PASS; existing `PENDING → TRIAL` atomic marker ordering and compatibility remain green after private core extraction.
- Bootloader Clean Build: `05_Tools\toolkit.bat build bootloader` — PASS; no errors or warnings. Map reports `Total ROM Size = 22372 bytes (21.85 KiB)`, below 64 KiB.
- Reset Cause snapshot/log includes BOR, POR, PIN, Software and IWDG flags; no target Reset Cause, rollback restore, RTT or GDB evidence has been executed yet — NOT_EXECUTED; pending Task 8/9 board verification.

## Task 8 Evidence

- Host regression matrix — PASS: S02 SPI chunking; S04 CRC, firmware format and storage; S05 storage write, YMODEM parser/receiver and flash sink; S07A startup; S07 IRQ, Key, Worker, Display, Metadata, sink and OTA service; S09 contract, prevalidate, installer and Metadata commit; S10 health, lifecycle, Metadata invariant, boot recovery and rollback transaction.
- Python tests — PASS: `05_Tools/Firmware/test_pack_firmware.py` 2/2; `05_Tools/Ymodem/tests` 24/24; `04_Test/Host/S04_Firmware_Image_Storage/test_s04_persistence_log.py` 15/15.
- Static contracts — PASS: all 11 scripts under `05_Tools/Contracts`; GDB automation, CmBacktrace integration/fault diagnostics, tool sequence and S05C parser fixture checks also PASS.
- Host fixture maintenance — commit `54c9827`; S04 test stubs now expose the current AT24C02/W25Q64 initializer contracts and the Metadata recovery fixture uses a distinct valid confirmed/pending Slot pair. The S07 UART test double uses independent test-owned TX storage instead of removed production struct fields. No production API or behavior was changed by this fixture commit.
- Application Clean Build — `05_Tools\toolkit.bat build application` PASS; 0 errors, 0 warnings. Map reports `Total ROM Size = 84596 bytes (82.61 KiB)`; observed heap-4 `.bss` 24576 bytes and startup stack 1024 bytes.
- Bootloader Clean Build — `05_Tools\toolkit.bat build bootloader` PASS; 0 errors, 0 warnings. Map reports `Total ROM Size = 22372 bytes (21.85 KiB)`, below the 64 KiB limit.
- Real-board automation — NOT_EXECUTED/PENDING: no Flash, RTT, GDB target session, IWDG target observation, reset/power-cycle or rollback fault-injection evidence was produced.
- S05C result-file checks — NOT_EXECUTED/PENDING: `test_i2c.ps1` and `test_spi.ps1` require a real logic-analyzer result file; the fixture parser check passed, but no target capture was available.
- Temporary production test code — none added; all Task 8 checks use existing Host/Contract test assets. Generated build outputs and Python caches were removed after evidence collection.

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
