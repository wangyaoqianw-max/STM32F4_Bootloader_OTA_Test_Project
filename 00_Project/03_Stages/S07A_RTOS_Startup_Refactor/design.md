# S07A RTOS Startup Refactor Design

## Metadata

- Stage: `S07A_RTOS_Startup_Refactor`
- Status: `DESIGN_APPROVED`
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
defaultTask (temporary bootstrap execution context)
    ↓
app_system_bootstrap() (ordinary function/module)
    ↓
Create IPC / Compose Runtime / Create Tasks
    ↓
Task-local Initialization
    ↓
DONE Barrier + Init Result
    ↓
RUNNING / DEGRADED / FAILED decision
    ↓
SYSTEM_RUN or SYSTEM_ABORT
    ↓
defaultTask Exit
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

- 将 `appSystem` 重定义为普通 Application System Composition / Bootstrap 模块，不再创建独立 `appSystem` RTOS Task；
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
- 验证 `defaultTask` Bootstrap 完成并删除后动态 Stack/TCB 内存可回收；
- 回归 S07 OTA、KEY、Display、Ymodem、Metadata PENDING 全链路；
- 同步整理 `01_APP` 物理目录，使目录结构直接反映 System / Task / Runtime / Contract 职责；
- 将现有 `app_main.c/.h` 在实施时重命名为 `app_main_task.c/.h`，避免与 `Core/main.c` 和 `appSystem` 语义混淆。

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
- 不改变 Application 分层边界；
- 不按 OTA / Display / LED 等业务功能在 App 层复制第二套 Service 风格目录；
- 不引入 `manager/controller/coordinator/handler` 等没有实际必要的角色层。

## Design

### 1. App Layer Physical Organization

S07A 同时整理 `01_APP` 的物理目录。目的不是增加新的架构层，而是让文件位置能够直接表达文件职责，解决当前所有 App 文件平铺后难以快速识别 System、Task、Runtime Wiring 和 IPC Contract 的问题。

目标目录冻结为：

```text
01_APP/
├─ system/
│  ├─ app_system.c
│  ├─ app_system.h
│  ├─ app_startup.c
│  └─ app_startup.h
│
├─ task/
│  ├─ app_main_task.c
│  ├─ app_main_task.h
│  ├─ app_ota_worker.c
│  ├─ app_ota_worker.h
│  ├─ app_display_task.c
│  └─ app_display_task.h
│
├─ runtime/
│  ├─ app_ota_runtime.c
│  └─ app_ota_runtime.h
│
├─ contract/
│  └─ app_runtime_contract.h
│
└─ README.md
```

目录职责：

```text
system/
→ Application 启动、生命周期、Composition Root、Startup Barrier

task/
→ 长期 RTOS Task 及其 task-local runtime entry

runtime/
→ Application 级依赖装配、Platform/Service instance wiring、Runtime Context

contract/
→ App 内 Task / Module 之间共享的数据契约、通知位和事件类型
```

边界规则：

1. `system/` 负责“系统如何组合并进入运行态”，不直接拥有 UART/SPI/GPIO HAL handle；
2. `task/` 只放长期 RTOS execution context，不放普通 Service 或 Driver；
3. `runtime/` 允许组合 Platform 与 Service 对象，但不承载 OTA 业务状态机；
4. `contract/` 只保存跨 Task/模块共享的紧凑数据合同，不保存实现状态；
5. App 层不再按 `ota/display/led/key` 业务名称重复建立一套层级结构；
6. 当前规模下不拆 `manager/controller/coordinator/handler` 等角色目录；
7. 只有后续确实出现新的 Runtime Wiring 或 Contract，才在对应目录新增文件，不预先制造空抽象。

文件迁移：

```text
01_APP/app_system.*          → 01_APP/system/app_system.*
01_APP/app_main.*            → 01_APP/task/app_main_task.*
01_APP/app_ota_worker.*      → 01_APP/task/app_ota_worker.*
01_APP/app_display_task.*    → 01_APP/task/app_display_task.*
01_APP/app_ota_runtime.*     → 01_APP/runtime/app_ota_runtime.*
01_APP/app_runtime_contract.h
                              → 01_APP/contract/app_runtime_contract.h
```

其中 `app_main.*` 不只是移动，还正式改名为 `app_main_task.*`。S07A 后三个长期 Application Task 的命名保持一致：

```text
appMainTask
otaWorker
displayTask
```

`app_startup.c/.h` 用于承载 Startup Context、Barrier bit/state 和启动同步辅助逻辑，避免把 Event Flags、Startup 状态和错误结果全部继续堆入 `app_system.c`。

### 2. 四阶段启动模型

Application 启动正式分成：

```text
Phase 1 - Hardware Bootstrap
Phase 2 - Application Composition
Phase 3 - Task-local Initialization
Phase 4 - Steady Runtime
```

#### Phase 1 - Hardware Bootstrap

由 CubeMX / Core 完成 MCU 外设基础初始化并启动 Kernel，不负责 Application 业务装配。

#### Phase 2 - Application Composition

FreeRTOS 启动后，CubeMX `defaultTask` 作为唯一临时 Bootstrap execution context，直接调用：

```c
platform_error_t app_system_bootstrap(void);
```

`app_system_bootstrap()` 是普通函数，不创建额外 `appSystem` Task。它负责：

```text
Startup Context creation
Shared IPC creation
Runtime dependency composition
appMainTask / otaWorker / displayTask creation
Startup supervision
Final RUNNING / DEGRADED / FAILED decision
SYSTEM_RUN / SYSTEM_ABORT publication
```

函数完成后返回 `defaultTask`，随后 `defaultTask` 自删除。

#### Phase 3 - Task-local Initialization

每个长期 Task 只初始化自己独占、且与 Task Identity/Ownership 有关的资源：

```text
appMainTask
→ foreground/LED local init
→ report MAIN_DONE + result
→ wait SYSTEM_RUN / SYSTEM_ABORT

otaWorker
→ obtain current thread handle
→ KEY binding
→ OTA Runtime / UART owner binding
→ report OTA_DONE + result
→ wait SYSTEM_RUN / SYSTEM_ABORT

displayTask
→ SPI1/ST7789/display local init
→ report DISPLAY_DONE + result
→ wait SYSTEM_RUN / SYSTEM_ABORT
```

System 负责初始化顺序和最终裁决，不等于所有 init 都在 defaultTask 栈上执行。

#### Phase 4 - Steady Runtime

`app_system_bootstrap()` 完成系统裁决后：

```text
RUNNING / DEGRADED
→ publish SYSTEM_RUN
→ return
→ defaultTask delete

FAILED
→ publish SYSTEM_ABORT
→ return failure
→ controlled fatal path
```

稳定运行态只保留长期业务 Task：

```text
appMainTask
otaWorker
displayTask
```

### 3. defaultTask 定位

`defaultTask` 是唯一临时 Bootstrap Task，不再只做“创建另一个 appSystem Task”的桥接。

目标：

```text
StartDefaultTask
↓
app_system_bootstrap()
↓
vTaskDelete(NULL)
```

第一版 stack 固定为：

```text
defaultTask = 4096 B
```

原因：它需要承载一次性系统装配、共享 IPC 创建和 Startup supervision；完成后其动态 stack/TCB 由 FreeRTOS Idle cleanup 回收。

不得在 `defaultTask` 中执行 Ymodem 数据处理、LCD 绘制、Flash 大数据处理等长期或重业务逻辑。

### 4. appSystem 定位

S07A 后不存在名为 `appSystem` 的 RTOS Task。

```text
appSystem
= Application System Composition Module
= Composition Root
= Startup Supervisor logic
```

推荐正式入口：

```c
platform_error_t app_system_bootstrap(void);
```

允许：

```text
Create shared Queue/Event Flags
Compose runtime objects
Create appMainTask / otaWorker / displayTask
Wait component DONE events
Read init results
Decide RUNNING / DEGRADED / FAILED
Publish SYSTEM_RUN / SYSTEM_ABORT
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

### 5. Task-local Initialization 原则

特别是当前 OTA Runtime：

```text
service_uart ownerThread = otaWorker
KEY event → otaWorker notification
```

因此与 Task Owner 绑定的 OTA 初始化必须由 `otaWorker` 自己完成。

Display SPI/ST7789 继续由 displayTask 初始化和独占。

前台 LED 继续由 appMainTask 初始化和使用。

Task start API 收敛为：

```text
bind provided startup/shared runtime resources
+
create task
```

共享 IPC 不允许再由某个业务 Task 的 start API 隐式创建。

### 6. Startup Barrier

采用通用 `platform_event_flags`，Impl 基于 CMSIS-RTOS2 Event Flags。

Platform 类型：

```c
typedef struct
{
    void *native;
} platform_event_flags_t;
```

冻结最小 API：

```c
platform_error_t platform_event_flags_create(
    platform_event_flags_t *eventFlags);

platform_error_t platform_event_flags_set(
    platform_event_flags_t *eventFlags,
    uint32 flags);

platform_error_t platform_event_flags_wait(
    platform_event_flags_t *eventFlags,
    uint32 flags,
    platform_bool_t waitAll,
    platform_bool_t clearOnExit,
    uint32 timeoutMs,
    uint32 *receivedFlags);

platform_error_t platform_event_flags_delete(
    platform_event_flags_t *eventFlags);
```

Impl 映射：

```text
osEventFlagsNew
osEventFlagsSet
osEventFlagsWait
osEventFlagsDelete
```

Startup 不再只上报 READY，而是上报“初始化已完成 + 初始化结果”。

冻结 flags：

```c
#define APP_STARTUP_DONE_MAIN       (1UL << 0)
#define APP_STARTUP_DONE_OTA        (1UL << 1)
#define APP_STARTUP_DONE_DISPLAY    (1UL << 2)

#define APP_STARTUP_RUN             (1UL << 8)
#define APP_STARTUP_ABORT           (1UL << 9)

#define APP_STARTUP_DONE_ALL        \
    (APP_STARTUP_DONE_MAIN |        \
     APP_STARTUP_DONE_OTA |         \
     APP_STARTUP_DONE_DISPLAY)
```

`APP_STARTUP_RUN` 为广播状态，Task wait 时不得由第一个消费者清除。

Startup Context 使用 Application static lifetime，不在 `defaultTask` 删除前销毁，避免 RUN 发布后仍有 Task 正从 Event Flags wait 返回时发生对象销毁竞态。

建议业务结构：

```c
typedef enum
{
    APP_SYSTEM_STATE_STARTING = 0,
    APP_SYSTEM_STATE_RUNNING,
    APP_SYSTEM_STATE_DEGRADED,
    APP_SYSTEM_STATE_FAILED
} app_system_state_t;

typedef struct
{
    platform_event_flags_t events;

    platform_error_t mainResult;
    platform_error_t otaResult;
    platform_error_t displayResult;

    app_system_state_t systemState;
} app_startup_context_t;
```

### 7. Startup Timeout and Failure Policy

统一 Startup Timeout：

```c
#define APP_SYSTEM_STARTUP_TIMEOUT_MS    (5000U)
```

组件明确 init error 与 Startup infrastructure failure 分开处理。

#### Component init error

任一长期业务 Task 完成本地 init 后，即使失败，也必须：

```text
save result
→ set its DONE bit
→ wait RUN / ABORT
```

如果 Runtime topology 建立完整，但一个或多个业务组件明确初始化失败：

```text
→ APP_SYSTEM_STATE_DEGRADED
→ publish SYSTEM_RUN
→ 成功组件继续工作
→ 失败组件不进入业务 loop，可安全退出
```

示例：

```text
Display fail
→ DEGRADED
→ appMainTask + otaWorker continue

OTA fail
→ DEGRADED
→ appMainTask + displayTask continue

appMain fail
→ DEGRADED
→ otaWorker + displayTask continue
```

没有某个具体业务 Task 被定义为“失败就整机必然 FAILED”。

#### Startup infrastructure failure

以下属于 `FAILED`：

```text
Startup Event Flags create failure
Required shared IPC create failure
Task create failure
Startup Barrier timeout
Startup synchronization corruption/error
```

`Startup Barrier timeout` 表示可能存在 deadlock、task crash 或 unexpected blocking，严重程度高于普通组件 init error。

`FAILED` 不发布 SYSTEM_RUN，只发布 SYSTEM_ABORT，并进入受控 fatal path。

### 8. IPC Creation Ownership

共享 IPC 由 `app_system_bootstrap()` 在创建业务 Task 前建立：

```text
Startup Event Flags
Display Queue
other cross-task shared IPC
```

Startup Context 与共享 IPC 使用 Application lifecycle；不得因为 `defaultTask` 自删除而销毁。

### 9. Stack / Heap Hard Constraints

当前 FreeRTOS：

```text
configTOTAL_HEAP_SIZE = 24576 B
```

S06 参考：

```text
xFreeBytesRemaining ≈ 7344 B
xMinimumEverFreeBytesRemaining ≈ 6720 B
```

S07A 第一版 stack 冻结：

```text
defaultTask   4096 B  temporary
appMainTask   2048 B  persistent
otaWorker     4096 B  persistent
displayTask   4096 B  persistent
```

原则：

1. 不在 Runtime 拓扑重构同时缩小 otaWorker/displayTask stack；
2. defaultTask 扩到 4096 B 后承担 Bootstrap；
3. appMainTask 第一版使用 2048 B；
4. defaultTask 不创建大块局部 buffer，不执行 OTA/LCD 大数据处理；
5. 创建所有长期 Task 时必须测 Startup Heap Peak；
6. defaultTask 删除后必须确认其动态 stack/TCB 被 Idle cleanup 回收；
7. 完整 OTA + Display + KEY 路径后重新测各长期 Task stack headroom；
8. 未获得真实板证据前不得进一步压栈；
9. 如 startup heap peak 不安全，优先调整生命周期/对象分配，不无依据扩大 heap。

需记录关键时刻：

```text
T0 Kernel started
T1 defaultTask bootstrap
T2 appMainTask created
T3 otaWorker created
T4 displayTask created
T5 all DONE
T6 SYSTEM_RUN / DEGRADED decision
T7 defaultTask deleted before Idle cleanup
T8 Idle cleanup completed
```

### 10. Stack Diagnostics

Platform Thread 正式增加最小诊断接口：

```c
platform_error_t platform_thread_get_stack_space(
    const platform_thread_t *thread,
    uint32 *freeStackBytes);
```

Impl 基于：

```text
osThreadGetStackSpace()
```

公共语义：

```text
remaining unused task stack
unit = byte
```

GDB A5 填充扫描继续作为独立交叉验证。

Heap 暂不增加 Platform abstraction。S07A 阶段验证直接使用 FreeRTOS/GDB：

```text
xPortGetFreeHeapSize()
xPortGetMinimumEverFreeHeapSize()
```

不新增 platform_heap_manager / RTOS diagnostics framework。

### 11. Runtime Ownership After Refactor

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

临时 execution context：

```text
defaultTask
→ app_system_bootstrap()
→ publish final startup decision
→ exit
```

`appSystem` 仅是普通模块，不再出现在 FreeRTOS Task List。

### 12. Relationship to S06/S07

保留：

```text
UART ISR/RX → otaWorker : Task Notification
otaWorker → displayTask : Queue
OTA Service V1
Metadata V2
KEY_1 double confirm
PENDING persistence
```

S07A 替换 S06 中：

```text
appSystem = foreground runtime task
```

为：

```text
defaultTask = temporary bootstrap task
appSystem   = non-task composition/bootstrap module
appMainTask = persistent foreground runtime task
```

## Interfaces and Data Flow

### Startup

```text
defaultTask
↓
app_system_bootstrap()
│
├─ create Startup Context / Event Flags
├─ create Display Queue
├─ start appMainTask
├─ start otaWorker
└─ start displayTask

appMainTask  ── MAIN_DONE + result ──────┐
otaWorker    ── OTA_DONE + result ───────┼→ appSystem logic
displayTask  ── DISPLAY_DONE + result ───┘

all DONE within 5000 ms
↓
RUNNING / DEGRADED
↓
SYSTEM_RUN
↓
app_system_bootstrap() returns
↓
defaultTask delete
```

若 Startup infrastructure failure / timeout：

```text
FAILED
↓
SYSTEM_ABORT
↓
controlled fatal path
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

- 业务组件明确 init error：组件上报 DONE + error；System 可进入 DEGRADED 并让其他成功组件运行；
- 失败组件收到 SYSTEM_RUN 后不得进入自身业务 loop，应安全退出或保持不可用状态；
- Startup Event Flags / required shared IPC / Task create 失败：FAILED；
- Startup timeout：FAILED；
- FAILED 不发布 SYSTEM_RUN，只发布 SYSTEM_ABORT；
- Stack warning/overflow：S07A 验收失败，禁止继续 S08；
- Heap allocation failure：记录具体创建阶段并进入 FAILED；
- defaultTask delete 后 heap 未回收：调查 Idle cleanup/lifecycle，不视为完成；
- OTA/Display 功能回归：按 S07 regression 处理，不允许以架构重构为理由接受功能退化。

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

- defaultTask 直接执行 `app_system_bootstrap()`；
- Runtime 裁决后 defaultTask 自删除；
- FreeRTOS Task List 中不存在独立 `appSystem` Task；
- 稳定运行态只保留预期长期业务 Task；
- Task priority 与 S07 一致；
- Startup READY 顺序可观察；
- Stack High Water Mark；
- Heap before/during/after defaultTask deletion；
- Idle cleanup 后 defaultTask stack/TCB 回收确认；
- RUNNING / DEGRADED / FAILED startup decision 证据。

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

1. `defaultTask` 是唯一临时 Bootstrap Task，直接执行 `app_system_bootstrap()`；
2. 不再创建独立 `appSystem` RTOS Task；
3. `appSystem` 仅作为 Composition Root / Startup Supervisor 普通模块；
4. 新增独立 `appMainTask` 承担 foreground Application behavior；
5. 共享 IPC 与 Startup Context 在业务 Task 创建前完成；
6. 三个长期 Task 均执行 task-local init，并上报 DONE + init result；
7. 高优先级 otaWorker 即使创建后立即抢占，也只能初始化并阻塞在 Startup Barrier；
8. Startup Barrier 使用 `platform_event_flags`，无 busy-loop；
9. 5000 ms 内全部 DONE 后，System 正确裁决 RUNNING 或 DEGRADED；
10. 普通组件 init error 不导致无关能力停机，成功组件在 DEGRADED 下继续工作；
11. Event Flags/shared IPC/Task create failure 或 Startup timeout 进入 FAILED，不发布 SYSTEM_RUN；
12. defaultTask 在 RUNNING/DEGRADED 发布后自删除；
13. 稳定 Task List 只保留 appMainTask / otaWorker / displayTask 等长期任务，不包含 appSystem；
14. otaWorker / displayTask / appMainTask ownership 清晰且无互相初始化对方私有硬件；
15. `01_APP` 已按 `system/task/runtime/contract` 分类；
16. `app_main.*` 已改名为 `app_main_task.*`；
17. Keil source groups/include paths 与 Host Test 引用同步完成；
18. `01_APP/README.md` 记录四类目录职责；
19. `platform_thread_get_stack_space()` 返回 byte 单位剩余 Stack；
20. Startup Peak Heap 有真实证据且不存在 allocation failure；
21. defaultTask 删除后动态 stack/TCB 能由 Idle cleanup 回收；
22. 所有长期 Task 在最坏已测路径下有明确 Stack Headroom；
23. 不通过猜测缩减 Stack；
24. S07 OTA Service / Metadata / Ymodem / KEY 双确认语义无变化；
25. 完整 S07 Host/Build/真实板回归通过后，S07A 才允许关闭。

## Frozen Design Decisions

进入实施前的开放项已经全部收束：

1. Startup Barrier：新增 `platform_event_flags`，最小 API 为 create/set/wait/delete；
2. Startup timeout：`5000 ms`；
3. Startup policy：业务组件 init error → DEGRADED；Runtime infrastructure failure / timeout → FAILED；
4. Bootstrap execution context：只使用 CubeMX `defaultTask`，不再创建 appSystem Task；
5. Stack budget：defaultTask 4096 B、appMainTask 2048 B、otaWorker 4096 B、displayTask 4096 B；
6. Stack diagnostics：新增 `platform_thread_get_stack_space()`，单位 byte；
7. Heap diagnostics：不新增 Platform abstraction，继续使用 FreeRTOS/GDB；
8. App 目录：冻结为 `system/task/runtime/contract`。

## Approval

- Decision: `APPROVED`
- Approved By: `Project Owner`
- Approval Date: `2026-09-18`
- Design Commit: `e4ae9dea8f6ab0088829fb4acda29ab89c734132`
