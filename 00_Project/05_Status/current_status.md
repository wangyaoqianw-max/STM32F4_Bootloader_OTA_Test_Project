# Current Project Status

## Context Metadata

- Active Stage: `S05B_Toolkit_Reuse`
- Status: `READY_FOR_REVIEW`
- S05A Implementation / Verification Commit: `bd8883d`
- S05A CmBacktrace Integration Commit: `1c27c8e`
- S05A Review Commit: `32f3368`
- S05B Initial Design Commit: `9d6b037`
- S05B Design Approval Commit: `5622b63a7cb3d532aab55de73eaf88d823b9acb9`
- S05B Implementation Plan Commit: `3a055a84397ab5eab6dcddf2dea0590c97daff4c`
- S04 Persistence Supplementary Regression Commit: `6f2fad5`
- Branch: `main`
- S05A Baseline Commit: `ec6119f64306d027c86208329823cf42b77ceebf`
- Baseline Commit: `5c26fe63`
- Design Commit: `400b8b4f4cb50672faea2c332379466bb637b3a6`
- Implementation Plan Commit: `a5417e47dc86546176ec87dff5f6c58ddfb14260`
- Design Approval Commit: `b62bdad9d1279158d4925a417ab5a0e1b4668db3`
- S05B Implementation Commits: `499df29`, `00cfbc7`, `ab4da98`, `ac1cc4b`, `e34e005`, `0719f83`, `92cf50a`
- Verification Commit: `Pending final verification commit`
- Review Commit: `Not created yet`
- S05 Merge Commit: `5b2b42136e0d8f1eb2d54463fb5319996d6f6b5f`
- Last Closed Stage: `S05A_Debug_Crash_Diagnostics`
- Last Closed Stage Status: `CLOSED / PASS`
- Next Planned Stage: `S06_RTOS_Runtime` (after S05B)
- Current Role: `Verification Role → Review Role`
- Updated At: `2026-09-16`

## Current Goal

`S05_UART_Ymodem` 已完成 Design、Implementation、Verification、Review，并已通过 PR #7 合并到 `main`。S05A 已关闭；在进入 `S06_RTOS_Runtime` 之前新增 `S05B_Toolkit_Reuse` 小阶段。

S05A 已完成 GDB 自动化与真实板测。手工 GDB 控制能力、Runtime Snapshot resume/halt、失败路径、进程清理和 J-Link 释放均已验证。CmBacktrace 源码已完成 Keil/FreeRTOS/RTT 工程接入；Invalid Address、Undefined Instruction、Divide by Zero 三类受控 Fault 均已完成真实板端 GDB/RTT 采集和现场交叉核对。S04 Reset / Power-cycle Persistence 补充回归也已完成。

S05B 已完成设计冻结、正式实施计划和 Task 1–8 实施，当前状态为 `READY_FOR_REVIEW`。本阶段目标不是简单整理目录，而是把已经验证的 PC 工具重构为便于后续扩展、升级和跨工程复用的配置驱动工具框架。冻结架构为 `Config + Core + Adapters + Workflows + Project Tests + Legacy Wrappers`；Adapter 按 Build / Probe / Debug 变化轴拆分；配置采用 `toolchain.local + project.defaults + project.local` 三层模型；增加统一 `toolkit.bat` Router，同时保留旧 `Scripts` 兼容入口。

正式设计与实施计划：

```text
00_Project/03_Stages/S05B_Toolkit_Reuse/design.md
00_Project/03_Stages/S05B_Toolkit_Reuse/implementation_plan.md
```

配置、Core、Adapters、Workflows、统一 Router、Legacy 兼容入口、S04 项目测试扩展、Firmware/Ymodem 路由和文档均已完成。全量回归、当前工程板级 smoke、Fault、S04 Reset/Power-cycle 以及第二工程真实板测记录于 `04_Test/Reports/Stages/S05B_Toolkit_Reuse/verification.md`。

## S05 Delivered Capabilities

```text
Tera Term / Python Ymodem Sender
        ↓
USART1 + DMA + RingBuffer
        ↓
service_uart
        ↓
ymodem_parser
        ↓
ymodem_receiver
        ↓
ymodem_sink
        ↓
Firmware Storage
        ↓
W25Q64 Slot B
        ↓
firmware_storage_validate_image() == VALID
```

已交付：

- Ymodem Receiver-only / Single-file 组件；
- SOH 128 Byte / STX 1 KiB Packet；
- Block 0 文件信息解析；
- CRC-16/XMODEM、ACK / NAK、Duplicate、Sequence、Timeout、Retry、CAN、EOT；
- Parser / Receiver / Sink 分层；
- Firmware Storage `write_payload()` / `write_header()`；
- compact `.img = [64B Header][Payload]` 到 Slot `Header @ +0x0000 / Payload @ +0x1000` 的映射；
- Header-last commit；
- Tera Term 5 自动化 Sender；
- Python Ymodem Sender 作为 Host/Agent/诊断辅助工具；
- Host Test、Keil Build、J-Link/RTT、真实 CH340 板测闭环。

## S05 Verification Summary

真实 Tera Term 板测已验证：

```text
file_size / received          55884 / 55884
packets received / accepted  57 / 57
bytes received / written      55884 / 55884
retry                         0
UART dropped / errors         0 / 0
Payload written               55820
Header commit                 1
Slot B validation             VALID
final result                  PASS
```

中途停止 Sender 后，Receiver 进入 Timeout/Error，`header_commit=0`；随后重新建立会话可再次成功传输并将 Slot B 校验为 VALID。

正式证据：

- `00_Project/03_Stages/S05_UART_Ymodem/review.md`
- `04_Test/Reports/Stages/S05_UART_Ymodem/verification.md`

## S05A GDB Debug Checkpoint

S05A 位于已关闭的 S05 与计划中的 S06 之间，当前状态为 `CLOSED`。

已完成的真实板测基线：

```text
J-Link GDB Server V7.92             PASS
STM32F411CE + SWD @ 4000 kHz        PASS
Keil OTA_APP.axf symbol loading     PASS
Breakpoint / Continue               PASS
Next / Step                         PASS
Backtrace / Memory read             PASS
Variable read                       PASS
```

已冻结的退出合同：

```text
halt state   → detach
             → MCU remains halted

running state → continue&
              → disconnect
              → quit
              → MCU continues running
```

`continue& → disconnect` 已通过重新连接和 `uwTick` 增长验证。S05A 自动化不得执行 GDB `load`，Resume 会话不得使用 `-batch`，也不得在 `continue&` 后使用 `detach`。

自动化板测结果：

```text
halt 后 uwTick 约 2 秒：0x38A62C -> 0x38A62C       PASS
resume 后 uwTick 约 2 秒：0x396313 -> 0x399082     PASS
continue& -> disconnect -> quit                      PASS
失败路径、非零退出码、PID 清理、J-Link 释放            PASS
```

正式证据：`04_Test/Reports/Stages/S05A_Debug_Crash_Diagnostics/verification.md`。

当前 S05A 已覆盖 GDB Runtime Snapshot、resume/halt 生命周期、失败清理、板测证据、CmBacktrace 的 Keil/FreeRTOS/RTT 工程接入，以及三类受控 Fault 注入、现场采集和 GDB/CmBacktrace 交叉验证。Review 已通过并关闭 S05A；S04 Reset/Power-cycle Persistence 已在补充回归中完成。

正式交接：

- `00_Project/03_Stages/S05A_Debug_Crash_Diagnostics/handoff.md`
- `00_Project/03_Stages/S05_UART_Ymodem/handoff.md`

## S05B Tools Toolkit Reuse

当前阶段为 `S05B_Toolkit_Reuse`，工作流状态为 `READY_FOR_REVIEW`，Roadmap 状态为 `ACTIVE`。

正式设计与计划：

```text
00_Project/03_Stages/S05B_Toolkit_Reuse/design.md
00_Project/03_Stages/S05B_Toolkit_Reuse/implementation_plan.md
```

冻结边界：

```text
Human / Agent
      ↓
Unified Entry / Legacy Entry
      ↓
Workflows
      ↓
Core + Adapters
      ↓
Keil / J-Link / GDB
```

实施计划共 8 个 Task：

```text
1 Config + Config/Path/Logging Core
2 Process / Lock Core
3 Build/Probe Adapter + Application Workflow
4 GDB Adapter + Debug Workflow
5 toolkit Router + Legacy Wrapper
6 S04 Project Test Extension
7 Firmware/Ymodem Router + Docs
8 Full Regression + Cross-project Reuse + Verification Handoff
```

第二工程复用首选：`wangyaoqianw-max/stm32f4_DMA_UART_ring_RTOS`，已使用本地仓库做临时副本演练，仅修改副本配置；通用 Core/Adapter/Workflow/Router 未改动，第二工程源仓库未修改。该工程没有 CmBacktrace，故 CmBacktrace 集成不属于本次复用验收范围。

## Stable Tooling After S05

Application 工具链：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Scripts/flash_app.bat
05_Tools/Scripts/rtt_capture.bat
05_Tools/Scripts/run_app_cycle.bat
05_Tools/Firmware/pack_firmware.py
05_Tools/Scripts/send_ymodem.bat
05_Tools/Scripts/send_ymodem_python.bat
```

Ymodem 板测必须发送 S04 `.img`，不能把原始 Application `.bin` 当作最终 Firmware Image。

当前已验证的自动化顺序为：

```text
先启动 Sender / Tera Term 并打开 COM 等待 'C'
→ 再 Flash / Reset MCU
→ Ymodem transfer
→ RTT evidence
→ Firmware validation
```

## S06 Design Entry After S05B

S06 名称保持 `S06_RTOS_Runtime`，但早期“集成 FreeRTOS”的前提已过时。当前 Application 已具备 RTOS Kernel 和任务基础；S05 板测也曾使用独立 `s05Ymodem` Thread，并验证 `service_uart` 的单 Consumer / ownerThread 约束。

S06 在 S05B 关闭后重点讨论 Task Topology/Lifecycle、UART Consumer Ownership、OTA/Ymodem Task Ownership、IPC、Flash/Storage 并发、Blocking API、Timeout/Cancel/Error Recovery、业务与 OTA 并发以及日志资源竞争。S06 尚未进入正式实现。

## Deferred Regression

S04 跨阶段回归已完成，当前无延期项：

```text
Reset Persistence       PASS
Power-cycle Persistence PASS
```

详细证据见 `04_Test/Reports/Stages/S04_Firmware_Image_Storage/verification.md`。

## Blockers

S05B 当前无已知阻塞项。设计、实施计划、Host/Contract 回归、真实 YMODEM/Tera Term 实传、Fault trigger/capture、S04 Reset/Power-cycle 专项和第二工程真实板测均已通过，当前等待 Review Role 最终审核。

下一步：Review Role 独立复核 `04_Test/Reports/Stages/S05B_Toolkit_Reuse/verification.md`、最终差异及全部实现提交，确认架构边界、API 一致性、编译/测试结果和文档一致性；不得直接关闭 S05B。
