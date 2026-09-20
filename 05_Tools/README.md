# Tools

`05_Tools` 是项目 PC 侧工具和自动化入口的统一目录。当前工具主要覆盖：

```text
Keil 编译 → J-Link 烧录 → RTT 采集
                     ↘ GDB 在线调试 / Runtime Snapshot
Fault Test Build → Flash prepare → GDB Server/GDB → monitor reset → halt → continue → Fault Capture → RTT 读取
PC 固件文件 → YMODEM 发送 → STM32F411 → External Flash
Keil .bin → Firmware Image V1 .img
```

当前文档基于 `2026-09-20` 的仓库状态。机器相关路径只保存在被 Git
忽略的 `Config/toolchain.local.bat`，不能把本机路径写入脚本或提交到 Git。

## 统一入口

推荐使用 `05_Tools\toolkit.bat`，旧 `Scripts\*.bat` 继续作为兼容入口：

```text
toolkit.bat build [application|bootloader]
toolkit.bat flash [application|bootloader] [run|prepare]
toolkit.bat run [application|bootloader] [rtt_seconds]
toolkit.bat rtt [application|bootloader] [seconds]
toolkit.bat sync-s08
toolkit.bat snapshot [application|bootloader] [halt|resume]
toolkit.bat fault [application|bootloader] [capture|trigger]
toolkit.bat firmware pack <pack_firmware.py arguments>
toolkit.bat ymodem [python] <ymodem_sender.py arguments>
toolkit.bat ymodem tera <COMx> <baud> <firmware.img>
toolkit.bat factory restore -ConfirmDestructive [-Image <v1.0.img>] [-Port COMx] [-Baud 115200]
```

Router 只负责参数校验、配置读取、Workflow/既有工具转发和统一退出码，不复制 Firmware
打包或 YMODEM 协议实现。

## 目录总览

| 目录 | 当前内容 | 支持的功能 |
|---|---|---|
| `Config/` | `toolchain.local.example.bat`、`project.defaults.bat`、`project.local.example.bat` | 分离机器工具路径、工程事实和本机工程覆盖 |
| `Scripts/` | `.bat` / `.ps1` 稳定入口 | 编译、烧录、RTT、GDB、S04 持久性测试、YMODEM 和开发闭环 |
| `Core/` | 配置、路径、进程、日志、锁 | 被 Adapter/Workflow 复用的公共能力 |
| `Adapters/` | Keil、J-Link、GDB 适配器 | 对接具体本机工具，不承载 Workflow 编排 |
| `Workflows/` | Application、Debug 工作流 | 表达 Build/Flash/RTT/Snapshot/Fault 动作和验收证据 |
| `Tests/S04_Persistence/` | S04 专用 runner、GDB 资产和入口 | 项目测试扩展，不进入通用 Core/Adapter/Workflow |
| `Firmware/` | `pack_firmware.py`、测试 | 生成 S04 Firmware Image V1，校验 Header/Payload 规则 |
| `Ymodem/` | Python Sender、串口/协议模块、Host Test | 自动识别串口、发送固件、JSON 结果、协议测试 |
| `TeraTerm/` | `send_ymodem.ttl` | Tera Term 5 真实板级 YMODEM 发送宏 |
| `Debug/` | GDB/CmBacktrace 命令脚本、自动化契约测试 | Runtime Snapshot、Fault Capture、CmBacktrace 接入和失败路径检查 |

## 当前工具与支持功能

### 1. Application / Bootloader 编译、烧录和运行时采集

| 入口 | 支持功能 | 主要输出 |
|---|---|---|
| `Scripts/build_app.bat` | 使用 Keil/uVision 编译 `OTA_APP` Target；区分成功、警告、错误退出码 | `06_Output/Logs/OTA_APP_build.log`；生成 Keil `.hex/.bin/.axf` |
| `Scripts/flash_app.bat run` | 使用 J-Link Commander 通过 SWD 烧录 `OTA_APP.hex`，随后 Reset → Halt → Go | `06_Output/Logs/OTA_APP_flash.log` |
| `Scripts/flash_app.bat prepare` | 烧录 `OTA_APP.hex` 后保持 MCU Halt，供必须在下一次 Reset 前启动的调试工具使用 | `06_Output/Logs/OTA_APP_flash.log` |
| `Scripts/rtt_capture.bat [秒数]` | 使用 J-Link RTT Logger 采集 RTT Up Channel 0；默认 10 秒 | `OTA_APP_rtt.log`、`OTA_APP_rtt_logger.log` |
| `Scripts/run_app_cycle.bat [秒数]` | `Build → Flash(run) → RTT Capture` 一键闭环；编译警告会保留为警告结果 | 上述编译、烧录、RTT 日志 |

Bootloader 使用同一套 Workflow/Adapter，通过目标参数切换：

```bat
05_Tools\toolkit.bat sync-s08
05_Tools\toolkit.bat build bootloader
05_Tools\toolkit.bat flash bootloader prepare
05_Tools\toolkit.bat run bootloader 20
```

`sync-s08` 用于 CubeMX 重新生成后恢复 S08 的 Keil IROM/OCR_RVCT4、Application
VTOR、Include Path、CmBacktrace 配置和 Bootloader 工程 Groups；不会复制第二套工具链。

这些入口默认使用 `STM32F411CE / SWD / 4000 kHz`。闭环成功只表示工具链动作成功，不能替代阶段级功能验收。

### 2. GDB 在线调试与 Runtime Snapshot

| 入口 | 支持功能 |
|---|---|
| `Scripts/start_gdb_server.bat` | 前台启动 J-Link GDB Server，供手工 GDB 会话使用 |
| `Scripts/gdb_runtime_snapshot.bat halt` | 启动临时 Server，读取 Application 寄存器、PC、源代码位置、Backtrace、栈内存，然后 `detach → quit`；目标保持暂停 |
| `toolkit.bat snapshot bootloader halt` | 使用 Bootloader AXF 读取 Bootloader 运行态信息；目标保持暂停 |
| `Scripts/gdb_runtime_snapshot.bat resume` | 读取同样的 Application 运行态信息，然后 `continue& → disconnect → quit`；目标继续运行 |
| `Debug/GDB/test_gdb_automation.ps1` | 检查 GDB 脚本合同、禁止 `load`、入口存在性和常见失败路径 |

GDB 使用 Keil 生成的 `OTA_APP.axf` 加载符号，但不会执行 GDB `load`，不会隐式烧录 Flash。Resume 模式不使用 `-batch`，也不在 `continue&` 后执行 `detach`。Runtime Snapshot 日志为：

```text
06_Output/Logs/OTA_APP_gdb_server.log
06_Output/Logs/OTA_APP_gdb_snapshot.log
```

### 3. S04 Reset / Power-cycle Persistence 临时测试

| 入口 | 支持功能 | 主要输出 |
|---|---|---|
| `Scripts/s04_persistence_test.bat reset` | GDB 读取复位前后快照，执行 `monitor reset → continue& → disconnect`；不烧录、不擦写 External Flash/EEPROM | `S04_reset_persistence.log`、`S04_persistence_gdb_server.log` |
| `Scripts/s04_persistence_test.bat power-cycle` | 先启动 RTT Logger，检测到 `READY` 后由操作者关闭/打开电源；按 AXF 固定 RTT 地址重连，覆盖 Logger 退出和无数据卡死 | `S04_power_cycle_persistence.log`、分段 RTT/诊断日志 |
| `Tests/S04_Persistence/s04_reset_persistence.gdb` | 读取 `g_s04PersistenceSnapshot`，验证 GDB 复位退出链 | GDB 快照文本 |
| `04_Test/Host/S04_Firmware_Image_Storage/s04_persistence_log.py` | 解析快照并比较 Image、Metadata、Slot 状态和版本号 | PASS / FAIL / NOT_READY |

板测固件只读取 Slot B 和 Metadata A/B，不调用擦除、写入或 Commit。临时板测入口已从正式
Application/Keil target 移除，源码保留在 `04_Test/Board/S04_Firmware_Image_Storage`；
复测时须先临时接入并单独生成测试 AXF，不能直接使用普通 `OTA_APP.axf`。
`power-cycle` 使用 RTT，不占用 `COM9`；当前串口模块 `COM9` 仅作为本机硬件记录，串口类
工具仍应显式传入该端口。

调用顺序：

```text
Build → Flash(run) 启动只读测试固件 → 关闭可能占用 J-Link 的工具 → 启动 S04 listener → READY → 操作电源键
```

`reset` 模式由 GDB 负责复位；`power-cycle` 模式由操作者负责电源断开/恢复。Power-cycle
模式先确认 AXF 含有 `app_s04_persistence_test_run` 测试入口，再用 ARM GDB 离线读取 AXF 中的
`_SEGGER_RTT` 地址，将该地址传给 RTT Logger；Logger
退出或连续 5 秒无新数据时自动重启，重连间隔按 1/2/4 秒退避并记录原因。Host 判定 PASS
要求控制器确认启动标志后还捕获到新快照，并比较事件前后的快照；电源键动作仍需操作者
确认并记录到 S04 验证报告。脚本使用 `06_Output/Logs/toolkit_jlink.lock` 防止
多个本地测试进程同时占用 J-Link，仍需关闭 Keil Debug、RTT Viewer 等外部 J-Link 客户端。

### 4. Fault / Crash 诊断

| 入口 | 支持功能 |
|---|---|
| `Scripts/gdb_fault_capture.bat capture` | 连接已经停在 Application Fault Handler 的 MCU，读取 Fault PC/LR、MSP/PSP、CFSR/HFSR/MMFAR/BFAR、源码位置、Backtrace 和栈内存；保持 Halt |
| `toolkit.bat fault bootloader capture` | 使用 Bootloader AXF 读取已停机的 Bootloader Fault 现场；保持 Halt |
| `Scripts/gdb_fault_capture.bat trigger` | 适用于启用 `DIAG_FAULT_TEST_ENABLE=1` 的测试固件；由已启动的 GDB 会话设置 Fault-loop 断点，再执行 `monitor reset → halt → continue`，命中后采集 Fault |
| `Debug/GDB/fault_capture.gdb` | 已发生 Fault 的只读现场采集脚本，不执行 `load`、`reset` 或 `continue&` |
| `Debug/GDB/fault_trigger_capture.gdb` | 在 GDB 已启动后设置捕获断点，执行复位、halt 和阻塞 `continue`，待工程 Fault Handler 保存现场后采集 |
| `Debug/CmBacktrace/test_fault_diagnostics.ps1` | 检查 Fault 类型、现场字段、CmBacktrace 调用和 Fault Capture 契约 |
| `Debug/test_tool_sequence.ps1` | 检查 prepare、预启动 GDB、复位触发和 J-Link 所有权顺序 |

Fault 测试的调用顺序固定为：`build_app.bat → flash_app.bat prepare → gdb_fault_capture.bat trigger`。
其中 `prepare` 烧录后不运行；`gdb_fault_capture.bat trigger` 会先启动 GDB Server/GDB 客户端并设置
`diagnostics_fault_capture_stop` 断点，随后才由 GDB 执行 `monitor reset`、`monitor halt` 和阻塞 `continue`。
RTT Logger 与 J-Link Commander/GDB Server 不能同时占用同一个 Probe，因此 RTT Fault 日志在 GDB
`detach` 释放 J-Link 后再读取 RTT 缓冲，不与 GDB 并行抢占设备。

### 5. Firmware Image V1 打包

`Firmware/pack_firmware.py` 将 Application Payload 打包为 S04 固定格式；统一入口只做转发：

```text
.img = 64 Byte Header + Payload
```

支持版本号、Payload 长度、Payload CRC32、Header CRC32 和 Slot 容量检查。示例：

```powershell
05_Tools\toolkit.bat firmware pack `
  --input .\03_Firmware\Application\OTA_APP\MDK-ARM\Objects\OTA_APP.bin `
  --output .\06_Output\Packages\OTA_APP_v1.1.0.img `
  --version 1.1.0
```

此工具只负责生成 `.img`，不负责串口传输、Flash 烧录或 Bootloader 安装。

### 5.1 S09 Factory Restore

`factory restore` 是 destructive operation，会清空 External Flash Slot A/B 和 AT24C02 Metadata，使用独立的 S09 board test 通过正式 Firmware Storage / Metadata / YMODEM 路径写入 Slot A v1.0 baseline，然后重新构建并烧录正式 v1.0 Application 到 Internal APP。必须显式传入 `-ConfirmDestructive`：

```powershell
05_Tools\toolkit.bat factory restore `
  -ConfirmDestructive `
  -Image .\06_Output\Packages\app_v1.0.img `
  -Port COM9 -Baud 115200
```

该流程不会把 Factory Restore board test 加入正式 `OTA_APP.uvprojx`，流程结束后会恢复工程文件并执行正式 Application Build / Flash / RTT 验证。

### 6. YMODEM 固件发送

#### Tera Term 真实板级入口

```bat
05_Tools\toolkit.bat ymodem tera COM10 115200 firmware.img
```

使用 `TeraTerm/send_ymodem.ttl` 执行单文件 YMODEM 发送，支持串口号、波特率和固件文件参数。当前 S05 板端完整验收输入是包含 64 Byte Header 的 `.img`，不能直接把原始 `.bin` 当作最终验收文件。

#### Python / Agent 入口

```powershell
05_Tools\toolkit.bat ymodem devices --json
05_Tools\toolkit.bat ymodem send firmware.img --port COM10 --baud 115200 --json
```

Python Sender 支持：

- 自动列出和选择串口，也支持 `--port` 手动指定；
- `--vid`、`--pid`、`--match` USB/描述过滤；
- YMODEM-1K 文件发送、超时、重试和取消结果；
- `--json` 输出机器可解析结果；
- 无真实串口的 Mock/Scripted Transport Host Test。

Python Sender 主要用于 Host Test、Agent 自动化和协议诊断；S05 默认真实板级验收仍使用 Tera Term 入口。

### 7. 监听器、烧录/复位和 J-Link 所有权

需要接收串口早期启动信息或 YMODEM 的场景，必须先启动串口监听器/Tera Term，确认已经打开并等待到设备发送 `C`，再执行会触发复位的烧录或 GDB 操作；否则会漏掉启动阶段信息。`flash prepare` 只在需要由后续 GDB 复位时使用，不能替代监听器预启动。

RTT Logger 是 J-Link 客户端，受单 Probe 所有权限制，不能与 J-Link Commander、GDB Server 或 RTT Viewer 并行连接。Fault 流程因此严格执行 `GDB detach/quit → 释放 J-Link → RTT Logger`；串口监听器不占用 J-Link，可提前打开。

工具只在共享资源上互斥：不共享同一客户端、设备、端口或输出文件/目录的 Workflow 可以并行运行。sigrok 逻辑分析仪与 J-Link、Keil、RTT 客户端使用独立设备，可以在输出路径不冲突时并行；同一逻辑分析仪实例、同一串口和同一 J-Link Probe 仍必须串行。

## 配置与依赖

首次使用：

```text
复制 Config/toolchain.local.example.bat
为   Config/toolchain.local.bat
填写本机实际路径；需要本机覆盖时再复制 project.local.example.bat
为   project.local.bat
```

配置项按用途分为：

```text
KEIL_UV4             Keil/uVision 编译器入口
JLINK_EXE            J-Link Commander 烧录入口
JLINK_RTT_LOGGER     J-Link RTT Logger 入口
ARM_GDB              arm-none-eabi-gdb.exe
JLINK_GDB_SERVER     JLinkGDBServerCL.exe
PYTHON_EXE           Python；为空时使用 PATH 中的 python
TERA_TERM_EXE        Tera Term 5 主程序
JLINK_DEVICE/IF/SPEED 目标连接参数
GDB_PORT             GDB Server 端口，默认 2331
JLINK_RTT_CHANNEL    RTT 通道，默认 0
SERIAL_PORT          外部串口模块，当前为 COM9；RTT 测试不使用
S04_PERSISTENCE_CAPTURE_SECONDS  S04 电源循环监听时长，默认 90 秒
```

配置加载顺序固定为：`project.defaults.bat → project.local.bat（可选） → toolchain.local.bat`。
`project.defaults.bat` 进入 Git；两个 `.local.bat` 只保留本机信息并被忽略。

统一退出码类别为：`0 SUCCESS`、`1 BUILD_WARNING`（仅 Build/Run 警告）、`10 CONFIG_ERROR`、
`20 BUILD_ERROR`、`30 PROBE_ERROR`、`40 DEBUG_ERROR`、`50 TRANSFER_ERROR`、`60 TEST_ERROR`。
Firmware/YMODEM 的外部工具非零退出码统一映射为 `50 TRANSFER_ERROR`，不能透传原始 `1`。

## Logic Analyzer

S05C 通过 `sigrok-cli` 提供 SPI / I2C 外部总线证据，统一入口为：

```bat
05_Tools\toolkit.bat logic doctor
05_Tools\toolkit.bat logic scan
05_Tools\toolkit.bat logic spi -Profile spi2_flash -CaptureTimeMilliseconds 20000
05_Tools\toolkit.bat logic i2c -Profile i2c_eeprom
05_Tools\toolkit.bat logic decode 06_Output\LogicAnalyzer\<run>\capture.sr -Protocol spi -Profile spi2_flash
```

机器配置模板中的 `SIGROK_CLI_EXE` 需要填写本机 `sigrok-cli` 路径；该路径只保存在未提交的
`Config\toolchain.local.bat`。默认通道映射保存在已提交的
`Config\logic_analyzer.profiles.json`，每次采集的实际配置会保存到
`06_Output\LogicAnalyzer\<run>\effective_config.json`。

通道覆盖只影响当前运行，例如：

```bat
05_Tools\toolkit.bat logic i2c -Profile i2c_eeprom -SclChannel D0 -SdaChannel D1
```

该命令不会修改默认 profile；只有明确要求固定新接线时才更新 profile。每次运行还会保存
`capture.sr`、`decode.json` 和 `result.json`，已有 `.sr` 可以直接重新 Decode。通用 Logic Workflow
的状态为 `SUCCESS / ERROR / INCONCLUSIVE`，无总线活动或空解码不会直接判定项目功能 FAIL。

采集既可以使用 `-Samples`，也可以使用 `-CaptureTimeMilliseconds` 指定时间窗口；后者适合启动后事务时机不固定的板测，前后保留较长空白区不会影响后续 Decode。

不要求把 GDB 或 ARM GCC 加入全局 `PATH`；填写 `ARM_GDB` 的完整路径即可，避免影响现有 Keil 编译链。

J-Link 同一时刻只能由一个客户端占用。Flash、RTT、GDB、Run 和 Fault 公共 Workflow 会共同获取
`PROJECT_LOG_DIR/toolkit_jlink.lock`，并在成功或失败时释放；锁冲突返回 `30 PROBE_ERROR`，不会启动第二个
J-Link owner。执行 J-Link 相关 Workflow 前仍应关闭 Keil Debug、RTT Viewer、RTT Logger 和其他外部 J-Link 客户端；不占用 J-Link 的 sigrok、串口或 Host Test 不需要为此全局等待。

## 测试入口

```powershell
# GDB BAT/PowerShell 契约和失败路径
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\05_Tools\Debug\GDB\test_gdb_automation.ps1

# Firmware Image 打包 Host Test
python -B -m unittest discover -s .\05_Tools\Firmware -p "test_*.py" -v

# Python YMODEM Host Test
python -B -m unittest discover -s .\05_Tools\Ymodem\tests -v

# S04 Persistence Host Test
python -B -m unittest discover -s .\04_Test\Host\S04_Firmware_Image_Storage -p "test_*.py" -v

# Toolkit contract tests
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Contracts\Compatibility\test_legacy_entries.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Contracts\Compatibility\test_firmware_transport_entries.ps1
```

Host Test/Contract Test 只能验证脚本、协议和解析逻辑；真实板测还需要连接目标板、正确的本机工具配置和可回读的串口/RTT/GDB 数据。云端、容器或没有 USB/J-Link 透传的 Agent 环境不得把脚本存在或编译成功视为硬件功能通过。

## 当前未提供的功能

```text
完整 Core Dump、GCC/CMake 构建迁移和 FreeRTOS 全任务栈解析
```

S09/S10 的 Bootloader 内部 Flash 安装、Trial / Confirm / Rollback 已在固件和阶段验证文档中实现；本目录提供构建、烧录、RTT、GDB、Factory Restore 和 YMODEM 等工具入口，但工具链动作成功不等价于 S10 硬件闭环通过。当前 S10 仍处于 `READY_FOR_VERIFICATION`，剩余 Trial 断电、Rollback 和 LED/LCD 人工验收将在后续会话继续。
