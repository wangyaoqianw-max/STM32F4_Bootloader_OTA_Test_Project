# S07A RTOS Startup Refactor Handoff

## Metadata

- Stage: `S07A_RTOS_Startup_Refactor`
- Status: `DRAFT / DESIGN_DISCUSSION`
- Branch: `main`
- Baseline Commit: `254f510498748294f44b0c059796dbaeaccdea3e`
- Design Commit: `284fe05e689e48d06267649c449266e1e8c029e3`
- Implementation Plan Commit: `4c4dc83e217da108f1e5d713fa2e06ecf339fff8`
- Implementation Commit: `Not created yet`
- Verification Commit: `Not created yet`

## Implementation Input

### Goal

在 S08 Bootloader Foundation 之前，整理 Application RTOS 启动生命周期，使 System Bootstrap、Task-local Init 和 Steady Runtime 有清晰边界，同时重新验证 Stack/Heap 安全。

### Upstream Stable Baseline

S07 已 `CLOSED / PASS`，必须保持：

```text
OTA Service V1
Metadata V2
KEY_1 double confirmation
Ymodem receive
External A/B image storage
PENDING persistence
Display flow
```

当前生产启动事实：

```text
defaultTask
→ app_system_start()
→ defaultTask delete

appSystem
→ start displayTask
→ start otaWorker
→ app_main() forever
```

S07A 要修正的是 `appSystem` 的长期职责，而不是重做 S07。

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

### Proposed Runtime

```text
defaultTask
→ appSystem Bootstrap
→ create shared IPC/startup sync
→ create appMainTask / otaWorker / displayTask
→ tasks perform local init
→ tasks report READY
→ appSystem publishes SYSTEM_RUN
→ appSystem exits

steady runtime:
appMainTask + otaWorker + displayTask
```

### Resource Ownership

```text
appSystem
→ composition / shared IPC / startup supervision

appMainTask
→ foreground business / LED

otaWorker
→ UART/Ymodem/KEY OTA events / service_ota execution shell

displayTask
→ SPI1/ST7789/display rendering
```

### App Layer Directory Contract

S07A 同时整理 `01_APP`，使物理目录直接表达职责。目标结构：

```text
01_APP/
├─ system/
│  ├─ app_system.c
│  ├─ app_system.h
│  ├─ app_startup.c
│  └─ app_startup.h
├─ task/
│  ├─ app_main_task.c
│  ├─ app_main_task.h
│  ├─ app_ota_worker.c
│  ├─ app_ota_worker.h
│  ├─ app_display_task.c
│  └─ app_display_task.h
├─ runtime/
│  ├─ app_ota_runtime.c
│  └─ app_ota_runtime.h
├─ contract/
│  └─ app_runtime_contract.h
└─ README.md
```

职责：

```text
system   → Bootstrap / Composition Root / Startup Barrier
task     → 长期 RTOS execution context
runtime  → Application dependency wiring / Runtime Context
contract → App 内跨 Task 数据契约
```

迁移规则：

```text
app_system.*          → system/
app_main.*            → task/app_main_task.*
app_ota_worker.*      → task/
app_display_task.*    → task/
app_ota_runtime.*     → runtime/
app_runtime_contract.h→ contract/
```

不要按 OTA / Display / LED 再复制一套 App 子架构，也不要引入无必要的 manager/controller/coordinator 目录。

`app_startup.c/.h` 专门承载 Startup Context、Barrier 状态/位和启动同步辅助逻辑，避免继续把所有启动状态塞入 `app_system.c`。

### Stack / Heap Safety

不得在本阶段一开始压缩现有栈。

当前参考：

```text
appSystem   4096 B
otaWorker   4096 B
displayTask 4096 B
defaultTask 512 B

configTOTAL_HEAP_SIZE 24576 B
```

新 appMainTask 第一版建议 2048 B，必须由真实 High Water Mark 再决定是否调整。

必须检查创建新长期 Task 后的 startup heap peak，以及 appSystem 删除后的 heap 回收。

### Prohibited Changes

- Bootloader implementation；
- Internal Flash installation；
- Trial / Confirm / Rollback；
- OTA Metadata 语义变更；
- 新的长期 manager task；
- 忙等 startup synchronization；
- 无实测依据压缩 stack；
- 把所有 private hardware init 塞进 appSystem。

### Acceptance Direction

S07A 完成后必须看到：

```text
defaultTask exit
appSystem exit after startup
appMainTask running
otaWorker blocked/running as expected
displayTask blocked/running as expected
no startup race
safe stack
safe heap
S07 full regression pass
```

## Implementation Output

- Status: `NOT_STARTED`

### Completed Work

- S07A Stage created.
- Initial RTOS startup refactor design drafted.
- App layer directory role model (`system/task/runtime/contract`) frozen in design.
- `app_main.* → app_main_task.*` rename direction frozen.
- Preliminary implementation plan drafted.
- No production code changed yet.

### Changed Files

- `00_Project/03_Stages/S07A_RTOS_Startup_Refactor/design.md`
- `00_Project/03_Stages/S07A_RTOS_Startup_Refactor/implementation_plan.md`
- `00_Project/03_Stages/S07A_RTOS_Startup_Refactor/handoff.md`
- Roadmap / current context files as recorded by subsequent commits.

### Open Design Items

- Startup Event Flags final public API；
- startup timeout / fatal policy；
- appMainTask initial stack budget final value；
- stack watermark 是否进入正式 Platform API。

### Verification Results

Not started.

### Known Issues

None. Stage is intentionally in design discussion.

### Review Focus

- 是否真正分离 Bootstrap 与 Runtime；
- `01_APP/system/task/runtime/contract` 是否与真实职责一致，是否避免目录过度抽象；
- `app_main_task` 是否真正成为长期 foreground task，而不是新的混合入口；
- 是否保持 Task-local ownership；
- 是否存在高优先级 Task creation race；
- startup stack/heap peak 是否有证据；
- 是否完整保留 S07 功能。
