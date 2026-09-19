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
