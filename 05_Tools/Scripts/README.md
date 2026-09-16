# Scripts

保存自动化检查、数据转换和批处理脚本。脚本必须说明输入、输出和运行环境。

## Local toolchain setup

首次使用时复制：

```text
05_Tools/Config/toolchain.local.example.bat
```

为：

```text
05_Tools/Config/toolchain.local.bat
```

然后填写当前电脑的实际工具路径。`toolchain.local.bat` 为本机配置，已被 `.gitignore` 排除，禁止提交 Git。

## Unified Router

推荐从 `05_Tools\toolkit.bat` 进入；本目录的 BAT 保留为兼容透传入口：

```bat
05_Tools\toolkit.bat build
05_Tools\toolkit.bat flash run
05_Tools\toolkit.bat flash prepare
05_Tools\toolkit.bat run 10
05_Tools\toolkit.bat rtt 10
05_Tools\toolkit.bat snapshot halt
05_Tools\toolkit.bat snapshot resume
05_Tools\toolkit.bat fault capture
05_Tools\toolkit.bat firmware pack --input app.bin --output app.img --version 1.1.0
05_Tools\toolkit.bat ymodem tera COM10 115200 app.img
05_Tools\toolkit.bat ymodem send app.img --port COM10 --baud 115200 --json
```

统一退出码类别：`0 SUCCESS`、`10 CONFIG_ERROR`、`20 BUILD_ERROR`、`30 PROBE_ERROR`、
`40 DEBUG_ERROR`、`50 TRANSFER_ERROR`、`60 TEST_ERROR`。

## 当前脚本入口总表

| 入口 | 功能 |
|---|---|
| `build_app.bat` | 透传到 `toolkit.bat build`，使用 Keil 编译当前工程 Target |
| `flash_app.bat run` | 透传到 `toolkit.bat flash run`，J-Link SWD 烧录并 Reset → Halt → Go |
| `flash_app.bat prepare` | 透传到 `toolkit.bat flash prepare`，烧录后保持 MCU Halt |
| `rtt_capture.bat [秒数]` | 透传到 `toolkit.bat rtt`，采集 RTT |
| `run_app_cycle.bat [秒数]` | 透传到 `toolkit.bat run`，编译 → 烧录 → RTT 采集 |
| `start_gdb_server.bat` | 前台启动 J-Link GDB Server，供手工调试 |
| `gdb_runtime_snapshot.bat halt` | 快照后 `detach → quit`，保持 MCU 暂停 |
| `gdb_runtime_snapshot.bat resume` | 快照后 `continue& → disconnect → quit`，恢复 MCU 运行 |
| `gdb_fault_capture.bat capture` | 已发生 Fault 时读取 GDB Fault 现场并保持 MCU Halt |
| `gdb_fault_capture.bat trigger` | GDB 已启动后执行复位、运行受控 Fault 测试并采集现场 |
| `s04_persistence_test.bat reset` | GDB 读取复位前后持久性快照；执行 `monitor reset → continue& → disconnect` |
| `s04_persistence_test.bat power-cycle` | 先启动 RTT 监听，等待 `READY` 后由操作者完成断电/上电；固定地址重连并处理退出/卡死 |
| `send_ymodem.bat` | 调用 Tera Term 宏发送 YMODEM 固件 |
| `send_ymodem_python.bat` | 调用 Python Sender，支持 Agent/JSON 场景 |

所有入口从 `toolchain.local.bat` 读取机器相关路径。GDB 使用 `ARM_GDB`
和 `JLINK_GDB_SERVER`；不需要修改系统 PATH，也不会切换现有 Keil 编译链。

## S04 Persistence 临时板测

板测源码位于 `04_Test/Board/S04_Firmware_Image_Storage`，已从正式 Keil target 移除。复测时
须临时接入并单独生成测试 AXF，通过 `S04_PERSISTENCE_AXF` 指定；脚本会离线检查
`app_s04_persistence_test_run` 符号，拒绝普通 Application AXF。测试固件只读验证 Slot B
镜像和 Metadata A/B，并每秒输出固定格式的 `[S04-PERSIST] SNAPSHOT`，不执行擦除、写入或
Commit。Host 解析器比较事件前后的快照，并校验字段枚举范围。

复位测试：

```bat
05_Tools\Scripts\s04_persistence_test.bat reset
```

该入口启动 GDB Server/GDB，读取基线，执行 `monitor reset`、`continue&`，读取复位后快照，最后
`disconnect -> quit`。GDB 脚本使用 AXF 符号，不执行 `load`；RTT Logger 不会与 GDB 并行启动。

电源循环测试：

```bat
05_Tools\Scripts\s04_persistence_test.bat power-cycle
```

该入口先用 ARM GDB 从指定测试 AXF 离线解析 `_SEGGER_RTT` 地址，再启动 RTT Logger，并在捕获第一条
快照后打印 `READY`。看到 `READY` 后关闭电源，等待几秒再打开电源；监听持续 90 秒。Logger
退出或连续 5 秒无新数据时，脚本会结束旧进程并按 1/2/4 秒退避重连，同时记录
`TARGET_UNAVAILABLE`、`RTT_CONTROL_BLOCK_NOT_FOUND` 或 `RTT_DATA_STALLED` 到正式输出日志。
控制器还要求启动标志之后出现新快照。当前串口模块为 `COM9`，但本测试的证据来自 RTT，
不占用 COM9。脚本还创建本地 J-Link 锁文件，防止两个测试进程并发运行；外部 Keil Debug
或 RTT Viewer 仍应在启动前关闭。

输出目录：

```text
06_Output/Logs/S04_reset_persistence.log
06_Output/Logs/S04_power_cycle_persistence.log
06_Output/Logs/S04_power_cycle_persistence_chunk_*.log
```

当前项目已固定的目标连接参数：

```text
Device    = STM32F411CE
Interface = SWD
Speed     = 4000 kHz
RTT       = Channel 0
```

## Application build

统一编译入口：

```bat
05_Tools\toolkit.bat build
```

`Scripts/build_app.bat` 仍可作为兼容入口。

功能：

- 调用 Keil uVision 命令行；
- 编译 `OTA_APP` Target；
- 输出 `06_Output/Logs/OTA_APP_build.log`；
- 通过退出码返回 PASS / WARN / FAIL。

## Application flash

统一烧录入口：

```bat
05_Tools\toolkit.bat flash run
```

功能：

- 使用 `JLink.exe` 连接 `STM32F411CE`；
- 通过 SWD 4 MHz 下载 Keil 生成的 `OTA_APP.hex`；
- `loadfile` 自带写入校验；
- `run` 模式下载后执行 Reset -> Halt -> Go，使 Application 进入运行状态；
- `prepare` 模式下载后保持 MCU Halt，供已经预启动的监听器或后续 GDB 会话使用；
- 输出 `06_Output/Logs/OTA_APP_flash.log`；
- J-Link 被 Keil、RTT Viewer 或其他调试器占用时返回失败。

执行烧录前应先运行 `toolkit.bat build`，确保 HEX 是当前代码对应的产物。需要接收串口早期信息或 YMODEM 时，必须先打开串口监听器/Tera Term 并等待设备发送 `C`，再执行会触发复位的烧录或 GDB 操作；需要捕获复位后早期事件时，使用 `flash prepare` 后由已启动的 GDB 执行复位和运行。

## RTT capture

统一 RTT 采集入口：

```bat
05_Tools\toolkit.bat rtt
```

默认采集 10 秒，也可以覆盖时间：

```bat
05_Tools\Scripts\rtt_capture.bat 20
```

功能：

- 调用 `JLinkRTTLogger.exe`；
- 连接 STM32F411CE / SWD / 4 MHz；
- 采集 RTT Up Channel 0；
- 将 MCU RTT 内容保存到 `06_Output/Logs/OTA_APP_rtt.log`；
- 将 Logger 诊断信息保存到 `06_Output/Logs/OTA_APP_rtt_logger.log`；
- 无 RTT 数据时返回失败，便于 Agent 识别“烧录成功但没有运行时证据”的情况。

`rtt_capture.ps1` 使用 .NET 进程接口启动 Logger，兼容从 `cmd.exe` 调用
Windows PowerShell 时同时存在 `PATH` / `Path` 环境变量的本机环境。

## GDB runtime snapshot

GDB 调试入口：

```bat
05_Tools\toolkit.bat snapshot resume
05_Tools\toolkit.bat snapshot halt
```

依赖 `toolchain.local.bat` 中的 `ARM_GDB` 和 `JLINK_GDB_SERVER`。入口自动启动并回收本次创建的 J-Link GDB Server，使用 Keil 生成的 `OTA_APP.axf` 读取运行态信息，不执行 GDB `load`，不会隐式烧录 Flash。

退出合同为：`resume = continue& -> disconnect -> quit`，`halt = detach -> quit`。日志输出到 `06_Output/Logs/OTA_APP_gdb_server.log` 和 `06_Output/Logs/OTA_APP_gdb_snapshot.log`。

需要手工维护 Server 生命周期时，可使用：

```bat
05_Tools\Scripts\start_gdb_server.bat
```

## GDB Fault capture

受控 Fault 测试入口：

```bat
05_Tools\toolkit.bat flash prepare
05_Tools\toolkit.bat fault trigger
```

`trigger` 模式要求当前 AXF 对应的测试固件已经启用 `DIAG_FAULT_TEST_ENABLE=1`。
它会先启动 J-Link GDB Server 和 GDB 客户端、设置工程 Fault-loop 断点，再由该 GDB 会话执行：

```text
set breakpoint -> monitor reset -> halt -> continue -> Fault Handler capture -> detach -> quit
```

如果 Fault 已经由其他方式产生，使用：

```bat
05_Tools\toolkit.bat fault capture
```

Fault Capture 不执行 `load`、不自动恢复 MCU。GDB 释放 J-Link 后，脚本才启动 RTT Logger 读取
CmBacktrace 已写入的 RTT 缓冲；J-Link 同一时刻只能由一个工具占用，不能把 RTT Logger 与 GDB
Server 并行启动后再烧录或复位。

输出：

```text
06_Output/Logs/OTA_APP_fault_gdb.log
06_Output/Logs/OTA_APP_fault_rtt.log
06_Output/Logs/OTA_APP_gdb_server.log
```

## One-command local cycle

统一开发闭环入口：

```bat
05_Tools\toolkit.bat run
```

或者指定 RTT 采集秒数：

```bat
05_Tools\toolkit.bat run 20
```

执行顺序：

```text
Keil Build
-> J-Link Flash
-> Reset / Run
-> RTT Capture
```

这个脚本只证明本地工具链动作执行成功，不等价于阶段级硬件验证 PASS。具体功能正确性仍必须依据当前 Stage 的测试条件、运行时数据检查和验证报告判断。

## Firmware Image pack tool

打包入口：

```bat
05_Tools\toolkit.bat firmware pack ^
    --input 03_Firmware\Application\OTA_APP\MDK-ARM\Objects\OTA_APP.bin ^
    --output 06_Output\Artifacts\OTA_APP.img ^
    --version 1.1.0
```

工具输出固定的 `[64 Byte Header][Payload]` 镜像；Header 使用 S04 Firmware
Image V1 fixed-offset little-endian 合同。`test_pack_firmware.py` 用于 Python
侧单元测试，生成镜像的兼容性由 S04 C Host Test 复核。

## Agent usage

GPT、DeepSeek、Claude 或其他模型只要运行在可访问本机 Shell 和 USB 调试器的 Agent 环境中，都应优先调用这些稳定入口，而不是自行搜索 Keil、J-Link 或工程路径。

云端、容器或无 USB 透传环境不能因为脚本存在就视为具备真实硬件访问能力。

Firmware/YMODEM 的 Host Test 只能验证镜像格式、协议和参数处理；Tera Term YMODEM 发送还需要真实串口和目标板。需要接收复位后的早期串口信息时，先启动监听工具并确认等待到 `C`，再执行会触发复位的烧录/复位动作。RTT Logger、J-Link Commander、GDB Server 和 RTT Viewer 共享同一个 J-Link Probe，不能并行启动；Fault 流程必须在 GDB 释放 Probe 后再采集 RTT。
