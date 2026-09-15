# YMODEM Sender V1.0 设计

## 目标

在 Windows 上提供一个可由 PowerShell、CMD、Codex 或其他自动化工具调用的 Python 命令行 YMODEM Sender，用于通过串口向 STM32 YMODEM Receiver 发送单个固件文件。

V1.0 采用 YMODEM-1K 数据包、CRC-16/XMODEM、Block 0、有限重试和明确退出码，不实现 GUI、YMODEM-G、多文件 Batch、自动烧录或 OTA 元数据管理。

## 仓库落位

```text
05_Tools/Ymodem/
├── ymodem_sender.py
├── ymodem_protocol.py
├── serial_transport.py
├── port_detector.py
├── crc16.py
├── config.py
├── README.md
├── requirements.txt
└── tests/

05_Tools/Scripts/send_ymodem_python.bat
```

`05_Tools/Ymodem` 保存可提交的 Python 工具和 Host Test；BAT 只作为稳定调用入口，不保存机器专属路径。Python 解释器可以由被忽略的 `05_Tools/Config/toolchain.local.bat` 中的 `PYTHON_EXE` 覆盖，未配置时使用 PATH 中的 `python`。

## 模块职责

### `crc16.py`

实现 CRC-16/XMODEM：poly `0x1021`、init `0x0000`、无反射、xorout `0x0000`。只提供纯函数，便于用公开向量测试。

### `serial_transport.py`

封装 `pyserial` 的打开、单字节读取、完整写入和关闭。协议层只依赖 `write(data)` 与 `read_byte(timeout)`，不直接依赖 `serial.Serial`，因此 Host Test 可使用脚本化传输对象。

### `port_detector.py`

使用 `serial.tools.list_ports.comports()` 扫描 Windows COM 设备。筛选字段包括 VID、PID，以及端口名、描述、制造商、产品、接口和 HWID。`--port` 手动指定时跳过自动选择；自动模式只有一个候选时选择，多候选或零候选都返回明确错误并列出扫描结果。

筛选来源按以下顺序合并：命令行 `--vid` / `--pid` / `--match` 覆盖 `config.py` 默认配置；没有任何筛选时不随机选择串口。

### `ymodem_protocol.py`

实现单文件 YMODEM-1K Sender：

1. 等待 Receiver 的 CRC 请求 `'C'`；
2. 发送 128 字节 Block 0，内容为 `filename\\0 filesize moddate mode serial`，字段以空格分隔，文件大小十进制，其余元数据按八进制发送；
3. 等待 `ACK + 'C'` 后按 1024 字节发送数据块，最后一块使用 `0x1A` 填充；
4. 执行 `EOT -> NAK -> EOT -> ACK + 'C'`；
5. 发送全零结束 Block 0，等待最终 ACK；
6. 对 NAK、阶段超时进行有限次重传；收到 CAN 立即终止。

文件按字节透明传输，只要求文件非空且大小不超过 Block 0 可表达的 `uint32_t` 范围，不限制 `.bin` 或 `.img` 扩展名。这样既满足 `.bin` 发送要求，也能直接发送当前 S05 Receiver 验收用的 `.img` 文件；文件内容是否符合 STM32 Sink 的 Firmware Image 合同由 Receiver/Sink 验证。

### `ymodem_sender.py`

负责参数解析、文件和串口选择、日志、资源关闭以及退出码映射。支持：

```text
python ymodem_sender.py --file app.bin --baud 115200
python ymodem_sender.py --port COM7 --file app.bin --baud 115200
```

额外支持超时、最大重试次数、VID、PID 和描述匹配参数，以便自动化环境明确控制选择策略。

## 可靠性和退出码

这个工具的首要调用方包括 Codex、GPT、PowerShell 和 CMD，因此 CLI 是自动化接口，不提供交互式选择、确认提示或 GUI。调用方必须能够仅凭命令行参数、固定日志前缀和进程退出码完成一次发送并判断结果；自动串口模式在多个候选时直接失败，不等待人工选择。

标准调用约束：

```text
输入：--file、可选 --port、--baud 及筛选/重试参数
输出：阶段日志写入标准输出，错误日志写入标准错误
结果：进程退出码表示最终状态
```

```text
0 SUCCESS
1 INVALID_ARGUMENT
2 PORT_NOT_FOUND / PORT_SELECTION_FAILED
3 SERIAL_OPEN_FAILED
4 RECEIVER_TIMEOUT（初始等待 'C' 超时）
5 TRANSFER_FAILED（数据包、EOT 或结束 Block 0 重试耗尽）
6 RECEIVER_CANCELLED（收到 CAN）
```

日志使用固定前缀，包括 `[PORT]`、`[WAIT]`、`[TX]`、`[RETRY]`、`[DONE]` 和 `[ERROR]`。发送失败时尽力发送双 CAN，但不能用取消动作覆盖原始失败原因。

## 测试策略

Host Test 使用标准库 `unittest` 和脚本化传输对象，不依赖真实 COM：

- CRC-16/XMODEM 已知向量；
- Block 0 字段、长度和填充；
- 1K Packet 结构、块号回卷和 CRC；
- 正常单文件发送、最后一块填充、EOT/结束 Block 0；
- NAK/超时重试上限；
- 初始 Receiver 超时和 CAN 取消；
- VID/PID/描述筛选、唯一候选和多候选拒绝。

Host Test 通过后，使用当前 S05 工程作为真实 Receiver：优先发送已有 `.img` 完成 Receiver/Sink 闭环，再以 `.bin` 做透明传输测试。COM9 的物理连接、板端 `rx_bytes`、Block 0、Slot B 和最终镜像校验分别记录，不能把 Python Host Test 结果表述为硬件验证通过。

## 非目标与约束

- 不修改 `03_Firmware` Receiver、Flash Sink 或现有 S05 未提交修改；
- 不自动烧录、复位、采集 RTT 或分析固件版本；
- 不随机选择多个未知串口；
- 不把 Tera Term 的本机路径写入提交文件；
- 不把真实串口测试结果伪装成代码或 Host 验证结果。
