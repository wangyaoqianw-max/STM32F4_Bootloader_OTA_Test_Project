# S05B Tools Toolkit Reuse Handoff

## Metadata

- Stage: `S05B_Toolkit_Reuse`
- Status: `READY_FOR_REVIEW`
- Branch: `main`
- Baseline Commit: `5c26fe63`
- Previous Design Commit: `9d6b037` (`docs(s05b): add reusable tools toolkit design`)
- Design Approval Commit: `5622b63a7cb3d532aab55de73eaf88d823b9acb9`
- Implementation Plan Commit: `3a055a84397ab5eab6dcddf2dea0590c97daff4c`
- Implementation Commits: `499df29`, `00cfbc7`, `ab4da98`, `ac1cc4b`, `e34e005`, `0719f83`, `92cf50a`
- Verification Commit: `Pending final verification commit`
- Review Commit: `Not created yet`
- Current Role: `Verification Role`
- Updated At: `2026-09-16`

## Input

- S05A 已关闭并完成 GDB、CmBacktrace、RTT、Fault Capture 和真实板测；
- S04 Persistence 补充回归已在 `6f2fad5` 完成；
- 当前 `05_Tools` 已包含 Keil、J-Link、GDB、RTT、Firmware Image、Ymodem、Tera Term、CmBacktrace 和测试脚本，但配置、公共能力、工具调用和项目专用测试仍存在职责混合；
- Project Owner 已确认 S05B 的核心目的为：整合现有工具，建立便于后续扩展、升级和跨工程复用的工具框架；
- 第一版仍以当前已经验证的 `STM32 + Keil + J-Link + GNU Arm GDB` 工具组合为基线，不提前实现其他编译链或 Probe。

## Frozen Design Output

正式详细设计：

```text
00_Project/03_Stages/S05B_Toolkit_Reuse/design.md
```

正式实施计划：

```text
00_Project/03_Stages/S05B_Toolkit_Reuse/implementation_plan.md
```

不使用 `docs/superpowers/specs/` 或 `docs/superpowers/plans/` 作为本项目阶段正式文档落点。

设计已冻结：

1. `Config + Core + Adapters + Workflows + Project Tests + Legacy Wrappers` 分层；
2. Adapter 按 `Build / Probe / Debug` 变化轴拆分；
3. 三层配置：`toolchain.local.bat`（machine, ignored）+ `project.defaults.bat`（project, committed）+ `project.local.bat`（machine override, ignored）；
4. `toolkit.bat` / `toolkit.ps1` 作为统一 Human / Agent Router；
5. 旧 `Scripts/*.bat` 最终只做薄包装，禁止双轨业务实现；
6. Firmware / Ymodem / TeraTerm 第一版继续作为独立工具能力；
7. S04 Persistence 收敛为 `Tests/S04_Persistence` 项目扩展；
8. 稳定 Exit Code 分类；
9. 不提前实现动态插件、GCC/CMake、OpenOCD、GD32 等未使用能力；
10. S05B 关闭前必须完成跨工程复用证明。

## Implementation Plan Summary

实施顺序固定为：

```text
Task 1  三层配置 + Config/Path/Logging Core
Task 2  Process / Lock / Cleanup Core
Task 3  Keil + J-Link Adapter 与 Application Workflows
Task 4  GDB Adapter 与 Debug Workflows
Task 5  toolkit Router + Legacy Wrappers
Task 6  S04 Persistence Project Test Extension
Task 7  Firmware / Ymodem Router 接入 + README/占位目录清理
Task 8  全量回归 + 第二工程复用演练 + Verification Handoff
```

第二工程复用首选 `wangyaoqianw-max/stm32f4_DMA_UART_ring_RTOS`，在临时 clone 中只替换配置，不修改通用 Core / Adapter / Workflow。

## Scope Boundary

- 不修改生产固件、Flash/Memory Layout、RTOS 接口、Firmware Image Contract 或 MCU 侧 Ymodem；
- 不提交本机工具路径，不修改系统 `PATH`；
- 不提交临时板测代码、构建缓存或调试输出；
- 每个 Task 先测试再迁移，失败时停在当前 Task；
- Host PASS 不替代 Board PASS；无 USB/J-Link 的执行环境必须明确记录硬件项 `PENDING`。

## Acceptance Direction

```text
Level 1  现有 Build / Flash / RTT / GDB / Fault / Firmware / Ymodem 能力无退化
Level 2  当前工程配置驱动，通用实现无工程专用硬编码
Level 3  Workflow / Adapter / Core 可独立演化，旧入口只做 wrapper
Level 4  第二同类 STM32 + Keil + J-Link 工程只改配置即可复用
```

## Tool Switch Information

当前状态：

```text
DESIGN_APPROVED
  → implementation_plan.md complete
  → plan self-review complete
  → READY_FOR_IMPLEMENTATION
  → Implementation Role
  → Task 1–8 complete
  → READY_FOR_REVIEW
```

实施时必须先读取：

```text
AGENTS.md
PROJECT_CONTEXT.md
00_Project/WORKFLOW.md
00_Project/03_Stages/S05B_Toolkit_Reuse/design.md
00_Project/03_Stages/S05B_Toolkit_Reuse/implementation_plan.md
00_Project/03_Stages/S05B_Toolkit_Reuse/handoff.md
05_Tools/README.md
05_Tools/Scripts/README.md
```

## Next Action

由 Review Role 回读 `design.md`、`implementation_plan.md`、本交接和验证报告，复核最终差异、架构边界、API 一致性、编译结果、测试结果及文档一致性。当前已进入 `READY_FOR_REVIEW`，不得直接关闭 S05B。

## Task 8 Implementation Output

### Full Host / Contract Regression

以下 13 组命令均返回 `EXIT=0`：

```text
Core contract
Application workflow contract
Debug workflow contract
Legacy compatibility
Firmware / transport compatibility
S04 isolation
GDB automation
Tool sequence
CmBacktrace integration
Fault diagnostics
Firmware unittest: 2 tests
YMODEM unittest: 24 tests
S04 Host unittest: 15 tests
```

`git diff --check`：`PASS`。

### Current Project Board Smoke

当前板卡已连接并通电，使用统一入口完成：

```text
toolkit.bat build           PASS
toolkit.bat flash run       PASS
toolkit.bat rtt 10          PASS
toolkit.bat snapshot resume PASS
```

证据包括：Keil build 完成但保留既有 warning（`ExitCode=1`、`TimedOut=False`、无编译 error）；RTT 日志包含日志初始化、Application 启动、Storage SPI 初始化和 `Application init result: 0`；GDB snapshot 包含 PC/SP/backtrace 和 `resume-and-disconnect`；独立 GDB 会话读取 `uwTick = 0x23adc` 后成功恢复运行。最终正常 Application 已重新 Flash/Run，测试后未残留 J-Link/GDB/RTT 相关进程。

串口/Tera Term/YMODEM 的顺序约束保持为：先打开监听工具并等待 `C`，再执行可能产生早期启动信息的烧录/复位；否则可能漏掉启动信息。RTT Logger 属于 J-Link 客户端，必须在 Flash/GDB 释放 Probe 后启动，不能与其他 J-Link owner 并发。

### Real YMODEM / Tera Term Transfer

按用户要求完成一次真实 YMODEM/Tera Term 测试。临时启用 `04_Test/Board/S05_UART_Ymodem` 中已有的板测入口和 `PROJECT_ENABLE_S05_YMODEM_BOARD_TEST=1`，测试结束后恢复 Keil 工程定义、IncludePath 和源文件引用；随后正常 Application 已重新 Build、Flash，RTT 启动正常。

实际检测到 CH340 为 `COM9`。先启动 Tera Term `ttermpro`/`ttpmacro` 并进入等待，再执行 `flash run` 复位目标板。Tera Term 宏真实退出码为 `0`，发送 `OTA_APP_s04_v1.1.0.img`，文件长度 `55884` bytes。

RTT 记录了 `YMODEM_READY` 以及 `16384/55884`、`32768/55884`、`49152/55884`、`55884/55884` 全部进度。随后用 GDB 独立确认测试固件状态：`receiver_state=6 (FINISHED)`、`receiver_error=0`、`received_size=55884`、`expected_size=55884`、`payload_written=55820`、`header_committed=1`、`file_started=0`、`packets_received/accepted=57/57`、`retries=0`。结论：真实传输、Slot B Payload/Header 提交和 Receiver 完成均 PASS。

### Second Project Reuse

复用源：`E:\my_project_2026\Git_test\stm32f4_DMA_UART_ring_RTOS`。通过临时副本调整 `project.defaults.bat` 后，发现并复用了：

```text
RTT_elog_DMA_UART_ring_project\MDK-ARM\RTT_elog_DMA_UART_ring_project.uvprojx
Target: RTT_elog_DMA_UART_ring_project
Device: STM32F411CEUx
Output: Objects\RTT_elog_DMA_UART_ring_project.axf
```

临时副本 `toolkit.bat build`：`PASS`（无 error/warning）；随后 `toolkit.bat flash run` 和 `toolkit.bat rtt 15` 均通过。RTT 记录了 EasyLogger、service_log、system composition 和 communication runtime 启动日志。Core、Adapters、Workflows 和 `toolkit.ps1` 与当前工程版本的聚合 SHA-256 均一致；未修改通用实现。该工程没有 CMake，也没有 CmBacktrace，后两者不属于该复用目标。

源仓库原有未提交删除项及一个 `codex_build.log` 未被本任务修改；临时复用目录已清理。

## Verification Completion

S04 Reset / Power-cycle Persistence、Fault trigger/capture、第二工程真实板级 Flash/RTT 以及既有 Host/Contract 回归均已完成并记录在：

```text
04_Test/Reports/Stages/S05B_Toolkit_Reuse/verification.md
```

当前无待执行的 S05B 验证项。阶段处于 `READY_FOR_REVIEW`，等待 Review Role 最终审核，不直接标记为 `CLOSED`。
