# S07A RTOS Startup Refactor Design

## Metadata

- Stage: `S07A_RTOS_Startup_Refactor`
- Status: `DRAFT`
- Owner: `Project Owner`
- Date: `2026-09-18`
- Baseline Commit: `254f510498748294f44b0c059796dbaeaccdea3e`

## Goal

在不改变 S07 OTA 功能合同的前提下，重新整理 Application 的 RTOS 启动阶段，使“硬件初始化、系统装配、任务本地初始化、稳定运行”四个阶段职责清晰，并消除当前 `appSystem` 同时承担 Bootstrap、Task Creator 和前台业务任务的职责混合。

目标启动链：

```text
main / CubeMX
    ↓
Hardware Bootstrap
    ↓
RTOS Kernel Start
    ↓
defaultTask
    ↓
appSystem Bootstrap
    ↓
Create IPC / Compose Runtime / Create Tasks
    ↓
Task-local Initialization
    ↓
READY Barrier
    ↓
SYSTEM_RUN
    ↓
appSystem Exit
    ↓
Steady Runtime
```

## Context

当前代码已经具备：

```text
MX_FREERTOS_Init()
↓
defaultTask
↓
app_system_start()
↓
创建 appSystem
↓
defaultTask 删除
```

但 `appSystem` 当前继续：

```text
appSystem
├─ app_display_task_start()
├─ app_ota_worker_start()
└─ app_main()      ← 永久进入 LED 前台业务
```

因此当前 `appSystem` 实际同时承担：

```text
System Bootstrap
+ Task Creation
+ Foreground Application Task
```

同时，`otaWorker`、`displayTask`、`app_main` 又各自在 Task Entry 内执行一部分资源初始化，启动顺序和系统进入 RUNNING 的边界不够明确。

S06/S07 已证明当前业务功能可用，因此 S07A 只整理启动生命周期，不重写已经验证的业务组件。

## In Scope

- 将 `appSystem` 重定义为一次性 Application Bootstrap Task；
- 从 `appSystem` 中分离长期运行的 `appMainTask`；
- 建立统一 Application Startup State / Startup Barrier；
- 明确 System Composition 与 Task-local Initialization 的边界；
- 保证高优先级 Task 创建后即使立即抢占，也不能提前进入正式业务运行；
- 集中创建共享 IPC / Runtime Contract；
- 保持 otaWorker 的 single-owner 语义；
- 保持 displayTask 的 LCD/SPI1 sole-owner 语义；
- 保持 appMainTask 的前台业务资源 ownership；
- 增加或复用通用 RTOS Event Flags / 等价同步能力；
- 重新验证 Task Stack High Water Mark；
- 重新验证启动阶段 Heap Peak / Minimum Ever Free Heap；
- 验证 Bootstrap Task 删除后动态 Stack/TCB 内存可回收；
- 回归 S07 OTA、KEY、Display、Ymodem、Metadata PENDING 全链路。

## Out of Scope

- 不修改 S07 OTA Service 状态机语义；
- 不修改 Firmware Image / Metadata V2 合同；
- 不实现 Bootloader；
- 不实现 Internal Flash Installation；
- 不实现 Trial / Confirm / Rollback；
- 不增加新的长期业务 Task；
- 不引入 Manager Framework；
- 不把所有模块 init 强行搬进 `app_system.c`；
- 不在启动重构同时激进压缩各 Task Stack；
- 不改变 Application 分层边界。

## Design

### 1. 四阶段启动模型

Application 启动正式分成：

```text
Phase 1 - Hardware Bootstrap
Phase 2 - Application Composition
Phase 3 - Task-local Initialization
Phase 4 - Steady Runtime
```

#### Phase 1 - Hardware Bootstrap

由 CubeMX / Core 完成：

```text
HAL_Init
SystemClock_Config
MX_GPIO_Init
MX_DMA_Init
MX_SPI...
MX_USART...
osKernelInitialize
MX_FREERTOS_Init
osKernelStart
```

这里负责 MCU 外设的基础初始化，不负责 Application 业务装配。

#### Phase 2 - Application Composition

由一次性的 `appSystem` Bootstrap Task 完成：

```text
shared IPC creation
startup synchronization object creation
runtime dependency composition
business task creation
startup supervision
```

`appSystem` 不再执行 LED Blink/Breath 等长期业务。

#### Phase 3 - Task-local Initialization

每个长期 Task 只初始化自己独占、且与 Task Identity/Ownership 有关的资源。

```text
appMainTask
→ status LED / foreground local resources
→ report APP_READY
→ wait SYSTEM_RUN

otaWorker
→ obtain current thread handle
→ KEY binding
→ OTA Runtime / UART owner binding
→ report OTA_READY
→ wait SYSTEM_RUN

displayTask
→ SPI1/ST7789/display local resources
→ report DISPLAY_READY
→ wait SYSTEM_RUN
```

不能为了“集中 init”而破坏资源 ownership。

#### Phase 4 - Steady Runtime

`appSystem` 收到所有 required READY 后：

```text
set SYSTEM_RUN
↓
appSystem self-delete
```

长期只保留：

```text
appMainTask
otaWorker
displayTask
```

### 2. defaultTask 定位

`defaultTask` 保持极薄：

```text
StartDefaultTask
↓
app_system_start()
↓
vTaskDelete(NULL)
```

它只是 CubeMX 与 Application Runtime 的桥接层。

不得把真实系统初始化重新堆回 `defaultTask`。

### 3. appSystem 定位

S07A 后：

```text
appSystem
= Application Composition Root
+ Startup Supervisor
```

它负责“系统怎么组合起来”，不负责“设备底层怎么绑定”。

允许：

```text
Create shared Queue/Event Flags
Create appMainTask
Create otaWorker
Create displayTask
Wait READY
Release SYSTEM_RUN
```

不允许直接操作：

```text
huart1
hspi1/hspi2
GPIOA/Pin
HAL device handles
Ymodem packet
Flash raw address
LCD draw primitive
```

底层硬件 binding 继续属于 BSP / Impl。

### 4. Task-local Initialization 原则

System 负责初始化顺序，不等于所有初始化都在 System Stack 上执行。

特别是当前 OTA Runtime：

```text
service_uart ownerThread = otaWorker
KEY event → otaWorker notification
```

因此 OTA Runtime 中与 Task Owner 绑定的初始化必须由 `otaWorker` 自己完成。

Display SPI/ST7789 继续由 displayTask 初始化和独占。

前台 LED 继续由 appMainTask 初始化和使用。

### 5. Startup Barrier

必须解决 Task 创建后立即参与调度的问题。

当前优先级中：

```text
otaWorker   ABOVE_NORMAL
appSystem   NORMAL
displayTask BELOW_NORMAL
```

因此 `appSystem` 创建 otaWorker 后，otaWorker 可能立即抢占 Bootstrap Task。

S07A 必须建立显式 Startup Barrier：

```text
Task Created
↓
Task-local Init
↓
report READY
↓
wait SYSTEM_RUN
```

`appSystem`：

```text
wait APP_READY
wait OTA_READY
wait DISPLAY_READY
↓
SYSTEM_RUN
↓
self-delete
```

同步机制优先增加通用 `platform_event_flags`，基于 CMSIS-RTOS2 Event Flags；如果实现调查发现现有 Platform 有更小且同样清晰的等价能力，可以在 Design Review 时替换，但不得用忙等轮询。

建议 Startup Bits：

```text
APP_STARTUP_READY_APP
APP_STARTUP_READY_OTA
APP_STARTUP_READY_DISPLAY
APP_STARTUP_RUN
APP_STARTUP_FAILED
```

### 6. Failure Policy

任何 required Task 初始化失败：

```text
Task init fail
↓
set APP_STARTUP_FAILED
↓
do not set SYSTEM_RUN
↓
appSystem records/logs failure
↓
enter controlled fatal/degraded policy
```

第一版对 required runtime 组件采用 fail-fast，不允许部分任务偷偷进入业务运行。

Display 是否未来允许 degraded mode 可另行设计；S07A 不扩大该策略，优先保持 S06/S07 已验证行为。

### 7. IPC Creation Ownership

共享 IPC 应优先由 `appSystem` 在创建消费者/生产者 Task 前创建。

例如：

```text
Display Queue
Startup Event Flags
```

然后将 opaque handle / Platform object 传给对应 Task start API。

Task start API 的职责应收敛为：

```text
bind provided runtime resources
create task
```

不再同时隐式创建其他系统级共享资源。

### 8. Stack / Heap Hard Constraints

启动重构必须优先保证 RAM 安全。

当前已验证 S06 Stack 证据：

```text
appSystem   4096 B stack, remaining ≈ 3680 B
otaWorker   4096 B stack, remaining ≈ 3412 B
displayTask 4096 B stack, remaining ≈ 3224 B
```

当前 FreeRTOS Heap：

```text
configTOTAL_HEAP_SIZE = 24576 B
S06 xFreeBytesRemaining ≈ 7344 B
S06 xMinimumEverFreeBytesRemaining ≈ 6720 B
```

S07A 第一版原则：

1. 不在拓扑重构同时压缩 otaWorker/displayTask stack；
2. Bootstrap `appSystem` 初始继续保留 4096 B；
3. 新 `appMainTask` 初始建议 2048 B，最终以实测为准；
4. `defaultTask` 保持 CubeMX 当前 512 B，继续只做桥接；
5. `appSystem` 不创建大块局部 buffer，不在自身栈执行大数据处理；
6. 创建所有长期 Task 时要测启动阶段 Heap Peak；
7. appSystem 删除后必须确认其动态 Stack/TCB 内存被 Idle cleanup 回收；
8. 完整 OTA + Display + KEY 路径后重新测所有长期 Task High Water Mark；
9. 未获得新板测证据前禁止缩小已有 stack；
10. 如启动峰值 Heap 不安全，优先调整生命周期/静态对象，不直接无依据扩大 heap。

### 9. Stack Diagnostics

当前 FreeRTOS 已开启：

```text
INCLUDE_uxTaskGetStackHighWaterMark = 1
configRECORD_STACK_HIGH_ADDRESS = 1
```

S07A 验证可继续使用 GDB A5 填充扫描，并可补充 `uxTaskGetStackHighWaterMark()` 证据。

必须分别记录：

```text
Startup peak
Steady idle
OTA receiving
Display update
READY_TO_INSTALL
Failure path
```

避免只测 Idle 后误判 Stack 安全。

### 10. Runtime Ownership After Refactor

最终长期 Runtime：

```text
appMainTask
→ foreground Application behavior
→ LED demo / future normal business

otaWorker
→ OTA execution shell
→ UART/Ymodem owner
→ KEY OTA event owner
→ service_ota driver

displayTask
→ Display Queue consumer
→ SPI1/ST7789 sole owner
```

一次性 Task：

```text
defaultTask
→ launch appSystem
→ exit

appSystem
→ compose + supervise startup
→ release SYSTEM_RUN
→ exit
```

### 11. Relationship to S06/S07

S07A 不否定 S06/S07 已验证的业务能力。

保留：

```text
UART ISR/RX → otaWorker : Task Notification
otaWorker → displayTask : Queue
OTA Service V1
Metadata V2
KEY_1 double confirm
PENDING persistence
```

S07A 只替换 S06 中：

```text
appSystem = foreground runtime task
```

这一启动/职责定义。

新的定义：

```text
appSystem = temporary bootstrap/supervisor
appMainTask = persistent foreground runtime task
```

## Interfaces and Data Flow

### Startup

```text
defaultTask
↓
app_system_start()
↓
appSystem

appSystem
├─ create Startup Event Flags
├─ create Display Queue
├─ start appMainTask
├─ start otaWorker
└─ start displayTask

appMainTask  ── APP_READY ──────┐
otaWorker    ── OTA_READY ──────┼→ appSystem
displayTask  ── DISPLAY_READY ──┘

all READY
↓
SYSTEM_RUN
↓
appSystem delete
```

### Runtime

```text
appMainTask
→ foreground work

KEY/UART ISR
→ otaWorker notification
→ service_ota
→ display queue
→ displayTask
```

## Failure Handling

- Task create failure：不进入 SYSTEM_RUN；
- Task-local init failure：上报 FAILED，不进入 SYSTEM_RUN；
- Startup timeout：记录未 Ready 的 Task，系统不进入假运行状态；
- Stack warning/overflow：S07A 验收失败，禁止继续 S08；
- Heap 创建失败：记录具体创建阶段并停止进入 Runtime；
- appSystem delete 后 heap 未回收：调查 Idle cleanup / lifecycle，不视为完成；
- OTA/Display 功能回归：按 S07 regression 处理，不允许以“架构更干净”为理由接受功能退化。

## Verification Strategy

### Code / Host

- Platform Event Flags abstraction tests；
- Startup bit/state transition tests；
- Task start API ownership review；
- 无 busy-loop startup wait；
- appSystem 不包含长期业务 loop；
- appMainTask 独立存在；
- S07 Host regression 全量通过；
- Keil Build 0 error。

### GDB / Runtime

- defaultTask 创建 appSystem 后退出；
- appSystem 在 SYSTEM_RUN 后退出；
- 稳定运行态只保留预期长期业务 Task；
- Task priority 与 S07 一致；
- Startup READY 顺序可观察；
- Stack High Water Mark；
- Heap before/during/after appSystem deletion；
- Idle cleanup 后回收确认。

### Real Board

- LED foreground behavior；
- LCD startup / OTA display；
- KEY_1 start；
- Ymodem receive；
- READY_TO_INSTALL；
- second KEY / PENDING；
- interrupted transfer；
- bad CRC；
- reset persistence。

## Acceptance Criteria

1. `defaultTask` 仍只负责启动 Application System 后退出；
2. `appSystem` 成为一次性 Bootstrap/Supervisor，不再执行长期前台业务；
3. 新增独立 `appMainTask` 承担 foreground Application behavior；
4. appSystem 创建共享 IPC 和 Startup synchronization；
5. 所有 required Task 完成本地 init 并报告 READY 后才允许进入 SYSTEM_RUN；
6. 高优先级 otaWorker 即使创建后立即抢占，也只能初始化并阻塞在 Startup Barrier；
7. appSystem 在 SYSTEM_RUN 发布后自行删除；
8. 稳定运行态不存在多余 Bootstrap Task；
9. otaWorker / displayTask / appMainTask ownership 清晰且无互相初始化对方私有硬件；
10. S07 OTA Service / Metadata / Ymodem / KEY 双确认语义无变化；
11. 启动峰值 Heap 有真实证据且不存在 allocation failure；
12. appSystem 删除后动态内存能够回收；
13. 所有 Task 在最坏已测路径下有明确 Stack Headroom；
14. 不通过猜测缩减 Stack；
15. 完整 S07 代码和真实板回归通过后，S07A 才允许关闭。

## Open Design Items

在进入实施前还需要最终确认：

1. Startup Barrier 使用 `platform_event_flags` 的最小 API；
2. startup timeout 的具体值和 fatal policy；
3. `appMainTask` 第一版 stack 是否采用 2048 B；
4. 是否在 Platform Thread 增加正式 stack watermark 查询 API，还是继续仅作为诊断/测试能力。

## Approval

- Decision: `NOT_REVIEWED`
- Approved By: `Not approved yet`
- Design Commit: `Not created yet`
