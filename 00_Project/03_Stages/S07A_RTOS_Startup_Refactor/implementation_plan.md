# S07A RTOS Startup Refactor Implementation Plan

## Metadata

- Stage: `S07A_RTOS_Startup_Refactor`
- Design: `00_Project/03_Stages/S07A_RTOS_Startup_Refactor/design.md`
- Baseline Commit: `254f510498748294f44b0c059796dbaeaccdea3e`
- Design Commit: `5ce9ceffea60fde87fe00107bf1d2648a8e26b62`
- Status: `DRAFT / NOT_STARTED`

> 本计划为设计讨论输入。Design Approval 前不得执行生产代码修改。

## Global Constraints

- 只重构 Application RTOS 启动生命周期，不修改 S07 OTA 业务合同。
- `defaultTask` 保持 CubeMX bridge，调用 Application System 后退出。
- `appSystem` 改为一次性 Bootstrap/Supervisor，不承担长期前台业务。
- 新增独立 `appMainTask`。
- Task-local resource init 保持资源 ownership，不把所有初始化机械搬入 `app_system.c`。
- 使用显式 Startup Barrier，禁止依赖“创建任务后暂时不会运行”的假设。
- 启动等待不得 busy-loop。
- 现有 otaWorker / displayTask stack 在获得新板测前不得缩减。
- `appSystem` 初始 stack 保持 4096 B。
- 新 appMainTask stack 第一版目标 2048 B，但 Design Approval 前仍可调整。
- 必须记录 startup peak heap、minimum ever free heap、各 Task stack high-water。
- S07 Host/Build/Board regression 是本阶段关闭条件。
- 不实现 S08/S09/S10 功能。

## Tasks

### Task 0: Baseline Capture and Startup Audit

**Goal:** 在修改拓扑前固定当前 S07 运行时事实和 RAM 基线。

**Inspect:**

- `Core/Src/freertos.c`
- `01_APP/app_system.*`
- `01_APP/app_main.*`
- `01_APP/app_ota_worker.*`
- `01_APP/app_ota_runtime.*`
- `01_APP/app_display_task.*`
- Platform OS Thread/Queue/Notify
- `FreeRTOSConfig.h`

- [ ] 记录当前 Task 拓扑、priority、stack bytes、创建顺序和 ownership。
- [ ] 重新确认 `platform_thread_create().stackSizeBytes` 单位为 byte。
- [ ] 记录当前 S07 稳态 Heap、Minimum Ever Free Heap 和 Task Stack High Water。
- [ ] 记录现有 `appSystem` / otaWorker / displayTask 创建与退出行为。
- [ ] Build/RTT/GDB baseline PASS 后提交。

### Task 1: Platform Event Flags / Startup Synchronization

**Goal:** 增加可复用的 Startup Barrier primitive。

**Preferred Design:**

```text
platform_event_flags
↓
CMSIS-RTOS2 osEventFlags
```

- [ ] 调查 CMSIS wrapper 当前 event flags 配置与项目命名规范。
- [ ] 定义最小 create/set/wait/delete API。
- [ ] 保持 Platform 不暴露 FreeRTOS EventGroup/CMSIS native handle。
- [ ] Host/compile test。
- [ ] 提交并记录 Commit。

### Task 2: Split Persistent appMainTask

**Goal:** 将 foreground behavior 从 `appSystem` 分离。

- [ ] 将现有 `app_main()` 长期 loop 改为独立 Task entry 或等价 runtime entry。
- [ ] appMainTask 自己初始化 LED/foreground private resources。
- [ ] local init 完成后发布 `APP_READY`。
- [ ] 等待 `SYSTEM_RUN` 后才进入 Blink/Breath 等业务 loop。
- [ ] 第一版 stack 使用保守预算，不先做栈优化。
- [ ] Build + foreground smoke 后提交。

### Task 3: Refactor appSystem into Bootstrap/Supervisor

**Goal:** 让 `appSystem` 只做 Application composition 和 startup supervision。

- [ ] appSystem 创建 Startup Event Flags。
- [ ] appSystem 创建 Display Queue 等共享 IPC。
- [ ] 按依赖关系创建 appMainTask / otaWorker / displayTask。
- [ ] 等待 required READY bits，带 bounded startup timeout。
- [ ] 任一 required init 失败时不得发布 SYSTEM_RUN。
- [ ] 全部 READY 后发布 SYSTEM_RUN。
- [ ] appSystem 自删除。
- [ ] 验证稳定运行态不再存在 appSystem。
- [ ] 提交并记录 Commit。

### Task 4: Normalize Task-local Initialization and Ownership

**Goal:** 将初始化放回正确 owner，而不是集中到错误 Task。

- [ ] otaWorker 保留 current thread handle、UART owner、KEY notification、OTA runtime init。
- [ ] displayTask 保留 SPI1/ST7789 private initialization。
- [ ] appMainTask 保留 foreground/LED private initialization。
- [ ] Task start API 只绑定共享 runtime resource + create task，不隐式创建别的模块资源。
- [ ] 共享 IPC 由 appSystem 统一创建。
- [ ] 检查没有 HAL handle 泄漏到 APP composition。
- [ ] 提交并记录 Commit。

### Task 5: Startup Failure and Runtime State Diagnostics

**Goal:** 让启动成功/失败具有明确状态和日志。

- [ ] 增加 Startup READY/RUN/FAILED 日志或诊断状态。
- [ ] 验证高优先级 otaWorker 创建后立即抢占时只完成 local init 并等待 RUN。
- [ ] 验证 startup timeout 能指出未 Ready component。
- [ ] 不增加长期轮询 Task。
- [ ] 提交并记录 Commit。

### Task 6: Stack / Heap Verification

**Goal:** 证明新拓扑不会因启动阶段并发创建任务导致 Stack/Heap 问题。

**Required Evidence:**

```text
defaultTask stack
appSystem startup stack
appMainTask stack
otaWorker stack
displayTask stack

free heap before task creation
startup minimum-ever-free heap
free heap after SYSTEM_RUN
free heap after appSystem Idle cleanup
```

- [ ] 保持 otaWorker/displayTask/appSystem 原有保守 stack 进行第一轮板测。
- [ ] 覆盖 Idle、OTA receiving、display render、READY_TO_INSTALL、failure path。
- [ ] 使用 GDB A5 扫描和/或 `uxTaskGetStackHighWaterMark()`。
- [ ] 验证 appSystem delete 后 stack/TCB 被 Idle Task 回收。
- [ ] 如果 appMainTask 2048 B 余量不足，先增大，不为追求 RAM 数字冒险。
- [ ] 未获得证据前不缩栈。
- [ ] 将真实值写入 Verification Report。

### Task 7: S07 Regression and Documentation

**Goal:** 确认启动重构没有改变 OTA 功能。

- [ ] Keil Build。
- [ ] S07 Metadata / OTA Service / Sink / Ymodem Host regression。
- [ ] KEY_1 start。
- [ ] Ymodem receive。
- [ ] LCD RECEIVE/VERIFY/READY/FAILED。
- [ ] second KEY → PENDING。
- [ ] Reset persistence。
- [ ] interrupted transfer / bad CRC。
- [ ] 更新 S07A verification / handoff / review。
- [ ] 同步 Runtime 文档和 Roadmap。
- [ ] Project Owner 硬件验收后再关闭阶段。

## Final Verification

```text
Boot
↓
defaultTask
↓
appSystem Bootstrap
↓
Create IPC / Tasks
↓
Task-local Init
↓
APP_READY + OTA_READY + DISPLAY_READY
↓
SYSTEM_RUN
↓
appSystem deleted
↓
appMainTask + otaWorker + displayTask stable runtime
```

必须同时证明：

```text
Stack safe
Heap safe
No startup race
No busy wait
S07 OTA behavior unchanged
```

## Completion Condition

设计正式批准后执行本计划。代码、Host/Build、GDB RAM evidence 和真实板 S07 regression 全部通过，且 Review 通过后才能将 S07A 标记为 `CLOSED / PASS`。
