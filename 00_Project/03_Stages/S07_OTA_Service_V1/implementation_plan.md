# S07 OTA Service V1 Implementation Plan

## Metadata

- Stage: `S07_OTA_Service_V1`
- Design: `00_Project/03_Stages/S07_OTA_Service_V1/design.md`
- Baseline Commit: `84f07303d2b6fbf0682e492ad79e32982e2fb17b`
- Design Commit: `a5c4c1b890cf232e8e884d9ddb72473212892c13`
- Status: `IN_PROGRESS`
- Branch: `main`

## Global Constraints

- 直接在当前 `main` 分支实施，不创建新分支或 worktree。
- 开始前读取 `AGENTS.md`、`PROJECT_CONTEXT.md`、`00_Project/WORKFLOW.md`、`03_Firmware/AGENTS.md`、C 代码规范、S06 design/handoff/review/verification。
- 修改 Application 固件后优先使用 `05_Tools` 已有统一入口，不重新探测 Keil/J-Link/RTT/GDB 安装路径。
- S06 三线程 Runtime 和 IPC 作为冻结基线，不随意改线程拓扑。
- `otaWorker` 保留为 OTA 单一 RTOS execution shell；不新增 Key Task 或第二个 OTA Task。
- W25Q64 Slot A/B 是 External Firmware Image Slots，不是 CPU 可直接执行的 APP 分区。
- Ymodem 不感知 Slot / Metadata / `PENDING` / Reset。
- Firmware Image Header 继续采用 S04 64 Byte V1 格式，不在 S07 扩展镜像头。
- Metadata 继续使用 AT24C02 双 128 Byte Copy、sequence、CRC32、commit-marker-last 原子提交。
- S07 只产生可靠 `PENDING` 请求，不实现 Internal Flash installation、Trial、Confirm 或 Rollback execution。
- 不引入复杂 Manager Framework，不建立第二套 Toolkit、Ymodem 或 Firmware Storage。
- 每个 Task 独立验证并保持单一职责提交；提交前执行 `git diff --check`。
- 真实硬件 PASS 必须基于真实板证据，不能用 Build/Host Test 代替。

## Task 0: CubeMX Regeneration Recovery and S06 Baseline Guard

**Goal:** 在 S07 功能代码开始前确认最近 PA0 CubeMX Generate Code 没有破坏 S06 已验证 Runtime、FreeRTOS heap 或 CmBacktrace fault ownership。

**Files:**

- Inspect/Modify: `03_Firmware/Application/OTA_APP/OTA_APP.ioc` 或仓库实际 `.ioc`
- Inspect/Modify: `03_Firmware/Application/OTA_APP/Core/Src/gpio.c` 或实际 generated GPIO source
- Inspect/Modify: `03_Firmware/Application/OTA_APP/Core/Src/stm32f4xx_it.c`
- Inspect/Modify: `03_Firmware/Application/OTA_APP/FreeRTOSConfig.h` 或实际路径
- Inspect: `03_Firmware/Application/OTA_APP/05_Vendors/CmBacktrace/fault_handler/keil/cmb_fault.S`
- Verify: S06 UART DMA / SPI / FreeRTOS / LCD / CmBacktrace baseline

- [ ] Step 1: 核对 PA0/KEY_1 当前 CubeMX 配置，确认 `GPIO_PULLUP`、falling-edge EXTI、EXTI0 priority 和生成代码与 `.ioc` 一致。
- [ ] Step 2: 核对 `configTOTAL_HEAP_SIZE`，恢复/保持 S06 冻结值 `24576`；若仓库最终冻结值已不同，以 S06 正式文档/代码为准并在 handoff 说明。
- [ ] Step 3: 核对 HardFault ownership，保证 `cmb_fault.S` 是唯一正式 HardFault handler，移除/恢复 CubeMX 生成导致的重复 owner。
- [ ] Step 4: 对比 S06 关闭后的关键配置，确认 USART1 DMA circular、SPI、FreeRTOS、HAL timebase、Keil source/include 和 LCD 路径没有意外回归。
- [ ] Step 5: 运行 `05_Tools\toolkit.bat build`；如可操作真实板，再执行最小 flash/RTT baseline smoke。记录结果后提交本任务，并将 Commit 写入 `handoff.md`。

## Task 1: Platform MCU IRQ Abstraction

**Goal:** 将 USART、DMA、EXTI 共用的 NVIC 控制能力抽象为通用 `platform_mcu_irq`，并明确 FreeRTOS-safe priority 约束。

**Files:**

- Modify/Create: `03_Firmware/Application/OTA_APP/03_Platform/...` 下 MCU IRQ public interface，按现有目录命名约定落盘
- Modify/Create: `03_Firmware/Application/OTA_APP/04_Impl/...` 下 STM32/CMSIS IRQ implementation
- Test: 对应 Host/compile-time tests 或最小单元测试

**Interfaces:**

- Produces: IRQ enable / disable / set-priority / clear-pending 最小 API
- Consumes: CMSIS NVIC API

- [ ] Step 1: 调查现有 Platform MCU 能力、错误码、命名规范，确定最小 public type 和 API，不引入 callback manager。
- [ ] Step 2: 为已实际使用的 IRQ 建立枚举/映射，至少覆盖 KEY EXTI0 与当前 UART/DMA 需要；避免把 STM32 `IRQn_Type` 直接泄漏到上层。
- [ ] Step 3: 使用 CMSIS `NVIC_EnableIRQ` / `NVIC_DisableIRQ` / `NVIC_SetPriority` / `NVIC_ClearPendingIRQ` 完成 Impl。
- [ ] Step 4: 增加或固化 FreeRTOS priority guard/documentation，保证会调用 `...FromISR()` 的 IRQ 不违反 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`。
- [ ] Step 5: Build + 相关 Host Test，提交并记录 Commit。

## Task 2: Platform Key and KEY_1 Event Forwarding

**Goal:** 增加薄 Platform Key 能力，并将 PA0 EXTI 轻量事件转发给 `otaWorker`，不新增 Key Task。

**Files:**

- Create/Modify: Platform Key public interface
- Create/Modify: KEY_1 STM32/board implementation
- Modify: HAL EXTI callback integration point
- Modify: `app_ota_worker` public/private notification interface

**Interfaces:**

- Produces: generic KEY_1 press/event signal
- Consumes: EXTI callback + `xTaskNotifyFromISR()`

- [ ] Step 1: 按项目 Platform/Impl 现有风格定义最小 Key abstraction，只暴露 key/input event，不暴露 OTA 语义。
- [ ] Step 2: 将 PA0 Falling Edge 从 HAL callback 转发到 Key Impl/Platform 层；ISR 路径只识别来源并发送 lightweight notification。
- [ ] Step 3: 给 `otaWorker` 分配 KEY notification bit，与现有 UART RX notification bit 明确分离。
- [ ] Step 4: 在 `otaWorker` 任务上下文实现 30~50 ms debounce，确认 `RECEIVING/VERIFYING/COMMITTING` 中重复 KEY 不产生重入。
- [ ] Step 5: Build + IRQ/notification smoke，提交并记录 Commit。

## Task 3: Firmware Metadata V2

**Goal:** 删除业务语义错误的 `activeSlot`，新增 `pendingSlot` / `upgradeState`，保持双副本原子性并支持 V1 backward-compatible read。

**Files:**

- Modify: `03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_metadata.h`
- Modify: `03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_metadata.c`
- Modify: 所有使用 `firmware_metadata_t` 的 production/tests
- Test: Metadata encode/decode / migration / dual-copy / fault-injection tests

**Interfaces:**

- Produces: V2 `firmware_metadata_t`
- Preserves: 128 Byte copies, sequence, CRC32, commit marker last

- [ ] Step 1: 从当前源码确认 V1 精确 raw offsets、字段校验和 CRC body，不凭计划猜测布局。
- [ ] Step 2: 冻结 V2 raw layout，优先保留现有稳定 offset，从 reserved 区加入 `pendingSlot` / `upgradeState`；禁止直接 memcpy C struct 到 EEPROM。
- [ ] Step 3: 实现 V2 encode/decode 和 cross-field validation；稳定态 `pendingSlot=NONE`、`upgradeState=NONE`。
- [ ] Step 4: 实现 V1 decode compatibility：读取旧字段后默认 `pendingSlot=NONE`、`upgradeState=NONE`，不在 read 时主动迁移。
- [ ] Step 5: 修改 metadata commit，使下一次正常写入自然编码为 V2，并保证旧/new valid record 原子语义不变。
- [ ] Step 6: 更新所有 `activeSlot` 使用点，按真实语义改为 `confirmedSlot` / runtime internal state / 删除；禁止机械重命名掩盖语义差异。
- [ ] Step 7: 测试 V1/V2、reserved bytes、CRC、commit marker、sequence wrap/latest-copy、write failure old-or-new record；Build 通过后提交并记录 Commit。

## Task 4: Production OTA Firmware Sink

**Goal:** 建立生产 `ota_firmware_sink`，支持动态 External target Slot，并移除 production 对 S05 Board Test sink 的依赖。

**Files:**

- Create: production OTA sink source/header，具体路径遵循当前 Service/Firmware 模块布局
- Modify: production `app_ota_worker` / later `service_ota` wiring
- Preserve: `04_Test/Board/S05_UART_Ymodem/s05_ymodem_flash_sink.*` 作为历史/测试代码，不作为 production dependency

**Interfaces:**

- Consumes: `ymodem_sink` interface + Firmware Storage selected Slot
- Produces: begin/write/end/abort implementation with Header-last commit

- [ ] Step 1: 提取 S05 sink 已验证的 compact `.img` → Slot mapping 和 Header buffering 规则，不复制 Board Test 全局状态。
- [ ] Step 2: 增加 target Slot/session context，使同一 sink 可写 Slot A 或 B。
- [ ] Step 3: 保持 Header-last：接收前 64B Header、先写 Payload、完整 session 成功后才 commit Header。
- [ ] Step 4: 确认 abort/timeout/short-file/oversize/error 路径不提交 Header。
- [ ] Step 5: 迁移 production wiring，搜索并确保 production 不再 include/link `s05_ymodem_flash_sink`；Host Test + Build 后提交并记录 Commit。

## Task 5: service_ota State Machine and Persistence Rules

**Goal:** 增加正式 OTA Service，集中管理 target selection、runtime state、validation、Metadata 事务和 PENDING commit。

**Files:**

- Create: `service_ota` public/private source/header，路径按现有 `02_Service` 组织
- Modify: Firmware Storage/Metadata integration only where required by public APIs
- Test: Host state-machine tests

**Interfaces:**

- Runtime states: `IDLE / PREPARING / RECEIVING / VERIFYING / READY_TO_INSTALL / COMMITTING / REBOOT_REQUIRED / FAILED`
- Public behavior: start / process or poll / confirm install / query state/event
- Output: reset-required indication, not direct reset call

- [ ] Step 1: 定义 state/event/error public contract，避免把 FreeRTOS notification、HAL 或 display type 暴露给 Service。
- [ ] Step 2: 实现 start preconditions：只允许 stable metadata、无 pending lifecycle、confirmed baseline 合法时开始。
- [ ] Step 3: 实现 target selection：稳定态 `opposite(confirmedSlot)`；在任何 erase 前先 commit `targetSlot.state=INVALID`。
- [ ] Step 4: 连接 Ymodem receiver + production sink，进入 `RECEIVING`，并输出节流后的业务 progress event。
- [ ] Step 5: session 完成后进入 `VERIFYING`，调用 `firmware_storage_validate_image()`；失败保持 target INVALID 且不得创建 PENDING。
- [ ] Step 6: validate PASS 后 commit `targetSlot.state=VALID`，保持 `pendingSlot=NONE`、`upgradeState=NONE`，进入 `READY_TO_INSTALL`。
- [ ] Step 7: 实现 second-confirm transaction：进入 `COMMITTING`，写 `pendingSlot=target` + `upgradeState=PENDING`；commit 成功后进入 `REBOOT_REQUIRED`。
- [ ] Step 8: 为 power-loss/retry/invalid transition/duplicate key/metadata write failure 编写 Host Test，Build 后提交并记录 Commit。

## Task 6: Refactor otaWorker into RTOS Execution Shell

**Goal:** 在不破坏 S06 Runtime ownership 的前提下，将 `app_ota_worker` 从大业务实现收敛为 Service 驱动 shell。

**Files:**

- Modify: `app_ota_worker.c/.h` 实际路径
- Modify: appSystem wiring only as needed
- Modify: display event mapping

**Interfaces:**

- Consumes: UART notification, KEY notification, `service_ota`
- Produces: Display Queue events, Platform reset call

- [ ] Step 1: 保持 `otaWorker` 任务创建、priority 和 S06 owner contract 不变，移出 storage/sink/metadata business decisions。
- [ ] Step 2: 将 UART RX notification 转换为对 `service_ota`/Ymodem 的驱动，不改变 `service_uart` single-consumer ownership。
- [ ] Step 3: 将 KEY event 在任务上下文 debounce 后按 Service 当前状态映射为 `start` 或 `confirm`；其他状态忽略。
- [ ] Step 4: 将 Service state/business event 映射到现有 `app_display_event_t` / Display Queue，保持 `displayTask` sole LCD owner。
- [ ] Step 5: Service 输出 `REBOOT_REQUIRED` 后由 worker 调用 Platform MCU Reset abstraction；Service 自身不得直接 reset。
- [ ] Step 6: 检查 `app_ota_worker.c` 依赖明显收敛，Build + S06 runtime regression 后提交并记录 Commit。

## Task 7: Display and User Interaction Completion

**Goal:** 用现有 LCD/Display Queue 完成 S07 双确认用户流程，不引入 LVGL。

**Files:**

- Modify: Display Model / OTA display event mapping / displayTask rendering files

- [ ] Step 1: 保留 S06 IDLE/RECEIVING/VERIFYING/SUCCESS/FAILED 基础能力，并根据新 Service state 重新命名/映射需要的显示状态。
- [ ] Step 2: 增加 `READY_TO_INSTALL` 显示：version、target Slot、verified、`Press KEY to install/reboot`。
- [ ] Step 3: 增加 `REBOOT_REQUIRED` 短暂状态或日志提示，避免在 Metadata commit 前显示“will install”。
- [ ] Step 4: 检查 progress throttle 仍满足 S06 5% 或 200ms 约束或当前冻结等价值，避免显示队列洪泛。
- [ ] Step 5: Build + display model tests/board smoke 后提交并记录 Commit。

## Task 8: Factory / Initial Provisioning Support

**Goal:** 为第一次 OTA 建立明确 confirmed rollback baseline，不实现 MCU Self-Provisioning。

**Files:**

- Modify/Create: `05_Tools` Workflow/Router/Project Test only if现有工具不能完成一致性预置
- Modify/Create: 最小 firmware/metadata provisioning helper or board test if required
- Document: provisioning usage

**Target State:**

```text
Internal Flash = v1.0 APP
Slot A         = v1.0 VALID image
Slot B         = EMPTY
confirmedSlot  = A
confirmedVersion = 1.0.0
pendingSlot    = NONE
upgradeState   = NONE
```

- [ ] Step 1: 调查现有 `toolkit firmware pack`、`toolkit ymodem`、Firmware Storage test 能否组合完成 provisioning，优先复用而不是新建命令。
- [ ] Step 2: 如确有必要新增 `toolkit provision`，仅作为现有 Core/Adapter/Workflow 的组合 workflow，不复制 J-Link/Serial/Firmware 实现。
- [ ] Step 3: 建立 Metadata V2 baseline 初始化方式，并防止对非空/未知设备状态无条件破坏性初始化。
- [ ] Step 4: 验证 Slot A header/payload、版本、CRC、Metadata confirmed baseline；记录可重复操作步骤。
- [ ] Step 5: Host/Toolkit regression 后提交并记录 Commit。

## Task 9: Host, Static, Build Regression

**Goal:** 在上板前证明 S07 各模块接口、状态机和持久化规则在 Host/Build 层闭环。

**Files:**

- Modify/Create: relevant Host Tests / project tests
- Verify: production build

- [ ] Step 1: 执行 Metadata V1/V2、dual-copy fault injection、OTA state machine、sink、target selection、key semantic tests。
- [ ] Step 2: 执行现有 S04/S05/S05B/S06 相关 Host/Toolkit regression，确认没有破坏 Ymodem/Firmware/Toolkit 合同。
- [ ] Step 3: 执行 `05_Tools\toolkit.bat build`，记录 exit code 和 Build log。
- [ ] Step 4: 执行 `git diff --check` 并检查 production source 不依赖 Board Test sink。
- [ ] Step 5: 修复失败项，全部重新运行后提交测试/修复 Commit，并记录到 `handoff.md`。

## Task 10: Real-board Acceptance

**Goal:** 使用已有 Build/Flash/RTT/Ymodem/GDB/Logic Analyzer 工具验证完整 S07 业务链路和失败路径。

**Verify with existing toolkit:**

```text
05_Tools\toolkit.bat build
05_Tools\toolkit.bat flash run
05_Tools\toolkit.bat rtt ...
05_Tools\toolkit.bat ymodem ...
05_Tools\toolkit.bat snapshot halt|resume
05_Tools\toolkit.bat logic ...   # only when waveform evidence is useful
```

- [ ] Step 1: 完成 Factory baseline，确认 Internal v1.0 + Slot A v1.0 VALID + Metadata confirmed A。
- [ ] Step 2: Reset 后确认正常 foreground Application、LCD、RTT 和任务运行无 S06 回归。
- [ ] Step 3: 按 KEY_1，确认进入 OTA READY / Ymodem handshake；Sender 等 `'C'` 后传输新 `.img`。
- [ ] Step 4: 确认 foreground behavior 持续，LCD RECEIVING progress → VERIFYING → READY_TO_INSTALL，target 为 non-confirmed Slot。
- [ ] Step 5: READY_TO_INSTALL 前复位一次，确认新 image 可保持 VALID 但 Metadata 无 PENDING，旧 APP 正常继续。
- [ ] Step 6: 再次完成 download，按第二次 KEY；确认 PENDING atomic commit 后才 reset。
- [ ] Step 7: Reset 后 OTA_APP startup diagnostics 读取相同 `pendingSlot` / `upgradeState=PENDING` / valid sequence / CRC / commit marker。
- [ ] Step 8: 验证 interrupted transfer：target INVALID、new Header 未提交、confirmed image untouched、foreground unaffected。
- [ ] Step 9: 验证 bad CRC / invalid image：不得进入 READY_TO_INSTALL，不得写 PENDING。
- [ ] Step 10: RECEIVING / VERIFYING / COMMITTING 中重复按 KEY，确认被忽略且 session 不损坏。
- [ ] Step 11: 如需要确认 SPI/I2C 实际事务，复用 `toolkit logic` 采集，不建立临时平行工具。
- [ ] Step 12: 将完整硬件证据写入 `04_Test/Reports/Stages/S07_OTA_Service_V1/verification.md`，提交验证证据并记录 Commit。

## Task 11: Documentation, Handoff and Review Readiness

**Goal:** 将真实实现、偏差、验证证据和后续阶段接口同步回正式工程上下文。

**Files:**

- Modify: `00_Project/03_Stages/S07_OTA_Service_V1/handoff.md`
- Modify: `PROJECT_CONTEXT.md`
- Modify: `00_Project/05_Status/current_status.md`
- Modify: Roadmap/status files as required by current repository workflow
- Create/Modify: cross-stage ADR in `00_Project/04_Decisions/` if not already recorded
- Later Review: `00_Project/03_Stages/S07_OTA_Service_V1/review.md`

- [ ] Step 1: 在 ADR/长期决策中记录：External A/B 是 Firmware Image Slots、Internal Flash 是执行区、Metadata 删除 `activeSlot`。
- [ ] Step 2: 更新 handoff 的 Completed Work、Changed Files、真实 Commit、偏差、验证结果、Known Issues 和 Review Focus。
- [ ] Step 3: 更新 PROJECT_CONTEXT/current_status 到真实工作流状态；没有真实硬件 PASS 时不得声明 `CLOSED / PASS`。
- [ ] Step 4: 明确 S08/S09/S10 的交接边界：S07 已产生 durable PENDING，但 Installation / Trial / Confirm / Rollback execution 尚未实现。
- [ ] Step 5: `git diff --check`、确认工作区只含预期正式变更，提交文档并推送 `main`。

## Final Verification

最终 Verification 必须逐项给出代码验证与硬件验证状态：

```text
Code Verification
- Metadata V1/V2 tests
- Dual-copy/fault-injection tests
- OTA state-machine tests
- Dynamic sink/Header-last tests
- Platform IRQ/Key compile/tests
- Existing regression
- Keil Build

Hardware Verification
- KEY_1 start
- Ymodem receive
- inactive external slot
- image validation
- READY_TO_INSTALL
- second KEY confirm
- PENDING commit
- reset persistence
- interrupted transfer
- CRC failure
- key-during-transfer
- S06 foreground/display/runtime regression
```

S07 PASS 链路：

```text
KEY_1
→ OTA READY
→ Ymodem Receive
→ Inactive External Slot
→ Image Validation PASS
→ READY_TO_INSTALL
→ KEY_1
→ pendingSlot = target
→ upgradeState = PENDING
→ Metadata atomic commit
→ Reset
→ Restart
→ PENDING persistence confirmed
```

明确不作为 S07 PASS 条件：

```text
Internal Flash Installation = NOT_IMPLEMENTED
Trial Boot                  = NOT_IMPLEMENTED
Confirm                     = NOT_IMPLEMENTED
Rollback Execution          = NOT_IMPLEMENTED
```

## Completion Condition

所有计划任务完成、Host/Build/真实板验证证据落盘，`handoff.md` 记录真实 Commit 和未决项，并按仓库工作流将阶段推进到 `READY_FOR_REVIEW`。只有 Review 通过且 Project Owner 确认真实硬件验收后，才允许进入 `CLOSED / PASS`。
