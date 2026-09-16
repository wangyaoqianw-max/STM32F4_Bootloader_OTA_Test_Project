# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S05B_Toolkit_Reuse`
- Active Stage Status: `CHANGES_REQUESTED`
- Branch: `main`
- S05B Initial Design Commit: `9d6b037`
- S05B Design Approval Commit: `5622b63a7cb3d532aab55de73eaf88d823b9acb9`
- S05B Implementation Plan Commit: `3a055a84397ab5eab6dcddf2dea0590c97daff4c`
- S05A Implementation / Verification Commit: `bd8883d`
- S05A CmBacktrace Integration Commit: `1c27c8e`
- S05A Review Commit: `32f3368`
- S04 Persistence Supplementary Regression Commit: `6f2fad5`
- S05 Merge Commit: `5b2b42136e0d8f1eb2d54463fb5319996d6f6b5f`
- Last Closed Stage: `S05A_Debug_Crash_Diagnostics`
- Last Closed Stage Status: `CLOSED / PASS`
- Next Planned Stage: `S06_RTOS_Runtime` (after S05B)
- Current Role: `Review Role → Implementation Role`
- Updated At: `2026-09-16`

## Current Goal

S05 和 S05A 已关闭；S04 Reset / Power-cycle Persistence 补充回归也已完成。当前活动阶段是 `S05B_Toolkit_Reuse`。

S05B 的目标不是简单整理目录，而是把已经验证过的 PC 工具从“当前工程专用脚本集合”重构为便于后续扩展、升级和跨工程复用的配置驱动工具框架。

冻结架构：

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

正式设计与实施计划：

```text
00_Project/03_Stages/S05B_Toolkit_Reuse/design.md
00_Project/03_Stages/S05B_Toolkit_Reuse/implementation_plan.md
00_Project/03_Stages/S05B_Toolkit_Reuse/handoff.md
```

Task 1–8 和 Verification 已完成，但 Review 发现通用 J-Link ownership 锁未接入实际 Workflow，且 Unified Exit Code 对外部失败码 `1` 映射不一致，当前状态为 `CHANGES_REQUESTED`。配置、Core、Adapters、Workflows、统一 Router、Legacy 兼容入口、S04 项目测试扩展、Firmware/Ymodem 路由和文档均已落地。全量回归、当前工程板级 smoke、Fault、S04 Reset/Power-cycle 以及第二工程真实板测记录于：

```text
04_Test/Reports/Stages/S05B_Toolkit_Reuse/verification.md
```

## Required Reading For S05B Implementation

按顺序读取：

1. `AGENTS.md`
2. `README.md`
3. `PROJECT_CONTEXT.md`
4. `00_Project/WORKFLOW.md`
5. `00_Project/02_Roadmap/development_roadmap.md`
6. `00_Project/03_Stages/S05A_Debug_Crash_Diagnostics/handoff.md`
7. `00_Project/03_Stages/S05A_Debug_Crash_Diagnostics/review.md`
8. `04_Test/Reports/Stages/S05A_Debug_Crash_Diagnostics/verification.md`
9. `00_Project/03_Stages/S05B_Toolkit_Reuse/design.md`
10. `00_Project/03_Stages/S05B_Toolkit_Reuse/implementation_plan.md`
11. `00_Project/03_Stages/S05B_Toolkit_Reuse/handoff.md`
12. `05_Tools/README.md`
13. `05_Tools/Scripts/README.md`
14. 当前 `05_Tools/Scripts/*.bat` / `*.ps1`
15. 当前 `05_Tools/Debug/GDB`、`Debug/CmBacktrace`、`Firmware`、`Ymodem`

## S05B Frozen Design

### Architecture

```text
Config
+ Core
+ Adapters
+ Workflows
+ Project Tests
+ Legacy Wrappers
```

Adapter 按变化轴拆分：

```text
Adapters/Build/Keil
Adapters/Probe/JLink
Adapters/Debug/GDB
```

不使用 `STM32_Keil_JLink` 组合式 Adapter，避免以后增加 GCC/CMake 或其他 Probe 时产生组合爆炸。

### Configuration Model

```text
toolchain.local.bat   machine tool paths, ignored
project.defaults.bat  repository project facts, committed
project.local.bat     machine-specific project overrides, ignored
```

工程 Target、Keil project 相对路径、AXF/HEX/BIN 相对路径和 MCU 型号属于工程事实；Keil/J-Link/GDB 安装位置属于机器事实；COM、GDB Port、J-Link Speed 等允许由 local override 覆盖。

### Unified Entry

目标稳定入口：

```text
toolkit.bat build
toolkit.bat flash run|prepare
toolkit.bat run
toolkit.bat rtt
toolkit.bat snapshot halt|resume
toolkit.bat fault capture|trigger
toolkit.bat firmware pack ...
toolkit.bat ymodem ...
```

旧 `05_Tools/Scripts/*.bat` 继续存在，但完成迁移后只允许做薄包装，不保留第二套核心实现。

### Stable Exit Classes

```text
0   SUCCESS
1   BUILD_WARNING (only Build/Run warnings)
10  CONFIG_ERROR
20  BUILD_ERROR
30  PROBE_ERROR
40  DEBUG_ERROR
50  TRANSFER_ERROR
60  TEST_ERROR
```

### Project Test Boundary

S04 Persistence 属于当前项目专用 Test Extension：

```text
05_Tools/Tests/S04_Persistence/
```

S04 专用 AXF、symbol、解析规则和 runner 不得进入通用 Core/Adapter/Workflow；GDB lifecycle、RTT capture、lock、timeout、logging 等仍由公共能力提供。

## S05B Implementation Order

实施计划共 8 个 Task：

```text
Task 1  三层配置 + Config/Path/Logging Core
Task 2  Process / Lock / Cleanup Core
Task 3  Build/Probe Adapter + Application Workflows
Task 4  GDB Adapter + Debug Workflows
Task 5  toolkit Router + Legacy Wrappers
Task 6  S04 Persistence Project Test Extension
Task 7  Firmware/Ymodem Router + README/占位目录清理
Task 8  Full Regression + Cross-project Reuse + Verification Handoff
```

每个 Task 必须先写/运行合同测试，再迁移实现，并独立提交。任一回归 FAIL 时停在当前 Task。

第二工程复用首选：

```text
wangyaoqianw-max/stm32f4_DMA_UART_ring_RTOS
```

演练使用本地仓库的临时副本，只替换 Toolkit 配置，不修改第二工程生产代码，也不得为了适配第二工程编辑通用 Core / Adapter / Workflow。该工程没有 CmBacktrace，CmBacktrace 集成不属于该复用目标的验收范围。

## Stable S04 Storage / Firmware Contract

External Flash：

```text
Slot A: 0x000000 ~ 0x07FFFF
Slot B: 0x080000 ~ 0x0FFFFF

Per Slot:
+0x0000 ~ +0x0FFF : Header Sector
+0x1000 ~          : Firmware Payload
Payload capacity  : 508 KiB
```

Firmware Image V1：

- Header fixed 64 Byte；
- Header / Payload CRC32；
- Image Header 是 Firmware Version / Size / CRC 权威来源；
- `firmware_storage_validate_image()` 为只读验证；
- `pack_firmware.py` 输出 compact `.img = [64 Byte Header][Payload]`；
- Slot 中映射为 Header Sector + Payload Offset，不能线性写入 compact `.img`。

AT24C02 Metadata 保持双副本 + sequence + CRC + commit marker。S05 没有建立 `PENDING`。

## Stable S05 Ymodem Capability

```text
PC Sender
   ↓
USART1 / DMA / RingBuffer
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
```

稳定边界：

- `service_uart` 拥有 UART DMA / RingBuffer / TX / error / data-loss；
- `ymodem_parser` 负责 framing / block complement / CRC-16；
- `ymodem_receiver` 负责 Block 0、sequence、ACK/NAK、retry、timeout、cancel、EOT；
- `ymodem_sink` 只定义 begin/write/end/abort；
- Ymodem 不感知 Slot、EEPROM Metadata、PENDING、Reset；
- Header-last commit 保证失败传输不会提交新有效 Header。

真实 Tera Term 板测已验证 Slot B 最终 `VALID`；中止时 `header_commit=0`，重新建立 Session 后可恢复。

当前 Sender 定位：

- Tera Term：S05 真实板默认验收入口；
- Python Sender：Host Test、Agent 自动化、协议诊断，支持 `--json`。

## Stable S05A GDB Contract

真实板已验证：Breakpoint / Continue / Next / Step / Backtrace / Memory Read / Variable Read、Runtime Snapshot、CmBacktrace、三类受控 Fault 和 GDB/RTT 交叉核对。

冻结退出合同：

```text
halt state
→ detach
→ MCU remains halted
```

```text
running state
→ continue&
→ disconnect
→ quit
→ MCU continues running
```

约束：

- GDB 自动化不得执行 `load`；
- Resume 不使用 `-batch`；
- `continue&` 后不得再执行 `detach`；触发型 Fault GDB 会话使用阻塞 `continue` 等待 `diagnostics_fault_capture_stop`，运行态 Snapshot 仍使用 `continue&`；
- GDB/RTT/J-Link Commander 同一时刻不能并发占用同一 Probe；
- 只清理当前工具自己创建的进程。

S05B 只能抽取配置与生命周期公共能力，不得改变上述调试语义。

## Current Tooling Before S05B Implementation

当前兼容入口：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Scripts/flash_app.bat
05_Tools/Scripts/rtt_capture.bat
05_Tools/Scripts/run_app_cycle.bat
05_Tools/Scripts/gdb_runtime_snapshot.bat
05_Tools/Scripts/gdb_fault_capture.bat
05_Tools/Scripts/send_ymodem.bat
05_Tools/Scripts/send_ymodem_python.bat
05_Tools/Firmware/pack_firmware.py
```

当前已知重复能力包括 GDB PowerShell 脚本中的 process start/capture、TCP readiness、stdout/stderr merge、timeout、owned process cleanup 等；这些应在 Task 2 收敛到 Core，而不是复制到新目录。

## Verification Gate

Implementation Role 完成 Task 8 后状态只能进入 `READY_FOR_VERIFICATION`；Verification Role 完成证据核验后进入 `READY_FOR_REVIEW`，不能直接关闭 S05B。

Verification Role 需要独立记录：

- Core/Application/Debug/Compatibility 合同测试；
- Firmware/Ymodem/S04 Host Test；
- 当前工程 Build / Flash / RTT / GDB；
- Legacy Entry compatibility；
- 第二同类工程只改配置的复用证据；
- 本机绝对路径、临时代码、缓存没有提交；
- 无法执行的硬件项明确为 `PENDING`，不能用 Host PASS 替代。

## Current RTOS Reality Before S06

FreeRTOS 已经存在并正常运行，S06 不再是“移植 FreeRTOS”，而是正式化 Application Runtime / Concurrency Model。

S06 在 S05B 关闭后重点讨论：Task Topology / Lifecycle、UART Consumer Ownership、OTA/Ymodem Task Ownership、IPC、Flash/Storage Serialization、Blocking API、Timeout/Cancel/Error Recovery、正常业务与后台 OTA 并发、日志资源竞争。

## Next Action

Implementation Role 修复 `00_Project/03_Stages/S05B_Toolkit_Reuse/review.md` 中的 J-Link ownership 与 Exit Code 映射问题，补充回归后交 Verification Role；未复核通过前不得关闭 S05B。
