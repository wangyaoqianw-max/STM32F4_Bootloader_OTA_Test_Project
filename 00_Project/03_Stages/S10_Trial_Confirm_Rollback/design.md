# S10 Trial Confirm Rollback Design

## Metadata

- Stage: `S10_Trial_Confirm_Rollback`
- Workflow Status: `DRAFT`
- Branch: `main`
- Baseline Commit: `de17162c179f3a6551c9edca0d5df35c03ffdc48`
- Design Owner: Project Owner
- Updated At: `2026-09-19`
- Design Review: `PASS_WITH_AMENDMENTS / READY_FOR_OWNER_APPROVAL`

## 1. Goal

在 S09 已完成的 Firmware Installation 和 `PENDING → TRIAL` 原子提交基础上，建立第一版完整的固件试运行、运行确认、看门狗恢复和自动回滚闭环。

S10 主链冻结为：

```text
Application OTA
→ Metadata PENDING
→ Reset

Bootloader
→ Validate / Install pendingSlot
→ Internal CRC / Vector PASS
→ Atomic PENDING → TRIAL
→ Jump Trial APP

Trial Application
→ Startup Health PASS
→ Runtime Ready PASS
→ Observation Window PASS
→ Atomic firmware_confirm()
→ Stable NONE

or

Trial Application
→ Reset before Confirm
→ Bootloader sees TRIAL
→ Validate confirmedSlot
→ Atomic TRIAL → ROLLBACK
→ Reinstall confirmedSlot
→ Verify Internal APP
→ Atomic ROLLBACK → NONE
→ Jump recovered APP
```

S10 的核心目标不是增加更多状态，而是建立以下可靠性原则：

1. 新 Firmware 在 Confirm 前始终不可信；
2. Confirm 必须由 Trial Application 主动完成；
3. Trial 未 Confirm 前再次进入 Bootloader，默认 Trial 失败；
4. Rollback 必须由 Bootloader 控制；
5. 任何 destructive operation 前必须有可恢复的持久化状态；
6. Watchdog 负责让失控系统重新进入 Bootloader，不负责判断 Firmware 是否可信。

## 2. Upstream Contracts

### 2.1 S09 Installation Contract

S09 已冻结并实现：

```text
PENDING
→ Candidate pre-validation
→ destructive gate
→ erase Internal APP
→ install pendingSlot
→ Internal CRC/vector verify
→ atomic PENDING → TRIAL
→ jump APP
```

S10 不重写 S09 安装主链。

TRIAL 的固定语义：

> Candidate 已完整安装并通过静态验证，但尚未通过运行确认。

TRIAL 不表示“正在安装”。

### 2.2 Firmware / Metadata Contract

继续沿用 Metadata V2：

```text
NONE
PENDING
TRIAL
ROLLBACK
```

当前 V2 已包含：

```text
sequence
confirmedSlot
pendingSlot
slotAState
slotBState
upgradeState
confirmedVersion
```

S10 第一版不新增 `CONFIRMED` 枚举。

稳定已确认状态使用：

```text
upgradeState     = NONE
confirmedSlot    = known-good slot
confirmedVersion = known-good version
pendingSlot      = NONE
```

`CONFIRMED` 是业务语义和事务结果，不是新的持久化 lifecycle state。

### 2.3 External A/B + Single Internal APP

本项目与 MCUboot 常见 Primary/Secondary Internal Slot 不同：

```text
External W25Q64
├─ Slot A
└─ Slot B

Internal Flash
└─ Single APP execution region
```

External `confirmedSlot` 是 Known-Good Image 的持久来源。

Rollback 不做 Internal Slot swap，而是：

```text
confirmedSlot
→ read-only source
→ reinstall Internal APP
```

## 3. Mature Project References

S10 参考成熟项目的设计原则，不直接复制其实现。

### 3.1 MCUboot

参考：

- Test Image / Confirm / Revert 生命周期；
- Test Image 未 Confirm，再次启动执行 revert；
- 状态必须在 destructive operation 前具备可恢复语义；
- Boot Policy 与 Flash Backend 分离。

本项目不引入 MCUboot 的 swap/scratch/trailer/TLV。

参考入口：

```text
https://github.com/mcu-tools/mcuboot
https://github.com/STMicroelectronics/stm32-mw-mcuboot
```

### 3.2 wolfBoot

参考：

- UPDATE / TESTING / SUCCESS；
- Application 主动调用 success/confirm；
- Testing 未确认后 rollback；
- update / rollback 共用相同底层复制和恢复能力；
- interrupted update / rollback 必须能够恢复。

参考入口：

```text
https://github.com/wolfSSL/wolfBoot
```

### 3.3 ESP-IDF OTA

参考：

- `PENDING_VERIFY`；
- Application-Controlled Confirmation；
- diagnostics PASS 后 mark valid；
- Confirm 前 reset / crash / power loss 触发 rollback；
- Boot Watchdog 与 Runtime Task Watchdog 分阶段管理。

本项目只借鉴状态语义和健康确认思想，不复制 ESP32 分区实现。

## 4. Lifecycle State Machine

### 4.1 Stable State

```text
NONE
→ Internal APP is expected to be confirmed firmware
→ confirmedSlot / confirmedVersion are authoritative baseline
→ pendingSlot = NONE
```

正常 Reset / POR：

```text
NONE
→ Validate Internal APP vector
→ Jump APP
```

### 4.2 Pending

沿用 S09：

```text
PENDING
→ Validate pendingSlot
→ Install
→ Verify
→ Atomic TRIAL
→ Jump
```

### 4.3 Trial

首次 Trial Boot 发生在 S09 同一次 Bootloader invocation 中：

```text
PENDING
→ install
→ commit TRIAL
→ direct jump Trial APP
```

因此新的 Reset 后 Bootloader 再次读取到：

```text
upgradeState == TRIAL
```

即可解释为：

> 上一次 Trial Application 没有成功完成 Confirm。

S10 V1 不采用“失败 N 次后再回滚”。

冻结策略：

```text
TRIAL + any reset before Confirm
→ rollback
```

Reset Cause 只用于 Diagnostics，不决定是否回滚。

### 4.4 Rollback

```text
TRIAL
→ prevalidate confirmedSlot
→ atomic TRIAL → ROLLBACK
→ reinstall confirmedSlot
→ Internal CRC/vector verify
→ atomic ROLLBACK → NONE
→ jump recovered APP
```

ROLLBACK 是持久化恢复事务，不是瞬时 RAM 状态。

### 4.5 State Overview

```text
                 OTA
                  │
                  ▼
               PENDING
                  │
               Install
                  │
                  ▼
                TRIAL
          ┌───────┴────────┐
          │                │
    Health PASS       Reset before Confirm
          │                │
          ▼                ▼
       Confirm          ROLLBACK
          │                │
          ▼          Restore confirmed
         NONE               │
                            ▼
                           NONE
```

## 5. Application Watchdog Capability

### 5.1 Architecture

Watchdog 是 MCU Platform Capability，不属于 OTA Service。

Application 继续遵守：

```text
App / Service
    ↓
Platform
    ↓
Impl
    ↓
STM32 HAL / CMSIS
```

建议能力：

```c
platform_error_t platform_watchdog_start(uint32_t timeoutMs);
platform_error_t platform_watchdog_feed(void);
```

第一版不提供 stop/deinit，因为 STM32 IWDG 一旦启动不能在正常运行中关闭。

Platform / Impl 只提供机制：

```text
start
feed
debug freeze configuration
```

Watchdog Feed Policy 必须由 Application 决定，Platform 不得感知 OTA、Task、TRIAL 或 Startup State。

### 5.2 No CubeMX Regeneration

S10 IWDG 不要求通过 CubeMX 打开。

采用代码初始化：

```text
Platform API
→ STM32 Impl
→ HAL_IWDG_Init / HAL_IWDG_Refresh
```

不得为了 IWDG 重新生成 CubeMX 工程并引入 heap、CmBacktrace、Task Stack 等无关回归。

Design Review 已确认当前仓库：

```text
HAL_IWDG_MODULE_ENABLED      currently disabled
stm32f4xx_hal_iwdg.c         exists in Vendor
Keil OTA_APP.uvprojx         currently does not include IWDG source
```

因此“不使用 CubeMX”仍然可行，但 Implementation Plan 必须显式处理 HAL IWDG 的工程接入。第一版优先沿用现有 HAL 风格：

```text
enable HAL IWDG module manually
+ add stm32f4xx_hal_iwdg.c to Application build
+ Platform/Impl wraps HAL_IWDG_Init / HAL_IWDG_Refresh
```

不得假定当前工程已经具备 IWDG HAL 链接能力。

### 5.3 Start Point

Application 每次启动都启用 IWDG，不只在 TRIAL 时启用。

推荐启动位置：

```text
HAL_Init()
↓
Watchdog START
↓
SystemClock_Config()
↓
configured peripheral init
↓
osKernelInitialize()
↓
MX_FREERTOS_Init()
↓
osKernelStart()
```

理由：

- IWDG 使用独立 LSI，不依赖 System Clock；
- 放在 `SystemClock_Config()` 前可以覆盖 Clock Init 卡死或进入 `Error_Handler()` 的场景；
- `HAL_Init()` 已建立 HAL 基础时基，可作为第一版代码初始化 IWDG 的安全边界。

第一版仍不覆盖：

```text
Reset_Handler
SystemInit
HAL_Init itself
```

这部分属于未来 Bootloader→Application Watchdog Handover / Earlier Boot Watchdog 增强，不作为 S10 阻塞项。

实际调用应放在 CubeMX USER CODE 安全区域或 App Early Startup 编排中，禁止直接污染 Vendor 生成区。

Watchdog 是系统基础可靠性机制：

```text
Watchdog
≠ OTA-only feature
```

TRIAL 只额外使用 Watchdog Reset 来形成 rollback 闭环。

### 5.4 Timeout

第一版冻结为约：

```text
IWDG Timeout ≈ 10 s
```

STM32 IWDG 使用 LSI，因此该值按 Prescaler / Reload 计算为近似时间，不要求精确 10.000 s。

当前 Startup Barrier：

```text
APP_SYSTEM_STARTUP_TIMEOUT_MS = 5000 ms
```

因此 IWDG timeout 必须明显大于单次正常 Startup Barrier 合法等待路径。

后续可根据真实启动最坏耗时收紧到更短值，S10 不以“越短越安全”为目标。

## 6. Watchdog Feed Ownership

### 6.1 Pre-RTOS

Pre-RTOS 不机械地在每个 `MX_xxx_Init()` 后 Feed。

只使用少量有意义的 checkpoint：

```text
Watchdog START
↓
MCU / peripheral startup progress
↓
Checkpoint Feed
↓
RTOS objects/tasks ready
↓
Checkpoint Feed
↓
Scheduler Start
```

Checkpoint 数量与位置在 Implementation Plan 中基于真实调用链确认。

原则：

> Feed 表示系统取得了新的有效启动进展，不是为了避免 timeout 而无条件刷新。

### 6.2 Startup Phase

复用现有 S07A Startup Contract：

```text
MAIN_DONE
OTA_DONE
DISPLAY_DONE
→ RUNNING / DEGRADED / FAILED
```

现有组件身份不重新抽象。

不新增重复的：

```c
app_health_component_t
```

Runtime Health 继续使用已有 MAIN / OTA / DISPLAY 组件划分。

Startup 期间允许基于真实初始化进展进行受控 Feed，但不得把 Watchdog Policy 隐藏进：

```text
app_startup_report_main()
app_startup_report_ota()
app_startup_report_display()
```

`app_startup` 继续只负责启动状态协调。

### 6.3 Runtime Long-term Owner

S10 V1 长期 IWDG Feed Ownership 交给当前主工作线程：

```text
appMainTask
```

当前演示 Firmware 中 `appMainTask` 承担 LED 周期业务，因此采用：

```text
complete one normal work cycle
→ feed IWDG
```

而不是创建独立高优先级 Watchdog Task 无脑喂狗。

冻结限制：

> S10 V1 只证明主工作线程和系统整体失控可以被 Watchdog 检测，不宣称已完成多任务 Heartbeat Health Manager。

例如未来：

```text
otaWorker deadlock
displayTask deadlock
appMainTask still alive
```

可能仍会持续 Feed。多任务持续健康监控可在后续阶段增强，不在 S10 强制实现。

## 7. Debug Freeze

现有 GDB/J-Link 自动化必须保持可用。

调试时策略：

```text
Normal runtime
→ IWDG counts normally

Debugger Halt
→ DBGMCU freezes IWDG

Debugger Continue
→ IWDG resumes counting
```

不得通过 Debug Task 不断 Feed 来掩盖 breakpoint timeout。

Debug Freeze 属于 STM32 Impl / Debug 配置能力，Application 不感知 J-Link 或 GDB。

S10 验证必须确认：

```text
Breakpoint > IWDG timeout
→ no unexpected watchdog reset
```

并保持 S05A 的 GDB exit contract 不变。

## 8. Trial Health Model

S10 不采用“启动后 delay N 秒直接 Confirm”。

Trial Health 分为三层。

### 8.1 Startup Health

必须满足：

```text
app_startup_components_succeeded() == TRUE
AND
systemState == APP_SYSTEM_STATE_RUNNING
```

特别注意当前 S07A：

```text
RUNNING  → SYSTEM_RUN
DEGRADED → SYSTEM_RUN
FAILED   → SYSTEM_ABORT
```

因此“`app_startup_wait_for_decision()` 正常返回”不等价于允许 Confirm。

TRIAL 下只有 `RUNNING` 可以进入下一阶段。

TRIAL + DEGRADED：

```text
Trial Health FAIL
→ must not Confirm
→ controlled reset or stop valid Feed
→ Bootloader sees TRIAL
→ Rollback
```

稳定 `NONE` Firmware 可以继续保留现有 DEGRADED 运行语义。

### 8.2 Runtime Ready

同一批现有组件需要证明“Task 不只是创建成功，而是已经真正进入正常运行”。

冻结第一版条件：

```text
appMainTask
→ after SYSTEM_RUN, completes at least one normal foreground work cycle

otaWorker
→ after SYSTEM_RUN, reaches its normal command-wait / blocked loop

displayTask
→ after SYSTEM_RUN, reaches its normal display-queue wait / blocked loop
```

不要要求 OTA Worker 实际完成一次 OTA，因为无升级请求时 Blocking 是正常健康状态。

注意：当前 displayTask 的 initial render 已在 Startup local init 阶段完成，因此“完成第一次 initial render”不能作为新的 Runtime Ready 证据；Runtime Ready 必须证明 Task 已经跨过 SYSTEM_RUN 并进入长期运行循环。

第一版增加轻量 `app_health` 状态模块，但不重新定义组件 enum。

建议语义：

```text
MAIN_RUNTIME_READY
OTA_RUNTIME_READY
DISPLAY_RUNTIME_READY
→ RUNTIME_HEALTH_READY
```

### 8.2.1 Runtime Ready Deadline

Design Review 增加硬性边界：

```text
TRIAL_RUNTIME_READY_TIMEOUT = 5 s
```

原因：如果 RUNNING 已发布，而 MAIN / OTA / DISPLAY 中某个 Task 永远没有进入 Runtime Ready，系统不能因为 appMainTask 仍在运行并 Feed IWDG 而永久停留在 TRIAL。

冻结行为：

```text
RUNNING
↓
start Runtime Ready deadline
├─ all Ready within 5 s → Observation
└─ timeout / explicit health failure
      → Trial Health FAIL
      → controlled reset when possible
      → otherwise stop valid Feed
      → Bootloader sees TRIAL
      → Rollback
```

IWDG 仍是 Hang/Fatal 的最终恢复手段；对于已经被软件明确检测到的 Health Failure，允许直接请求 MCU Reset，避免无意义等待完整 watchdog timeout。

### 8.3 Observation Window

当且仅当：

```text
Startup == RUNNING
+
all Runtime Ready
```

后开始 Trial Observation Window。

第一版冻结：

```text
Observation Window = 5 s
```

该 5 s 是附加观察期，不是唯一健康依据。

期间：

```text
no fatal error
no reset
Watchdog is still serviced by valid health progress
```

窗口通过后才允许发起 `firmware_confirm()`。

### 8.4 Feed Policy Across Trial Health

appMainTask 是实际长期 Feed 执行者，但 Feed 权限由 Health State 决定：

```text
STARTUP / WAIT_RUNTIME_READY
→ only feed while health progress is still valid and deadline not expired

OBSERVING
→ feed after valid main work cycles

CONFIRMING
→ appMainTask does not unconditionally feed
→ confirmation transaction must complete within the watchdog safety window

STABLE / NONE
→ normal long-term appMainTask feed

FAILED
→ no valid feed; request reset when possible
```

该规则避免以下死角：

```text
otaWorker/displayTask not ready
+
appMainTask keeps feeding forever
→ permanent TRIAL
```

也避免 strict Confirm 自身若异常卡死时被另一个正常循环永久喂狗掩盖。

## 9. Firmware Lifecycle Service

Runtime Confirmation 不放进现有 `service_ota`。

现有 `service_ota_confirm_install()` 表示：

> 用户确认把已经下载好的 Candidate 提交为 PENDING 并请求安装。

这与 Trial Runtime Confirm 是不同语义。

S10 建议在现有：

```text
02_Service/service_firmware/
```

下增加轻量 Firmware Lifecycle 能力，例如：

```text
firmware_lifecycle
```

职责：

```text
load current Metadata
identify NORMAL / TRIAL boot semantics
strict firmware_confirm transaction
```

它不负责 Watchdog 策略，不负责 Task Health，也不负责 Firmware Download。

### 9.1 Storage Ownership and Confirm Execution Context

Design Review 确认当前 Firmware Storage 真实资源：

```text
W25Q64 / Soft-I2C / AT24C02 / firmware_storage_t
→ currently private to app_ota_runtime
→ accessed in otaWorker context
```

S10 不允许 appMainTask/app_health 直接取得该私有 Storage 指针并并发访问 W25Q64/EEPROM。

冻结原则：

```text
app_health
→ decides WHEN confirmation is allowed
→ requests confirmation

otaWorker
→ remains Application-side Firmware Storage execution owner
→ executes strict firmware lifecycle transaction in its own task context

firmware_lifecycle
→ provides confirmation rules/transaction logic
→ does not depend on app_ota_runtime
```

Application Runtime 可以提供窄接口把现有 `g_otaFirmwareStorage` 传给 `firmware_lifecycle`，但不得把 Storage Raw Driver ownership 暴露给多个 Task。

这样保持：

```text
Health policy ownership ≠ Storage I/O ownership
```

并避免为了 S10 给 Firmware Storage 引入无计划的多任务 mutex / 并发访问模型。

## 10. Strict firmware_confirm Transaction

### 10.1 Preconditions

`firmware_confirm()` 只能在 Application Health Policy 明确授权后调用。

事务开始后仍必须自行重新校验持久化事实：

```text
Metadata latest copy valid
upgradeState == TRIAL
confirmedSlot is A/B
pendingSlot is A/B
pendingSlot != confirmedSlot
pending slot state == VALID
pending image Header valid
pending imageSize <= Internal APP capacity
pending Payload CRC valid
pending image version readable
```

不得只相信启动阶段缓存的 Metadata。

由于成功 Confirm 后 `pendingSlot` 会立即成为新的 `confirmedSlot` 和未来 Rollback Source，因此 strict Confirm 必须调用完整只读 Image Validation（Header + Payload CRC），而不是只读取 Header/Version 后直接提升为 Known-Good。

### 10.2 Confirm Result

假设：

```text
confirmedSlot    = A
confirmedVersion = 1.0.0
pendingSlot      = B
upgradeState     = TRIAL
Slot B Header    = 1.1.0
```

成功 Confirm 构造：

```text
confirmedSlot    = B
confirmedVersion = 1.1.0
pendingSlot      = NONE
upgradeState     = NONE
```

Slot A/B state 第一版保持其镜像完整性语义，不因 Confirm 自动清除旧 Slot。

### 10.3 Atomic Commit

复用当前 Firmware Storage 双副本事务：

```text
load latest
→ build target Metadata
→ sequence + 1
→ choose opposite copy
→ invalidate target marker
→ write body + CRC
→ read-back verify
→ write COMMIT marker LAST
→ read-back marker
→ reload latest
→ verify final fields
```

Confirm 的合法掉电结果只有：

```text
Before valid final commit
→ authoritative state remains TRIAL
→ next boot rolls back

After valid final commit
→ authoritative state is NONE with new confirmedSlot
→ new firmware is stable
```

不得出现半 Confirm。

## 11. Bootloader Rollback Decision

### 11.1 Any Unconfirmed Trial Reset

S10 V1 不依赖 IWDG Reset 才回滚。

Bootloader 只需要判断：

```text
loaded Metadata == TRIAL
```

即可得出：

> previous Trial did not Confirm.

因此：

```text
IWDG Reset
Software Reset
POR / BOR
Power Cycle
Fault-triggered Reset
other reset
```

在 Metadata 仍为 TRIAL 时均进入 Rollback path。

Reset Cause 仅用于诊断日志，不作为 rollback gate。

### 11.2 No Failure Threshold

S10 V1 不使用：

```text
trialFailureCount >= N
```

作为是否回滚的条件。

冻结为：

```text
one Trial opportunity
→ no Confirm before next boot
→ immediate rollback
```

Failure Counter 如后续需要，可作为 Diagnostics 数据，不改变 S10 Reliability Decision。

因此 S10 第一版无需为了主链升级 Metadata V3。

## 12. Rollback Pre-validation

Rollback 的 destructive gate 前必须先只读验证 `confirmedSlot`。

顺序冻结：

```text
Bootloader sees TRIAL
↓
load confirmedSlot / confirmedVersion
↓
validate confirmed image
↓
PASS
↓
atomic TRIAL → ROLLBACK
↓
====== destructive gate ======
↓
erase Internal APP
```

Confirmed Image Gate：

```text
confirmedSlot is A/B
pendingSlot is A/B
pendingSlot != confirmedSlot
confirmed slot state == VALID
Header valid
Header.version == confirmedVersion
imageSize valid for Internal APP
Payload CRC PASS
Vector PASS
```

如果 Known-Good Image 校验失败：

```text
do not erase Internal APP
do not claim rollback success
log explicit fatal/recovery condition
```

S10 V1 可进入 controlled halt / recovery-required 状态，不在本阶段新增完整 Recovery UI/Protocol。

## 13. Installer Refactor

当前 S09：

```text
boot_prevalidate_candidate()
→ requires PENDING
→ selects pendingSlot

boot_installer_run()
→ internally calls candidate prevalidation
→ installs candidate
```

S10 必须复用同一安装核心，而不是复制一套 rollback installer。

### 13.1 A/B Recovery Invariant

S10 Rollback 成立的前提是 Candidate 不得覆盖当前 Known-Good Slot。

虽然现有 OTA Service 正常路径已经选择 inactive Slot，Bootloader 仍必须在 destructive install / recovery gate 处独立验证：

```text
confirmedSlot is A/B
pendingSlot is A/B
pendingSlot != confirmedSlot
confirmed slot state == VALID
pending slot state == VALID
```

若 `pendingSlot == confirmedSlot`，必须在任何 Internal APP erase 前拒绝 PENDING/TRIAL 流程，因为此时系统已经失去独立的 rollback source。

该检查不改变 Metadata V2 Binary Format，只增强跨字段安全约束。

### 13.2 Prevalidate Split

建议抽取通用只读镜像验证核心：

```text
boot_prevalidate_image_slot()
→ slot state
→ Header
→ size
→ vector
→ payload CRC
```

上层安全入口：

```text
boot_prevalidate_candidate()
→ require PENDING
→ source = pendingSlot
→ common image validation

boot_prevalidate_confirmed()
→ require TRIAL / ROLLBACK recovery path
→ source = confirmedSlot
→ Header.version == confirmedVersion
→ common image validation
```

### 13.3 Installer Split

建议保留安全入口：

```text
install pending
restore confirmed
```

二者共用 private installation core：

```text
validated external image
→ erase Internal APP
→ chunk copy
→ local read-back
→ whole-image Internal CRC
→ Internal vector verify
```

不建议公开一个“任意 Slot 都可以直接擦 APP”的宽泛 API。

## 14. Rollback Metadata Transactions

S10 扩展现有 `boot_metadata_commit`，不建立第二套 Metadata Writer。

建议语义：

```text
boot_metadata_commit_trial()
boot_metadata_commit_rollback_begin()
boot_metadata_commit_rollback_complete()
```

三者内部复用统一 atomic commit core。

### 14.1 TRIAL → ROLLBACK

在 confirmed image pre-validation PASS 后、Internal erase 前提交：

```text
sequence++
upgradeState = ROLLBACK
```

保持：

```text
confirmedSlot
confirmedVersion
pendingSlot
slot states
```

不变。

### 14.2 ROLLBACK → NONE

只有当 confirmed image 已重新安装且 Internal CRC/vector PASS 后：

```text
sequence++
pendingSlot  = NONE
upgradeState = NONE
```

保持：

```text
confirmedSlot
confirmedVersion
```

不变。

因为恢复后的 Internal APP 本来就是原来的 Known-Good Firmware。

## 15. Rollback Power-loss Strategy

S10 不引入 MCUboot-style sector-level resume。

利用 External `confirmedSlot` immutable source：

```text
Metadata = ROLLBACK
External confirmed image = durable source
Internal APP = disposable recovery target
```

采用 restart-from-zero：

```text
ROLLBACK
→ prevalidate confirmed image
→ erase Internal APP
→ reinstall from beginning
```

掉电场景：

```text
confirmed image validation during TRIAL
→ still TRIAL
→ next boot decides rollback again

before ROLLBACK commit
→ still TRIAL

after ROLLBACK commit
→ next boot sees ROLLBACK

erase/program/CRC during rollback
→ state remains ROLLBACK
→ next boot restarts full restore

after internal verify, before NONE commit
→ still ROLLBACK
→ duplicate reinstall is allowed and safe

after valid ROLLBACK → NONE commit
→ stable confirmed state
```

第一版优先：

```text
correct
recoverable
simple
auditable
```

不为减少一次 Flash 擦写增加恢复进度 Metadata。

## 16. Slot State Semantics

S10 第一版不新增 `REJECTED` Slot State。

现有：

```text
VALID
```

只表达：

> Image Header / Payload 等静态镜像完整性有效。

Trial Runtime Failure 不等于镜像 CRC 无效。

例如：

```text
Slot A = v1.0 VALID, confirmed
Slot B = v1.1 VALID, failed Trial
```

Rollback 后可以保持：

```text
slotAState = VALID
slotBState = VALID
confirmedSlot = A
pendingSlot = NONE
upgradeState = NONE
```

Slot B 不会自动重试，因为 Bootloader 只依据 pending/confirmed lifecycle。

下一次 OTA 对 inactive Slot 继续沿用现有 erase / INVALID / rewrite 逻辑。

## 17. Reset Cause Diagnostics

S10 建议增加轻量 Reset Cause snapshot / log：

```text
POR / BOR
PIN Reset
Software Reset
IWDG Reset
...
```

原则：

1. 尽早读取 RCC Reset Flags；
2. 保存本次 boot 的诊断快照；
3. 输出 Boot Log；
4. 按 STM32 合同清除 flags；
5. Reset Cause 不参与 TRIAL 是否回滚的决策。

Failure Counter 第一版不进入 Reliability Contract。

## 18. Application Failure Semantics

### 18.1 Trial RUNNING

```text
Startup RUNNING
→ Runtime Ready
→ Observation
→ Confirm
```

### 18.2 Trial DEGRADED

```text
Startup DEGRADED
→ do not Confirm
→ controlled reset or stop valid watchdog feed
→ rollback
```

### 18.3 Trial FAILED / Fatal Error

```text
Startup FAILED
or Error_Handler
or fatal runtime fault
→ no valid long-term Feed
→ IWDG reset
→ Bootloader sees TRIAL
→ rollback
```

当前 `Error_Handler()` 的 disable IRQ + infinite loop 在 IWDG 启用后可自然成为最终恢复路径，不要求 S10 把所有 Fatal Path 改成直接 `NVIC_SystemReset()`。

### 18.4 Stable NONE Firmware

Confirm 后 IWDG 继续运行。

Watchdog 是长期系统能力，不随 OTA Confirm 关闭。

`appMainTask` 继续按正常工作周期 Feed。

## 19. Verification Strategy

复用现有 Toolkit / RTT / GDB / Logic Analyzer，不建立第二套测试系统。

### 19.1 Code / Host Verification

至少覆盖：

```text
Application / Bootloader clean build
Metadata compatibility
firmware_confirm preconditions
firmware_confirm atomic commit
TRIAL → ROLLBACK commit
ROLLBACK → NONE commit
pending/confirmed source selection
confirmedVersion consistency gate
installer core reuse
```

### 19.2 Application Board Verification

至少覆盖：

```text
IWDG starts before RTOS scheduler
Pre-RTOS normal startup no false reset
Startup RUNNING reaches runtime health
MAIN / OTA / DISPLAY runtime ready
5 s Observation Window
strict Confirm success
confirmedSlot/version updated
pendingSlot cleared
NONE reached
appMainTask continues long-term watchdog feed
```

### 19.3 Debug Verification

必须覆盖：

```text
normal running IWDG active
breakpoint halt longer than watchdog timeout
DBGMCU freezes IWDG
continue resumes normal execution
existing S05A GDB workflow no regression
```

### 19.4 Trial Failure / Rollback

至少覆盖：

```text
Trial normal Confirm
Trial before Runtime Ready reset
Trial during Observation reset
Trial IWDG reset
Trial software reset
Trial power cycle
TRIAL → ROLLBACK atomic boundary
confirmed image restore
ROLLBACK → NONE
recovered v1.0 behavior visible
```

### 19.5 Rollback Fault Injection

重点覆盖：

```text
before ROLLBACK commit
after ROLLBACK commit
after Internal erase
program ~25% / ~50%
program complete
Internal CRC before/after
ROLLBACK → NONE body/marker boundary
power loss during restore
```

结果必须证明：

> 任意已进入 ROLLBACK 的破坏性失败都可以在下一次启动从 confirmedSlot 重新恢复。

### 19.6 Known-Good Image Failure

至少验证：

```text
confirmedSlot Header invalid
confirmedSlot Payload CRC invalid
confirmedSlot Version != confirmedVersion
```

均必须发生在 Internal erase 前并阻止 destructive restore。

### 19.7 S09 Deferred Fault Injection

S09 已接受的 erase/program/CRC/Metadata marker/Power Loss Deferred Follow-up 可以在 S10 的板级验证窗口中补测。

规则：

- 补测结果必须回填 S09 verification/review evidence；
- 不得把 S09 未完成项伪装为 S10 新功能；
- 不得在未执行时标记 PASS。

## 20. Non-goals

S10 第一版不实现：

- Metadata V3；
- `CONFIRMED` lifecycle enum；
- N 次 Trial Failure 后才 rollback；
- Failure Counter 作为 rollback gate；
- 多任务持续 Heartbeat Manager；
- MCUboot swap / scratch / trailer；
- sector-level rollback resume；
- rejected-image blacklist；
- Recovery UI / Ymodem in Bootloader；
- SHA / AES / HMAC / Signature；
- CK02AT；
- Anti-rollback security version policy；
- Bootloader IWDG 全链路接管；
- External Flash write/erase in Bootloader；
- LVGL / Diagnostics UI。

以上可以在 S11/S12 或后续可靠性增强阶段继续演进。

## 21. Acceptance Criteria

必须满足：

1. S10 继续使用 `NONE / PENDING / TRIAL / ROLLBACK`，不新增 CONFIRMED 状态；
2. Stable confirmed firmware 使用 `NONE + confirmedSlot + confirmedVersion` 表达；
3. Trial 未 Confirm 前再次进入 Bootloader 时立即进入 rollback path，不等待 Failure Counter；
4. Application IWDG 不依赖 CubeMX 重新生成；
5. Watchdog 作为 Platform MCU Capability，通过 Platform/Impl 隔离 HAL；
6. IWDG 在 RTOS scheduler 前启动；
7. IWDG 第一版 timeout 约 10 s；
8. Pre-RTOS / Startup Feed 使用受控 checkpoint，不使用独立无脑 Feed Task；
9. Runtime 长期 Feed Owner 为 `appMainTask`；
10. `appMainTask` 只在完成正常工作周期后 Feed；
11. Debug Halt 时 IWDG 被 DBGMCU 冻结；
12. GDB Continue 后 IWDG 恢复；
13. Trial Startup 必须达到 `APP_SYSTEM_STATE_RUNNING` 才能继续 Confirm；
14. Trial DEGRADED 不允许 Confirm；
15. MAIN / OTA / DISPLAY Runtime Ready 均被验证；
16. Runtime Ready 组件不重复定义新的 component enum；
17. Trial Observation Window 第一版为 5 s；
18. Observation Window 不是唯一健康条件；
19. Runtime Confirm 不复用 `service_ota_confirm_install()`；
20. `firmware_confirm()` 属于 Firmware Lifecycle 语义；
21. Confirm 前重新 load latest Metadata，不只相信缓存；
22. Confirm 前校验 TRIAL / pendingSlot / slot state / Header/version；
23. Confirm 成功原子更新 confirmedSlot / confirmedVersion / pendingSlot / upgradeState；
24. Confirm commit marker 最后写入并重新 load 验证；
25. Confirm 掉电只能落在完整 TRIAL 或完整 NONE 两种权威状态；
26. Bootloader 在 rollback destructive gate 前完整验证 confirmedSlot；
27. confirmed image version 必须等于 `confirmedVersion`；
28. Confirmed Image invalid 时禁止擦 Internal APP；
29. `TRIAL → ROLLBACK` 在 Internal erase 前原子提交；
30. ROLLBACK restore 使用 External confirmedSlot 作为只读源；
31. PENDING install 与 ROLLBACK restore 共用镜像验证/安装核心；
32. 不复制第二套 Internal Flash installer；
33. ROLLBACK 中掉电后可从头重新恢复；
34. Internal CRC/vector PASS 前不得提交 `ROLLBACK → NONE`；
35. Rollback 完成后 `pendingSlot = NONE`；
36. Rollback 完成后 confirmedSlot / confirmedVersion 保持原 Known-Good 基线；
37. Slot VALID 仍表示静态镜像有效，不因 Runtime Trial Failure 自动改为 INVALID；
38. Reset Cause 只用于 Diagnostics，不参与 rollback 判断；
39. Stable NONE Firmware Confirm 后 Watchdog 继续长期运行；
40. Existing S05A GDB、S07A Startup、S09 Installer contracts 无无计划回归；
41. Application / Bootloader Clean Build 通过；
42. Bootloader 仍小于 64 KiB；
43. Trial Confirm / IWDG / Rollback 正常链完成真实板验证；
44. Rollback power-loss / reset fault injection 有可回读证据；
45. S09 Deferred Fault Injection 若补测，结果回填原 S09 证据，不混淆阶段归属。

## 22. Design Review Result

Design Review 结论：

```text
Blocking Findings : 0
Important Findings: 5
Important Findings: resolved in design amendment
Result            : READY_FOR_OWNER_APPROVAL
```

Review 修订项：

1. IWDG Start 前移到 `HAL_Init()` 后、`SystemClock_Config()` 前；
2. 明确当前 HAL IWDG 模块/Keil Source 尚未接入，实施计划必须手工接入且不使用 CubeMX regeneration；
3. 增加 Trial Runtime Ready 5 s deadline 与 Feed gating，禁止永久 TRIAL；
4. 保持 otaWorker 为 Firmware Storage I/O owner，Health 只决定 Confirm 时机；
5. strict Confirm 增加完整 Candidate Payload CRC/size/slot identity 校验，并强制 `pendingSlot != confirmedSlot`。

经上述修订，未发现需要推翻 S10 生命周期、Metadata V2 或 rollback transaction 的 Blocking 问题。

## 23. Approval

- Decision: `PENDING_PROJECT_OWNER_APPROVAL`
- Approved By: `Not approved yet`
- Initial Design Commit: `223fae71d3c641cf9e36d6048d22a73815152b94`
- Design Review Amendment Commit: `Will be recorded after this update`
