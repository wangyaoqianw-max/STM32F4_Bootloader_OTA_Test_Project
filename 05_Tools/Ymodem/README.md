# YMODEM Sender V1.0

这是一个面向 Codex、GPT、PowerShell 和 CMD 的非交互式 YMODEM-1K 固件发送工具，不提供 GUI 或人工串口选择。

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

当前 S05 的 COM9 真实测试属于硬件验证，不能用 Host Test 结果替代。
