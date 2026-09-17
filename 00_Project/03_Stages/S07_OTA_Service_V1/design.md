# S07 OTA Service V1 Design

## Metadata

- Stage: `S07_OTA_Service_V1`
- Status: `DESIGN_APPROVED`
- Owner: `Project Owner`
- Date: `2026-09-17`
- Baseline Commit: `84f07303d2b6fbf0682e492ad79e32982e2fb17b`

## Goal

在 S06 已冻结的 FreeRTOS 三线程 Runtime 上建立正式 OTA Service V1，使 Application 能通过物理按键进入 OTA 接收流程，将完整 Firmware Image 可靠写入 W25Q64 非确认槽，完成镜像验证，并在用户二次确认后通过 AT24C02 Metadata 提交可跨 Reset 保存的 `PENDING` 升级请求。

S07 的终点是：

```text
KEY_1
→ OTA Receive
→ External Inactive Slot
→ Image Validation
→ READY_TO_INSTALL
→ KEY_1 Confirm
→ Metadata PENDING
→ Reset Request
```

S07 不负责 Bootloader 将镜像安装到 STM32 Internal Flash，也不负责 Trial / Confirm / Rollback 的执行。

## Context

S06 已冻结 Runtime：

```text
appSystem
├─ Application lifecycle
└─ foreground demo behavior

otaWorker
├─ UART/Ymodem execution context
├─ firmware receive
└─ image validation

displayTask
└─ ST7789 / Graphics / Display Model owner
```

冻结 IPC：

```text
UART ISR/RX → otaWorker   : Task Notification
otaWorker   → displayTask : Queue
```

S04 已冻结 Firmware Storage：

```text
W25Q64
Slot A: 0x000000 ~ 0x07FFFF
Slot B: 0x080000 ~ 0x0FFFFF

Per Slot:
+0x0000 ~ +0x0FFF : Header Sector
+0x1000 ~          : Payload
```

Firmware Image V1 使用 64 Byte Header，Image Header 是版本、大小和 Payload CRC32 的权威来源；Firmware Storage 已提供 Header/Payload 写入和只读 `firmware_storage_validate_image()`。

S05 已冻结：

```text
USART1/DMA/RingBuffer
→ service_uart
→ ymodem_parser
→ ymodem_receiver
→ ymodem_sink
→ Firmware Storage
```

Ymodem 只负责传输协议，不感知 Slot、Metadata、`PENDING` 或 Reset；Header-last commit 保证失败传输不会提交新的有效 Header。

## In Scope

- 正式 `service_ota` facade / runtime state machine；
- 保留 `otaWorker` 作为 S06 已验证的 RTOS execution shell；
- 将生产 OTA sink 从 S05 Board Test sink 中解耦；
- 支持动态选择 External Firmware Image Slot；
- Metadata V2：删除 `activeSlot`，增加 `pendingSlot` 和 `upgradeState`；
- Metadata V1 backward-compatible read；
- KEY_1 / PA0 作为 OTA 用户确认输入；
- KEY_1 通过 EXTI → ISR → Task Notification 唤醒 `otaWorker`；
- 最小 Platform Key abstraction；
- 通用 Platform MCU IRQ abstraction；
- 两次用户确认：开始下载、确认安装；
- 下载前目标 Slot 状态失效事务；
- 下载完成后的 Image Validation；
- `READY_TO_INSTALL` runtime state；
- 二次确认后 Metadata `PENDING` atomic commit；
- Reset request handoff；
- Factory / Initial Provisioning 设计与最小工具支持；
- Build、Host Test、真实板 UART/Ymodem/RTT/按键/持久化验收；
- 同步阶段设计、实施、交接与验证文档。

## Out of Scope

S07 不实现：

- Bootloader 从 External Flash 安装 Application 到 Internal Flash；
- Internal Flash 擦写安装状态机；
- Trial Boot；
- Application Confirm API；
- Watchdog 启动失败判定；
- Boot attempt / failure counter；
- Rollback reinstall；
- AES / SHA / HMAC / Digital Signature；
- CK02AT 集成；
- MCU 从 Internal Flash 自动构造 Factory Image 的 Self-Provisioning；
- LVGL 或新的 UI Framework；
- 复杂 OTA Manager Framework。

## Design

### 1. 存储模型

STM32F411 当前 Application 真实执行位置为 Internal Flash。W25Q64 Slot A/B 仅保存 Firmware Image，不是 CPU 可直接执行的 A/B Application Partition。

```text
STM32 Internal Flash
└─ Installed Application
   当前真实执行代码

W25Q64
├─ Slot A : Firmware Image
└─ Slot B : Firmware Image

AT24C02
└─ Boot / Upgrade Metadata
```

因此本项目中的 A/B 定义冻结为：

```text
External Firmware Image A/B Storage
```

而不是：

```text
Executable Application A/B Partition
```

### 2. Metadata 角色

Firmware Image Header 与 Metadata 的权威范围分离：

```text
Image Header
→ 单个 Slot 内 Firmware Version / Size / Payload CRC 的权威来源

Metadata
→ confirmed image、pending image、slot health、upgrade lifecycle 的权威来源

Internal Flash
→ 当前实际运行代码
```

### 3. Metadata V2

S04 Metadata V1 中的 `activeSlot` 删除。原因是 External Slot 并非运行位置，`activeSlot` 容易被误解为 CPU 当前执行的 Slot。

业务模型冻结为：

```c
typedef enum
{
    FIRMWARE_UPGRADE_STATE_NONE = 0,
    FIRMWARE_UPGRADE_STATE_PENDING,
    FIRMWARE_UPGRADE_STATE_TRIAL,
    FIRMWARE_UPGRADE_STATE_ROLLBACK
} firmware_upgrade_state_t;

typedef struct
{
    uint32_t sequence;

    firmware_slot_t confirmedSlot;
    firmware_slot_t pendingSlot;

    firmware_slot_state_t slotAState;
    firmware_slot_state_t slotBState;

    firmware_upgrade_state_t upgradeState;

    firmware_version_t confirmedVersion;
} firmware_metadata_t;
```

字段语义：

```text
confirmedSlot
= W25Q64 中最后一个已确认可运行版本的完整镜像
= 后续 Rollback 的恢复来源

pendingSlot
= Bootloader 下一次需要安装的镜像 Slot
= NONE 表示没有待安装镜像

slotAState / slotBState
= External Firmware Image 的镜像健康状态

confirmedVersion
= 系统最后确认成功运行的版本

upgradeState
= 跨 Reset 保存的升级生命周期状态
```

升级生命周期冻结为：

```text
NONE → PENDING → TRIAL → NONE
                 ↓
             ROLLBACK → NONE
```

S07 只负责产生 `PENDING`。`TRIAL` 和 `ROLLBACK` 的实际转换属于后续阶段。

### 4. Image Health 与 Upgrade Lifecycle 分离

Slot health：

```text
EMPTY / VALID / INVALID
```

只描述镜像本身是否存在、是否完整有效。

Upgrade lifecycle：

```text
NONE / PENDING / TRIAL / ROLLBACK
```

描述系统升级事务当前处于哪个阶段。

因此：

```text
VALID != CONFIRMED
VALID != PENDING
Trial failure != INVALID image
```

例如一个 CRC 完整但运行后触发 Watchdog 的 v1.1 镜像仍可以保持 `VALID`；运行失败由后续 Trial / Rollback 机制描述，不滥用 `INVALID`。

### 5. Metadata Raw Layout 与兼容策略

AT24C02 继续保持两个 128 Byte Copy：

```text
Copy A: 0x00 ~ 0x7F
Copy B: 0x80 ~ 0xFF
```

继续使用：

```text
sequence + CRC32 + commit marker last
```

保证 old-or-new valid record 的原子提交语义。

V2 优先保留 V1 已有稳定字段偏移，并从原 reserved 区加入新字段。实施时以当前源码真实布局为准，目标布局原则如下：

```text
0x00 magic
0x04 format_version = 2
0x06 size = 128
0x08 sequence
0x0C.. existing stable role/health fields
0x10 confirmedVersion (existing stable offset when possible)
0x18 pendingSlot
0x19 upgradeState
0x1A..0x77 reserved = 0
0x78 crc32
0x7C commitMarker
```

最终准确 offset 必须在实施前依据当前 `firmware_metadata.c` 固化，并写入代码注释/测试；不得通过直接序列化 C struct 改变 EEPROM wire format。

Backward compatibility：

```text
formatVersion == 2
→ decode V2

formatVersion == 1
→ 按 V1 layout decode
→ pendingSlot = NONE
→ upgradeState = NONE
```

读取 V1 时不为了迁移立即写 EEPROM；下一次正常 Metadata commit 自动编码为 V2。

### 6. Stable / Pending / Trial 语义

稳定状态：

```text
Internal Flash = 当前 confirmed firmware
confirmedSlot  = 对应的外部恢复镜像
pendingSlot    = NONE
upgradeState   = NONE
```

S07 提交安装请求后：

```text
Internal Flash = 仍运行旧 confirmed firmware
confirmedSlot  = old confirmed image
pendingSlot    = new image slot
upgradeState   = PENDING
```

后续 S09/S10 安装完成准备 Trial 时：

```text
Internal Flash = new firmware
confirmedSlot  = old confirmed image
pendingSlot    = new image slot
upgradeState   = TRIAL
```

新版本确认后：

```text
confirmedSlot    = new slot
confirmedVersion = new version
pendingSlot      = NONE
upgradeState     = NONE
```

后续 Trial 失败时 Rollback 来源始终是 `confirmedSlot`。

### 7. Target Slot 选择规则

稳定态下只允许在：

```text
upgradeState == NONE
pendingSlot  == NONE
```

开始新的 OTA Session。

第一版选择规则：

```text
targetSlot = opposite(confirmedSlot)
```

因此确认成功后 A/B 自然轮换：

```text
v1.0 confirmed → A
v1.1 target    → B
v1.1 confirmed → B
v1.2 target    → A
```

在 `PENDING / TRIAL / ROLLBACK` 生命周期中禁止开始新的 OTA 下载，避免覆盖 rollback source 或未完成事务。

### 8. OTA Runtime State Machine

Application RAM runtime state：

```text
IDLE
↓
PREPARING
↓
RECEIVING
↓
VERIFYING
↓
READY_TO_INSTALL
↓
COMMITTING
↓
REBOOT_REQUIRED
```

失败进入：

```text
FAILED
```

完成错误清理后允许回到可重新开始状态。

`READY_TO_INSTALL` 只存在 RAM，不写 EEPROM。它表示：

```text
new external image is valid
+
user has NOT authorized bootloader installation yet
```

### 9. Download Transaction

下载前必须先使目标 Slot 在 Metadata 中失效：

```text
select target
↓
targetSlot.state = INVALID
pendingSlot = NONE
upgradeState = NONE
↓
commit Metadata
↓
erase target Slot
```

不能先擦除再修改 Metadata，否则掉电窗口中可能出现 EEPROM 仍标记 `VALID`、而 Flash 已被部分擦除的不一致状态。

之后：

```text
Ymodem receive
↓
ota_firmware_sink
↓
write payload
↓
Header-last commit
↓
firmware_storage_validate_image()
```

验证失败：

```text
target state remains INVALID
pendingSlot = NONE
upgradeState = NONE
PENDING must not be created
```

验证成功：

```text
target state = VALID
pendingSlot = NONE
upgradeState = NONE
↓
commit Metadata
↓
runtime = READY_TO_INSTALL
```

### 10. 二次确认与 PENDING Commit

第一次 KEY_1：开始 OTA 接收。

第二次 KEY_1 只在 `READY_TO_INSTALL` 有效：

```text
pendingSlot = targetSlot
upgradeState = PENDING
↓
commit Metadata
↓
commit success
↓
runtime = REBOOT_REQUIRED
```

Metadata PENDING commit 是 Application OTA 与 Bootloader 之间的事务交接点：

```text
commit 前
→ Application owns OTA transaction

commit 后
→ next boot is allowed to act on pendingSlot
```

Reset 必须发生在 PENDING commit 成功之后。

### 11. otaWorker 职责

S06 已验证的 `otaWorker` 保留，不新增 OTA Task。

S07 将其收敛为：

```text
otaWorker = RTOS execution shell
```

负责：

- 等待 UART Task Notification；
- 等待 KEY Task Notification；
- 任务上下文按键消抖；
- 驱动 `service_ota`；
- pump Ymodem receiver；
- 将 OTA business event 映射为 Display Queue event；
- 收到 `RESET_REQUIRED` 后调用 Platform MCU Reset abstraction。

`otaWorker` 不继续承担完整 OTA business state machine。

### 12. service_ota

新增正式 OTA Service，职责：

```text
service_ota
├─ runtime state
├─ target slot selection
├─ session start/control
├─ Ymodem coordination
├─ image validation
├─ metadata transition
├─ READY_TO_INSTALL
├─ user confirm
└─ RESET_REQUIRED output
```

约束：

- 不创建 FreeRTOS Task；
- 不拥有 display hardware；
- 不直接执行 `NVIC_SystemReset()`；
- 不把 FreeRTOS notification 作为 public OTA API；
- 不引入复杂 manager hierarchy。

### 13. Production OTA Firmware Sink

当前生产路径对 `04_Test/Board/S05_UART_Ymodem/s05_ymodem_flash_sink.*` 的依赖必须移除。

建立 production `ota_firmware_sink`：

```text
compact .img
→ selected external firmware slot
```

职责限定为 begin/write/end/abort 与 Header-last commit；target Slot 由上层在 session 开始前提供。

Ymodem receiver 继续不知道 Slot / Metadata / PENDING / Reset。

### 14. KEY_1 / PA0

PA0 已通过 CubeMX 配置为：

```text
KEY_1
Pull-up
Falling Edge EXTI
```

用户交互冻结为：

```text
IDLE
KEY_1 → START OTA

READY_TO_INSTALL
KEY_1 → CONFIRM INSTALL

RECEIVING / VERIFYING / COMMITTING
KEY_1 → IGNORE
```

不新增 Key Task。

数据流：

```text
PA0 Falling Edge
↓
EXTI0 IRQ
↓
HAL_GPIO_EXTI_Callback
↓
Impl / Platform Key lightweight forwarding
↓
xTaskNotifyFromISR(otaWorker)
↓
otaWorker task-context debounce
↓
service_ota action
```

ISR 中禁止：

- Flash erase/write；
- Metadata commit；
- Ymodem business start；
- LCD 操作；
- `HAL_Delay()`；
- OTA state transition business logic。

第一版 task-context debounce 约 30~50 ms。

### 15. Platform Key

建立薄 Platform Key abstraction：

```text
HAL / STM32 EXTI
↓
Impl Key
↓
Platform Key
↓
APP otaWorker
```

Platform 只表达：

```text
KEY_1 / pressed / key event
```

不表达：

```text
START OTA / CONFIRM INSTALL
```

业务语义属于 APP / OTA Service。

### 16. Platform MCU IRQ

项目已经存在 USART IRQ、DMA IRQ、PA0 EXTI IRQ，因此 S07 将 IRQ 作为通用高频 MCU 能力加入 Platform。

最小能力：

```text
enable
disable
set_priority
clear_pending
```

抽象名称使用 `IRQ`，不使用 STM32 特有 `EXTI` 作为 Platform MCU 接口名。

Impl 优先直接使用 CMSIS NVIC：

```text
NVIC_EnableIRQ
NVIC_DisableIRQ
NVIC_SetPriority
NVIC_ClearPendingIRQ
```

IRQ abstraction 只管理 NVIC control，不成为 UART/DMA/KEY callback manager。

所有会调用 FreeRTOS `...FromISR()` API 的 IRQ 必须满足 `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` / `configMAX_SYSCALL_INTERRUPT_PRIORITY` 约束。

### 17. Reset Ownership

`service_ota` 不直接 Reset MCU。

```text
service_ota
→ RESET_REQUIRED
→ otaWorker / APP
→ Platform MCU Reset
```

这样业务层不依赖 Cortex-M reset implementation，并便于 Host Test 验证 PENDING commit 后的状态。

### 18. Factory / Initial Provisioning

第一次 OTA 前必须保证存在一个 confirmed rollback image。

第一版初始状态：

```text
Internal Flash
= v1.0 Application

W25Q64
Slot A = v1.0 VALID confirmed image
Slot B = EMPTY

Metadata
confirmedSlot    = A
confirmedVersion = 1.0.0
slotAState       = VALID
slotBState       = EMPTY
pendingSlot      = NONE
upgradeState     = NONE
```

这一步属于 Factory / Initial Provisioning，不属于 OTA download transaction。

第一版优先由 PC / Toolkit 主动预置，不实现 MCU 自己从 Internal Flash 构造 `.img` 的 Self-Provisioning。

Provisioning 可以复用：

```text
toolkit firmware pack
toolkit ymodem
Firmware Storage
Metadata APIs
```

如新增 `toolkit provision`，必须复用现有 Toolkit Core / Adapter / Workflow，不建立第二套工具链。

## Interfaces and Data Flow

### Normal OTA Flow

```text
KEY_1
  ↓
EXTI / IRQ
  ↓
otaWorker notification
  ↓
service_ota_start()
  ↓
select opposite(confirmedSlot)
  ↓
Metadata target=INVALID commit
  ↓
Ymodem receiver
  ↓
ota_firmware_sink
  ↓
W25Q64 target slot
  ↓
Header-last commit
  ↓
firmware_storage_validate_image()
  ↓
Metadata target=VALID commit
  ↓
READY_TO_INSTALL
  ↓
KEY_1
  ↓
service_ota_confirm_install()
  ↓
pendingSlot=target
upgradeState=PENDING
  ↓
Metadata atomic commit
  ↓
RESET_REQUIRED
  ↓
Platform MCU Reset
```

### Display Flow

```text
service_ota state/event
↓
otaWorker
↓
app_display_event_t
↓
Display Queue
↓
displayTask
```

LCD 第一版至少支持：

```text
NORMAL / IDLE
OTA READY
RECEIVING + progress
VERIFYING
READY TO INSTALL
FAILED
REBOOT REQUIRED
```

### Runtime Ownership

```text
appSystem
→ foreground Application behavior

otaWorker
→ OTA execution shell / UART / key event processing

displayTask
→ sole LCD/ST7789 owner

service_ota
→ OTA business state and persistence decisions
```

## Failure Handling

### Download Interrupted

目标 Slot 在擦除前已经持久化为 `INVALID`，且 Header-last 未提交新 Header：

```text
running confirmed APP unaffected
confirmedSlot unchanged
pendingSlot = NONE
upgradeState = NONE
target state = INVALID
```

### Image Validation Failure

```text
target remains INVALID
READY_TO_INSTALL not entered
PENDING not written
running APP unaffected
```

### Power Loss After Image VALID but Before User Confirm

```text
target image remains VALID
pendingSlot = NONE
upgradeState = NONE
```

下次启动继续运行旧 confirmed APP；Bootloader 不应擅自安装该 image。

### Metadata Commit Power Loss

依赖已有双副本机制：

```text
write uncommitted body
→ read-back verify
→ commit marker last
```

下次读取必须得到 old valid record 或 new valid record，不接受半提交 record。

### Power Loss After PENDING Commit

`PENDING` 已持久化，因此下一次启动仍应读取：

```text
pendingSlot = target
upgradeState = PENDING
```

S07 只验证 persistence；真正安装由后续 Bootloader 阶段实现。

### Key During Active Transfer

`RECEIVING / VERIFYING / COMMITTING` 中的 KEY_1 被忽略，不允许破坏当前 session 或重复 commit。

### Invalid Stable Metadata

如果没有可用 Metadata 或 `confirmedSlot` 无法指向合法 confirmed image，不允许直接开始普通 OTA overwrite。应进入明确错误/Provisioning 路径，避免覆盖唯一潜在恢复镜像。

## Verification Strategy

### Static / Host

- Metadata V1/V2 encode/decode；
- V1 backward compatibility；
- sequence / CRC / reserved / commit marker；
- Metadata dual-copy failure injection；
- target slot selection；
- OTA runtime transition；
- invalid transition rejection；
- KEY state semantic mapping；
- production sink dynamic Slot；
- Header-last interrupted transfer behavior；
- PENDING 只在 valid image + second confirm 后生成。

### Build Regression

使用仓库统一 Toolkit：

```text
05_Tools\toolkit.bat build
```

并执行适用 Host / Toolkit regression。

### Real Board

复用：

```text
toolkit flash run
toolkit rtt
toolkit ymodem
toolkit snapshot halt|resume
toolkit logic ...
```

验证：

- KEY_1 启动 OTA；
- Ymodem `'C'` handshake；
- background transfer 不阻塞 foreground behavior；
- inactive Slot 写入；
- progress / VERIFYING / READY_TO_INSTALL LCD；
- second KEY 写入 PENDING；
- Reset 后 Metadata PENDING persistence；
- interrupted transfer；
- bad CRC；
- transfer 中重复按键；
- S06 Runtime regression。

逻辑分析仪不是所有验收的强制条件，但在需要确认 SPI/I2C 实际事务时复用已有 sigrok Workflow。

## Acceptance Criteria

S07 至少满足：

```text
1. PA0 KEY_1 可在任务上下文可靠触发 OTA start。
2. KEY ISR 不执行 OTA/Flash/Display 重业务。
3. otaWorker 保留为单一 OTA RTOS execution shell。
4. service_ota 成为正式 OTA business facade/state machine。
5. 生产代码不再依赖 S05 Board Test flash sink。
6. OTA sink 支持动态 External target Slot，并保持 Header-last commit。
7. Metadata V2 删除 activeSlot，增加 pendingSlot / upgradeState。
8. Metadata V1 可兼容读取，并在下一次正常 commit 自然迁移到 V2。
9. 下载前先持久化 target Slot INVALID，再 erase。
10. 下载成功后必须完整 validate image 后才将 target 标记 VALID。
11. READY_TO_INSTALL 不持久化为 Bootloader 命令。
12. 只有第二次 KEY 确认才提交 pendingSlot + PENDING。
13. PENDING commit 成功后才允许 Reset。
14. Reset 后 OTA_APP 能再次读取相同 PENDING 状态。
15. confirmedSlot 对应的 rollback image 在新版本确认前不被覆盖。
16. 中断传输、CRC 错误、Metadata 写失败不得产生错误 PENDING。
17. Platform Key 与 Platform MCU IRQ 边界明确，IRQ 满足 FreeRTOS priority 约束。
18. Factory Provisioning 能建立 v1.0 Internal APP + Slot A confirmed image + Metadata baseline。
19. S06 foreground/display/UART runtime 无回归。
20. 不提前实现 Internal Flash Installation / Trial / Confirm / Rollback execution。
```

S07 最终业务链路：

```text
KEY_1
→ OTA READY
→ Ymodem Receive
→ Inactive External Slot
→ Image Validation PASS
→ READY_TO_INSTALL
→ KEY_1
→ Metadata PENDING
→ Reset
→ Restart
→ PENDING persistence confirmed
```

## Approval

- Decision: `APPROVED`
- Approved By: `Project Owner`
- Approval Date: `2026-09-17`
- Design Commit: `See Git history for this document`
