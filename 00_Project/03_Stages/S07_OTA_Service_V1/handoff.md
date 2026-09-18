# S07 OTA Service V1 Handoff

## Metadata

- Stage: `S07_OTA_Service_V1`
- Status: `CLOSED / PASS`
- Branch: `main`
- Baseline Commit: `84f07303d2b6fbf0682e492ad79e32982e2fb17b`
- Design Commit: `a5c4c1b890cf232e8e884d9ddb72473212892c13`
- Implementation Plan Commit: `3049034ea3472eb303aec50a196e785b4bd6b84c`
- Implementation Commit: `2ab34f1` (Task 10 production fix; Task 8: `23ac3db`; Task 7: `a8045d297442fe7b97bc318a37b53b05b4a4b968`; Task 6: `ae264974de810272a83314c4c3fcddb4d9ef6991`; interface style correction: `4e5a8e5`; Task 5: `b9c619801b5913b25c52b1e57fe9cffbbb21d41b`; Task 4 XML correction: `e0b8b8f58145f2ca73132a0f5a8700b9d36f3cc2`; Task 4 main: `29c1f35aa8d8a3330cd8a8280bcfcb08eda7b534`; Task 3 Storage Host test correction: `7536cf06f6af539284dcaa4fc0396834ea7362fb`; Task 3 main: `bb0f96b643da137805b586eb817347a92dc0f140`; Task 2 correction: `6ac6a49756a87079f1bc2b95d190fc44d82f64c3`; Task 2 initial: `b4a94245f09be9dac40ad8e3135302722504ca5b`; Task 1: `13d67efcecfa1e99b14eb0781e77aed3749bda2c`; Task 0: `9adba52aa75ddaff906e42aa8bae433b654ae761`)
- Verification Commit: `ade703e0d72bdef4a26cabd2afe36ecb86a09fa6`
- Review Commit: `ade703e0d72bdef4a26cabd2afe36ecb86a09fa6`
- Updated At: `2026-09-18`

## Implementation Input

### Goal

在 S06 已验证的 FreeRTOS Runtime 上建立正式 OTA Service V1：通过 KEY_1 启动 Ymodem OTA，将完整 Firmware Image 写入 W25Q64 非确认槽并验证；验证成功后进入 `READY_TO_INSTALL`，仅在用户第二次按键确认后，以 AT24C02 Metadata 原子提交 `pendingSlot + PENDING`，再请求 Reset。

S07 的职责终点：

```text
KEY_1
→ OTA READY
→ Ymodem Receive
→ Inactive External Slot
→ Image Validation
→ READY_TO_INSTALL
→ KEY_1
→ Metadata PENDING
→ Reset
→ PENDING persistence confirmed
```

### Required Reading

实施前必须按顺序读取：

```text
AGENTS.md
PROJECT_CONTEXT.md
00_Project/WORKFLOW.md
03_Firmware/AGENTS.md
03_Firmware/00_Doc/Standards/嵌入式C代码规范.md
00_Project/03_Stages/S07_OTA_Service_V1/design.md
00_Project/03_Stages/S07_OTA_Service_V1/implementation_plan.md
00_Project/03_Stages/S06_RTOS_Runtime/design.md
00_Project/03_Stages/S06_RTOS_Runtime/handoff.md
00_Project/03_Stages/S06_RTOS_Runtime/review.md
04_Test/Reports/Stages/S06_RTOS_Runtime/verification.md
```

随后调查真实代码：

- `app_ota_worker`；
- `service_uart`；
- Ymodem parser / receiver / sink；
- Firmware Storage / Firmware Metadata；
- W25Q64 / AT24C02；
- Platform MCU / GPIO；
- FreeRTOS Task Notification / Queue；
- Display Model / displayTask；
- `04_Test/Board/S05_UART_Ymodem/s05_ymodem_flash_sink.*`；
- CubeMX `.ioc`；
- CmBacktrace HardFault ownership；
- FreeRTOS heap 配置；
- `05_Tools` Toolkit 现有入口和测试。

### Frozen Design Decisions

#### 1. Internal vs External Flash

```text
STM32 Internal Flash
= 当前实际执行的 Application

W25Q64 Slot A/B
= External Firmware Image Slots
= OTA / rollback image storage
```

W25Q64 A/B 不得描述成 F411 可直接执行的 Application A/B partition。

#### 2. Metadata V2

删除 `activeSlot`。

核心业务字段：

```text
sequence
confirmedSlot
pendingSlot
slotAState
slotBState
upgradeState
confirmedVersion
```

`confirmedSlot` 是最后确认成功版本的外部恢复镜像；`pendingSlot` 是 Bootloader 下一次需安装的镜像。

Upgrade lifecycle：

```text
NONE → PENDING → TRIAL → NONE
                 ↓
             ROLLBACK → NONE
```

S07 只负责 `NONE → PENDING`。

#### 3. Image Health != Upgrade Lifecycle

```text
EMPTY / VALID / INVALID
```

描述镜像健康。

```text
NONE / PENDING / TRIAL / ROLLBACK
```

描述升级事务。

CRC 完整但 Trial 失败的镜像不应仅因运行失败被标成 `INVALID`。

#### 4. Metadata Persistence

保留 AT24C02 双 128 Byte copies：

```text
Copy A 0x00~0x7F
Copy B 0x80~0xFF
```

保留：

```text
sequence
CRC32
commit marker last
```

V1 必须 backward-compatible read；V1 decode 默认：

```text
pendingSlot = NONE
upgradeState = NONE
```

读取 V1 不立即写 EEPROM，下一次正常 commit 自然迁移 V2。

#### 5. OTA Runtime State

```text
IDLE
→ PREPARING
→ RECEIVING
→ VERIFYING
→ READY_TO_INSTALL
→ COMMITTING
→ REBOOT_REQUIRED
```

失败进入 `FAILED`。

`READY_TO_INSTALL` 仅为 RAM runtime state，不持久化成 Bootloader command。

#### 6. Download Transaction Ordering

必须：

```text
select opposite(confirmedSlot)
→ target state = INVALID
→ commit Metadata
→ erase target
→ Ymodem receive
→ Header-last commit
→ validate image
→ target state = VALID
→ commit Metadata
→ READY_TO_INSTALL
```

不得先 erase 再把 Metadata target 标记 INVALID。

#### 7. Second Confirm / PENDING

只有 `READY_TO_INSTALL` 的第二次 KEY_1 才允许：

```text
pendingSlot = target
upgradeState = PENDING
→ Metadata commit
→ REBOOT_REQUIRED
```

Reset 必须晚于 Metadata commit success。

#### 8. otaWorker

保留 S06 已验证任务，不新增 OTA Task。

```text
otaWorker = RTOS execution shell
```

负责 UART notification、KEY notification、task-context debounce、Service drive、Display event bridge 和 Reset handoff。

OTA 业务状态机迁入 `service_ota`。

#### 9. Production OTA Sink

Production 不得继续依赖：

```text
04_Test/Board/S05_UART_Ymodem/s05_ymodem_flash_sink.*
```

建立支持动态 Slot 的 production sink，并保留 Header-last contract。

Ymodem receiver 继续不知道 Slot / Metadata / PENDING / Reset。

#### 10. KEY_1

PA0 当前硬件/生成配置：

```text
KEY_1
Pull-up
Falling Edge EXTI
```

路径：

```text
PA0
→ EXTI0
→ HAL_GPIO_EXTI_Callback
→ lightweight key forwarding
→ xTaskNotifyFromISR(otaWorker)
→ otaWorker debounce
→ service_ota action
```

ISR 中不得做 Flash/Metadata/Ymodem business/LCD/HAL_Delay。

按键语义：

```text
IDLE             → START OTA
READY_TO_INSTALL → CONFIRM INSTALL
active transfer  → IGNORE
```

#### 11. Platform Key

增加薄 Platform Key abstraction；Platform 只知道 KEY/input event，不知道 OTA 业务语义。

#### 12. Platform MCU IRQ

现在 USART、DMA、EXTI 都需要中断，S07 增加通用高频 IRQ abstraction：

```text
enable
disable
set_priority
clear_pending
```

Platform 使用 `IRQ` 概念，不使用 STM32-specific `EXTI` 作为通用接口名。Impl 优先 CMSIS NVIC。

所有调用 FreeRTOS `...FromISR()` 的 IRQ 必须满足 FreeRTOS max syscall priority 规则。

#### 13. Factory Provisioning

第一次 OTA 前必须存在 confirmed rollback image。

目标 baseline：

```text
Internal Flash = v1.0 APP
Slot A         = v1.0 VALID image
Slot B         = EMPTY
confirmedSlot  = A
confirmedVersion = 1.0.0
pendingSlot    = NONE
upgradeState   = NONE
```

第一版由 PC/Toolkit 主动预置，不实现 MCU Self-Provisioning。

### Allowed Changes

允许按 `implementation_plan.md` 修改：

- S07 stage implementation files；
- Application App / Service / Platform / Impl 中与 OTA、Key、IRQ、Metadata 直接相关代码；
- Ymodem production wiring；
- Display OTA state mapping；
- Firmware Metadata tests；
- OTA Service Host tests；
- `05_Tools` 中必要的 provisioning workflow/router/test；
- `04_Test/Reports/Stages/S07_OTA_Service_V1/verification.md`；
- `PROJECT_CONTEXT.md` / `current_status.md` / roadmap 等施工和验证所需正式状态文件；
- 必要的跨阶段 ADR。

### Prohibited Changes

禁止：

- 新建功能分支/worktree；
- 改变 S06 三线程拓扑而无设计变更；
- 新增 Key Task；
- 新增第二 OTA Task；
- 把 OTA business logic 放进 ISR；
- 让 `service_ota` 直接调用 `NVIC_SystemReset()`；
- 让 Ymodem 感知 Slot / Metadata / PENDING / Reset；
- 扩展 Firmware Image V1 Header；
- 实现 Internal Flash installation；
- 实现 Trial / Confirm / Rollback execution；
- 引入 AES/SHA/signature/CK02AT；
- 引入复杂 Manager Framework；
- 建立第二套 Build/Flash/RTT/Ymodem/Provisioning 工具链；
- 为无关模块做重构、格式化或重命名；
- 用 Build/Host Test 声称真实硬件 PASS。

### Acceptance Criteria

必须满足：

```text
1. CubeMX regenerate 后 S06 heap/CmBacktrace/UART DMA/runtime 无回归。
2. Platform MCU IRQ 最小通用能力落地并满足 FreeRTOS priority rule。
3. Platform Key + PA0 lightweight ISR forwarding 落地。
4. KEY_1 不新增专用 Task，事件由 otaWorker 消费。
5. Metadata V2 删除 activeSlot，引入 pendingSlot / upgradeState。
6. V1 Metadata 可兼容读取。
7. Metadata dual-copy atomicity 不回归。
8. Production OTA sink 支持动态 Slot，且不依赖 S05 Board Test sink。
9. service_ota 成为正式 OTA business state machine。
10. target Slot 在 erase 前先持久化为 INVALID。
11. receive success 后必须 validate image 才能标记 VALID。
12. READY_TO_INSTALL 不产生 PENDING。
13. 第二次 KEY 才提交 pendingSlot + PENDING。
14. PENDING commit success 后才 Reset。
15. Reset 后 PENDING persistence 可读取。
16. confirmedSlot 在新版本确认前不被覆盖。
17. interrupted transfer / bad CRC / Metadata write failure 不产生错误 PENDING。
18. RECEIVING/VERIFYING/COMMITTING 中 KEY 被忽略。
19. Factory provisioning 建立 v1.0 confirmed baseline。
20. S06 foreground/display/UART runtime 继续正常。
21. Internal Flash install / Trial / Confirm / Rollback execution 仍未实现。
```

### Required Verification

优先复用现有 Toolkit：

```text
05_Tools\toolkit.bat build
05_Tools\toolkit.bat flash run
05_Tools\toolkit.bat rtt ...
05_Tools\toolkit.bat ymodem ...
05_Tools\toolkit.bat snapshot halt|resume
05_Tools\toolkit.bat logic ...
```

必须区分：

```text
Code Verification
Hardware Verification
```

至少验证：

- Metadata V1/V2 / dual-copy fault injection；
- OTA state machine；
- dynamic sink + Header-last；
- build/regression；
- KEY start；
- Ymodem receive；
- inactive slot；
- Image Validation；
- READY_TO_INSTALL；
- second KEY confirm；
- PENDING commit；
- Reset persistence；
- interrupted transfer；
- CRC failure；
- active transfer key ignore；
- S06 runtime regression。

## Implementation Output

- Status: `CLOSED / PASS`

### Completed Work

Task 0 completed the CubeMX regeneration recovery before S07 production implementation. S06 heap sizing, CmBacktrace HardFault ownership and FreeRTOS task introspection exports were restored; PA0 Falling EXTI configuration and S06 UART/DMA/LCD runtime baseline were verified.

Tasks 1–7 completed the Platform IRQ/Key layering, Metadata V2, production dynamic sink, `service_ota`, RTOS execution-shell refactor and Display interaction contract. Tasks 8–9 completed safe Factory baseline provisioning and full Host/Toolkit/code regression. Task 10 completed real PA0/COM9 success, READY reset, interrupted, bad CRC, duplicate-key, durable PENDING and LCD full-flow acceptance; the terminal-progress queue flood remains fixed in `2ab34f1`. Project Owner confirmed the physical acceptance, so S07 is closed.

### Changed Files

Current S07 implementation changes:

```text
00_Project/03_Stages/S07_OTA_Service_V1/design.md
00_Project/03_Stages/S07_OTA_Service_V1/implementation_plan.md
00_Project/03_Stages/S07_OTA_Service_V1/handoff.md
03_Firmware/Application/OTA_APP/Core/Inc/FreeRTOSConfig.h
03_Firmware/Application/OTA_APP/Core/Src/stm32f4xx_it.c
03_Firmware/Application/OTA_APP/Middlewares/Third_Party/FreeRTOS/Source/tasks.c
03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/irq/platform_mcu_irq.h
03_Firmware/Application/OTA_APP/04_Impl/impl_mcu/impl_platform_mcu_irq.c
04_Test/Host/S07_OTA_Service/s07_irq_host_test.c
03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx
03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/key/platform_key.h
03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/key/platform_key.c
03_Firmware/Application/OTA_APP/04_Impl/impl_bsp/impl_platform_bsp_key.h
03_Firmware/Application/OTA_APP/04_Impl/impl_bsp/impl_platform_bsp_key.c
03_Firmware/Application/OTA_APP/01_APP/app_runtime_contract.h
03_Firmware/Application/OTA_APP/01_APP/app_ota_worker.c
03_Firmware/Application/OTA_APP/01_APP/app_display_task.c
03_Firmware/Application/OTA_APP/Core/Src/main.c
04_Test/Host/S07_OTA_Service/s07_key_host_test.c
03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_metadata.h
03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_metadata.c
04_Test/Host/S07_OTA_Service/s07_metadata_host_test.c
04_Test/Host/S04_Firmware_Image_Storage/s04_firmware_format_host_test.c
04_Test/Host/S04_Firmware_Image_Storage/s04_firmware_storage_host_test.c
04_Test/Board/S04_Firmware_Image_Storage/app_s04_firmware_image_test.c
04_Test/Board/S04_Firmware_Image_Storage/app_s04_persistence_test.c
04_Test/Board/S04_Firmware_Image_Storage/app_s04_persistence_test.h
03_Firmware/Application/OTA_APP/02_Service/service_firmware/ota_firmware_sink.h
03_Firmware/Application/OTA_APP/02_Service/service_firmware/ota_firmware_sink.c
04_Test/Host/S07_OTA_Service/s07_ota_firmware_sink_host_test.c
03_Firmware/Application/OTA_APP/02_Service/service_ota/service_ota.h
03_Firmware/Application/OTA_APP/02_Service/service_ota/service_ota.c
03_Firmware/Application/OTA_APP/01_APP/app_ota_worker.c
03_Firmware/Application/OTA_APP/01_APP/app_ota_runtime.h
03_Firmware/Application/OTA_APP/01_APP/app_ota_runtime.c
03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/reset/platform_mcu_reset.h
03_Firmware/Application/OTA_APP/04_Impl/impl_mcu/impl_platform_mcu_reset.c
04_Test/Host/S07_OTA_Service/s07_worker_contract_host_test.c
04_Test/Host/S07_OTA_Service/s07_display_contract_host_test.c
04_Test/Host/S07_OTA_Service/s07_service_ota_host_test.c
03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx
04_Test/Board/S07_OTA_Service/README.md
04_Test/Board/S07_OTA_Service/app_s07_provision_test.h
04_Test/Board/S07_OTA_Service/app_s07_provision_test.c
04_Test/Reports/Stages/S07_OTA_Service_V1/verification.md
00_Project/04_Decisions/ADR-0001-external-firmware-slots-and-metadata-lifecycle.md
00_Project/03_Stages/S07_OTA_Service_V1/review.md
PROJECT_CONTEXT.md
00_Project/05_Status/current_status.md
```

### Deviations From Plan

Task 0 found and recovered CubeMX regeneration regressions: `configTOTAL_HEAP_SIZE` had reverted from the S06 frozen `24576` to `15360`; generated C fault handlers had reintroduced a HardFault ownership conflict with `cmb_fault.S`; and the CmBacktrace FreeRTOS task introspection exports had been removed from `tasks.c`. No S07 design boundary was changed.

Task 1 added the minimal Platform MCU IRQ abstraction. The public interface exposes only project IRQ IDs and enable/disable/set-priority/clear-pending operations; the STM32 implementation maps KEY EXTI0, USART1 and the two USART1 DMA streams to CMSIS NVIC APIs. Priority values below `PLATFORM_MCU_IRQ_FREERTOS_SAFE_PRIORITY` (5) are rejected for the IRQs that may call FreeRTOS ISR APIs. No callback manager or STM32 IRQ type is exposed above Impl.

Task 2 added the thin Platform BSP Key path. `HAL_GPIO_EXTI_Callback` only passes the pin to `impl_platform_bsp_key`; the BSP mapping converts `KEY_1` into a Platform Key event and the worker callback sets the independent `APP_OTA_NOTIFY_KEY_1` Thread Flag through the existing ISR-safe adapter. The worker applies a 40 ms task-context debounce and clears key flags when stopping a transfer, so key presses during the current transfer are not replayed as a new session. No Key Task was added.

Task 2 review correction: the first implementation placed the board-specific Key files under MCU capability directories and used a reduced file format. Before continuing, the files were moved to `platform_bsp/key` and `impl_bsp`, while the generic IRQ implementation remains under `platform_mcu/irq` and `impl_mcu`. The new files now follow the repository C format, include contract comments, and keep board pin mapping out of MCU capability code.

Task 3 confirmed the existing V1 raw contract from `firmware_metadata.c`: magic `0x00`, format `0x04`, size `0x06`, sequence `0x08`, legacy active/confirmed/health bytes `0x0C..0x0F`, confirmed version `0x10..0x17`, CRC body `0x00..0x77`, CRC `0x78..0x7B`, and commit marker `0x7C..0x7F`. V2 keeps confirmed/health/version offsets, reserves `0x0C`, adds `pendingSlot` at `0x18` and `upgradeState` at `0x19`, and keeps `0x1A..0x77` zero. Metadata V1 reads default the new lifecycle fields to `NONE`; normal commit encoding is V2. `activeSlot` was removed from production and test Metadata consumers.

Task 4 added production `ota_firmware_sink` under `02_Service/service_firmware`. It keeps target Slot in caller-owned session context, buffers and validates the 64 Byte Image Header before erasing, writes Payload first, and commits the Header only after the complete file length and Payload length are satisfied. `app_ota_worker` now binds this production sink to the existing YMODEM receiver; the worker still uses Slot B until Task 5 moves target selection into `service_ota`. The Keil project no longer includes or links the S05 Board Test sink.

Task 5 added `service_ota` under `02_Service/service_ota`. It owns stable Metadata preconditions, opposite confirmed-Slot selection, pre-erase `INVALID` persistence, YMODEM/production-sink coordination, throttled progress, image validation, `READY_TO_INSTALL`, and the second-confirm `PENDING` transaction. It reports `REBOOT_REQUIRED` only and does not call a reset implementation. The public header exposes Service/Platform contracts only; no FreeRTOS notification, HAL, or display type is included.

Task 6 reduced `app_ota_worker` to the S06 RTOS execution shell. Storage/UART hardware binding is isolated in `app_ota_runtime`; the worker now only consumes UART/KEY notifications, drives `service_ota`, maps Service events to Display Queue events, and hands `REBOOT_REQUIRED` to the Platform MCU reset abstraction. A Service abort API was added for UART/runtime cancellation paths so failed sessions enter `FAILED` and can be retried without creating `PENDING`. No additional OTA Task was introduced.

Task 7 completed the Display/User Interaction bridge. Existing S06 display states remain available; `READY_TO_INSTALL` now renders the target Slot, verified result and second-key action, while `REBOOT_REQUIRED` renders the committed-PENDING/reset indication. Display event payloads now carry the mapped target Slot, and `displayTask` remains the sole LCD owner. The existing Display Model firmware-version line is retained because the frozen Service status contract does not expose a separate target-image version field. No initialization binding or Service ownership was expanded.

Task 8 established the Factory baseline through a temporary board test under `04_Test/Board/S07_OTA_Service`. The test reuses the production Firmware Storage, Metadata, `ota_firmware_sink` and YMODEM Receiver; it accepts only an empty device or the explicitly approved S04 residual state, writes the new image to Slot A, validates it, clears the approved legacy Slot B when applicable, and commits Metadata V2 with `confirmedSlot=A`, `pendingSlot=NONE` and `upgradeState=NONE`. No `toolkit provision` command or production startup dependency was added. The actual CH340 sender port is `COM9`.

Task 10 physical acceptance continuation completed on 2026-09-18. After the approved temporary provisioning reset, the formal Application was rebuilt, flashed and exercised on the real board through CH340 `COM9`. The user pressed PA0 to start a real `1.1.0` transfer: sender completed `77468` bytes / `76` blocks / exit `0` / retries `2`, and GDB reached `READY_TO_INSTALL` on Slot B. The user then pressed PA0 again; after reset, GDB read `confirmedSlot=A`, `pendingSlot=B`, Slot A/B VALID, `upgradeState=PENDING`, sequence `10`. The reset-time key release was sampled again by the new application and left the RAM Service in `FAILED/INVALID_STATE`, but did not change the already committed Metadata.

The same board session also passed: READY reset without second confirmation (`state=4` before reset, normal RTT boot and `state=0 IDLE` afterward); interrupted receive (sender Ctrl+C, `state=2 RECEIVING`, received `0`, Slot B INVALID, pending NONE); bad CRC (payload byte changed only, full COM9 send, `state=7 FAILED`, error `3`, Slot B INVALID, pending NONE); and duplicate PA0 during receiving (final `state=4 READY_TO_INSTALL`, pending NONE, upgrade NONE, both slots VALID). RTT confirmed Application/foreground/displayTask startup and Display initialization for every reset. Project Owner confirmed the LCD displayed `RECEIVING → VERIFYING → READY_TO_INSTALL` for the normal flow and `FAILED` for bad CRC.

### Verification Results

Task 0 code verification: `05_Tools\\toolkit.bat build` PASS with 0 errors and 0 warnings; `git diff --check` PASS. Board baseline: `05_Tools\\toolkit.bat flash run` PASS and `05_Tools\\toolkit.bat rtt 5` PASS; RTT confirmed appSystem/otaWorker/displayTask startup, UART/YMODEM_READY and Display initialization. This is S06 baseline smoke only, not full S07 hardware acceptance.

Task 1 code verification: the test-first Host contract test initially failed because the new public header was absent, then passed with `gcc -std=c99 -Wall -Wextra -Werror`; `05_Tools\\toolkit.bat build` PASS with the Keil build log reporting 0 errors and 0 warnings; project XML parse confirmed the new Impl source is included; `git diff --check` PASS. Hardware verification: NOT APPLICABLE for the abstraction contract; no new board behavior was claimed.

Task 2 code verification: after the layering and style correction, the IRQ and Platform Key Host contract tests passed with `gcc -std=c99 -Wall -Wextra -Werror`; the project XML parse confirmed the BSP Key and MCU IRQ source paths; `05_Tools\\toolkit.bat build` PASS; `git diff --check` PASS. Hardware verification: PENDING; the PA0 physical press and ISR-to-worker trace still require board evidence.

Task 3 code verification: `s07_metadata_host_test.c` passed V2 round-trip, V1 backward-compatible decode, natural V2 migration encoding, cross-field validation, reserved-byte rejection and CRC rejection; existing S04 format and sequence/latest-copy tests passed; the adapted S04 Storage Host test passed image validation, Metadata V2 commit, write-failure old-copy retention and next-copy recovery. `05_Tools\\toolkit.bat build` PASS with 0 errors and 0 warnings; `git diff --check` PASS. Hardware verification: PENDING; no real-board Metadata V2 persistence or S07 OTA acceptance was claimed.

Task 4 code verification: `s07_ota_firmware_sink_host_test.c` passed dynamic Slot A/B mapping, split Header buffering, Header-last ordering, short-file, oversize, abort, erase/payload/header failure paths; project XML parse confirmed the production source and absence of the S05 sink; production search found no `s05_ymodem_flash_sink` reference under `03_Firmware/Application/OTA_APP`; `05_Tools\\toolkit.bat build` PASS; `git diff --check` PASS. Hardware verification: PENDING; no real-board dynamic Slot OTA transfer was claimed.

Task 5 code verification: `s07_service_ota_host_test.c` passed opposite target selection, pre-erase `INVALID` commit, YMODEM/production sink flow, image validation failure retention, `READY_TO_INSTALL` without PENDING, second-confirm PENDING commit, cancel/retry, Metadata write failure retry, invalid transitions, duplicate confirm, and restart with persisted PENDING. Host compilation passed with `gcc -std=c99 -Wall -Wextra -Werror`; `OTA_APP.uvprojx` XML parse PASS; `05_Tools\\toolkit.bat build` final PASS with no errors or warnings; `git diff --check` PASS. Hardware verification: PENDING; no real-board Service state-machine or PENDING persistence acceptance was claimed.

Task 6 code verification: `s07_worker_contract_host_test.c`, `s07_service_ota_host_test.c`, `s07_ota_firmware_sink_host_test.c`, `s07_metadata_host_test.c`, `s07_key_host_test.c` and `s07_irq_host_test.c` passed with `gcc -std=c99 -Wall -Wextra -Werror`; the Service Host regression also covered runtime abort entering `FAILED` and retry. Production dependency scan confirmed `app_ota_worker.c` no longer directly includes or calls Firmware Storage, production Sink, YMODEM Receiver or Metadata operations; XML parse passed; `05_Tools\\toolkit.bat build` passed with 0 errors and 0 warnings; `git diff --check` passed. Hardware verification: PENDING; no real-board KEY/UART/Display/Reset acceptance was claimed.

Task 7 code verification: `s07_display_contract_host_test.c` and the existing `s07_worker_contract_host_test.c` passed with `gcc -std=c99 -Wall -Wextra -Werror`; the Service progress throttle remains `5%` or `200 ms`; `05_Tools\\toolkit.bat build` passed with 0 errors and 0 warnings; `git diff --check` passed. Board smoke verification: `05_Tools\\toolkit.bat flash run` PASS and `05_Tools\\toolkit.bat rtt 5` PASS; RTT confirmed `displayTask` startup, Display SPI initialization and Display init result `0`. This is startup/display smoke only; full S07 READY/RESET interaction remains hardware PENDING.

Task 8 board verification: existing Toolkit composition was investigated. The current Keil build exposes `OTA_APP.axf` but not the configured `OTA_APP.bin`, so the already confirmed repository toolchain `fromelf.exe` was used only to create a temporary ignored BIN before `toolkit firmware pack`; no production or Toolkit implementation was changed. `toolkit ymodem python devices --json` identified `USB-SERIAL CH340 (COM9)`. With the sender started before reset, the temporary board test completed on COM9: `blocks_sent=67`, `bytes_sent=67664`, `exit_code=0`, `retries=2`. RTT reported `baseline PASS copy=2 sequence=1 Slot A=1.0.0`; the final target state was validated by the test after Slot A image validation and Metadata reload. The safety guard also refused unapproved non-empty states before erase/write. The temporary test integration was removed, the formal application was rebuilt, flashed and restarted; final production RTT showed only `appSystem`, `otaWorker` and `displayTask` startup plus Display initialization. This proves Factory baseline provisioning and formal-app restoration, but does not prove the complete S07 OTA acceptance flow. Full Host/Toolkit regression remains Task 9.

Task 9 verification: all S07 Host tests passed with `gcc -std=c99 -Wall -Wextra -Werror`: IRQ, BSP Key, Display/Worker contracts, Metadata V1/V2, production sink and Service state machine. Existing S04/S05 regression passed: Firmware image format, current Firmware Storage, storage write, YMODEM parser/receiver and historical S05 sink tests. Python/toolkit regression passed: Firmware pack (2 tests), Python YMODEM (24 tests), S04 persistence (15 tests), legacy/transport compatibility, Application workflow, Toolkit Core, Debug, S04 isolation and Logic Analyzer contracts. `05_Tools\\toolkit.bat build` completed with 0 errors and 0 warnings. Production dependency scan found no `s05_ymodem_flash_sink` or `04_Test/Board/S05_UART_Ymodem` reference under `03_Firmware/Application/OTA_APP`. `git diff --check` passed. No S07 production code was changed during this regression.

Task 10 verification: the formal Application build produced `OTA_APP.build_log.htm` with `0 Error(s), 13 Warning(s)`; the Toolkit maps warnings to exit code `1`, so this remains Build-with-warnings. `flash run`, RTT captures, COM9 Python YMODEM, Toolkit GDB reset/halt/resume and the final regression suites completed with the results recorded in `04_Test/Reports/Stages/S07_OTA_Service_V1/verification.md`. Project Owner confirmed the normal LCD flow and bad-CRC failure display; code and hardware verification are PASS.

Task 10 acceptance evidence summary:

```text
COM9 normal transfer:          PASS, 77468 bytes / 76 blocks / exit 0 / retries 2
Physical PA0 start:             PASS
Physical PA0 confirm + PENDING: PASS, pending B / upgrade PENDING / sequence 10
READY reset without confirm:   PASS, state 4 -> reset -> state 0, no PENDING
Interrupted transfer:           PASS, Slot B INVALID / pending NONE / received 0
Bad CRC image:                  PASS, FAILED / error 3 / Slot B INVALID / no PENDING
Duplicate PA0 during receive:  PASS, final READY / no PENDING
LCD visual full-flow:           PASS, Project Owner confirmed normal and bad-CRC screens
```

Task 11 documentation sync: ADR-0001 records the External Firmware Slot / Internal Flash boundary and the removal of Metadata `activeSlot`. `PROJECT_CONTEXT.md`, `current_status.md`, this handoff and `review.md` now record `CLOSED / PASS`; S07/S09/S10 boundaries remain explicit. The review decision is code/architecture/regression/hardware PASS.

### Known Issues

- Full S07 OTA transfer, physical PA0 start/confirm, durable Metadata PENDING, READY reset, interrupted transfer, bad CRC, duplicate-key paths and LCD normal/failed screens have real-board evidence. Project Owner acceptance is complete.
- Task 1 is complete in commit `13d67efcecfa1e99b14eb0781e77aed3749bda2c`; Task 2 initial implementation is recorded in `b4a94245f09be9dac40ad8e3135302722504ca5b` and its layering/style correction is recorded in `6ac6a49756a87079f1bc2b95d190fc44d82f64c3`; Task 3 implementation is recorded in `bb0f96b643da137805b586eb817347a92dc0f140` and its Storage Host fault-injection correction in `7536cf06f6af539284dcaa4fc0396834ea7362fb`; Task 4 implementation is recorded in `29c1f35aa8d8a3330cd8a8280bcfcb08eda7b534` and XML minimal-diff correction in `e0b8b8f58145f2ca73132a0f5a8700b9d36f3cc2`; Task 5 implementation is recorded in `b9c619801b5913b25c52b1e57fe9cffbbb21d41b`.
- Factory baseline provisioning is complete for the tested device through the temporary board test in `23ac3db`; the test is intentionally not part of the production startup path. The configured build artifact still does not emit `OTA_APP.bin`, so repeatable pack steps must continue to use the documented temporary `fromelf` conversion until the project/toolkit artifact contract is separately addressed.
- Bootloader does not yet consume `PENDING`; S07 persistence testing therefore restarts the current OTA Application and inspects Metadata only.
- Task 5 Service is implemented in `b9c619801b5913b25c52b1e57fe9cffbbb21d41b`; Task 6 execution-shell refactor is committed in `ae264974de810272a83314c4c3fcddb4d9ef6991`; Task 7 display rendering is committed in `a8045d297442fe7b97bc318a37b53b05b4a4b968`.
- Task 8 safe Factory baseline board test and usage documentation are committed in `23ac3db`. The 2026-09-18 full applicable Host/Toolkit regression and final physical acceptance passed.
- The current device is left running at `READY_TO_INSTALL` after the duplicate-key pass with `pendingSlot=NONE` and `upgradeState=NONE`; no S09/S10 installation or consume path was executed.
- `service_ota_init()` intentionally only initializes the RAM Service object; Metadata is loaded when `service_ota_start()` is requested. The durable EEPROM result was verified by a temporary read-only board test, while Bootloader consumption remains outside S07.

### Review Focus

Future review should focus on:

```text
Metadata semantic correctness
Metadata V1/V2 compatibility
Metadata atomicity
confirmedSlot protection
OTA state-transition correctness
PENDING transaction boundary
KEY ISR/task boundary
FreeRTOS IRQ priority safety
production/test dependency separation
Header-last preservation
S06 runtime regression
power-loss behavior
```

### Task 0 Commit

```text
9adba52aa75ddaff906e42aa8bae433b654ae761
```

### Task 2 Correction Commit

```text
6ac6a49756a87079f1bc2b95d190fc44d82f64c3
```

### Task 3 Commits

```text
bb0f96b643da137805b586eb817347a92dc0f140
7536cf06f6af539284dcaa4fc0396834ea7362fb
```

### Task 4 Commits

```text
29c1f35aa8d8a3330cd8a8280bcfcb08eda7b534
e0b8b8f58145f2ca73132a0f5a8700b9d36f3cc2
```

### Task 5 Commit

```text
b9c619801b5913b25c52b1e57fe9cffbbb21d41b
```

### Task 6 Commits

```text
4e5a8e5
ae264974de810272a83314c4c3fcddb4d9ef6991
```

### Task 7 Commit

```text
a8045d297442fe7b97bc318a37b53b05b4a4b968
```

### Task 8 Commit

```text
23ac3db
```

### Task 10 Production Fix Commit

```text
2ab34f1
```

### Task 10 Physical Acceptance Documentation Commit

```text
ade703e0d72bdef4a26cabd2afe36ecb86a09fa6
```
