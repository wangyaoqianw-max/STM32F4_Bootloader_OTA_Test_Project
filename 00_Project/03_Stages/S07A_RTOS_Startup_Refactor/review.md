# S07A RTOS Startup Refactor Review

## Metadata

- Stage: `S07A_RTOS_Startup_Refactor`
- Status: `CLOSED / PASS`
- Branch: `main`
- Baseline Commit: `254f510498748294f44b0c059796dbaeaccdea3e`
- Design Commit: `e4ae9dea8f6ab0088829fb4acda29ab89c734132`
- Implementation Plan Commit: `fa8409c335727720e387887d254caada5939f346`
- Implementation Commits: `306c76b`, `c30c60d`, `d631fb8`
- Verification Commit: `8703415`
- Reviewed Head: `ad887e5`
- Review Commit: `pending current review commit`
- Review Date: `2026-09-18`
- Reviewer Role: `Review Role`

## Review Scope

本次为轻量 Review，对照 S07A 冻结 Design、Implementation Plan、最终实现、Verification、Handoff 和 S07 物理回归证据，重点检查：

- 是否移除长期 `appSystem` RTOS Task；
- `defaultTask → app_system_bootstrap()` 启动链是否按冻结设计落地；
- Startup Barrier / RUNNING / DEGRADED / FAILED 语义；
- Task-local initialization 与资源 ownership；
- `01_APP/system/task/runtime/contract` 目录整理；
- Stack / Heap 证据与 defaultTask 回收；
- Task init failure 的安全退出；
- S07 OTA 业务合同是否保持；
- 是否存在阻塞 S08 的问题。

## Findings

### Blocking Findings

无。

### Important Findings

无。

### Resolved Findings

实施后的板级 fault injection 发现长期 Task entry 在 init failure 分支直接返回时会进入 FreeRTOS `prvTaskExitError`。该问题已通过：

```text
c30c60d  fix: safely terminate failed startup tasks
d631fb8  fix: terminate failed app main task
```

修复。三个业务 Task 的 DEGRADED 路径已重新在真实板上验证，失败 Task 可安全终止，其他成功 Task 保持运行。

### Non-blocking Notes

1. S07A 主拓扑实现提交为 `306c76b`；此前部分状态文档顶部只列出 `c30c60d / d631fb8`，本次关闭时统一修正为完整实现链 `306c76b → c30c60d → d631fb8`。
2. Startup Event Flags 使用 Application 生命周期，不在 `defaultTask` 删除时销毁，这是冻结设计的一部分，用于避免 RUN 广播后对象提前释放的竞态。
3. Event Flags 自身创建失败时无法再依赖该对象发布 `SYSTEM_ABORT`；当前以 Startup Context=`FAILED`、不创建业务 Task、不发布 `SYSTEM_RUN`、进入 controlled `Error_Handler` 作为该基础设施失败路径证据，符合设计意图。
4. 当前 Heap minimum-ever free 为 `5144 B`；在已验证场景下无 allocation failure。当前没有依据继续压缩 otaWorker/displayTask Stack。
5. `uxCurrentNumberOfTasks=6` 包含 FreeRTOS idle/timer、EasyLogger 等系统任务；代码、工程源文件和 GDB 符号均确认不存在长期 `appSystem` RTOS Task。

## Review Results

| Review Area | Result | Evidence |
| --- | --- | --- |
| Frozen startup architecture | PASS | `defaultTask → app_system_bootstrap()` 已落地，无独立 appSystem Task |
| Startup Barrier | PASS | Event Flags DONE/RUN/ABORT Host + board evidence |
| RUNNING / DEGRADED / FAILED | PASS | Host contract + 真实板 fault injection |
| Task lifecycle | PASS | `c30c60d` / `d631fb8`，Task lifecycle contract |
| Task-local ownership | PASS | OTA/UART/KEY、Display/SPI1、foreground/LED 仍由各自 Task 管理 |
| App directory organization | PASS | `01_APP/system/task/runtime/contract`，旧平面生产路径已移除 |
| Platform OS additions | PASS | `platform_event_flags`、`platform_thread_get_stack_space()` |
| Build / style / project integrity | PASS | Keil 0 error / 0 warning；XML/stale path/style checks PASS |
| Stack safety | PASS | appMain 1628 B free；OTA 场景最低 2544/2784 B；display ≥3148 B |
| Heap safety / bootstrap cleanup | PASS | minimum-ever 5144 B；`uxDeletedTasksWaitingCleanUp=0` |
| S07 physical regression | PASS | KEY/Ymodem/PENDING/Reset/interrupted/bad CRC/duplicate KEY |
| Display regression | PASS | Project Owner 肉眼确认正常/失败状态显示 |
| S08 scope isolation | PASS | 未实现 Bootloader、Install、Trial、Confirm、Rollback |
| Documentation consistency | PASS after closure sync | Verification/Handoff/Context/Status/Roadmap 同步 |

## Decision

```text
Implementation:      PASS
Architecture:        PASS
Code Verification:   PASS
Hardware Verification: PASS
Stack / Heap:        PASS
Regression:          PASS
Review:              PASS
Stage:               CLOSED / PASS
Closure:             APPROVED
```

S07A 已完成目标。Application Startup Contract 现在以 S07A 为准：

```text
defaultTask (temporary bootstrap)
→ app_system_bootstrap()
→ appMainTask + otaWorker + displayTask
```

`appSystem` 仅作为 Composition Root / Startup Supervisor 模块，不再是长期 RTOS Task。

下一计划阶段为 `S08_Bootloader_Foundation`。
