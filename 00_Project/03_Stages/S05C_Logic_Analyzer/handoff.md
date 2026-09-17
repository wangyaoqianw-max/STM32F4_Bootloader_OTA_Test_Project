# S05C Logic Analyzer Agent Workflow Handoff

## Metadata

- Stage: `S05C_Logic_Analyzer`
- Status: `READY_FOR_IMPLEMENTATION`
- Branch: `main`
- Baseline Commit: `31456f0e7020d79f31cb7dbcf006afe6fc687286`
- Design Commit: `e98ebe6a0dbdde52cc7d802e45ee88b1e1b485a0`
- Implementation Plan Commit: `a096f7ecf04d30c7bd718b1d92c1603b15ed6805`
- Current Role: `Implementation Role`
- Owner: Project Owner
- Updated At: `2026-09-17`

## Input

S05B 已关闭并完成 Toolkit 框架重构。当前稳定结构为：

```text
Config
+ Core
+ Adapters
+ Workflows
+ Contracts
+ Project Tests
+ Unified Router
```

现有 Toolkit 已覆盖 Build / Flash / RTT / GDB Snapshot / Fault / Firmware / Ymodem。

2026-09-16 已完成一次独立 sigrok-cli 逻辑分析仪可行性实验，并形成：

```text
04_Test/Reports/Tools/Sigrok_CLI_Logic_Analyzer_2026-09-16.md
```

该实验已证明：

```text
Logic Analyzer Device Control      PASS
SPI2 Decode                         PASS
Software I2C Decode                 PASS
J-Link / Keil / RTT Recovery        PASS
Overall Feasibility                 PASS
```

该报告只作为 Feasibility Baseline，不替代 S05C 正式 Verification。

## Frozen Scope

S05C 第一版只交付：

```text
SPI
I2C
```

当前硬件连线已经完成，因此第一轮实施不要求重新接线。

当前验证接线：

```text
SPI2 / W25Q64
D0 → PB12 → CS
D1 → PB13 → CLK
D3 → PB14 → MISO
D5 → PB15 → MOSI

Software I2C / AT24C02
D2 → PB6 → SCL
D4 → PB7 → SDA
```

延期：

```text
UART
GPIO Timing
```

原因：需要改线，且不影响当前 SPI/I2C Agent Workflow 的架构验证。

## Optimized Design Output

正式设计：

```text
00_Project/03_Stages/S05C_Logic_Analyzer/design.md
```

正式实施计划：

```text
00_Project/03_Stages/S05C_Logic_Analyzer/implementation_plan.md
```

本方案已参考并吸收成熟 Logic Analyzer Agent 项目的关键设计，但不整体移植其 MCP/GUI/Runtime：

```text
sigrok-cli as sole backend
Executor / Parser separation
Structured Result
SUCCESS / ERROR / INCONCLUSIVE
Golden Fixtures
Capture / Decode separation
Agent temporary wiring override
Project assertions outside generic adapter
```

## Frozen Architecture

```text
Human / Agent
      ↓
toolkit.bat logic ...
      ↓
Workflows/LogicAnalyzer
      ↓
Adapters/LogicAnalyzer/Sigrok
      ├─ Executor
      └─ Parser
      ↓
sigrok-cli
      ↓
USB Logic Analyzer
```

项目级判断保持在：

```text
05_Tools/Tests/S05C_LogicAnalyzer/
```

禁止将 W25Q64 / AT24C02 语义下沉到通用 Adapter/Workflow。

## Configuration Contract

机器事实：

```text
SIGROK_CLI_EXE
```

只进入：

```text
05_Tools/Config/toolchain.local.bat
```

模板更新：

```text
05_Tools/Config/toolchain.local.example.bat
```

项目 committed profile：

```text
05_Tools/Config/logic_analyzer.profiles.json
```

默认保存当前验证接线，但不形成硬编码。

优先级：

```text
Agent / CLI override
    > selected profile
    > project default
```

如果用户说：

```text
D0 接 PB6
D1 接 PB7
检查 EEPROM I2C
```

默认视为当前运行 Temporary Override；除非 Project Owner 明确要求永久修改默认接线。

每次执行必须输出并保存 Effective Config。

## Device Selection Contract

当前设备使用 `fx2lafw`。

禁止永久固化：

```text
fx2lafw:conn=4.7
```

运行规则：

```text
0 matching devices → DEVICE_NOT_FOUND
1 matching device  → auto select
>1 devices          → AMBIGUOUS_DEVICE unless explicit selector
```

USB connection number 不是稳定项目身份。

## Result Contract

通用 Logic Workflow 状态：

```text
SUCCESS
ERROR
INCONCLUSIVE
```

`INCONCLUSIVE` 适用于：

```text
Capture succeeded but bus idle
No decoder annotations
Capture window missed transaction
Likely wrong mapping / insufficient sample evidence
```

不得将无协议数据直接映射为项目 FAIL。

项目 Test 才输出：

```text
PASS
FAIL
ERROR
```

## Evidence Contract

每次有效采集输出：

```text
06_Output/LogicAnalyzer/<run>/
├─ effective_config.json
├─ capture.sr
├─ decode.json
└─ result.json
```

正式 Verification 最终放：

```text
04_Test/Reports/Stages/S05C_Logic_Analyzer/verification.md
```

## SPI Acceptance

第一项板测使用 W25Q64 只读证据。

优先目标：

```text
JEDEC command  = 0x9F
Expected ID    = EF 40 17
```

通用 Workflow 只负责输出 SPI transaction；项目 Test 负责确认该 transaction 是否符合 W25Q64 预期。

不得为了本阶段验证执行额外 Flash erase/program。

## I2C Acceptance

第二项板测使用 AT24C02 只读 transaction。

验证重点：

```text
Address = 0x50
START
Repeated START
Read / Write direction
ACK / NACK
STOP
Data annotations
```

EEPROM payload 不作为永久 expected value，除非测试明确拥有固定内容。

不得为了本阶段验证执行任意 EEPROM 写入。

## Implementation Order

```text
Task 1  Config + Result Contract
Task 2  Sigrok Executor
Task 3  Parser + Golden Fixtures
Task 4  Capture / Decode Workflow
Task 5  toolkit logic Router
Task 6  W25Q64 / AT24C02 Project Assertions
Task 7  Real-board SPI Acceptance
Task 8  Real-board I2C Acceptance
Task 9  Full Regression + Verification + Handoff
```

实施必须按 `implementation_plan.md` 执行；每个 Task 先测试再实现，失败时停止推进。

## Required Reading Before Implementation

```text
AGENTS.md
PROJECT_CONTEXT.md
00_Project/WORKFLOW.md
00_Project/02_Roadmap/development_roadmap.md
00_Project/03_Stages/S05B_Toolkit_Reuse/handoff.md
00_Project/03_Stages/S05C_Logic_Analyzer/design.md
00_Project/03_Stages/S05C_Logic_Analyzer/implementation_plan.md
04_Test/Reports/Tools/Sigrok_CLI_Logic_Analyzer_2026-09-16.md
05_Tools/README.md
05_Tools/toolkit.ps1
05_Tools/Core/Toolkit.Core.psm1
```

## Safety Boundary

本阶段主要是外部被动观测能力。

禁止为了方便测试：

- 修改 Flash/EEPROM 正式数据布局；
- 引入不必要的生产固件功能；
- 修改系统 PATH；
- 提交本机工具绝对路径；
- 对 `conn=4.7` 等临时 USB 枚举值形成依赖；
- 为 UART/GPIO 扩大当前 Stage 范围；
- 建立第二套与现有 Toolkit 并行的 Agent 调用框架。

## Stage Close Gate

```text
Host / Contract
- Config contract                 PASS
- Executor contract               PASS
- Parser golden fixtures          PASS
- Workflow contract               PASS
- Router contract                 PASS

Real Board
- Logic analyzer discovery        PASS
- SPI capture/decode              PASS
- W25Q64 read-only assertion      PASS
- I2C capture/decode              PASS
- AT24C02 read-only assertion     PASS

Regression
- build                           PASS
- flash run                       PASS
- RTT                             PASS
- snapshot resume                 PASS
- existing Toolkit contracts      PASS

Deferred
- UART                            DEFERRED
- GPIO Timing                     DEFERRED
```

## Next Action

进入 S05C Implementation Role，从 `implementation_plan.md` Task 1 开始。

在 S05C SPI/I2C 验证关闭前，不进入 UART/GPIO 实现，也不提前进入 S06。