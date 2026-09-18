# S07A RTOS Startup Refactor Handoff

## Metadata

- Stage: `S07A_RTOS_Startup_Refactor`
- Status: `READY_FOR_VERIFICATION`
- Branch: `main`
- Baseline Commit: `254f510498748294f44b0c059796dbaeaccdea3e`
- Approved Design Commit: `e4ae9dea8f6ab0088829fb4acda29ab89c734132`
- Design Reference Update Commit: `4cc1d3defb56862e7d491f724cce8ed54d29cb74`
- Implementation Plan Commit: `fa8409c335727720e387887d254caada5939f346`
- Implementation Commit: `306c76b`
- Verification Commit: `Not created yet`

## Implementation Input

### Goal

在 S08 Bootloader Foundation 前，整理 Application RTOS 启动生命周期和 App 层物理结构，消除旧 `appSystem` Task 的职责混合，并重新验证 Startup Stack / Heap 安全。

### Stable Upstream Contract

S07 已 `CLOSED / PASS`，以下必须保持：

```text
OTA Service V1
Metadata V2
KEY_1 double confirmation
Ymodem receive
External A/B firmware image slots
PENDING persistence
Display flow
UART ISR/RX → otaWorker notification
otaWorker → displayTask queue
```

### Frozen Startup Model

S07A 不再创建独立 `appSystem` RTOS Task。

```text
main / CubeMX
↓
RTOS Kernel
↓
defaultTask (temporary, 4096 B)
↓
app_system_bootstrap()
│
├─ create Startup Context / Event Flags
├─ create shared IPC
├─ create appMainTask
├─ create otaWorker
└─ create displayTask
↓
Task-local Init
↓
MAIN_DONE + OTA_DONE + DISPLAY_DONE
↓
RUNNING / DEGRADED / FAILED
├─ RUNNING/DEGRADED → SYSTEM_RUN
└─ FAILED           → SYSTEM_ABORT
↓
app_system_bootstrap() return
↓
defaultTask delete
↓
steady runtime
```

`appSystem` 是普通 Composition Root / Startup Supervisor 模块，不是 Task。

### Startup Policy

- Startup timeout：`5000 ms`。
- 业务组件明确 init error：上报 DONE + error，System 进入 `DEGRADED`，其他成功组件继续运行。
- Event Flags / required shared IPC / Task create failure：`FAILED`。
- Startup timeout：`FAILED`。
- FAILED 不发布 SYSTEM_RUN，只发布 SYSTEM_ABORT。
- 高优先级 otaWorker 创建后即使立即抢占，也必须在 local init 后阻塞于 Startup Barrier。

### Platform OS Additions

新增最小 `platform_event_flags`：

```text
create
set
wait
delete
```

Impl 使用 CMSIS-RTOS2 `osEventFlags*`。

Platform Thread 新增：

```c
platform_error_t platform_thread_get_stack_space(
    const platform_thread_t *thread,
    uint32 *freeStackBytes);
```

单位固定为 byte，Impl 使用 `osThreadGetStackSpace()`。

Heap 不增加 Platform abstraction；S07A 验证直接使用 FreeRTOS/GDB。

### App Layer Directory Contract

```text
01_APP/
├─ system/
│  ├─ app_system.*
│  └─ app_startup.*
├─ task/
│  ├─ app_main_task.*
│  ├─ app_ota_worker.*
│  └─ app_display_task.*
├─ runtime/
│  └─ app_ota_runtime.*
├─ contract/
│  └─ app_runtime_contract.h
└─ README.md
```

职责：

```text
system   → Bootstrap / Composition Root / Startup Barrier
task     → persistent RTOS execution contexts
runtime  → Application dependency wiring / Runtime Context
contract → cross-task data contracts
```

迁移时必须同步 Keil source groups、include paths、Host Test includes 和所有引用。

### Runtime Ownership

```text
appMainTask
→ foreground Application behavior / LED

otaWorker
→ UART/Ymodem/KEY OTA event owner
→ service_ota execution shell

displayTask
→ Display Queue consumer
→ SPI1/ST7789 sole owner
```

Task-local private init 继续由各自 owner 执行，不搬到 app_system。

### Stack / Heap Frozen First-pass Budget

```text
defaultTask   4096 B temporary
appMainTask   2048 B persistent
otaWorker     4096 B persistent
displayTask   4096 B persistent

configTOTAL_HEAP_SIZE = 24576 B
```

要求：

- 不先缩 otaWorker/displayTask stack；
- 测 startup peak heap / minimum-ever-free heap；
- 确认 defaultTask 删除后 stack/TCB 经 Idle cleanup 回收；
- 用 `platform_thread_get_stack_space()` + GDB A5 扫描交叉验证；
- 覆盖 idle / OTA receive / display render / READY_TO_INSTALL / failure path。

### Required Reading

1. `AGENTS.md`
2. `PROJECT_CONTEXT.md`
3. `00_Project/WORKFLOW.md`
4. `03_Firmware/AGENTS.md`
5. C coding standard
6. S06 design/handoff/verification
7. S07 design/handoff/review/verification
8. S07A `design.md`
9. S07A `implementation_plan.md`

### Prohibited Changes

- Bootloader / Internal Flash installation；
- Trial / Confirm / Rollback；
- OTA Metadata / Image contract changes；
- 新的长期 manager task；
- busy-loop startup wait；
- 无证据压缩 stack；
- 把所有 private hardware init 塞进 app_system；
- 为 S07A 新建 heap manager 或大型 RTOS diagnostics framework；
- 按 OTA / Display / LED 再复制一套 App 子架构。

## Task 0 Baseline Capture

采集日期：`2026-09-18`。

```text
Branch                  main
HEAD                    f43e44e034e8df48dce39d275ed5c716e5e7a1dc
Worktree                clean
Current topology        defaultTask → appSystem → app_main / otaWorker / displayTask
Task stacks             defaultTask 512 B; appSystem 4096 B; otaWorker 4096 B; displayTask 4096 B
Task priorities         appSystem NORMAL; otaWorker ABOVE_NORMAL; displayTask BELOW_NORMAL
Keil Build              PASS, 0 error / 0 warning
Flash Run               PASS
RTT Capture             PASS, old appSystem/otaWorker/displayTask startup observed
GDB Snapshot            PASS, stopped in FreeRTOS prvIdleTask
Reference Heap          xFreeBytesRemaining=7344 B; minimum-ever=6720 B
Reference Stack         appSystem=3680 B; otaWorker=3412 B; displayTask=3224 B
```

基线确认：`appSystem` 仍为独立长期 RTOS Task；Display Queue 仍由 `displayTask` 创建；三个 Task 尚未使用统一 Startup Barrier。以上是 S07A 实施前基线，不是 S07A 最终验收证据。

## Implementation Output

- Status: `READY_FOR_VERIFICATION`
- Implementation Commit: `306c76b` (`refactor: implement s07a rtos startup topology`)

### Completed Design Work

- S07A Stage created and inserted before S08.
- Startup model frozen.
- Independent appSystem Task removed from target architecture.
- Startup Event Flags API frozen.
- RUNNING / DEGRADED / FAILED policy frozen.
- Startup timeout frozen at 5000 ms.
- Stack first-pass budget frozen.
- Stack-space diagnostic API frozen.
- `01_APP/system/task/runtime/contract` directory contract frozen.
- `app_main.* → app_main_task.*` rename frozen.
- Implementation plan finalized.
- Task 0 baseline captured and implementation completed in `306c76b`.

### Delivered Implementation

- 新增 `platform_event_flags` opaque Platform OS API 和 CMSIS-RTOS2 FreeRTOS implementation。
- 新增 `platform_thread_get_stack_space()`，返回 byte 单位的剩余 Stack。
- 将 `01_APP` 整理为 `system / task / runtime / contract`，同步 Keil Group、include path、Host Test 和源码引用；旧平面生产文件已迁移，不重复参与编译。
- 新增 Startup Context / Event Flags Barrier；`SYSTEM_RUN` 使用广播等待，不使用 busy-loop。
- `defaultTask` 直接调用 `app_system_bootstrap()`，完成装配后自删除；不再创建独立 `appSystem` RTOS Task。
- 长期 Runtime 为 `appMainTask / otaWorker / displayTask`，Task-local hardware/runtime init ownership 保持不变。
- 未实现 S08/S09/S10 功能，未加入生产 fault injection 开关或测试专用业务逻辑。

### Style Review Corrections

根据嵌入式 C 代码规范复查新增和迁移文件，已修正公开 API 的 Doxygen 参数/返回格式、只读 Startup Context 的 `const` 边界、静态函数分区、冗余 display 状态变量、直接依赖 include，以及一个无效 unsigned 下界比较导致的编译告警。改动文件通过 whitespace/line-length 检查。

### Verification Results

代码和自动化验证已完成，真实 S07 物理全流程仍待在 S07A 当前固件上重新执行。

```text
Keil Build                         PASS, 0 error / 0 warning
S07A Startup Host Test             PASS
S07 Host regression                7/7 PASS
S04 Host regression                PASS; persistence 15/15
S05 Host regression                PASS
Python Pack / Ymodem tests         PASS; 2 / 24
git diff --check                   PASS
Keil project XML parse             PASS
style whitespace/line-length       PASS
Flash / Reset / RTT smoke          PASS
GDB runtime snapshot               PASS
```

### Current Board and RAM Evidence

当前正常启动/idle 路径的 GDB 证据：

```text
xFreeBytesRemaining             9352 B
xMinimumEverFreeBytesRemaining  5144 B
configTOTAL_HEAP_SIZE           24576 B
peak allocated heap (derived)    19432 B
uxCurrentNumberOfTasks           6
uxDeletedTasksWaitingCleanUp     0
startupState                     RUNNING (1)
g_appSystemBootstrapped          1
g_appStartupInitialized          1
```

```text
appMainTask stack high water     407 words  (~1628 B)
otaWorker stack high water       908 words  (~3632 B)
displayTask stack high water     800 words  (~3200 B)
```

Task count 6 包含 FreeRTOS idle/timer 和 EasyLogger 等系统任务；采样时待 Idle cleanup 列表为空。Stack 日志没有出现在本次 RTT capture 中，以上 Stack 数据来自 GDB，与 Platform API 交叉采集。当前尚未取得 OTA receiving、display render、READY_TO_INSTALL 和 failure path 的 S07A 新 Stack 证据。

当前板级工具链已证明 Flash/Reset/RTT 启动冒烟和正常 RUNNING，但 S07 KEY/Ymodem/PENDING/Reset/interrupted/bad CRC 完整物理交互尚未用 S07A 当前固件重跑。

### Known Issues

无设计冲突或外部实现阻塞。未完成项是当前 S07A 固件的完整物理业务回归，以及真实 degraded/failure fault path 证据；在这些证据完成前不得进入 `READY_FOR_REVIEW` 或 `CLOSED / PASS`。

### Review Focus

- FreeRTOS Task List must not contain appSystem after refactor；
- defaultTask must not overflow during Bootstrap；
- Startup Barrier must be broadcast-safe；
- DEGRADED must preserve unrelated working components；
- Task-local ownership must remain intact；
- App directory relocation must not create duplicate source/include paths；
- S07 full regression must remain PASS；当前验证状态为 `PENDING`。
- 需要确认 `uxCurrentNumberOfTasks=6` 中系统任务的解释与 defaultTask 删除后的回收证据。
