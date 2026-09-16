# S05B Tools Toolkit Reuse Design

## Metadata

- Stage: `S05B_Toolkit_Reuse`
- Status: `DESIGN_APPROVED`
- Branch: `main`
- Baseline Commit: `9208cfd`
- Current Role: `S05B Design Role`
- Owner: Project Owner
- Updated At: `2026-09-16`

## 1. Objective

将当前工程已经验证过的 PC 侧工具，从“项目专用脚本集合”重构为一个配置驱动、职责分离、入口稳定的嵌入式开发工具框架。

本阶段不新增嵌入式业务功能，重点解决三类问题：

1. **扩展**：以后增加 CMake/GCC、OpenOCD、串口自动测试、逻辑分析仪、Core Dump、CI 等能力时，不重复实现路径、日志、进程、超时和资源清理逻辑；
2. **升级**：底层 Build / Probe / Debug 工具可以替换或升级，而上层 Workflow 和 Agent 调用入口尽量保持稳定；
3. **复用**：`05_Tools` 可以低成本迁移到其他同类 STM32 工程，通过配置完成适配，而不是修改通用脚本中的工程名、芯片、AXF、串口和本机绝对路径。

第一版复用边界仍限定为当前已经验证的 `STM32 + Keil + J-Link + GNU Arm GDB` 组合，不在 S05B 提前实现其他编译链或 Probe。

## 2. Design Principles

```text
Tool capability   可替换
Common capability 不重复
Project facts     配置化并进入 Git
Machine facts     本地化且不进入 Git
Workflow          保持稳定
Project tests     与通用能力隔离
Legacy entry      兼容但不双轨实现
New capability    按真实需求逐步扩展
```

`05_Tools` 的长期定位是“小型嵌入式开发工具平台”，但 S05B 不建立动态插件系统，不为尚不存在的工具预留复杂抽象。

## 3. Scope

### 3.1 In scope

- 重塑 `05_Tools` 的 Config、Core、Adapters、Workflows、Tests 和兼容入口边界；
- 将机器工具安装路径、工程固有参数、本机工程覆盖参数分离；
- 将 Build、Probe、Debug Adapter 按变化轴拆分，避免组合式 Adapter；
- 建立统一 Human / Agent 入口，现有 `Scripts/*.bat` 保留为兼容薄包装；
- 收敛路径解析、进程生命周期、Timeout、日志、J-Link Lock / Cleanup 等公共能力；
- 保留并回归现有 Firmware Image、Python Ymodem、Tera Term、GDB、CmBacktrace 能力；
- 将 S04 Persistence 归入项目专用测试扩展；
- 定义统一 Exit Code / Result Contract 的最低要求；
- 更新 `05_Tools/README.md` 与工具契约文档；
- 完成至少一次第二同类工程的配置替换复用演练，或获得 Project Owner 明确接受的等价证据。

### 3.2 Out of scope

- 不修改 `03_Firmware` 生产代码、Flash Layout、Firmware Image Contract、RTOS 接口或 Ymodem MCU 侧协议实现；
- 不实现 GCC/CMake、OpenOCD、GD32 或其他尚未实际使用的 Adapter；
- 不建立运行时插件发现、注册表或通用依赖注入框架；
- 不自动下载或安装 Keil、J-Link、GDB、Python、Tera Term；
- 不修改系统 `PATH`；
- 不把 S04 临时测试固件重新纳入正式 Application Target；
- 不为目录整齐而强制一次性迁移所有稳定协议/格式工具。

## 4. Architecture Decision

采用：

```text
Human / Agent
      │
      ▼
Unified Entry / Legacy Entry
      │
      ▼
Workflows
      │
 ┌────┴────┐
 ▼         ▼
Core    Adapters
          │
          ▼
 Keil / J-Link / GDB
```

Firmware、Ymodem、TeraTerm 和项目 Tests 作为独立能力位于该框架旁，由具体 Workflow 按需调用。

### 4.1 Core

`Core` 只提供与具体芯片、工程、第三方工具无关的公共能力：

```text
Config loading
Path resolution
Process lifecycle
Timeout
Exit code handling
Logging
Lock / ownership
Cleanup
```

Core 禁止硬编码：

```text
OTA_APP
STM32F411CE
COMx
S04 symbol
Keil/J-Link 专用命令参数
```

### 4.2 Adapters

Adapter 按变化轴拆分，而不是采用 `STM32_Keil_JLink` 组合 Adapter。

第一版：

```text
Adapters/
├─ Build/
│  └─ Keil/
├─ Probe/
│  └─ JLink/
└─ Debug/
   └─ GDB/
```

职责：

- `Build/Keil`：Keil/uVision 工程、Target、Build 参数、Build Exit Code；
- `Probe/JLink`：J-Link Commander、Device、Interface、Speed、Flash、Reset/Run；
- `Debug/GDB`：GNU Arm GDB、Symbol、Command Script、连接和退出合同。

未来若增加 `GCC + CMake`，优先新增 `Adapters/Build/GCC_CMake`，不复制 J-Link/GDB 能力。

### 4.3 Workflows

Workflow 表达“要完成什么动作”，而不是“某个工具怎么调用”。

第一版：

```text
Workflows/
├─ Application/
│  ├─ build
│  ├─ flash
│  ├─ run
│  └─ rtt
└─ Debug/
   ├─ snapshot
   └─ fault_capture
```

例如：

```text
toolkit build
→ Application Build Workflow
→ Build Adapter
→ Keil
```

底层 Adapter 变化时，Workflow 和 Agent 入口尽量保持不变。

## 5. Target Directory Layout

只创建实际有实现内容的目录，不预留空目录。

```text
05_Tools/
├─ Config/
│  ├─ toolchain.local.example.bat
│  ├─ project.defaults.bat
│  ├─ project.local.example.bat
│  └─ *.local.bat                 # ignored
│
├─ Core/
│  ├─ config
│  ├─ path
│  ├─ process
│  ├─ logging
│  └─ lock
│
├─ Adapters/
│  ├─ Build/
│  │  └─ Keil/
│  ├─ Probe/
│  │  └─ JLink/
│  └─ Debug/
│     └─ GDB/
│
├─ Workflows/
│  ├─ Application/
│  └─ Debug/
│
├─ Contracts/
├─ Firmware/
├─ Ymodem/
├─ TeraTerm/
│
├─ Tests/
│  └─ S04_Persistence/
│
├─ Scripts/                       # legacy compatibility wrappers
├─ toolkit.bat                    # unified router entry
└─ README.md
```

现有 `Firmware`、`Ymodem`、`TeraTerm` 和稳定 Debug 资源第一轮允许保留原路径。只有在引用、测试和兼容入口全部更新后才允许物理迁移。

当前 `CI/`、`Packaging/` 若在 S05B 结束时仍只有占位说明且没有实际职责，可以删除，不为“目录完整”保留空架构。

## 6. Configuration Contract

配置分为三层。

### 6.1 Machine configuration

`Config/toolchain.local.bat`：描述“这台电脑安装了什么”，不进入 Git。

```bat
set "KEIL_UV4="
set "JLINK_EXE="
set "JLINK_GDB_SERVER="
set "JLINK_RTT_LOGGER="
set "ARM_GDB="
set "PYTHON_EXE="
set "TERA_TERM_EXE="
```

仓库只提交 `toolchain.local.example.bat`。

### 6.2 Project defaults

`Config/project.defaults.bat`：描述“这个 Git 工程是什么”，进入 Git。

典型内容：

```bat
set "PROJECT_KEIL_PROJECT_FILE="
set "PROJECT_KEIL_TARGET="
set "PROJECT_OUTPUT_DIR="
set "PROJECT_APP_AXF="
set "PROJECT_APP_HEX="
set "PROJECT_APP_BIN="
set "PROJECT_LOG_DIR="
set "JLINK_DEVICE=STM32F411CE"
set "JLINK_IF=SWD"
```

工程名、Target、相对路径、芯片型号属于工程事实，不应要求每台开发机重复填写。

### 6.3 Local project override

`Config/project.local.bat`：描述“这台电脑运行这个工程时有什么不同”，不进入 Git。

典型覆盖：

```bat
set "SERIAL_PORT=COM10"
set "GDB_PORT=2331"
set "JLINK_SPEED=4000"
set "JLINK_RTT_CHANNEL=0"
```

没有覆盖需求时可以不存在。

### 6.4 Load and validation rules

最终配置由：

```text
project.defaults
+ project.local override
+ toolchain.local
= effective configuration
```

并遵守：

1. 工具包根目录从入口脚本自身位置推导；
2. 工程相对路径以 Project Root 解析；
3. 通用 Core / Adapter / Workflow 不拼接 `OTA_APP`、`STM32F411`、固定 COM 口等本工程专用字符串；
4. 缺少必需变量或路径不存在时，在启动外部进程前失败并指出变量名；
5. 本机绝对路径不得进入提交文件；
6. S04 专用变量只能存在于 `Tests/S04_Persistence` 边界中。

## 7. Unified Entry and Compatibility

新增稳定统一入口：

```text
toolkit.bat build
toolkit.bat flash
toolkit.bat run
toolkit.bat rtt
toolkit.bat snapshot halt
toolkit.bat snapshot resume
toolkit.bat fault capture
toolkit.bat fault trigger
toolkit.bat ymodem ...
toolkit.bat firmware pack ...
```

`toolkit.bat` 只负责参数解析和 Workflow 路由，不重复实现 Build / Flash / Debug 业务。

现有稳定入口继续保留：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Scripts/flash_app.bat
05_Tools/Scripts/rtt_capture.bat
05_Tools/Scripts/run_app_cycle.bat
05_Tools/Scripts/send_ymodem.bat
05_Tools/Scripts/send_ymodem_python.bat
```

迁移完成后旧入口必须成为薄包装，例如：

```text
build_app.bat
→ toolkit.bat build
```

禁止同时维护 Legacy 与 Workflow 两套核心实现。

## 8. Firmware / Ymodem / TeraTerm Boundary

第一版不强制将 Firmware、Ymodem、TeraTerm 改造成 Adapter。

- `Firmware`：Firmware Image 文件格式与打包能力；
- `Ymodem`：PC 侧文件传输协议能力；
- `TeraTerm`：当前真实板级 Ymodem Sender 实现；
- `Python Ymodem`：Host Test、Agent 自动化和协议诊断入口。

只有未来出现多个可互换 Transport 实现并产生真实重复逻辑时，再讨论 `Transport Adapter`。

## 9. Project Test Extension

S04 Persistence 明确属于：

```text
Tests/S04_Persistence/
```

其中可以包含：

```text
S04_PERSISTENCE_AXF
S04-specific parser
S04 GDB command
Board-test runner
Stage-specific result rules
```

以下能力仍属于通用框架：

```text
J-Link ownership
GDB process lifecycle
RTT capture
Timeout
Log output
Cleanup
```

普通 Application AXF 必须继续被拒绝，不能以“能加载符号”代替专用测试固件身份校验。

## 10. Workflow Contracts

### 10.1 Application

```text
load config
→ validate config / paths
→ build
→ validate build result
→ flash
→ reset / run
→ optional RTT capture
→ write log / result
→ return stable exit code
```

### 10.2 Debug

```text
load config
→ validate AXF / GDB / J-Link GDB Server
→ acquire J-Link ownership
→ start exactly one GDB Server owner
→ run snapshot / fault command
→ collect result
→ exit according to session contract
→ cleanup owned child processes
→ release J-Link ownership
```

S05A 已验证的 GDB 合同保持不变：

```text
halt state
→ detach
→ MCU remains halted

running state
→ continue&
→ disconnect
→ quit
→ MCU continues running
```

S05B 只抽取配置、生命周期和复用边界，不重新定义 GDB 调试语义。

## 11. Tool Ownership and Ordering

- 同一时刻只允许一个工具持有 J-Link；
- 只清理当前 Workflow 自己启动的进程，不关闭用户已有进程；
- 失败时按启动顺序逆序清理；
- 必须先启动的 Listener / Sender / Debugger 顺序继续由 Workflow 明确表达；
- `run_app_cycle` 语义继续保持 `Build → Flash → Reset/Run → RTT Capture`；
- 所有外部调用记录命令摘要、开始/结束时间、退出码和日志路径，不记录敏感凭证。

## 12. Result Contract

S05B 至少冻结统一 Exit Code 分类：

```text
0   SUCCESS
10  CONFIG_ERROR
20  BUILD_ERROR
30  PROBE_ERROR
40  DEBUG_ERROR
50  TRANSFER_ERROR
60  TEST_ERROR
```

具体脚本可以继续保留工具原始日志，但 Workflow 必须把外部工具错误映射为稳定分类。

结构化结果文件作为演进方向：

```text
06_Output/Results/build.json
06_Output/Results/flash.json
06_Output/Results/debug_snapshot.json
```

S05B 不要求一次性让所有历史入口 JSON 化；现有 Python Ymodem `--json` 能力继续保留。

## 13. Migration Order

1. 建立三层 Config 合同与加载规则；
2. 抽取 Core 公共路径、进程、日志、锁和 Cleanup；
3. 拆分 Build/Keil、Probe/JLink、Debug/GDB Adapter；
4. 建立 Application / Debug Workflow；
5. 增加 `toolkit.bat` Router；
6. 将旧 `Scripts` 改为薄包装，并删除重复实现；
7. 将 S04 Persistence 收敛到项目 Test Extension；
8. 回归 Firmware、Ymodem、GDB、CmBacktrace、Tool Sequence；
9. 执行第二同类工程复用演练；
10. 同步 README、验证报告、handoff 和 review。

## 14. Verification and Acceptance

### Level 1 - Regression

现有稳定能力不得退化：

```text
Build
Flash
RTT
GDB Runtime Snapshot
Fault Capture
CmBacktrace contracts
Firmware Pack
Ymodem Sender
```

### Level 2 - Configuration-driven current project

通用工具实现不得硬编码：

```text
OTA_APP
STM32F411CE
COM9
机器绝对路径
S04-specific symbol
```

其中 STM32F411CE 可以存在于 `project.defaults.bat`，但不能散落在 Core / Adapter / Workflow 实现中。

### Level 3 - Extensibility

证明 Workflow 与具体 Adapter 已解耦；新增 Build 实现时不需要修改 Core，也不应复制 Probe/GDB 公共能力。

S05B 不要求实现真正的新编译链，可以通过契约测试或 Mock Adapter 证明边界。

### Level 4 - Reuse

在另一个同类 `STM32 + Keil + J-Link` 工程中：

```text
copy 05_Tools
→ fill toolchain.local
→ provide project.defaults
→ optional project.local
→ invoke toolkit / workflow
```

验收至少覆盖新工程的 Keil Project、Target、AXF/HEX/BIN、芯片型号、输出目录和日志目录；不得编辑 Core / Adapter / Workflow 来适配第二工程。

## 15. Completion Criteria

S05B 只有在以下条件同时满足后才能关闭：

1. `design.md`、`implementation_plan.md`、`handoff.md`、`review.md` 和验证报告完整；
2. `05_Tools/README.md` 与实际结构和配置合同一致；
3. 当前工程全部稳定入口完成回归；
4. 旧 `Scripts` 仅作为兼容入口，不存在双份核心实现；
5. 至少完成一次第二同类工程复用演练，或由 Project Owner 明确接受等价配置替换证据；
6. 未提交本机绝对路径、临时板测代码、构建缓存和调试输出；
7. Host / Contract / Board 验证证据明确区分；
8. 未验证项明确记录，不把脚本调用成功写成新的板级功能 PASS。

## 16. Approved Decision

Project Owner 已确认：

- S05B 的目的不是简单整理目录，而是为后续工具扩展、功能升级和跨工程复用建立稳定基础；
- 采用 `Config + Core + Adapters + Workflows + Project Tests + Legacy Wrappers`；
- Adapter 按 Build / Probe / Debug 变化轴拆分；
- 配置采用 `toolchain.local + project.defaults + project.local` 三层模型；
- 增加统一 `toolkit.bat` Router，同时保留旧入口兼容；
- 不在 S05B 建立重型插件系统；
- 正式阶段设计文件保存在本 Stage 目录，不再以 `docs/superpowers/specs/` 作为项目设计落点。

下一步进入 Implementation Plan 编写；在计划完成并进入 `READY_FOR_IMPLEMENTATION` 前，不修改工具实现。