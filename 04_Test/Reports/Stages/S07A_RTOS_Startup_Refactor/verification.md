# S07A RTOS Startup Refactor Verification

## Metadata

- Stage: `S07A_RTOS_Startup_Refactor`
- Status: `READY_FOR_VERIFICATION`
- Implementation Commit: `306c76b`
- Verification Commit: `b6896cb` (`docs: record s07a physical regression evidence`)
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
| Display 肉眼状态逐阶段确认 | PENDING | 本轮记录了 RTT 初始化和 Service/GDB 状态，未把无照片/人工观察记录的 LCD 画面误记为 PASS |

RTT 冒烟未输出新增的 Stack 日志；Stack 结论以下面的 GDB A5 交叉证据为准。

### Additional Startup Recheck

在继续验证过程中，第一次重采样曾读到 Fault RTT 文本。只读 J-Link/GDB 检查确认目标 FPB `COMP0=0x48000199` 残留了 `0x08000198` 的临时入口断点，目标实际被调试器停在 ArmCC `__main`，不是已确认的生产 Fault。清除该 comparator、恢复正常 DEMCR 后重新 Reset/Run：

- RTT 正常输出 EasyLogger、OTA Runtime、`appMainTask` 和 `displayTask` 初始化；
- GDB 再次停在 FreeRTOS `prvIdleTask`，无 Fault Handler；
- 当前验证未修改生产代码，临时探针文件已移除。

因此，当前 S07A 正常启动/idle 路径仍为 PASS；此前 Fault 文本不作为 S07A 生产故障证据。

### Current S07A Physical Regression

本轮使用正式镜像 `OTA_APP_s07_baseline_v1.0.0.img`（`67664` bytes，`67` blocks），真实串口为 `COM9`，按键为 `KEY_1 / PA0`。临时慢速发送器、坏 CRC 镜像和监视脚本均为验证辅助，测试结束后已删除，未进入生产工程。

| 路径 | 结果 | 当前 S07A 证据 |
| --- | --- | --- |
| KEY_1 启动 + 正常 YMODEM | PASS | 真实按键后传输 `67664/67664` bytes、`67` blocks，sender exit `0`，重试 `2`；GDB 为 `READY_TO_INSTALL`、target `B` |
| second KEY → PENDING | PASS | 当前 S07A 正常传输后的第二次确认已提交 Metadata；Reset 后回读 `confirmed=A`、`pending=B`、`upgrade=PENDING`，额外按键被 `FAILED/error=5` 拒绝，证明 PENDING 状态仍受保护 |
| Reset persistence | PASS | Reset 后 RTT 重新输出 EasyLogger、OTA Runtime、`appMainTask`、`displayTask` 初始化；GDB 读取到上述 PENDING Metadata |
| interrupted transfer | PASS | 真实传输收到 `Block 0`～`7` 后发送双 `CAN`；GDB：`state=FAILED`、`error=19`、target `B`、`6144/67664` bytes、progress `9%`，Metadata `pending=NONE`、Slot B `INVALID` |
| bad CRC | PASS | 仅修改 payload 偏移 `164` 的一字节；YMODEM 仍完成 `67664/67664`，GDB：`state=FAILED`、`error=3`、progress `100%`，Metadata `pending=NONE`、Slot B `INVALID` |
| duplicate KEY during receive | PASS | 在 `Block 7` 后再次真实按键；传输仍完成 `67664/67664`，GDB：`state=READY_TO_INSTALL`、`error=0`、target `B`、Slot B `VALID`、`pending=NONE` |

本轮物理证据覆盖了 S07A 当前代码下的正常接收、PENDING/Reset、取消、中途 CRC 失败和接收中的重复 KEY。它不等价于 S07A 组件 degraded/failure fault injection，也不替代 Display 肉眼验收或 OTA receiving/READY/failure 状态下的 Stack 专项采样。

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

`uxCurrentNumberOfTasks=6` 包含 FreeRTOS idle/timer 和 EasyLogger 等系统任务，不应解读为只有三个系统任务。`uxDeletedTasksWaitingCleanUp=0` 证明采样时 defaultTask 删除后的待回收列表为空。当前采样仅覆盖正常启动/idle 路径，尚未覆盖 OTA receiving、display render、READY_TO_INSTALL 和 failure path。

代码、Keil 工程 source group 和 GDB 符号检查均未发现长期 `appSystem` RTOS Task；`appSystem` 只保留为普通 Bootstrap / Startup Supervisor 模块。上述物理回归的 GDB Service 状态采样未发现启动拓扑变化；Stack 数值仍以正常启动/idle 采样为准。

## Degraded / Failure Verification

Host contract 已验证 startup timeout、未完成 DONE、组件错误导致 DEGRADED、FAILED/ABORT 不发布 RUN，以及 RUN 广播等待不清除 flag。

以下真实 fault injection 尚未执行：

- Display init failure 后 foreground + OTA 继续；
- OTA init failure 后 foreground + display 继续；
- appMain init failure 后 OTA + display 继续；
- 真实 Task create / Event Flags / shared IPC failure 的 FAILED 路径。

生产代码没有添加默认开启的 fault injection 开关。

## Verification Status

```text
代码验证：PASS
硬件验证：PENDING
```

代码实现、自动化回归和 S07 当前固件的主要物理业务路径已完成；组件 degraded/failure fault injection、Display 肉眼记录和 OTA 场景专项 Stack 证据尚未完成。因此 S07A 当前仍未达到 `READY_FOR_REVIEW`，不得标记 `CLOSED / PASS`。

## Next Actions

1. 补齐当前 S07A 固件 Display `RECEIVING / VERIFYING / READY / FAILED` 的肉眼或可回读状态证据。
2. 补齐 OTA receiving、display render、READY_TO_INSTALL 和 failure path 的 Stack 专项采样。
3. 如现场条件允许，补齐三个业务组件的 degraded fault path 和基础设施 FAILED path 证据。
4. 证据完整后再更新状态并进入 `READY_FOR_REVIEW`。
