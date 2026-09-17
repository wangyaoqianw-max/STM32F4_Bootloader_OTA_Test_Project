# S06 RTOS Runtime Handoff

## Metadata

- Stage: `S06_RTOS_Runtime`
- Status: `READY_FOR_REVIEW`
- Branch: `main`
- Baseline Commit: `c99730e`
- Design Commit: `eb57291`
- Implementation Plan Commit: `9f304c7`
- Implementation Commits: `f6f50fd`, `b90d462`, `6d0d323`, `38f7c60`, `20ec343`, `66e2934`, `014b617`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Current Role: `Review Role`
- Owner: `Project Owner`
- Updated At: `2026-09-17`

## Input

S06 按已冻结的 `design.md` 和 `implementation_plan.md` 实施。S05 YMODEM、Firmware Storage、Toolkit、GDB、RTT 和 S05C SPI2/I2C 验证作为上游输入保持有效。

## Frozen Runtime Contract

```text
defaultTask
    -> appSystem

appSystem
    -> v1.0 LED foreground behavior
    -> creates displayTask and otaWorker

otaWorker
    -> USART1 service_uart / DMA / RingBuffer consumer
    -> YMODEM receiver
    -> Slot B Firmware Storage write and validation
    -> publishes compact OTA business events

displayTask
    -> owns ST7789, SPI1 display bus and display model
    -> receives app_display_event_t through Display Queue
```

IPC remains:

```text
UART ISR/RX callback -> otaWorker : Task Notification
otaWorker -> displayTask         : Queue
```

S06 does not provide a public OTA start/cancel/service facade. `APP_OTA_NOTIFY_START` remains an internal reserved flag and must not be exposed as an accidental S07 API.

## Verified Ownership / Blocking Boundaries

- `appSystem` owns foreground LED behavior and does not touch ST7789.
- `otaWorker` owns the active receive session and Slot B write/validation path.
- `displayTask` is the only Application owner of ST7789/SPI1/graphics.
- `otaWorker` blocks on notification when idle and waits on UART events during receive.
- `displayTask` blocks on the Display Queue when idle.
- Display progress is throttled at 5% or 200 ms and terminal events use bounded queue wait.
- Display queue events contain business state only; no YMODEM packet, Flash address or UART DMA internals cross the boundary.

## Verification Handoff

Formal evidence is in:

```text
04_Test/Reports/Stages/S06_RTOS_Runtime/verification.md
```

Important board observations confirmed by Project Owner:

- LCD displays IDLE, RECEIVING/progress, VERIFYING, SUCCESS/100% and FAILED/ERR.
- LED keeps blinking during both successful and failed OTA runs.
- Two reset-started transfers completed with Slot B validation PASS.
- Mid-transfer sender termination produced `header_commit=0` and FAILED without stopping foreground LED behavior.

Toolkit evidence uses explicit `COM9` because the host enumerates multiple serial ports. Existing S05 package format remains `[64 Byte Header][Payload]`; raw `.bin` is not the final YMODEM input.

## S07 Boundary

S07 may add the formal OTA Service facade and explicit session control only after reviewing this handoff. S07 must not infer that S06 already implements `PENDING`, Reset Request, Bootloader Trial/Confirmed or Rollback.

## Outstanding / Not Applicable

- No destructive Display Fault injection was performed; the code-level degraded path was reviewed.
- No no-reboot second session was claimed because S06 has no public START control API. Repeated reuse was validated after reset, within the current contract.
- No SPI1 logic analyzer capture was performed or required.
