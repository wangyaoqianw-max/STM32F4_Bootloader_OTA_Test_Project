# S07A RTOS Startup Refactor Implementation Plan

## Metadata

- Stage: `S07A_RTOS_Startup_Refactor`
- Design: `00_Project/03_Stages/S07A_RTOS_Startup_Refactor/design.md`
- Baseline Commit: `254f510498748294f44b0c059796dbaeaccdea3e`
- Approved Design Commit: `e4ae9dea8f6ab0088829fb4acda29ab89c734132`
- Status: `READY_FOR_IMPLEMENTATION`
- Branch: `main`

## Global Constraints

- 只重构 Application RTOS 启动生命周期和 App 目录组织，不修改 S07 OTA 业务合同。
- `defaultTask` 是唯一临时 Bootstrap Task；不得再创建独立 `appSystem` Task。
- `appSystem` 是普通 Composition Root / Startup Supervisor 模块，正式入口为 `app_system_bootstrap()` 或设计等价命名。
- 长期 Runtime 仅保留 `appMainTask / otaWorker / displayTask`。
- Task-local resource init 保持资源 ownership，不把私有硬件初始化机械搬入 `app_system.c`。
- 使用显式 `platform_event_flags` Startup Barrier，禁止 busy-loop。
- 业务组件 init error 可进入 `DEGRADED`；Runtime infrastructure failure / startup timeout 进入 `FAILED`。
- Startup timeout 固定 `5000 ms`。
- 第一版 stack：defaultTask 4096 B、appMainTask 2048 B、otaWorker 4096 B、displayTask 4096 B。
- 未获得新板测证据前不得缩小 otaWorker/displayTask stack。
- 必须记录 startup peak heap、minimum-ever-free heap、defaultTask 删除后的 heap 回收和各长期 Task stack space。
- S07 Host/Build/Board regression 是关闭条件。
- 不实现 S08/S09/S10 功能。
- 每个 Task 独立验证、`git diff --check`、单一职责提交，并将 Commit 写回 handoff。

## Task 0: Baseline Capture and Startup Audit

**Goal:** 修改前固定当前 S07 Runtime 和 RAM 基线。

- [ ] 读取当前 `freertos.c`、`01_APP`、Platform OS、FreeRTOSConfig 和 S06/S07 verification。
- [ ] 记录当前 `defaultTask → appSystem → otaWorker/displayTask/app_main` 拓扑、priority、stack bytes 和 ownership。
- [ ] 确认 `platform_thread_create().stackSizeBytes` 单位为 byte。
- [ ] 记录当前稳定态 free heap、minimum-ever-free heap 和现有 Task stack evidence。
- [ ] Build/RTT/GDB baseline PASS。
- [ ] 提交并记录 Commit。

## Task 1: Platform Event Flags and Stack Space API

**Goal:** 增加 S07A 需要的最小可复用 OS 能力。

### Event Flags

新增 opaque `platform_event_flags_t`，最小接口冻结为：

```text
create
set
wait
delete
```

Impl 使用 CMSIS-RTOS2：

```text
osEventFlagsNew
osEventFlagsSet
osEventFlagsWait
osEventFlagsDelete
```

### Stack Space

Platform Thread 增加：

```c
platform_error_t platform_thread_get_stack_space(
    const platform_thread_t *thread,
    uint32 *freeStackBytes);
```

Impl 使用 `osThreadGetStackSpace()`，返回单位必须为 byte。

- [ ] 实现类型/API/Impl。
- [ ] 验证 RUN bit 多 Task 等待时不得 clear-on-exit。
- [ ] Host/compile tests。
- [ ] 不新增 heap manager / RTOS diagnostics framework。
- [ ] 提交并记录 Commit。

## Task 2: Reorganize 01_APP Physical Layout

**Goal:** 让文件物理位置直接表达 App 层职责。

目标：

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

- [ ] 移动现有 App 文件到对应目录。
- [ ] `app_main.*` 正式改名 `app_main_task.*`。
- [ ] 新增 `app_startup.*` 承载 Startup Context / Barrier / system state。
- [ ] 更新 Keil source groups、include paths、Host Test includes 和其他引用。
- [ ] 更新 `01_APP/README.md` 说明 system/task/runtime/contract 职责。
- [ ] 不引入 manager/controller/coordinator 等额外目录。
- [ ] Clean Build 后提交。

## Task 3: Create Startup Context and Barrier Contract

**Goal:** 建立统一的 Task init result + final system decision。

冻结 DONE/RUN/ABORT contract：

```text
MAIN_DONE
OTA_DONE
DISPLAY_DONE
SYSTEM_RUN
SYSTEM_ABORT
```

System states：

```text
STARTING
RUNNING
DEGRADED
FAILED
```

- [ ] Startup Context 使用 Application static lifetime。
- [ ] 保存 main/ota/display init result。
- [ ] Task init 完成后无论成功失败都上报对应 DONE。
- [ ] Task 上报 DONE 后等待 RUN / ABORT。
- [ ] SYSTEM_RUN 为广播状态，不由第一个 Task 消费后清除。
- [ ] 单测 state/result/flag transitions。
- [ ] 提交并记录 Commit。

## Task 4: Split Persistent appMainTask

**Goal:** 将 foreground behavior 从旧 appSystem 分离为正式长期任务。

- [ ] appMainTask stack 初始 2048 B。
- [ ] Task-local 初始化 status LED / foreground private resources。
- [ ] 初始化结果写入 Startup Context 并上报 MAIN_DONE。
- [ ] 等待 RUN / ABORT。
- [ ] 成功 + RUN 后进入 Blink/Breath 等 foreground loop。
- [ ] init fail + RUN 时不得进入业务 loop，应安全退出。
- [ ] foreground smoke / Build PASS 后提交。

## Task 5: Adapt otaWorker and displayTask to Startup Barrier

**Goal:** 保留资源 ownership，同时消除 Task create 后立即运行的启动竞态。

### otaWorker

- [ ] 保留 current thread handle、KEY、UART owner、OTA Runtime init。
- [ ] 完成 init 后保存结果并上报 OTA_DONE。
- [ ] 在 RUN / ABORT 前不得进入 KEY/UART 正式业务等待。
- [ ] 保持 S07 OTA execution shell 语义。

### displayTask

- [ ] 保留 SPI1/ST7789/display private init。
- [ ] 完成 init 后保存结果并上报 DISPLAY_DONE。
- [ ] RUN 后才进入 Display Queue 正式业务循环。
- [ ] display init fail 时允许系统 DEGRADED，不阻断 OTA/foreground。

- [ ] Build + targeted Host regression 后提交。

## Task 6: Refactor appSystem into Non-task Bootstrap Module

**Goal:** 移除独立 appSystem Task，只保留 `defaultTask → app_system_bootstrap()`。

- [ ] 删除/停止创建 `g_appSystemThread` 和 appSystem thread config。
- [ ] `defaultTask` stack 调整为 4096 B。
- [ ] `StartDefaultTask()` 直接调用 `app_system_bootstrap()`。
- [ ] Bootstrap 创建 Startup Event Flags、Display Queue 等共享 IPC。
- [ ] Bootstrap 创建 appMainTask / otaWorker / displayTask。
- [ ] 等待 DONE_ALL，timeout = 5000 ms。
- [ ] 全部 init OK → RUNNING。
- [ ] 任一业务组件明确 init error → DEGRADED。
- [ ] Event Flags/shared IPC/Task create failure 或 timeout → FAILED。
- [ ] RUNNING/DEGRADED 发布 SYSTEM_RUN；FAILED 发布 SYSTEM_ABORT。
- [ ] bootstrap 返回后 defaultTask 自删除；FAILED 进入受控 fatal path。
- [ ] 稳态 FreeRTOS Task List 中不存在 appSystem。
- [ ] 提交并记录 Commit。

## Task 7: Startup Failure / Degraded-mode Verification

**Goal:** 验证“能力故障可降级，Runtime 拓扑故障才失败”。

- [ ] 正常启动 → RUNNING。
- [ ] Display init fault injection → DEGRADED，foreground + OTA 继续。
- [ ] OTA init fault injection → DEGRADED，foreground + display 继续。
- [ ] appMain init fault injection → DEGRADED，OTA + display 继续。
- [ ] Task create / startup synchronization failure 使用可控 Host/Fault injection 验证 FAILED。
- [ ] Startup timeout 路径验证 FAILED，不发布 SYSTEM_RUN。
- [ ] 所有临时 fault injection 不得遗留 production 开关默认开启。
- [ ] 提交并记录 Commit。

## Task 8: Stack / Heap Verification

**Goal:** 证明新的 Bootstrap 拓扑不会在启动峰值耗尽 RAM。

第一版 stack：

```text
defaultTask   4096 B temporary
appMainTask   2048 B persistent
otaWorker     4096 B persistent
displayTask   4096 B persistent
```

记录：

```text
T0 Kernel started
T1 defaultTask bootstrap
T2 appMainTask created
T3 otaWorker created
T4 displayTask created
T5 all DONE
T6 SYSTEM_RUN / DEGRADED
T7 defaultTask deleted before Idle cleanup
T8 Idle cleanup completed
```

- [ ] 记录 `xPortGetFreeHeapSize()` / `xPortGetMinimumEverFreeHeapSize()`。
- [ ] 确认 defaultTask 删除后 stack/TCB 由 Idle cleanup 回收。
- [ ] 用 `platform_thread_get_stack_space()` 记录长期 Task 剩余 stack。
- [ ] 用 GDB A5 扫描独立交叉验证。
- [ ] 覆盖 idle / OTA receiving / display render / READY_TO_INSTALL / failure path。
- [ ] appMainTask 2048 B 如果余量不足只允许增大。
- [ ] 未取得证据前不缩小 otaWorker/displayTask。
- [ ] 写入 Verification Report。

## Task 9: S07 Regression and Documentation

**Goal:** 证明 Runtime 重构未改变 OTA 业务合同。

- [ ] Keil Build。
- [ ] S07 Metadata / OTA Service / Sink / Ymodem Host regression。
- [ ] KEY_1 start。
- [ ] Ymodem receive。
- [ ] LCD RECEIVING / VERIFYING / READY / FAILED。
- [ ] second KEY → PENDING。
- [ ] Reset persistence。
- [ ] interrupted transfer / bad CRC。
- [ ] 更新 S07A verification / handoff。
- [ ] 同步 PROJECT_CONTEXT/current_status/Roadmap 到真实实施状态。
- [ ] Review 后由 Project Owner 决定是否 CLOSED / PASS。

## Final Verification

```text
Boot
↓
defaultTask (4096 B temporary)
↓
app_system_bootstrap()
↓
Create shared IPC / Tasks
↓
Task-local Init
↓
MAIN_DONE + OTA_DONE + DISPLAY_DONE
↓
RUNNING / DEGRADED / FAILED
├─ RUNNING/DEGRADED → SYSTEM_RUN → defaultTask delete
└─ FAILED           → SYSTEM_ABORT → controlled fatal path
↓
appMainTask + otaWorker + displayTask steady runtime
```

必须同时证明：

```text
No appSystem RTOS task
No startup race
No busy wait
Stack safe
Heap safe
defaultTask memory reclaimed
Degraded-mode behavior correct
S07 OTA behavior unchanged
```

## Completion Condition

本计划已获准进入实施。代码、Host/Build、GDB RAM evidence、degraded/failure tests 和真实板 S07 regression 全部通过，Review 通过后才能将 S07A 标记为 `CLOSED / PASS`。
