# S07A RTOS Startup Refactor Handoff

## Metadata

- Stage: `S07A_RTOS_Startup_Refactor`
- Status: `READY_FOR_IMPLEMENTATION`
- Branch: `main`
- Baseline Commit: `254f510498748294f44b0c059796dbaeaccdea3e`
- Approved Design Commit: `e4ae9dea8f6ab0088829fb4acda29ab89c734132`
- Design Reference Update Commit: `4cc1d3defb56862e7d491f724cce8ed54d29cb74`
- Implementation Plan Commit: `fa8409c335727720e387887d254caada5939f346`
- Implementation Commit: `Not created yet`
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

## Implementation Output

- Status: `NOT_STARTED`

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
- No production code changed yet.

### Verification Results

Not started.

### Known Issues

No external blocker. Main implementation risks are startup heap peak, task-create scheduling order, and preserving S07 runtime behavior after file relocation.

### Review Focus

- FreeRTOS Task List must not contain appSystem after refactor；
- defaultTask must not overflow during Bootstrap；
- Startup Barrier must be broadcast-safe；
- DEGRADED must preserve unrelated working components；
- Task-local ownership must remain intact；
- App directory relocation must not create duplicate source/include paths；
- S07 full regression must remain PASS.
