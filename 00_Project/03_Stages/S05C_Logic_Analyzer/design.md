# S05C Logic Analyzer Agent Workflow Design

## Metadata

- Stage: `S05C_Logic_Analyzer`
- Status: `DESIGN_APPROVED`
- Branch: `main`
- Baseline Commit: `31456f0e7020d79f31cb7dbcf006afe6fc687286`
- Current Role: `S05C Design Role`
- Owner: Project Owner
- Updated At: `2026-09-17`

## 1. Objective

在现有 `05_Tools` Toolkit 中增加可被 Human / Agent 稳定调用的逻辑分析仪能力，用于获取 MCU 外部数字总线证据，并与现有 Build / Flash / RTT / GDB / Fault 自动化形成互补。

第一版正式范围只包含已经完成硬件接线并经过 sigrok-cli 可行性验证的：

```text
SPI
I2C
```

UART 与 GPIO Timing 作为后续扩展，不阻塞 S05C 关闭。

本阶段不重新实现 sigrok，不建立独立 MCP 平台。底层继续使用已经实测可用的 `sigrok-cli`，上层接入现有 Toolkit。

## 2. Reference Projects And Adopted Lessons

设计参考了当前成熟度较高的 Logic Analyzer + Agent 项目模式：

- `KenosInc/sigrok-mcp-server`：采用 `sigrok-cli` 作为唯一 sigrok 接口，Executor 与 Parser 分离，真实 sigrok 输出作为 parser fixture；
- `ankayca/boardex` / `boardex-logic`：Agent-facing API 使用粗粒度动作与机器可读 verdict，明确区分 pass / fail / error / inconclusive；
- Espressif AI Agent + Saleae 示例：把 Firmware/Runtime evidence 与真实外部波形 evidence 交叉核对；
- Sipeed Logic Analyzer Agent 模式：允许用户直接告诉 Agent 当前通道接线，再由 Agent 生成采集配置。

本项目只吸收这些设计原则，不整体移植其 MCP Server、Runtime、GUI 或插件系统。

## 3. Position In Existing Toolkit

当前 Toolkit 已冻结为：

```text
Human / Agent
      ↓
Unified Entry
      ↓
Workflows
      ↓
Core + Adapters
      ↓
External Tools
```

S05C 只做横向扩展：

```text
Human / Agent
      ↓
toolkit.bat logic ...
      ↓
Workflows/LogicAnalyzer
      ↓
Adapters/LogicAnalyzer/Sigrok
      ↓
sigrok-cli
      ↓
USB Logic Analyzer
```

职责边界保持：

```text
Keil / J-Link   → Build / Flash / Reset
GDB             → Halt / Run / Register / Memory / Backtrace
RTT             → Runtime Log
Logic Analyzer  → External Digital Bus Evidence
```

Logic Analyzer Adapter 不负责 Flash、Reset、GDB 或 RTT。需要组合时由更高层 Workflow / Project Test 编排。

## 4. V1 Scope

### 4.1 In Scope

- sigrok-cli 本机路径和可用性检查；
- USB Logic Analyzer 扫描与运行时设备选择；
- SPI 数字采集与协议解码；
- I2C 数字采集与协议解码；
- Capture 与 Decode 内部职责分离；
- 默认接线 Profile；
- Agent/CLI 临时 Channel Mapping Override；
- Effective Config 记录；
- `.sr` Raw Capture 保存；
- Structured Result / JSON 输出；
- `SUCCESS / ERROR / INCONCLUSIVE` 工作流状态；
- Golden Fixtures / Parser Host Test；
- W25Q64 SPI 只读板测；
- AT24C02 I2C 只读板测；
- 与现有 Toolkit Config/Core/Router/Exit Code 体系兼容。

### 4.2 Deferred

- UART decode；
- GPIO edge / frequency / duty / pulse-width timing；
- CAN / USB / SWD / 1-Wire 等其他协议；
- Analog measurement；
- Long-duration recording；
- MCP Server；
- GUI；
- Simulator backend；
- Runtime plugin discovery；
- 多逻辑分析仪 fleet 管理；
- Flash / EEPROM 写入或擦除型逻辑分析仪验收。

## 5. Target Architecture

```text
05_Tools/
├─ Config/
│  ├─ toolchain.local.example.bat
│  ├─ project.defaults.bat
│  └─ logic_analyzer.profiles.json
│
├─ Adapters/
│  └─ LogicAnalyzer/
│     └─ Sigrok/
│        ├─ sigrok_exec.ps1
│        └─ sigrok_parse.ps1
│
├─ Workflows/
│  └─ LogicAnalyzer/
│     └─ logic.ps1
│
├─ Contracts/
│  └─ LogicAnalyzer/
│     └─ test_logic_analyzer.ps1
│
├─ Tests/
│  └─ S05C_LogicAnalyzer/
│     ├─ fixtures/
│     ├─ test_spi.ps1
│     └─ test_i2c.ps1
│
├─ toolkit.ps1
└─ toolkit.bat
```

只创建实际使用文件。若实施时发现 `logic.ps1` 拆成 capture/decode 更清晰，可以在保持外部合同不变的前提下拆分。

## 6. Adapter Design

### 6.1 Executor

`sigrok_exec.ps1` 只负责具体工具调用：

```text
version
scan
capture
protocol decode
timeout
stdout/stderr
exit code
owned process cleanup
```

Executor 不认识：

```text
W25Q64
AT24C02
JEDEC ID
EEPROM address
Project test PASS/FAIL
```

### 6.2 Parser

`sigrok_parse.ps1` 只负责将 sigrok 输出转换为稳定的 Structured Result。

这样可以：

- 不接真实硬件也测试 parser；
- 使用 Golden Fixture 防止 sigrok 输出格式变化造成静默误判；
- Agent 不直接解析自由文本。

## 7. Capture / Decode Separation

内部能力分离：

```text
Capture
  ↓
capture.sr
  ↓
Decode
  ↓
decode.json
```

Agent-facing convenience command 可以一次完成 Capture + Decode：

```text
toolkit.bat logic spi ...
toolkit.bat logic i2c ...
```

但必须允许对已有 `.sr` 重跑 Decode，以便接线映射或 decoder 参数修正后无需重新采集。

## 8. Result Contract

Workflow 结果至少区分：

```text
SUCCESS
ERROR
INCONCLUSIVE
```

语义：

- `SUCCESS`：采集和协议解析成功，获得可使用的总线证据；
- `ERROR`：工具、设备、配置、进程或文件操作失败；
- `INCONCLUSIVE`：采集动作成功，但没有足够协议证据得出结论，例如总线空闲、采样窗口未覆盖事务、映射错误或 decoder 没有产生有效 annotation。

禁止把 `no decoded transaction` 直接当作项目功能 FAIL。

建议通用结果形状：

```json
{
  "status": "SUCCESS",
  "operation": "SPI_CAPTURE_DECODE",
  "device": {
    "driver": "fx2lafw"
  },
  "capture": {
    "sample_rate_hz": 48000000,
    "sample_count": 9216,
    "artifact": "capture.sr"
  },
  "mapping": {
    "cs": "D0",
    "clk": "D1",
    "miso": "D3",
    "mosi": "D5"
  },
  "transactions": []
}
```

项目 Test 才产生：

```text
PASS / FAIL / ERROR
```

例如 W25Q64 Test 才知道：

```text
0x9F → JEDEC ID
expected → EF 40 17
```

## 9. Channel Mapping Model

禁止在 Adapter / Workflow 中硬编码 D0~D7。

抽象保持：

```text
Signal Name
    ↓
Logic Analyzer Channel
    ↓
Protocol Decoder
```

而不是：

```text
PB12 permanently = D0
PB13 permanently = D1
```

默认 Profile 保存当前已验证接线：

```text
SPI2 Flash
CS   → D0 → PB12
CLK  → D1 → PB13
MISO → D3 → PB14
MOSI → D5 → PB15

Software I2C EEPROM
SCL  → D2 → PB6
SDA  → D4 → PB7
```

用户可以直接告诉 Agent 新接线，例如：

```text
D0 接 PB6
D1 接 PB7
检查 AT24C02 I2C
```

Agent 将其转换为本次 Temporary Override，而不是默认修改仓库配置。

只有 Project Owner 明确要求“以后固定这样连接”时，才修改 committed profile。

优先级：

```text
CLI / Agent override
    > selected profile
    > project default
```

## 10. Effective Configuration

每次采集前必须输出并保存本次真正生效的配置，包括：

```text
protocol
sample rate
sample count / duration
MCU pin
logic channel
decoder options
device driver / selected device
```

防止 Agent 误解自然语言接线后直接执行而无法追踪。

## 11. Device Selection

当前实验设备由 `fx2lafw` 驱动。

不得永久固化：

```text
fx2lafw:conn=4.7
```

因为 USB 拔插后 connection selector 可能变化。

V1 规则：

```text
scan matching driver
  ├─ 0 devices → DEVICE_NOT_FOUND / ERROR
  ├─ 1 device  → auto select
  └─ >1 device → AMBIGUOUS_DEVICE / ERROR unless local selector provided
```

机器相关 selector 如确有需要，只允许放 local config。

## 12. Configuration Placement

`toolchain.local.bat` 增加机器事实：

```bat
set "SIGROK_CLI_EXE="
```

不提交本机绝对路径。

Committed project profile 可保存：

```text
protocol
channel mapping
MCU pin metadata
recommended sample rate
decoder options
```

不新增与当前 Toolkit 三层配置模型冲突的第二套 machine config。

## 13. Golden Fixtures

Host Test 保存少量来自真实 sigrok-cli 的文本 fixture：

```text
Tests/S05C_LogicAnalyzer/fixtures/
├─ device_scan.txt
├─ spi_decode.txt
├─ i2c_decode.txt
└─ decode_empty.txt
```

用于验证：

```text
raw sigrok output
      ↓
Parser
      ↓
Structured Result
```

Fixture 不包含本机绝对路径、USB 临时拓扑信息或无关日志。

## 14. Output Evidence

运行时证据保存到：

```text
06_Output/LogicAnalyzer/<run>/
├─ effective_config.json
├─ capture.sr
├─ decode.json
└─ result.json
```

`06_Output` 属于运行产物，不作为正式长期验证报告。

正式阶段验证结论仍落：

```text
04_Test/Reports/Stages/S05C_Logic_Analyzer/verification.md
```

## 15. Project Acceptance Tests

### 15.1 SPI / W25Q64

优先采用只读、确定性的 JEDEC ID transaction：

```text
MOSI: 9F
MISO: EF 40 17
```

验证：

- CS / CLK / MOSI / MISO Mapping 正确；
- decoder 能识别 transaction；
- 读取命令与真实数据存在；
- 项目 Test 对比 expected JEDEC；
- RTT 如同时存在，可作为第三份证据，但不是 Logic Workflow 的依赖。

### 15.2 I2C / AT24C02

只读验证：

- START / Repeated START / STOP；
- Device Address `0x50`；
- Read / Write direction；
- ACK / NACK；
- data byte annotations。

EEPROM 当前 payload 不作为永久硬编码 expected value，除非板测 fixture 显式拥有该内容。

## 16. Read-only Safety Boundary

S05C V1 的真实板测不为了验证逻辑分析仪而执行：

```text
W25Q64 erase/program
AT24C02 arbitrary write
Firmware Slot modification
Metadata modification
```

优先复用正常 Application 中已经存在的只读访问，或使用明确的只读测试入口。

## 17. Agent Usage Model

目标交互：

```text
User:
“D0 接 PB6，D1 接 PB7，检查 EEPROM I2C。”

Agent:
resolve signal mapping
→ print effective config
→ toolkit logic i2c ...
→ capture
→ decode
→ structured result
→ project assertion if requested
```

Agent 不依赖对话记忆作为唯一接线来源；本次 Effective Config 必须进入 evidence。

## 18. Failure Classification

至少覆盖：

```text
TOOL_NOT_FOUND
DEVICE_NOT_FOUND
AMBIGUOUS_DEVICE
DEVICE_BUSY
CONFIG_ERROR
CAPTURE_TIMEOUT
NO_ACTIVITY
DECODE_EMPTY
DECODE_FAILED
WORKFLOW_ERROR
```

其中 `NO_ACTIVITY / DECODE_EMPTY` 默认映射为 `INCONCLUSIVE`，除非具体 Project Test 的前置条件明确保证事务必须发生。

## 19. Completion Criteria

S05C 第一版关闭要求：

```text
sigrok tool discovery             PASS
runtime device discovery          PASS
SPI capture                       PASS
SPI decode                        PASS
I2C capture                       PASS
I2C decode                        PASS
structured result                 PASS
effective config evidence         PASS
golden fixture parser tests       PASS
W25Q64 read-only board test       PASS
AT24C02 read-only board test      PASS
existing Toolkit regression       PASS
UART/GPIO                         DEFERRED
```

已有 `04_Test/Reports/Tools/Sigrok_CLI_Logic_Analyzer_2026-09-16.md` 作为 Feasibility Baseline，不替代 S05C 正式 Verification。

## 20. Design Summary

S05C 不复制成熟 MCP 项目，而是在现有 Toolkit 内采用其成熟思想：

```text
sigrok-cli only hardware interface
+
Executor / Parser separation
+
Structured Result
+
SUCCESS / ERROR / INCONCLUSIVE
+
Golden Fixtures
+
Capture / Decode separation
+
Agent temporary wiring override
+
Project-specific assertions outside generic adapter
```

这样既保留当前工程统一自动化入口，也为后续 UART、GPIO、MCP、更多仪器能力留下自然扩展点。