# S07A RTOS Startup Refactor Verification

## Metadata

- Stage: `S07A_RTOS_Startup_Refactor`
- Status: `CLOSED / PASS`
- Implementation Commits: `306c76b`, `c30c60d`, `d631fb8`
- Verification Commit: `8703415` (`docs: record s07a task lifecycle verification`)
- Branch: `main`
- Verification Date: `2026-09-18`

## Scope

本报告验证 S07A 启动拓扑、App 目录整理、Startup Barrier、任务初始化 ownership 以及 Stack/Heap 证据。S07 OTA Service、Metadata V2、Ymodem、KEY 双确认、PENDING 和 Display 业务合同保持不变。

S08/S09/S10 的 Bootloader、Internal Flash Installation、Trial、Confirm 和 Rollback 不在本阶段范围内，未实施。

## Code Verification

| 项目 | 结果 | 证据 |
| --- | --- | --- |
| `git diff --check` | PASS | implementation commit staged diff and post-change checks |
| Keil Application Build | PASS | `05_Tools\\toolkit.bat build`; 0 error / 0 warning |
| Keil project XML | PASS | `OTA_APP.uvprojx` / `OTA_APP.uvoptx` parse successfully |
| App layout / stale path scan | PASS | no old flat production App paths remain; no duplicate source entries |
| Embedded C style check | PASS | changed/new files have no tabs and no line longer than 120 columns |
| Startup contract Host Test | PASS | `S07A Startup contract host test passed.` |
| Task lifecycle contract | PASS | `05_Tools\\Contracts\\Application\\test_task_lifecycle.ps1` |

实现包含：`platform_event_flags`、线程剩余 Stack API、`app_startup` Context/Barrier、`app_system_bootstrap()`、`defaultTask` 自删除、`appMainTask / otaWorker / displayTask` 长期运行拓扑，以及 `01_APP/system/task/runtime/contract` 目录迁移。

## Host Regression

| 范围 | 结果 |
| --- | --- |
| S07A startup contract: timeout / degraded / abort / broadcast RUN | PASS |
| S07 IRQ / KEY / worker contract / display contract / Metadata / firmware sink / OTA Service | 7/7 PASS |
| S04 CRC / firmware format / firmware storage | PASS |
| S04 persistence unittest | 15/15 PASS |
| S05 firmware storage / YMODEM parser / receiver / flash sink regression | PASS |
| Firmware Pack Python tests | 2/2 PASS |
| YMODEM Python discovery | 24/24 PASS |

S07A Host Test 使用独立 stub 验证 Event Flags transition；测试文件位于 `04_Test/Host/S07A_RTOS_Startup/`，未加入生产工程。未遗留 fault injection 默认开关或测试专用业务逻辑。

## Current Board Evidence

以下是 S07A 当前代码取得的板级工具和启动证据，不等价于完整 S07 物理验收：

| 项目 | 结果 | 说明 |
| --- | --- | --- |
| Flash / reset / run | PASS | `05_Tools\\toolkit.bat run` |
| RTT capture | PASS | 当前固件完成 EasyLogger、OTA Runtime、appMainTask、displayTask 初始化冒烟 |
| GDB runtime snapshot | PASS | Startup state、Task handle、Heap 和 Stack 证据已采集 |
| S07 KEY/Ymodem/PENDING/Reset/interrupted/bad CRC/duplicate KEY 物理回归 | PASS | 当前 S07A 固件已在真实 `COM9` / `KEY_1(PA0)` 上重跑；详见下表 |
| Display 肉眼状态逐阶段确认 | PASS | Project Owner 确认每次验证 LCD 显示正常；RTT 同时确认 Display 初始化、初始渲染和背光开启返回 `0` |

OTA 场景 Stack 采样使用临时 RTT instrumentation，未启动 GDB 介入传输；采样结束后临时代码已删除并恢复正式镜像。

### Additional Startup Recheck

在继续验证过程中，第一次重采样曾读到 Fault RTT 文本。只读 J-Link/GDB 检查确认目标 FPB `COMP0=0x48000199` 残留了 `0x08000198` 的临时入口断点，目标实际被调试器停在 ArmCC `__main`，不是已确认的生产 Fault。清除该 comparator、恢复正常 DEMCR 后重新 Reset/Run：

- RTT 正常输出 EasyLogger、OTA Runtime、`appMainTask` 和 `displayTask` 初始化；
- GDB 再次停在 FreeRTOS `prvIdleTask`，无 Fault Handler；
- 当前验证未修改生产代码，临时探针文件已移除。

因此，当前 S07A 正常启动/idle 路径仍为 PASS；此前 Fault 文本不作为 S07A 生产故障证据。

### Current S07A Physical Regression

本轮使用正式镜像 `OTA_APP_s07_baseline_v1.0.0.img`（`67664` bytes，`67` blocks），真实串口为 `COM9`，按键为 `KEY_1 / PA0`。临时慢速发送器、坏 CRC 镜像、Stack RTT instrumentation 和监视脚本均为验证辅助，测试结束后已删除，未进入生产工程。

| 路径 | 结果 | 当前 S07A 证据 |
| --- | --- | --- |
| KEY_1 启动 + 正常 YMODEM | PASS | 真实按键后传输 `67664/67664` bytes、`67` blocks，sender exit `0`，重试 `2`；GDB 为 `READY_TO_INSTALL`、target `B` |
| second KEY → PENDING | PASS | 当前 S07A 正常传输后的第二次确认已提交 Metadata；Reset 后回读 `confirmed=A`、`pending=B`、`upgrade=PENDING`，额外按键被 `FAILED/error=5` 拒绝，证明 PENDING 状态仍受保护 |
| Reset persistence | PASS | Reset 后 RTT 重新输出 EasyLogger、OTA Runtime、`appMainTask`、`displayTask` 初始化；GDB 读取到上述 PENDING Metadata |
| interrupted transfer | PASS | 真实传输收到 `Block 0`～`7` 后发送双 `CAN`；GDB：`state=FAILED`、`error=19`、target `B`、`6144/67664` bytes、progress `9%`，Metadata `pending=NONE`、Slot B `INVALID` |
| bad CRC | PASS | 仅修改 payload 偏移 `164` 的一字节；YMODEM 仍完成 `67664/67664`，GDB：`state=FAILED`、`error=3`、progress `100%`，Metadata `pending=NONE`、Slot B `INVALID` |
| duplicate KEY during receive | PASS | 在 `Block 7` 后再次真实按键；传输仍完成 `67664/67664`，GDB：`state=READY_TO_INSTALL`、`error=0`、target `B`、Slot B `VALID`、`pending=NONE` |

本轮物理证据覆盖了 S07A 当前代码下的正常接收、PENDING/Reset、取消、中途 CRC 失败、接收中的重复 KEY，以及 Display 肉眼状态确认。组件 degraded/failure fault injection 在修复后重新完成，所有临时注入均已移除。

### OTA Scenario Stack Evidence

临时 RTT instrumentation 只在 OTA 事件的 `0% / 75% / 100%`、`READY_TO_INSTALL` 和 `FAILED` 节点采样，单位为剩余 byte：

| 场景 | otaWorker | displayTask | 物理结果 |
| --- | --- | --- | --- |
| 正常接收 | `3032 B` at 0%；`2912 B` at 75%/100% | `3184 B` at 0%；`3148 B` at 75%/100% | 67 blocks、67664 B、sender exit 0 |
| READY_TO_INSTALL | `2544 B` | `3148 B` | `state=READY_TO_INSTALL`、error 0、Slot B VALID |
| bad CRC / FAILED | `3024 B` at 0%；`2944 B` at 75%/100%；`2784 B` at FAILED | `3160 B` at 0%；`3148 B` at 75%/100%/FAILED | `state=FAILED`、error 3、67664/67664 B |

采样后的正式镜像 RTT 启动冒烟再次 PASS。`appMainTask` 的正常 idle GDB 高水位仍为 `407 words / 1628 B`；本轮没有用 GDB 在 OTA 传输中暂停目标机。

## GDB / RAM Evidence

当前正常启动/idle 路径的 GDB 采样：

| 指标 | 实测值 |
| --- | ---: |
| `configTOTAL_HEAP_SIZE` | 24576 B |
| `xFreeBytesRemaining` | 9352 B |
| `xMinimumEverFreeBytesRemaining` | 5144 B |
| 峰值已分配 Heap（推算） | 19432 B |
| `uxCurrentNumberOfTasks` | 6 |
| `uxDeletedTasksWaitingCleanUp` | 0 |
| Startup state | RUNNING (`1`) |
| `g_appSystemBootstrapped` | 1 |
| `g_appStartupInitialized` | 1 |

Task Stack High Water Mark：

| Task | Free words | 约 free bytes（4 B/word） |
| --- | ---: | ---: |
| `appMainTask` | 407 | 1628 B |
| `otaWorker` | 908 | 3632 B |
| `displayTask` | 800 | 3200 B |

`uxCurrentNumberOfTasks=6` 包含 FreeRTOS idle/timer 和 EasyLogger 等系统任务，不应解读为只有三个系统任务。`uxDeletedTasksWaitingCleanUp=0` 证明采样时 defaultTask 删除后的待回收列表为空。OTA receiving、display render、READY_TO_INSTALL 和 failure path 的专项 Stack 数值见上表。

代码、Keil 工程 source group 和 GDB 符号检查均未发现长期 `appSystem` RTOS Task；`appSystem` 只保留为普通 Bootstrap / Startup Supervisor 模块。上述物理回归的 GDB Service 状态采样未发现启动拓扑变化；正常 idle 与 OTA 场景 Stack 数值分别见 GDB 表和 RTT 专项表。

## Degraded / Failure Verification

Host contract 已验证 startup timeout、未完成 DONE、组件错误导致 DEGRADED、FAILED/ABORT 不发布 RUN，以及 RUN 广播等待不清除 flag。

本轮在 `c30c60d` / `d631fb8` 修复了 Task entry 失败分支直接返回的问题。三个长期 Task 现在通过 Platform Thread termination 自身安全退出；FreeRTOS 适配层识别 current Task，并在终止前清理对应句柄。若终止 API 异常返回，Task 进入带 delay 的不可返回兜底，不会落入 `prvTaskExitError`。Startup Event Flags 创建失败时，Context 也明确记录 `FAILED` 后进入 controlled fatal path。

真实板级 fault injection 结果如下。每次验证结束后均恢复正式源码并重新 Build/Flash/RTT；未留下 fault injection 开关、测试入口或临时 GDB 脚本。

| 路径 | 结果 | GDB / RTT 证据 |
| --- | --- | --- |
| Display init failure | PASS | `startupState=DEGRADED(2)`、`displayResult=15`；`appMainTask.native` 和 `otaWorker.native` 保持有效，`displayTask.native=0`；Idle cleanup 完成；未停在 `prvTaskExitError` |
| OTA init failure | PASS | `startupState=DEGRADED(2)`、`otaResult=15`；`appMainTask.native` 和 `displayTask.native` 保持有效，`otaWorker.native=0`；Idle cleanup 完成；未停在 `prvTaskExitError` |
| appMain init failure | PASS | `startupState=DEGRADED(2)`、`mainResult=15`；`otaWorker.native` 和 `displayTask.native` 保持有效，`appMainTask.native=0`；Idle cleanup 完成；未停在 `prvTaskExitError` |
| Display Queue / shared IPC create failure | PASS | `startupState=FAILED(3)`；三个业务 Task handle 均为 `0`，未发布 `SYSTEM_RUN`；GDB 停在 `Error_Handler` |
| Task create failure | PASS | `startupState=FAILED(3)`；业务 Task handle 均为 `0`，未发布 `SYSTEM_RUN`；GDB 停在 `Error_Handler`。`uxDeletedTasksWaitingCleanUp=2` 是 fatal handler 占用 defaultTask、Idle 尚未运行的现场状态，不是 DEGRADED 路径泄漏 |
| Startup Event Flags create failure | PASS | `startupState=FAILED(3)`；业务 Task 未创建，未发布 `SYSTEM_RUN`；GDB 停在 `Error_Handler`。因同步对象不存在，该路径以 FAILED state + controlled fatal 作为证据，无法再通过 Event Flags 发布 abort bit |

生产代码没有添加默认开启的 fault injection 开关，也没有保留测试专用业务逻辑。

## Verification Status

```text
代码验证：PASS
硬件验证：PASS
```

代码实现、自动化回归、S07 当前固件主要物理业务路径、Display 肉眼确认、OTA 场景专项 Stack 证据，以及组件 degraded/failure 和启动基础设施 failure 路径均已完成。Review 已通过，S07A 正式关闭为 `CLOSED / PASS`。

## Next Actions

S07A Verification / Review 已完成，阶段关闭。下一计划阶段为 `S08_Bootloader_Foundation`。
