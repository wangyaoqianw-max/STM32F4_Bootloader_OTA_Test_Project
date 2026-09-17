# S06 RTOS Runtime / Concurrency Model Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use `superpowers:subagent-driven-development` or `superpowers:executing-plans` to implement this plan task-by-task. Use checkbox (`- [x]`) syntax for execution tracking.

**Goal:** 基于当前已经运行 FreeRTOS 的 Application，完成 ST7789/LCD 板级适配，并把现有单线程长期循环重构为 `appSystem + otaWorker + displayTask` 三线程 Runtime。复用 S05 的 UART DMA/RingBuffer/Ymodem/Firmware Storage 能力，实现 OTA 后台接收与 LCD 状态显示并发运行，为 S07 OTA Service V1 提供稳定 Runtime Contract。

**Architecture:** `appSystem` 保持前台 Application Runtime；`otaWorker` 作为后台固件接收 Worker，平时阻塞等待；`displayTask` 是 ST7789/Graphics 唯一 Owner，平时阻塞等待 Display Queue。ISR/RX 侧通过 Task Notification 唤醒 `otaWorker`，`otaWorker` 通过 Queue 向 `displayTask` 发送 OTA 状态、进度和结果。

**Tech Stack:** STM32F411CEU6、FreeRTOS/CMSIS-RTOS、HAL、Platform RTOS abstraction、USART1 DMA + RingBuffer、Ymodem、W25Q64/SPI2、ST7789/SPI1、RTT + EasyLogger、J-Link、GDB、现有 Agent Toolkit。

**Spec:** `00_Project/03_Stages/S06_RTOS_Runtime/design.md`

## Global Constraints

- S06 不重新移植 FreeRTOS。
- 长期 Application Task 第一版只保留 `appSystem / otaWorker / displayTask` 三个，不额外创建 UART/Flash/Ymodem/LED Task。
- `defaultTask` 继续只作为启动桥接线程，启动 `appSystem` 后删除自身。
- `displayTask` 为 ST7789/Graphics 唯一 Application Owner；其他线程不得直接刷新 LCD。
- `otaWorker` 为 Ymodem receive session 和 Firmware Download 的执行 Owner。
- UART DMA/RingBuffer、Ymodem Parser/Receiver/Sink、Firmware Storage 的 S05 已验证边界优先复用，不为 S06 重写协议栈。
- OTA Worker 空闲时必须 Block，不允许长期轮询。
- Display Task 无事件时必须 Block，不允许固定高频全屏刷新。
- Display Progress 必须节流，避免每个 Ymodem Packet 都触发刷新。
- S06 不引入 LVGL、CTP、Global Event Bus、Device Manager。
- S06 不提交 `PENDING`、不请求 Reset、不实现 Bootloader Trial/Confirm/Rollback。
- LCD SPI1 第一版不要求逻辑分析仪波形验收；采用肉眼显示 + RTT。
- Logic Analyzer 保持当前 SPI2/W25Q64 与 I2C/AT24C02 接线。
- 所有测试优先复用当前 `05_Tools/toolkit.bat` 能力。
- 任一 Task 验证失败时停在当前 Task，先修复再继续。

## Planned File Map

以下是预期影响范围。实施时允许根据当前代码实际结构做小幅调整，但不得改变设计边界。

```text
00_Project/03_Stages/S06_RTOS_Runtime/
├─ design.md
├─ implementation_plan.md
├─ handoff.md                  # final stage handoff
└─ review.md                   # final review

03_Firmware/Application/OTA_APP/
├─ 01_APP/
│  ├─ app_main.c/.h            # foreground application behavior
│  ├─ app_system.c/.h          # runtime orchestration / startup
│  ├─ app_ota_worker.c/.h      # new, if not better placed by existing convention
│  └─ app_display.c/.h         # new, display task/model if kept in App layer
│
├─ 02_Service/
│  ├─ service_uart/            # reuse
│  ├─ service_ymodem/          # reuse
│  └─ service_firmware/        # reuse
│
├─ 03_Platform/
│  ├─ platform_bsp/            # LCD GPIO/SPI/ST7789 board binding
│  ├─ platform_graphics/       # reuse / minimal extension only if needed
│  └─ platform_os/             # reuse queue/notify/thread APIs
│
├─ 04_Impl/
│  └─ ...                      # only board binding changes required by LCD
│
└─ 06_Config/
   └─ project_config.h         # display static config if current repository convention uses it
```

If current project conventions prefer worker/task files under another existing directory, follow that convention rather than creating parallel architecture. The important contract is ownership and lifecycle, not exact filenames.

## Task 1: Complete LCD / ST7789 Board Adaptation

**Goal:** 先完成屏幕最小可用 Bring-up，不引入 Display Task 并发复杂度。

**Primary areas:**

- `03_Platform/platform_bsp`
- corresponding `04_Impl` board GPIO implementation
- display/static config
- existing `platform_graphics`

**Frozen pin mapping:**

```text
PB10 → LCD_RST
PA1  → LCD_BL
PA4  → LCD_CS
PA5  → SPI1_SCK
PA6  → LCD_DC
PA7  → SPI1_MOSI
```

- [x] **Step 1: Re-read current CubeMX/HAL GPIO + SPI1 configuration** and confirm these six bindings are present in generated code.
- [x] **Step 2: Inspect existing `platform_bsp_st7789` and GPIO constructor dependencies**; list exactly which BSP constructor declarations/implementations are missing.
- [x] **Step 3: Add LCD GPIO constructors/bindings** for CS/DC/RESET/BACKLIGHT without duplicating GPIO driver logic.
- [x] **Step 4: Confirm existing display bus maps to SPI1** and storage bus remains SPI2.
- [x] **Step 5: Add/freeze display static config** required by existing ST7789 BSP, including width/height/X offset/Y offset/MADCTL/max SPI clock.
- [x] **Step 6: Implement a temporary/minimal LCD bring-up path** using current ST7789 + Graphics APIs. Do not add LVGL.
- [x] **Step 7: Build with `toolkit.bat build`, Expected PASS.**
- [x] **Step 8: Flash/run with Toolkit and collect RTT init evidence.**
- [x] **Step 9: Visual board acceptance:** full-screen basic colors or equivalent fill test, ASCII string render, correct orientation, no clipping, backlight control works.
- [x] **Step 10: If orientation/window is wrong, tune X/Y Offset and MADCTL from real visual evidence.**
- [x] **Step 11: Remove any one-off debug code that violates future Display Owner boundary; preserve only reusable display initialization/render primitives.**
- [x] **Step 12: Commit** LCD adaptation separately before Runtime refactor.

**Acceptance:**

```text
Build            PASS
Flash            PASS
RTT init         PASS
Backlight        PASS
Fill/render      PASS
ASCII text       PASS
Orientation      PASS
No clipping      PASS
```

No SPI1 Logic Analyzer capture is required in this task.

## Task 2: Freeze Runtime Task Interfaces And Minimal Data Contracts

**Goal:** 在重构线程前先冻结 API/消息结构，避免实施过程中反复改方向。

**Files:** likely new App/runtime headers plus existing config/types location.

- [x] **Step 1: Define `otaWorker` lifecycle API** (start/init entry only; no S07 business API yet).
- [x] **Step 2: Define `displayTask` lifecycle API** and Display Event contract.
- [x] **Step 3: Define OTA runtime notification bits** required by S06, at minimum RX wakeup and optional start/cancel placeholders if actually used.
- [x] **Step 4: Define Display Event enum** at minimum:

```text
OTA_IDLE
OTA_RECEIVING
OTA_VERIFYING
OTA_SUCCESS
OTA_FAILED
```

- [x] **Step 5: Define compact Display Event payload** for progress/image info/error code only where currently available.
- [x] **Step 6: Define `display_model_t` internal fields** for firmware version, system state, OTA state, progress, target slot and last error.
- [x] **Step 7: Keep these contracts independent of raw Ymodem packet and raw Flash address.**
- [x] **Step 8: Build, Expected PASS.**

## Task 3: Refactor appSystem Into Runtime Orchestrator + Foreground Worker

**Goal:** 保留现有 `appSystem` 线程身份，不额外创建 foregroundTask。

**Primary files:**

- `01_APP/app_system.c/.h`
- `01_APP/app_main.c/.h`
- `Core/Src/freertos.c` only if strictly necessary

- [x] **Step 1: Preserve current `defaultTask → app_system_start() → delete self` startup contract.**
- [x] **Step 2: Move long-lived runtime creation responsibility into `appSystem`.**
- [x] **Step 3: Start `otaWorker` and `displayTask` once; do not dynamically recreate them during runtime.**
- [x] **Step 4: Keep `appSystem` as foreground Application worker.**
- [x] **Step 5: Preserve v1.0 normal behavior as LED Blink.**
- [x] **Step 6: Prepare v1.1 normal behavior contract as PWM Breath without over-abstracting version-specific demo logic. If current LED pin/timer capability does not yet support hardware PWM, document that as a later firmware-image implementation item rather than software bit-banging PWM in S06.**
- [x] **Step 7: Ensure `appSystem` does not perform Ymodem receive, Flash program loop or LCD render directly.**
- [x] **Step 8: Build/flash/run and verify foreground LED behavior still works.**

## Task 4: Implement displayTask As Exclusive Display Owner

**Goal:** 将已通过 Task 1 验证的显示能力正式纳入 RTOS Runtime。

- [x] **Step 1: Create the Display Queue using existing `platform_queue` abstraction.**
- [x] **Step 2: Create `displayTask` using existing `platform_thread` abstraction.**
- [x] **Step 3: Set initial priority to `PLATFORM_THREAD_PRIORITY_BELOW_NORMAL`.**
- [x] **Step 4: Move ST7789 runtime initialization/backlight/initial render into `displayTask` ownership.**
- [x] **Step 5: Initialize a local `display_model_t`.**
- [x] **Step 6: Render initial screen, for example:**

```text
FW VERSION : V1.0
SYSTEM     : RUNNING
OTA STATE  : IDLE
TARGET     : SLOT B
PROGRESS   : 0%
RESULT     : NONE
```

- [x] **Step 7: Enter blocking Queue wait after initialization.**
- [x] **Step 8: On event, update model then render only required fields/regions where practical; avoid mandatory full-screen redraw for every event.**
- [x] **Step 9: Verify no other Application thread directly calls ST7789/Graphics runtime drawing after this refactor.**
- [x] **Step 10: Build/flash/run, visually confirm initial screen and confirm Application LED still runs.**

**Failure behavior:** if display init fails, log `DISPLAY DEGRADED`; do not stop `appSystem` or OTA runtime.

## Task 5: Move S05 Ymodem Runtime Into Long-lived otaWorker

**Goal:** 将 S05 已验证的 Ymodem/Storage receive flow 从测试式/单次入口整理为正式后台 Worker，不重写协议模块。

**Primary areas:**

- `01_APP` runtime worker
- `02_Service/service_uart`
- `02_Service/service_ymodem`
- `02_Service/service_firmware`
- any S05 board-test hook currently embedded in `app_main`

- [x] **Step 1: Re-read current S05 board-test/start path and identify the exact receive entry point and UART ownerThread assumptions.**
- [x] **Step 2: Create `otaWorker` as long-lived thread with initial priority `PLATFORM_THREAD_PRIORITY_ABOVE_NORMAL`.**
- [x] **Step 3: Transfer UART consumer / ownerThread identity to `otaWorker` according to current `service_uart` contract.**
- [x] **Step 4: Preserve DMA/RingBuffer single-consumer semantics.**
- [x] **Step 5: Preserve S05 Ymodem timeout/retry/cancel/header-last-commit behavior.**
- [x] **Step 6: Use existing notify path or add the minimum notify bridge required so RX activity wakes `otaWorker` without polling.**
- [x] **Step 7: When idle, `otaWorker` must block.**
- [x] **Step 8: When a transfer starts, run the existing Ymodem receiver and Firmware Storage pipeline in Task context.**
- [x] **Step 9: On completion or failure, clean session state and return to recoverable wait state.**
- [x] **Step 10: Do not add PENDING metadata or reset request in S06.**
- [x] **Step 11: Build and run existing Ymodem host/board regression before Display integration.**

## Task 6: Implement otaWorker → displayTask Status Queue

**Goal:** 显示线程只接收 OTA 业务语义，不进入实时传输路径。

- [x] **Step 1: On OTA session idle/start, post appropriate `OTA_IDLE/OTA_RECEIVING` event.**
- [x] **Step 2: Calculate UI progress from bytes received/written using already available receiver/image size data; do not duplicate protocol accounting.**
- [x] **Step 3: Throttle progress update. Initial policy:**

```text
progress delta >= 5%
OR
elapsed >= 200 ms
```

Implementation may tune values from board evidence.

- [x] **Step 4: Before final image validation, post `OTA_VERIFYING`.**
- [x] **Step 5: Post `OTA_SUCCESS` when transfer + validation succeed.**
- [x] **Step 6: Post `OTA_FAILED` with bounded error code/summary for timeout/cancel/storage/validation failure.**
- [x] **Step 7: Queue-full policy must be explicit. Do not block OTA critical receive path indefinitely waiting for UI. Prefer drop/coalesce stale progress while preserving terminal state where practical.**
- [x] **Step 8: Verify Display Task never calls back into Ymodem internals to query current state.**

## Task 7: Blocking / Priority / Resource Ownership Audit

**Goal:** 验证三线程模型没有 Busy Loop、资源抢占和优先级设计反作用。

- [x] **Step 1: Inspect `otaWorker` UART wait path and confirm it blocks on notify/event rather than polling.**
- [x] **Step 2: Inspect W25Q64 busy polling path. If it remains continuously READY at ABOVE_NORMAL during long erase/program wait, add a bounded RTOS-friendly yield/delay strategy without breaking driver correctness.**
- [x] **Step 3: Inspect Display Queue wait and confirm Display Task blocks when idle.**
- [x] **Step 4: Confirm SPI1/ST7789 has single Application owner and does not need a new display mutex.**
- [x] **Step 5: Confirm Slot B OTA write operation has one business owner during receive.**
- [x] **Step 6: Inspect multi-task logging path (`service_log` / EasyLogger / RTT) for actual thread-safety. Add protection only if evidence shows shared unsafe state; do not create Log Task by default.**
- [x] **Step 7: Check ISR boundaries: ISR/callback only updates low-level receive state and wakes task; protocol parsing/storage must stay in Task context.**
- [x] **Step 8: Build + static review.**

## Task 8: Runtime Diagnostics And Stack/Heap Measurement

**Goal:** 使用已有 GDB/RTT 工具检查真实 Runtime，而不是凭估计冻结 Stack Size。

- [x] **Step 1: Run normal system with all three Application Tasks created.**
- [x] **Step 2: Use RTT to record task startup and ready/degraded states.**
- [x] **Step 3: Use `toolkit snapshot halt` / GDB to inspect task list or relevant task objects/variables according to what current symbols expose.**
- [x] **Step 4: Confirm expected idle state:**

```text
appSystem      running/periodic blocked
otaWorker      blocked
DisplayTask    blocked
```

- [x] **Step 5: Measure stack high-water marks using available FreeRTOS API/debug symbols.**
- [x] **Step 6: Tune current large/default stacks only after measured evidence; preserve margin.**
- [x] **Step 7: Check heap before/after repeated OTA sessions for leaks or repeated object creation.**
- [x] **Step 8: Resume MCU using frozen GDB contract (`continue& → disconnect → quit`) if snapshot workflow does not already perform resume.**

## Task 9: End-to-End Background OTA Concurrency Acceptance

**Goal:** 证明前台、后台 OTA、Display 三条 Runtime 路径能够同时工作。

**Preparation:** use S04 image pack tooling; do not send raw `.bin` as final image.

- [x] **Step 1: Build v1.0 runtime image.**
- [x] **Step 2: Flash/run with Toolkit.**
- [x] **Step 3: Confirm visually:** LED Blink + LCD `FW V1.0 / OTA IDLE`.
- [x] **Step 4: Start RTT capture.**
- [x] **Step 5: Start Ymodem Sender using current validated Toolkit flow.**
- [x] **Step 6: During transfer confirm simultaneously:**

```text
LED continues blinking
LCD shows RECEIVING / progress
RTT shows OTA receive activity
```

- [x] **Step 7: After receive, confirm LCD enters VERIFYING then SUCCESS/FAILED.**
- [x] **Step 8: Confirm Slot B image validates using existing Firmware Storage validation path.**
- [x] **Step 9: Confirm `otaWorker` returns to blocked/wait state.**
- [x] **Step 10: Repeat transfer at least once to check runtime reuse and no stale session state.**

## Task 10: Failure / Recovery Acceptance

**Goal:** 验证 OTA 或 Display 失败不会破坏前台 Application。

- [x] **Step 1: Interrupt/cancel Ymodem mid-transfer.**
- [x] **Step 2: Confirm existing failure semantics remain: no invalid header commit; session exits with clear result.**
- [x] **Step 3: Confirm LCD shows OTA FAILED.**
- [x] **Step 4: Confirm LED foreground behavior continues.**
- [x] **Step 5: Start a new session and confirm successful recovery without reboot where current S05 contract allows it.**
- [x] **Step 6: Exercise timeout or bad-transfer path supported by current sender/test tooling.**
- [x] **Step 7: Simulate/force Display init failure only if it can be done safely without destructive hardware changes; otherwise verify code path by controlled config/test seam.**
- [x] **Step 8: Confirm Display failure degrades UI only and does not stop OTA/Application.**

## Task 11: Toolkit Regression And Optional Bus Evidence

**Goal:** 复用已有工具完成 S06 回归，不建立新的临时脚本体系。

Run at minimum:

```text
toolkit.bat build
toolkit.bat flash run
toolkit.bat rtt <duration>
toolkit.bat snapshot resume/halt as required
toolkit.bat firmware pack ...
toolkit.bat ymodem ...
```

- [x] **Step 1: Build regression PASS.**
- [x] **Step 2: Flash/run PASS.**
- [x] **Step 3: RTT PASS.**
- [x] **Step 4: GDB snapshot/recovery PASS.**
- [x] **Step 5: Firmware pack + Ymodem PASS.**
- [x] **Step 6: Existing host/toolkit tests PASS.**
- [x] **Step 7: Logic Analyzer is optional for S06 unless storage/I2C behavior requires external evidence. If used, keep current physical wiring:**

```text
SPI2 / W25Q64
I2C / AT24C02
```

- [x] **Step 8: Do not require SPI1/LCD waveform capture for stage closure.**

## Task 12: Verification Report, Handoff, Review And Status Update

**Files:**

- Create: `04_Test/Reports/Stages/S06_RTOS_Runtime/verification.md`
- Create: `00_Project/03_Stages/S06_RTOS_Runtime/handoff.md`
- Create: `00_Project/03_Stages/S06_RTOS_Runtime/review.md`
- Update: `PROJECT_CONTEXT.md`
- Update: `00_Project/05_Status/current_status.md`
- Update: `00_Project/02_Roadmap/development_roadmap.md` if implementation reveals approved roadmap correction

- [x] **Step 1: Preserve evidence for LCD visual acceptance, RTT, Ymodem, validation, task/runtime diagnostics and recovery tests.**
- [x] **Step 2: Write Verification report with explicit PASS/FAIL per acceptance item.**
- [x] **Step 3: Write Handoff freezing the S06 Runtime Contract for S07.**
- [x] **Step 4: Perform code/design Review focusing on ownership, blocking, queue/notify use, error recovery, and accidental S07 scope creep.**
- [x] **Step 5: Resolve Blocking/Important findings before closing stage.**
- [x] **Step 6: Update project context/status only after Verification + Review pass.**
- [x] **Step 7: Mark S06 `CLOSED / PASS`; set S07 as next planned stage.**

## Completion Notes

- Task 9 的第二轮传输通过重新烧录/复位启动，符合当前 S06 只有初始会话启动入口的冻结边界。
- Task 10 Step 5 的“无复位启动新会话”在当前 S06 中为 `NOT_APPLICABLE`：`app_ota_worker_start()` 是唯一公开入口，正式 OTA Service / START 控制留给 S07。
- Task 10 的 Display Fault 注入未进行破坏性板测；已完成 `DISPLAY DEGRADED` 代码路径审查，未引入硬件改线或故障注入。
- Task 11 的 Keil 工程当前只生成 `.hex/.axf`，不存在计划示例中的 `Objects/OTA_APP.bin`；pack 回归使用现有 S04 payload `06_Output/Firmware/OTA_APP_s04_test.bin`，生成镜像与已验证输入 SHA-256 一致。
- Task 8 的栈高水位使用现有 GDB 调试符号和 FreeRTOS A5 填充扫描完成，没有保留临时生产诊断代码。

## Final Acceptance Checklist

S06 cannot close until all applicable items pass:

```text
[ ] LCD/ST7789 board adaptation works visually
[ ] appSystem / otaWorker / displayTask created as designed
[ ] defaultTask remains bootstrap-only
[ ] otaWorker idle state is blocked, no busy loop
[ ] displayTask idle state is blocked
[ ] UART DMA/RingBuffer retains one consumer
[ ] Ymodem S05 protocol behavior remains valid
[ ] Slot B image still validates after transfer
[ ] v1.0 LED continues blinking during background OTA
[ ] LCD shows IDLE/RECEIVING/VERIFYING/SUCCESS or FAILED
[ ] Display update does not block OTA critical path indefinitely
[ ] Ymodem cancel/timeout recovers without killing foreground Application
[ ] Display failure is degraded, not fatal
[ ] Stack/heap evidence collected and no obvious leak found
[ ] Existing Build/Flash/RTT/GDB/Ymodem Toolkit regression passes
[ ] No mandatory SPI1 logic analyzer requirement introduced
[ ] No PENDING/Reset/Bootloader/Trial/Confirm/Rollback scope creep
[ ] Verification report complete
[ ] Review passes
```

## Implementation Order Summary

```text
1. LCD/ST7789 board adaptation
2. Runtime interfaces and event contracts
3. appSystem runtime refactor
4. displayTask
5. otaWorker
6. OTA → Display queue
7. blocking/priority/resource audit
8. stack/heap runtime diagnostics
9. end-to-end concurrency acceptance
10. failure/recovery acceptance
11. toolkit regression
12. verification/handoff/review
```

This order deliberately validates the screen before introducing RTOS display ownership, then migrates S05 receive capability into a long-lived worker, and finally proves real concurrent behavior on hardware.
