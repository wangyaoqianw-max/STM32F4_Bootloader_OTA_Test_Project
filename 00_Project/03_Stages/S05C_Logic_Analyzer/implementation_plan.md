# S05C Logic Analyzer Agent Workflow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** 在现有 `05_Tools` Toolkit 中增加基于 `sigrok-cli` 的 SPI / I2C Logic Analyzer Agent Workflow，输出可复现、机器可读的硬件总线证据，并完成 W25Q64 与 AT24C02 真实板级只读验收。

**Architecture:** 保留现有 `Config + Core + Adapters + Workflows + Project Tests + Router` 架构。新增 `LogicAnalyzer/Sigrok` Adapter，并将底层拆为 Executor / Parser；Workflow 负责 Capture / Decode、Effective Config 和 Evidence；项目级 SPI/I2C 语义与 PASS/FAIL 判断只放在 `Tests/S05C_LogicAnalyzer`。

**Tech Stack:** Windows PowerShell 5.1+、CMD/BAT、sigrok-cli、libsigrok/libsigrokdecode、Saleae-compatible FX2 Logic Analyzer、STM32F411CEU6、J-Link、RTT。

**Spec:** `00_Project/03_Stages/S05C_Logic_Analyzer/design.md`

## Global Constraints

- 第一版只实现 SPI 与 I2C；UART/GPIO 明确延期。
- 不建立 MCP Server、GUI、Simulator、Plugin Registry。
- `sigrok-cli` 为唯一逻辑分析仪底层接口，不直接绑定 libsigrok C API。
- `Adapters/LogicAnalyzer/Sigrok` 不允许出现 W25Q64 / AT24C02 项目语义。
- Adapter / Workflow 不允许硬编码 D0~D7 永久映射。
- 不永久固化 `fx2lafw:conn=x.y`。
- `SIGROK_CLI_EXE` 属于 machine config，不得提交本机绝对路径。
- Capture 与 Decode 内部职责分离；必须允许对已有 `.sr` 重跑 Decode。
- Workflow 必须返回 `SUCCESS / ERROR / INCONCLUSIVE`，不得把无事务直接判定为 Project FAIL。
- 每次采集必须输出并保存 Effective Config。
- S05C 真实板测只读，不为了测试逻辑分析仪修改 W25Q64/AT24C02 内容。
- 所有新代码必须先通过 Host/Contract Test，再执行真实硬件验收。
- 任一 Task 验证失败时停在当前 Task，不继续后续实施。

## Planned File Map

```text
05_Tools/
├─ Config/
│  ├─ toolchain.local.example.bat        # add SIGROK_CLI_EXE
│  └─ logic_analyzer.profiles.json       # committed profiles
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
│     │  ├─ device_scan.txt
│     │  ├─ spi_decode.txt
│     │  ├─ i2c_decode.txt
│     │  └─ decode_empty.txt
│     ├─ test_parser.ps1
│     ├─ test_spi.ps1
│     └─ test_i2c.ps1
│
├─ toolkit.ps1
└─ README.md

06_Output/LogicAnalyzer/<run>/
├─ effective_config.json
├─ capture.sr
├─ decode.json
└─ result.json
```

## Task 1: Freeze Config And Result Contract

**Files:**
- Modify: `05_Tools/Config/toolchain.local.example.bat`
- Create: `05_Tools/Config/logic_analyzer.profiles.json`
- Create: `05_Tools/Contracts/LogicAnalyzer/test_logic_analyzer.ps1`

**Produces:** committed channel profiles, machine path contract, normalized workflow result schema.

- [x] **Step 1: Write failing contract tests** for missing `SIGROK_CLI_EXE`, profile load, CLI override priority, invalid channel names and result status enum.
- [x] **Step 2: Run contract test and confirm FAIL** because S05C config support does not exist yet.
- [x] **Step 3: Add `SIGROK_CLI_EXE` to `toolchain.local.example.bat`** only; do not commit a machine path.
- [x] **Step 4: Create `logic_analyzer.profiles.json`** with current verified profiles:

```json
{
  "profiles": {
    "spi2_flash": {
      "protocol": "spi",
      "signals": {
        "cs":   {"mcu_pin": "PB12", "logic_channel": "D0"},
        "clk":  {"mcu_pin": "PB13", "logic_channel": "D1"},
        "miso": {"mcu_pin": "PB14", "logic_channel": "D3"},
        "mosi": {"mcu_pin": "PB15", "logic_channel": "D5"}
      }
    },
    "i2c_eeprom": {
      "protocol": "i2c",
      "signals": {
        "scl": {"mcu_pin": "PB6", "logic_channel": "D2"},
        "sda": {"mcu_pin": "PB7", "logic_channel": "D4"}
      }
    }
  }
}
```

- [x] **Step 5: Define normalized result fields** at minimum: `status`, `operation`, `device`, `capture`, `mapping`, `transactions`, `error_class`, `artifacts`.
- [x] **Step 6: Re-run contract test, Expected PASS.**
- [x] **Step 7: Commit** `feat(tools): add logic analyzer configuration contract`.

## Task 2: Implement Sigrok Executor

**Files:**
- Create: `05_Tools/Adapters/LogicAnalyzer/Sigrok/sigrok_exec.ps1`
- Modify: `05_Tools/Contracts/LogicAnalyzer/test_logic_analyzer.ps1`

**Produces:** stable process wrapper for version, scan, capture and decode commands.

- [x] **Step 1: Add fake-executable Host Tests** covering version, scan, non-zero exit, timeout, stdout/stderr capture and owned-process cleanup.
- [x] **Step 2: Confirm tests FAIL.**
- [x] **Step 3: Implement Executor using existing Toolkit Core process primitives**, not a second process/timeout framework.
- [x] **Step 4: Add runtime scan logic**: zero match -> `DEVICE_NOT_FOUND`; one match -> auto select; multiple matches -> `AMBIGUOUS_DEVICE` unless explicit local selector exists.
- [x] **Step 5: Ensure Executor contains no W25Q64/AT24C02 assertions and no permanent `conn=` value.**
- [x] **Step 6: Re-run Host Tests, Expected PASS.**
- [x] **Step 7: Commit** `feat(tools): add sigrok logic analyzer executor`.

## Task 3: Implement Parser With Golden Fixtures

**Files:**
- Create: `05_Tools/Adapters/LogicAnalyzer/Sigrok/sigrok_parse.ps1`
- Create: `05_Tools/Tests/S05C_LogicAnalyzer/fixtures/*`
- Create: `05_Tools/Tests/S05C_LogicAnalyzer/test_parser.ps1`

**Produces:** deterministic structured parsing independent of real hardware.

- [x] **Step 1: Capture minimal real sigrok-cli fixture text** from existing verified SPI/I2C/device-scan outputs; remove machine absolute paths and transient USB topology where unnecessary.
- [x] **Step 2: Write parser tests** for device scan, SPI annotations, I2C annotations and empty decode.
- [x] **Step 3: Confirm parser tests FAIL.**
- [x] **Step 4: Implement parser** converting sigrok text to stable PowerShell objects / JSON records.
- [x] **Step 5: Map empty protocol evidence to `INCONCLUSIVE`, not FAIL.**
- [x] **Step 6: Run parser tests repeatedly without USB hardware, Expected PASS.**
- [x] **Step 7: Commit** `feat(tools): add structured sigrok parser fixtures`.

## Task 4: Implement Capture / Decode Workflow

**Files:**
- Create: `05_Tools/Workflows/LogicAnalyzer/logic.ps1`
- Modify: `05_Tools/Contracts/LogicAnalyzer/test_logic_analyzer.ps1`

**Produces:** generic Capture, Decode, SPI and I2C workflow actions.

- [x] **Step 1: Write Workflow contract tests** proving config load, profile resolution, CLI override, effective config generation and artifact path creation.
- [x] **Step 2: Confirm FAIL.**
- [x] **Step 3: Implement internal actions**:

```text
scan
capture
decode
spi = capture + decode
i2c = capture + decode
```

- [x] **Step 4: Preserve standalone decode** so an existing `.sr` can be decoded again with different channel mapping/options.
- [x] **Step 5: Before capture, print and persist Effective Config.**
- [x] **Step 6: Write artifacts to `06_Output/LogicAnalyzer/<run>/`.**
- [x] **Step 7: Re-run Workflow contract tests, Expected PASS.**
- [x] **Step 8: Commit** `feat(tools): add logic analyzer capture decode workflow`.

## Task 5: Add Unified Toolkit Router Commands

**Files:**
- Modify: `05_Tools/toolkit.ps1`
- Modify: `05_Tools/README.md`
- Modify: `05_Tools/Contracts/LogicAnalyzer/test_logic_analyzer.ps1`

**Public commands:**

```text
toolkit.bat logic doctor
toolkit.bat logic scan
toolkit.bat logic spi [profile/override args]
toolkit.bat logic i2c [profile/override args]
toolkit.bat logic decode <capture.sr> ...
```

Exact PowerShell parameter shape may follow current Router conventions, but external command family must remain `toolkit logic ...`.

- [x] **Step 1: Add Router contract tests** for valid/invalid subcommands and exit mapping.
- [x] **Step 2: Confirm FAIL.**
- [x] **Step 3: Add only routing/argument validation to `toolkit.ps1`; do not duplicate sigrok logic in Router.**
- [x] **Step 4: Update `05_Tools/README.md`** with Agent examples and temporary wiring override semantics.
- [x] **Step 5: Run Router/legacy regression and ensure existing commands remain unchanged.**
- [x] **Step 6: Commit** `feat(tools): expose logic analyzer toolkit commands`.

## Task 6: Add Project-Specific SPI / I2C Assertions

**Files:**
- Create: `05_Tools/Tests/S05C_LogicAnalyzer/test_spi.ps1`
- Create: `05_Tools/Tests/S05C_LogicAnalyzer/test_i2c.ps1`

**Produces:** Project Test verdicts independent of generic Logic Workflow.

- [x] **Step 1: Implement W25Q64 assertion** against a decoded JEDEC transaction:

```text
command  = 0x9F
expected = EF 40 17
```

- [x] **Step 2: Implement AT24C02 assertion** for address `0x50`, START / repeated START / STOP and ACK/NACK transaction structure.
- [x] **Step 3: Do not permanently assert EEPROM payload bytes** unless the test explicitly owns known fixture contents.
- [x] **Step 4: Run assertions against Golden Fixtures, Expected PASS.**
- [x] **Step 5: Verify generic Adapter/Workflow contains zero device-specific matches.**
- [x] **Step 6: Commit** `test(s05c): add spi and i2c project assertions`.

## Task 7: Real-Board SPI Acceptance

**Prerequisite:** Existing physical wiring remains unchanged.

Current verified mapping:

```text
D0 → PB12 → SPI2 CS
D1 → PB13 → SPI2 CLK
D3 → PB14 → SPI2 MISO
D5 → PB15 → SPI2 MOSI
```

- [x] **Step 1: Run `toolkit logic doctor/scan` and verify analyzer discovery.**
- [x] **Step 2: Start SPI capture before triggering/allowing target transaction.**
- [x] **Step 3: Capture W25Q64 read-only traffic.**
- [x] **Step 4: Confirm `.sr`, effective config, decode JSON and result JSON exist.**
- [x] **Step 5: Decode expected SPI transaction and run W25Q64 project assertion.**
- [x] **Step 6: Expected Project Verdict: PASS.**
- [x] **Step 7: Confirm no Flash erase/program was introduced for S05C.**
- [x] **Step 8: Preserve evidence for final Verification report.**

## Task 8: Real-Board I2C Acceptance

Current verified mapping:

```text
D2 → PB6 → I2C SCL
D4 → PB7 → I2C SDA
```

- [x] **Step 1: Start I2C capture before target access.**
- [x] **Step 2: Capture AT24C02 read-only transaction.**
- [x] **Step 3: Confirm address `0x50`, START/repeated START/STOP, R/W direction and ACK/NACK annotations.**
- [x] **Step 4: Confirm `.sr`, effective config, decode JSON and result JSON exist.**
- [x] **Step 5: Run project assertion, Expected PASS.**
- [x] **Step 6: Confirm no arbitrary EEPROM write was introduced.**
- [x] **Step 7: Preserve evidence for final Verification report.**

## Task 9: Full Regression, Verification And Handoff

**Files:**
- Create: `04_Test/Reports/Stages/S05C_Logic_Analyzer/verification.md`
- Update: `00_Project/03_Stages/S05C_Logic_Analyzer/handoff.md`
- Update: `00_Project/05_Status/current_status.md`
- Update: `PROJECT_CONTEXT.md`
- Update: `00_Project/02_Roadmap/development_roadmap.md`

- [x] **Step 1: Run all S05C Host/Contract/Fixture tests.**
- [x] **Step 2: Run existing Toolkit regression:** build, flash run, RTT, snapshot resume and existing contract suites.
- [x] **Step 3: Confirm no J-Link/sigrok owned process or lock remains after runs.**
- [x] **Step 4: Record SPI/I2C real-board evidence and failure-path observations.**
- [x] **Step 5: Explicitly record UART/GPIO as DEFERRED, not failed.**
- [x] **Step 6: Create final Verification report.**
- [x] **Step 7: Update handoff/status/context/roadmap only after verification evidence is complete.**
- [x] **Step 8: Commit** `docs(s05c): verify and hand off logic analyzer workflow`.

## Verification Matrix

```text
Host
├─ Config contract                  PASS required
├─ Executor fake-process contract   PASS required
├─ Parser golden fixtures           PASS required
├─ Workflow contract                PASS required
├─ Router contract                  PASS required
└─ Project assertions on fixtures   PASS required

Board
├─ Logic analyzer discovery         PASS required
├─ SPI capture/decode               PASS required
├─ W25Q64 assertion                 PASS required
├─ I2C capture/decode               PASS required
└─ AT24C02 assertion                PASS required

Regression
├─ toolkit build                    PASS required
├─ toolkit flash run                PASS required
├─ toolkit rtt                      PASS required
├─ toolkit snapshot resume          PASS required
└─ existing contracts               PASS required
```

## Stage Exit Condition

S05C 可以关闭的最低条件：

```text
SPI external evidence path      CLOSED / PASS
I2C external evidence path      CLOSED / PASS
Agent wiring override contract  PASS
Structured result contract      PASS
Golden fixture parser tests     PASS
Existing Toolkit regression     PASS
UART                            DEFERRED
GPIO                            DEFERRED
```

本阶段完成后，再决定是否单独增加 UART/GPIO 扩展任务；不得为了“协议齐全”扩大当前 S05C 实施范围。