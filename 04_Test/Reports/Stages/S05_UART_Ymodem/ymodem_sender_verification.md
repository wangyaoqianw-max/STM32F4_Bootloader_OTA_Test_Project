# S05 Python YMODEM Sender 验证记录

## Metadata

- Date: `2026-09-15`
- Tool: `05_Tools/Ymodem/ymodem_sender.py`
- CLI: `devices` / `send`
- Hardware target: current S05 STM32 Receiver
- Hardware port: `COM9` / USB-SERIAL CH340 / VID:PID `1A86:7523`

## Code Verification

| Item | Result |
| --- | --- |
| CRC-16/XMODEM known vector | PASS |
| Block 0 and YMODEM-1K packet structure | PASS |
| Normal single-file flow | PASS |
| NAK retry and retry exhaustion | PASS |
| Initial receiver timeout | PASS |
| Receiver CAN cancellation | PASS |
| Mock serial transport | PASS |
| VID/PID/description/HWID port filtering | PASS |
| Multiple-port rejection | PASS |
| CLI exit-code mapping `0..6` | PASS |
| `--json` single-result output | PASS |
| Python AST syntax check | PASS |
| `git diff --check` for Sender files | PASS |

Host command:

```powershell
python -B -m unittest discover -s 05_Tools/Ymodem/tests -v
```

Result: `Ran 24 tests ... OK`。

Dependency:

```text
pyserial 3.5
```

## CLI / Host Environment Evidence

```powershell
python -B 05_Tools/Ymodem/ymodem_sender.py devices --json
05_Tools\Scripts\send_ymodem_python.bat devices --json
```

Both entry points returned `exit_code=0` in the JSON result and enumerated:

```text
COM1  ELTIMA Virtual Serial Port
COM2  ELTIMA Virtual Serial Port
COM3  JLink CDC UART Port
COM9  USB-SERIAL CH340
```

Automatic `send` without `--port` correctly rejected the four candidates with JSON `exit_code=2` and listed all candidates. No random port selection occurred.

## Hardware Verification

Attempt:

```powershell
python -B 05_Tools/Ymodem/ymodem_sender.py send `
  06_Output/Packages/OTA_APP_s04_v1.1.0.img `
  --port COM9 --baud 115200 --json
```

Observed result:

```text
[PORT] Selected COM9
[WAIT] Receiver C
exit_code=4
phase=wait_receiver
error=receiver did not send initial C
```

Hardware verification: `PENDING`。

This is consistent with the current S05 handoff blocker: the board has previously recorded `rx_events=0` and `rx_bytes=0` for the COM9 path. The Sender code and Host Test are PASS, but no Block 0, Payload, EOT, ending Block 0, Slot B, or final `firmware_storage_validate_image()` hardware evidence was obtained.

## Standalone Serial Transport Verification

本轮绕过 YMODEM，仅调用 `serial_transport.py`：

### COM1 / COM2 Virtual Loopback

```text
COM1 -> send: SERIAL_TEST_55AA
COM2 <- recv: SERIAL_TEST_55AA
match=True
```

结果：`PASS`。Python 串口层的打开、写入、读取、超时处理和关闭路径可独立工作。

### COM9 Raw Probe

```text
open: COM9 @ 115200       PASS
send: 55 AA 33            PASS
receive window: 2 seconds
received_count: 0         
close: COM9               PASS
```

结果：串口模块本身能够打开和发送，但 COM9 对端在探测窗口内没有返回任何字节。该结果进一步支持：当前阻塞不在 Python 串口层，而在 CH340 到 STM32 USART1 的 TX/RX/GND/电平连接或板端 Receiver 启动路径；硬件验证继续保持 `PENDING`。

## Implementation Commits

```text
80da206 feat: add YMODEM sender CRC and config
7dd9bf6 feat: add YMODEM sender protocol state machine
3fcc4d7 feat: add YMODEM serial transport and port detection
5300cba feat: add AI-callable YMODEM sender CLI
```

Existing user modifications in the S05 firmware, board test, design, implementation plan, and original verification report were not staged or changed by this Sender implementation.
