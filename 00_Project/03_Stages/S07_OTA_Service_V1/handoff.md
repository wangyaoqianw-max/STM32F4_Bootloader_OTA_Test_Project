# S07 OTA Service V1 Handoff

## Metadata

- Stage: `S07_OTA_Service_V1`
- Status: `IN_PROGRESS`
- Branch: `main`
- Baseline Commit: `84f07303d2b6fbf0682e492ad79e32982e2fb17b`
- Design Commit: `a5c4c1b890cf232e8e884d9ddb72473212892c13`
- Implementation Plan Commit: `3049034ea3472eb303aec50a196e785b4bd6b84c`
- Implementation Commit: `9adba52aa75ddaff906e42aa8bae433b654ae761`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Updated At: `2026-09-17`

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

- Status: `IN_PROGRESS`

### Completed Work

Task 0 completed the CubeMX regeneration recovery before S07 production implementation. S06 heap sizing, CmBacktrace HardFault ownership and FreeRTOS task introspection exports were restored; PA0 Falling EXTI configuration and S06 UART/DMA/LCD runtime baseline were verified.

### Changed Files

Current S07 implementation changes:

```text
00_Project/03_Stages/S07_OTA_Service_V1/design.md
00_Project/03_Stages/S07_OTA_Service_V1/implementation_plan.md
00_Project/03_Stages/S07_OTA_Service_V1/handoff.md
03_Firmware/Application/OTA_APP/Core/Inc/FreeRTOSConfig.h
03_Firmware/Application/OTA_APP/Core/Src/stm32f4xx_it.c
03_Firmware/Application/OTA_APP/Middlewares/Third_Party/FreeRTOS/Source/tasks.c
```

### Deviations From Plan

Task 0 found and recovered CubeMX regeneration regressions: `configTOTAL_HEAP_SIZE` had reverted from the S06 frozen `24576` to `15360`; generated C fault handlers had reintroduced a HardFault ownership conflict with `cmb_fault.S`; and the CmBacktrace FreeRTOS task introspection exports had been removed from `tasks.c`. No S07 design boundary was changed.

### Verification Results

Task 0 code verification: `05_Tools\\toolkit.bat build` PASS with 0 errors and 0 warnings; `git diff --check` PASS. Board baseline: `05_Tools\\toolkit.bat flash run` PASS and `05_Tools\\toolkit.bat rtt 5` PASS; RTT confirmed appSystem/otaWorker/displayTask startup, UART/YMODEM_READY and Display initialization. This is S06 baseline smoke only, not full S07 hardware acceptance.

### Known Issues

- Full S07 Metadata, Service, sink, key/IRQ, provisioning and board acceptance work remains pending.
- Factory Slot A provisioning capability has not yet been implemented; Task 8 determines whether existing Toolkit composition is sufficient or a thin `provision` workflow is warranted.
- Bootloader does not yet consume `PENDING`; S07 persistence testing therefore restarts the current OTA Application and inspects Metadata only.

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
