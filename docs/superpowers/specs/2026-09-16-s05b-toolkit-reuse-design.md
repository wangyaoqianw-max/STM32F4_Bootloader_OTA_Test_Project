# S05B Tools Toolkit Reuse Design

## Metadata

- Stage: `S05B_Toolkit_Reuse`
- Status: `DRAFT`
- Branch: `main`
- Baseline Commit: `9208cfd`
- Target: 同类 `STM32 + Keil + J-Link` 工程
- Owner: Project Owner

## 1. Goal

将当前工程的 `05_Tools` 从“本工程脚本集合”重塑为可复制到同类 STM32 + Keil + J-Link 工程的 PC 工具包。

复制工具目录后，使用者只需要填写机器工具配置和目标工程配置，就可以复用编译、烧录、RTT、GDB 调试、固件打包和 Ymodem 等能力，不需要把本工程名称、芯片型号、AXF 路径、日志路径和串口号写进通用脚本。

本阶段只冻结当前目标平台的复用边界。未来接触 GD 等其他厂商芯片，或使用 GCC/CMake 等其他编译链时，通过新增 Adapter 扩展，不在 S05B 中提前实现。

## 2. Scope

### In scope

- 重新划分 `05_Tools` 的公共核心、平台适配、工作流、协议/格式包和项目测试扩展边界；
- 将机器相关配置与项目/目标相关配置分离；
- 把编译、烧录、RTT、GDB 和测试入口改为配置驱动；
- 保留现有 `05_Tools\Scripts\*.bat` 公共入口作为兼容包装层；
- 保留并验证当前 Firmware Image、Python Ymodem 和 Tera Term 能力；
- 将 S04 Persistence 视为项目测试扩展，不能污染通用 STM32 + Keil + J-Link 工作流；
- 增加配置缺失、路径解析、工具调用顺序、日志输出和 J-Link 占用释放的契约检查；
- 更新 `05_Tools\README.md`，明确目录职责、支持能力、配置方式和复用边界。

### Out of scope

- 不修改 `03_Firmware` 生产代码、固件接口、Flash Layout 或硬件资源；
- 不实现 GD 或其他厂商 Adapter；
- 不实现其他编译链 Adapter；
- 不把本地工具安装路径加入系统 `PATH`；
- 不自动下载或安装 Keil、J-Link、GNU Arm GDB、Python 或 Tera Term；
- 不把 S04 临时板测固件源代码重新纳入正式 Application 工程；
- 不要求把所有既有文件一次性物理迁移；迁移必须在兼容入口和引用更新完成后进行。

## 3. Architecture Decision

采用“公共核心 + `STM32_Keil_JLink` 适配器 + 工作流入口 + 项目测试扩展”的分层方案。

```text
Public wrapper / workflow entry
              ↓
Config loader + Core path/process/logging helpers
              ↓
STM32_Keil_JLink Adapter
              ↓
Keil / J-Link / GNU Arm GDB / RTT Logger
              ↓
Configured project outputs and logs
```

职责边界如下：

| 层 | 职责 | 禁止依赖 |
| --- | --- | --- |
| `Config` | 读取机器工具路径、项目路径和目标参数 | 生产固件代码 |
| `Core` | 路径解析、前置检查、进程生命周期、日志和锁 | 具体工程名、具体芯片默认值 |
| `Adapters/STM32_Keil_JLink` | 把统一操作映射到 Keil、J-Link 和 GNU Arm GDB 参数 | S04 专用符号、临时板测源 |
| `Workflows` | 编译、烧录、RTT、GDB 和通用测试的编排 | 机器绝对路径 |
| `Firmware` / `Ymodem` / `TeraTerm` | 固件格式和传输协议能力 | J-Link 生命周期 |
| `Tests/S04_Persistence` | 当前工程的 Persistence 测试 runner、解析器和 AXF 约束 | 通用工作流接口 |
| `Scripts` | 兼容旧入口的薄包装脚本 | 重复实现核心逻辑 |

## 4. Target Directory Layout

S05B 的目标目录如下。只创建实际有文件的目录，不为未来功能预留空目录。

```text
05_Tools/
├── Config/
│   ├── toolchain.local.example.bat
│   ├── project.local.example.bat
│   └── *.local.bat                         # ignored, never committed
├── Core/
│   ├── path/                               # shared path and config helpers
│   ├── process/                            # process, timeout and cleanup helpers
│   ├── logging/                            # common log naming and output helpers
│   └── README.md
├── Adapters/
│   └── STM32_Keil_JLink/
│       └── README.md                       # current supported adapter contract
├── Workflows/
│   ├── Application/                        # build / flash / RTT / run cycle
│   ├── Debug/                              # GDB server / snapshot / fault capture
│   └── Test/                               # reusable host and tool contract tests
├── Contracts/                              # command/data contracts for tool tests
│   ├── GDB/
│   ├── CmBacktrace/
│   └── ToolSequence/
├── Debug/                                  # existing GDB/CmBacktrace assets
├── Firmware/                               # Firmware Image V1 package
├── Ymodem/                                 # Python protocol package and tests
├── TeraTerm/                               # reference sender macro
├── Tests/
│   └── S04_Persistence/                    # project-specific extension
├── Scripts/                                # backward-compatible thin wrappers
└── README.md
```

现有 `Firmware`、`Ymodem`、`TeraTerm` 和 `Debug` 中已经形成稳定职责的内容，可以在第一轮保留原路径；只有在全部引用、测试和兼容包装完成后才允许物理迁移。目录分类先解决依赖边界，避免为目录改名引入无关风险。

## 5. Configuration Contract

### 5.1 Machine configuration

`Config\toolchain.local.bat` 只保存本机安装路径，继续保持 ignored：

```bat
set "KEIL_UV4="
set "JLINK_EXE="
set "JLINK_GDB_SERVER="
set "JLINK_RTT_LOGGER="
set "ARM_GDB="
set "PYTHON_EXE="
set "TERA_TERM_EXE="
```

这些变量不得写入提交的生产脚本，也不得通过脚本修改系统 `PATH`。示例文件只提供变量名，不提交本机绝对路径。

### 5.2 Project and target configuration

新增 `Config\project.local.example.bat`，本机使用者复制为 ignored 的 `project.local.bat`：

```bat
set "PROJECT_ROOT="
set "PROJECT_KEIL_PROJECT_FILE="
set "PROJECT_KEIL_TARGET="
set "PROJECT_OUTPUT_DIR="
set "PROJECT_APP_AXF="
set "PROJECT_APP_HEX="
set "PROJECT_APP_BIN="
set "PROJECT_LOG_DIR="
set "JLINK_DEVICE="
set "JLINK_IF=SWD"
set "JLINK_SPEED=4000"
set "GDB_PORT=2331"
set "JLINK_RTT_CHANNEL=0"
set "SERIAL_PORT="
```

配置解析规则：

1. 脚本从自身位置推导工具包根目录；`PROJECT_ROOT` 为空时才使用工具包根目录的上级目录作为默认工程根目录；
2. `PROJECT_*` 路径优先使用用户配置，不在脚本中拼接本工程 `OTA_APP`、`MDK-ARM` 或 `STM32F411` 字样；
3. `JLINK_DEVICE`、接口、速率、GDB 端口和 RTT 通道由项目配置提供；Adapter 只负责校验和转换参数；
4. 缺少必需配置或路径不存在时，在启动外部进程前失败，并明确指出变量名；
5. 日志统一写入 `PROJECT_LOG_DIR`，未配置时使用项目约定的输出目录，并在 README 中说明；
6. S04 专用变量只能由 `Tests\S04_Persistence` 读取，不进入公共 Application/Debug 工作流。

### 5.3 S04 project extension

Persistence runner 可以继续使用外部 AXF 和当前工程专用解析器，但配置放在测试扩展边界内，例如：

```bat
set "S04_PERSISTENCE_AXF="
set "S04_PERSISTENCE_PARSER="
set "S04_PERSISTENCE_GDB_SCRIPT="
set "S04_PERSISTENCE_CAPTURE_SECONDS=90"
```

普通 Application AXF 必须继续被拒绝，不能以“能加载符号”代替专用测试固件身份校验。

## 6. Workflow Contracts

### Application workflow

```text
load machine config
→ load project config
→ validate Keil project and output paths
→ build
→ validate build result
→ flash through J-Link
→ reset/run
→ optionally start RTT capture
→ write logs and return status
```

### Debug workflow

```text
load config
→ validate AXF / GDB / J-Link GDB Server
→ start exactly one GDB Server owner
→ run snapshot / fault command
→ collect result
→ disconnect or detach according to session contract
→ release child processes and J-Link ownership
```

GDB 退出行为继续遵守 S05A 已验证合同：运行态使用 `continue& → disconnect → quit`，暂停态使用 `detach` 后保持目标暂停。S05B 只抽取配置、生命周期和入口边界，不重新定义 GDB 调试语义。

### Tool ownership and ordering

- 同一时间只能有一个工具持有 J-Link；
- 烧录/复位前，必须先打开依赖的 RTT、GDB Server 或 Ymodem 等监听工具；
- 工具失败时按启动顺序逆序清理自己启动的进程，不关闭用户未启动的进程；
- `run_app_cycle` 的语义仍是 `Build → Flash → Reset/Run → RTT Capture`，但实现移动到 `Workflows/Application`，旧入口只做转发；
- 所有外部调用都记录命令摘要、开始/结束时间、退出码和日志路径，不记录敏感凭证。

## 7. Compatibility and Migration

采用分阶段迁移：

1. 先拆分 `toolchain.local.bat` 与 `project.local.bat`，建立公共路径/进程/日志入口；
2. 再将现有 BAT/PowerShell 逻辑收敛到 `Workflows` 和 `Adapters/STM32_Keil_JLink`；
3. 将 GDB、CmBacktrace 和工具顺序检查归入 `Contracts`；
4. 将 S04 runner 收敛到 `Tests/S04_Persistence`，保留外部 AXF 和项目解析器约束；
5. 最后删除重复实现，但不删除 `Scripts` 兼容入口。

第一轮迁移完成后，以下入口必须继续可用：

```text
05_Tools\Scripts\build_app.bat
05_Tools\Scripts\flash_app.bat
05_Tools\Scripts\rtt_capture.bat
05_Tools\Scripts\run_app_cycle.bat
05_Tools\Scripts\send_ymodem.bat
05_Tools\Scripts\send_ymodem_python.bat
```

## 8. Verification and Acceptance

### Code / host verification

- 配置示例不包含本机绝对路径；`*.local.bat` 被 `.gitignore` 忽略；
- 缺配置、错路径、端口冲突和工具退出码均能在外部进程启动前或工作流结束时被识别；
- 新工作流不包含本工程名、`OTA_APP`、固定芯片型号或固定 `COM9`；
- 现有 Firmware Image、Ymodem、GDB、CmBacktrace 和工具顺序契约测试通过；
- `git diff --check` 通过；
- 当前 Keil Application 编译结果不因工具目录重塑而改变。

### Reuse verification

在另一个同类 STM32 + Keil + J-Link 工程中，只修改两个本地配置文件即可：

```text
copy 05_Tools
→ copy toolchain.local.example.bat / project.local.example.bat
→ fill toolchain.local.bat / project.local.bat
→ invoke Application / Debug workflow
```

验收必须证明脚本能够使用新工程的 Keil 工程、AXF、芯片型号、输出目录和日志目录；不得通过编辑通用脚本来适配第二个工程。

### Board verification status

S05B 的第一目标是工具包结构和可复用入口。若只完成主机验证，硬件验证记录为 `PENDING`，不得将脚本调用成功写成新的板级功能 PASS。需要实际板测时，仍按 S05A 已验证的 J-Link / RTT / GDB 顺序执行。

## 9. Completion Criteria

S05B 只有在以下条件同时满足后才能关闭：

1. 设计、实施计划、交接、Review 和验证报告完整落盘；
2. `05_Tools` README 与实际目录、配置和入口一致；
3. 当前工程的全部稳定入口通过主机/契约验证；
4. 至少完成一次“第二个同类工程只改本地配置”的复用演练，或由 Project Owner 明确接受等价的配置替换证据；
5. 未提交机器绝对路径、临时板测代码、构建缓存和调试输出；
6. 代码验证与硬件验证分别记录，未验证项明确列出。
