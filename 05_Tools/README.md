# Tools

`05_Tools` 是项目 PC 侧工具和自动化入口的统一目录。当前工具主要覆盖：

```text
Keil 编译 → J-Link 烧录 → RTT 采集
                     ↘ GDB 在线调试 / Runtime Snapshot
PC 固件文件 → YMODEM 发送 → STM32F411 → External Flash
Keil .bin → Firmware Image V1 .img
```

当前文档基于 `2026-09-15` 的仓库状态。机器相关路径只保存在被 Git
忽略的 `Config/toolchain.local.bat`，不能把本机路径写入脚本或提交到 Git。

## 目录总览

| 目录 | 当前内容 | 支持的功能 |
|---|---|---|
| `Config/` | `toolchain.local.example.bat`、本机 `toolchain.local.bat` | 收口 Keil、J-Link、GDB、Python、Tera Term 路径及目标参数 |
| `Scripts/` | `.bat` / `.ps1` 稳定入口 | 编译、烧录、RTT、GDB 快照、YMODEM 和开发闭环 |
| `Firmware/` | `pack_firmware.py`、测试 | 生成 S04 Firmware Image V1，校验 Header/Payload 规则 |
| `Ymodem/` | Python Sender、串口/协议模块、Host Test | 自动识别串口、发送固件、JSON 结果、协议测试 |
| `TeraTerm/` | `send_ymodem.ttl` | Tera Term 5 真实板级 YMODEM 发送宏 |
| `Debug/` | GDB 命令脚本、自动化契约测试 | Runtime Snapshot、halt/resume 生命周期和失败路径检查 |
| `CI/` | README 占位目录 | 当前没有独立 CI 配置或可执行工具 |
| `Packaging/` | README 占位目录 | 当前没有独立打包工具，打包功能在 `Firmware/` |

## 当前工具与支持功能

### 1. Application 编译、烧录和运行时采集

| 入口 | 支持功能 | 主要输出 |
|---|---|---|
| `Scripts/build_app.bat` | 使用 Keil/uVision 编译 `OTA_APP` Target；区分成功、警告、错误退出码 | `06_Output/Logs/OTA_APP_build.log`；生成 Keil `.hex/.bin/.axf` |
| `Scripts/flash_app.bat` | 使用 J-Link Commander 通过 SWD 烧录 `OTA_APP.hex`，随后 Reset → Halt → Go | `06_Output/Logs/OTA_APP_flash.log` |
| `Scripts/rtt_capture.bat [秒数]` | 使用 J-Link RTT Logger 采集 RTT Up Channel 0；默认 10 秒 | `OTA_APP_rtt.log`、`OTA_APP_rtt_logger.log` |
| `Scripts/run_app_cycle.bat [秒数]` | `Build → Flash → RTT Capture` 一键闭环；编译警告会保留为警告结果 | 上述编译、烧录、RTT 日志 |

这些入口默认使用 `STM32F411CE / SWD / 4000 kHz`。闭环成功只表示工具链动作成功，不能替代阶段级功能验收。

### 2. GDB 在线调试与 Runtime Snapshot

| 入口 | 支持功能 |
|---|---|
| `Scripts/start_gdb_server.bat` | 前台启动 J-Link GDB Server，供手工 GDB 会话使用 |
| `Scripts/gdb_runtime_snapshot.bat halt` | 启动临时 Server，读取寄存器、PC、源代码位置、Backtrace、栈内存，然后 `detach → quit`；目标保持暂停 |
| `Scripts/gdb_runtime_snapshot.bat resume` | 读取同样的运行态信息，然后 `continue& → disconnect → quit`；目标继续运行 |
| `Debug/GDB/test_gdb_automation.ps1` | 检查 GDB 脚本合同、禁止 `load`、入口存在性和常见失败路径 |

GDB 使用 Keil 生成的 `OTA_APP.axf` 加载符号，但不会执行 GDB `load`，不会隐式烧录 Flash。Resume 模式不使用 `-batch`，也不在 `continue&` 后执行 `detach`。Runtime Snapshot 日志为：

```text
06_Output/Logs/OTA_APP_gdb_server.log
06_Output/Logs/OTA_APP_gdb_snapshot.log
```

### 3. Firmware Image V1 打包

`Firmware/pack_firmware.py` 将 Application Payload 打包为 S04 固定格式：

```text
.img = 64 Byte Header + Payload
```

支持版本号、Payload 长度、Payload CRC32、Header CRC32 和 Slot 容量检查。示例：

```powershell
python .\05_Tools\Firmware\pack_firmware.py `
  --input .\03_Firmware\Application\OTA_APP\MDK-ARM\Objects\OTA_APP.bin `
  --output .\06_Output\Packages\OTA_APP_v1.1.0.img `
  --version 1.1.0
```

此工具只负责生成 `.img`，不负责串口传输、Flash 烧录或 Bootloader 安装。

### 4. YMODEM 固件发送

#### Tera Term 真实板级入口

```bat
05_Tools\Scripts\send_ymodem.bat COM10 115200 firmware.img
```

使用 `TeraTerm/send_ymodem.ttl` 执行单文件 YMODEM 发送，支持串口号、波特率和固件文件参数。当前 S05 板端完整验收输入是包含 64 Byte Header 的 `.img`，不能直接把原始 `.bin` 当作最终验收文件。

#### Python / Agent 入口

```powershell
05_Tools\Scripts\send_ymodem_python.bat devices --json
05_Tools\Scripts\send_ymodem_python.bat send firmware.img --port COM10 --baud 115200 --json
```

Python Sender 支持：

- 自动列出和选择串口，也支持 `--port` 手动指定；
- `--vid`、`--pid`、`--match` USB/描述过滤；
- YMODEM-1K 文件发送、超时、重试和取消结果；
- `--json` 输出机器可解析结果；
- 无真实串口的 Mock/Scripted Transport Host Test。

Python Sender 主要用于 Host Test、Agent 自动化和协议诊断；S05 默认真实板级验收仍使用 Tera Term 入口。

## 配置与依赖

首次使用：

```text
复制 Config/toolchain.local.example.bat
为   Config/toolchain.local.bat
填写本机实际路径
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
```

不要求把 GDB 或 ARM GCC 加入全局 `PATH`；填写 `ARM_GDB` 的完整路径即可，避免影响现有 Keil 编译链。

J-Link 同一时刻只能由一个工具占用。执行烧录、RTT 或 GDB 前应关闭 Keil Debug、RTT Viewer、RTT Logger 和其他 J-Link 客户端。

## 测试入口

```powershell
# GDB BAT/PowerShell 契约和失败路径
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\05_Tools\Debug\GDB\test_gdb_automation.ps1

# Firmware Image 打包 Host Test
python -B -m unittest discover -s .\05_Tools\Firmware -p "test_*.py" -v

# Python YMODEM Host Test
python -B -m unittest discover -s .\05_Tools\Ymodem\tests -v
```

云端、容器或没有 USB/J-Link 透传的 Agent 环境只能执行静态检查和 Host Test，不能把脚本存在视为真实板测能力。

## 当前未提供的功能

```text
CmBacktrace 移植与 Fault 现场采集
Fault 注入和 RTT Fault 输出工作流
Bootloader 内部 Flash 安装
Trial / Confirm / Rollback
独立 CI 流水线配置
独立 Packaging 工具链
```

这些功能属于后续阶段，不应从当前工具目录说明中推断为已实现。
