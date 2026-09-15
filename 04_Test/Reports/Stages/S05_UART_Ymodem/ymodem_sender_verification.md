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

## Implementation Commits

```text
80da206 feat: add YMODEM sender CRC and config
7dd9bf6 feat: add YMODEM sender protocol state machine
3fcc4d7 feat: add YMODEM serial transport and port detection
5300cba feat: add AI-callable YMODEM sender CLI
```

Existing user modifications in the S05 firmware, board test, design, implementation plan, and original verification report were not staged or changed by this Sender implementation.
