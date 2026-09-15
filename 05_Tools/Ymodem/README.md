# YMODEM Sender V1.0

这是一个面向 Codex、GPT、PowerShell 和 CMD 的非交互式 YMODEM-1K 固件发送工具，不提供 GUI 或人工串口选择。

S05 真实板级测试默认使用 Tera Term 5 自动化入口：`05_Tools\Scripts\send_ymodem.bat`。本目录中的 Python Sender 主要用于 Host Test、协议诊断和需要机器可解析 JSON 结果的辅助场景，不替代 S05 的默认 Tera Term 验收入口。

## 安装

```powershell
python -m pip install -r 05_Tools/Ymodem/requirements.txt
```

## 调用

列出串口：

```powershell
python 05_Tools/Ymodem/ymodem_sender.py devices
python 05_Tools/Ymodem/ymodem_sender.py devices --json
```

自动识别并发送：

```powershell
python 05_Tools/Ymodem/ymodem_sender.py send app.bin --baud 115200
```

手动指定串口：

```powershell
python 05_Tools/Ymodem/ymodem_sender.py send app.bin --port COM7 --baud 115200
```

按 USB 身份或描述筛选：

```powershell
python 05_Tools/Ymodem/ymodem_sender.py send app.bin --vid 0x10C4 --pid 0xEA60
python 05_Tools/Ymodem/ymodem_sender.py send app.bin --match "USB Serial"
```

项目稳定入口：

```powershell
05_Tools\Scripts\send_ymodem_python.bat send app.bin --port COM7 --baud 115200
```

文件内容按字节透明传输，不限制 `.bin` 或 `.img` 扩展名。当前 S05 STM32 Receiver 的完整 Flash Sink 验收输入仍是已有 `.img`；原始 `.bin` 是否符合 Receiver/Sink 的固件格式由板端校验。

### S05 固件输入与自动化调用顺序

当前 S05 接收端要求传输 S04 Firmware Image 格式：

```text
.img = 64 Byte Firmware Image Header + Payload
```

不能直接把原始 Application `.bin` 当作 S05 的最终传输文件。应先打包：

```powershell
python .\05_Tools\Firmware\pack_firmware.py `
  --input .\06_Output\Firmware\OTA_APP_s04_test.bin `
  --output .\06_Output\Packages\OTA_APP_s04_v1.1.0.img `
  --version 1.1.0
```

真实板级自动化链路固定顺序（默认使用 Tera Term）：

```text
关闭串口助手，释放 COM
→ devices --json，确认 CH340 端口
→ 准备正确的 .img
→ 先启动 Tera Term 宏，打开 COM 并等待 C
→ 再执行 flash_app.bat 或手动复位目标板
→ 等待 C / 发送 Block 0 / ACK
→ 发送 Block 1...N，并检查 ACK/NAK/重试
→ EOT：NAK → EOT：ACK → 空 Block 0：ACK
→ RTT 捕获进度、Storage 写入和最终镜像校验
```

特别注意：如果先复位 MCU、再启动 Sender，当前板端复位阶段发出的初始 `C` 可能已经结束，Sender 会返回 `exit_code=4`（等待初始 `C` 超时）。因此“打开串口并进入等待”必须发生在烧录/复位之前。

Python Sender 入口示例：

```powershell
.\05_Tools\Scripts\send_ymodem_python.bat send `
  .\06_Output\Packages\OTA_APP_s04_v1.1.0.img `
  --port COM10 --baud 115200 --timeout 15 --json
```

Tera Term 入口示例：

```powershell
.\05_Tools\Scripts\send_ymodem.bat `
  COM10 115200 `
  .\06_Output\Packages\OTA_APP_s04_v1.1.0.img
```

当前 CH340 实测端口为 `COM10`；`COM3` 是未接线的 J-Link CDC 串口，不作为本板级测试端口。

## 输出与退出码

默认模式将 `[PORT]`、`[WAIT]`、`[RX]`、`[TX]`、`[RETRY]`、`[DONE]` 和 `[ERROR]` 日志输出到标准输出/错误流。协议关键交互包括 `C`、`ACK`、`NAK`、`CAN`、Block 序号和 `EOT`。

`--json` 模式将标准输出限制为一个最终 JSON 对象，诊断日志写到标准错误，适合 AI Agent 解析：

```powershell
python 05_Tools/Ymodem/ymodem_sender.py send app.bin --port COM7 --json
```

主要结果字段为 `ok`、`command`、`exit_code`、`port`、`file`、`bytes_sent`、`blocks_sent`、`retries`、`phase` 和 `error`。

```text
0  SUCCESS
1  INVALID_ARGUMENT
2  PORT_NOT_FOUND / PORT_SELECTION_FAILED
3  SERIAL_OPEN_FAILED
4  RECEIVER_TIMEOUT（初始等待 C 超时）
5  TRANSFER_FAILED（重试耗尽或传输错误）
6  RECEIVER_CANCELLED（收到 CAN）
```

自动识别在没有筛选条件时只允许唯一串口；多个候选不会随机选择。`--port` 会覆盖自动识别。

## Host Test

测试不需要真实串口，使用 Mock/Scripted Transport：

```powershell
python -B -m unittest discover -s 05_Tools/Ymodem/tests -v
```

当前 S05 的 Tera Term `COM10` 真实测试属于硬件验证，不能用 Host Test 结果替代。此前 Python Sender 的 `COM9` 记录仅为历史串口诊断。
