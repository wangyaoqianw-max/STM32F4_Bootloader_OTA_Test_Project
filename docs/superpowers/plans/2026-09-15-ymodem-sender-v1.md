# YMODEM Sender V1.0 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Windows Python CLI YMODEM-1K Sender with stable `devices` and `send` subcommands that can be called non-interactively by Codex/GPT, PowerShell, or CMD and can send `.bin` or current-project `.img` files to the S05 STM32 Receiver.

**Architecture:** Keep the CLI, protocol state machine, serial transport, port detector, and CRC function in separate modules under `05_Tools/Ymodem`. The protocol depends only on a small transport interface, allowing deterministic Host Tests with a Mock/Scripted Transport while the production path uses `pyserial`. The CLI emits human-readable logs by default and one final JSON result object under `--json`, leaving room for a later Python API or MCP adapter without adding either in V1.

**Tech Stack:** Python 3, standard library `unittest`, `pyserial`, Windows batch entry point.

**Spec:** `docs/superpowers/specs/2026-09-15-ymodem-sender-v1-design.md`

## Global Constraints

- The tool is a non-interactive CLI for Codex/GPT, PowerShell, and CMD; it must never wait for a human port choice or confirmation.
- The stable CLI surface is `ymodem_sender devices [--json]` and `ymodem_sender send <file> [--port COMx] [--baud 115200]`.
- With `--json`, stdout contains exactly one final JSON result object; protocol trace and diagnostics go to stderr.
- Data packets use YMODEM-1K (`STX`, 1024-byte data, CRC-16/XMODEM); Block 0 and the empty end Block 0 use 128-byte data packets (`SOH`).
- File data is transferred transparently; both `.bin` and `.img` are accepted, and file content validation remains the Receiver/Sink responsibility.
- Auto-detection selects only one candidate; zero or multiple candidates return exit code `2` and list the observed ports.
- Initial absence of Receiver `'C'` returns exit code `4`; transfer retry exhaustion returns `5`; received `CAN` returns `6`.
- Host tests must not require a real COM port. Real COM9 testing is hardware evidence only and must not be reported as Host Test success.
- Do not modify existing `03_Firmware` files or unrelated uncommitted user changes.
- Use `python -B` in verification commands so tests do not create new `__pycache__` files.

---

### Task 1: CRC and Sender Configuration

**Files:**
- Create: `05_Tools/Ymodem/crc16.py`
- Create: `05_Tools/Ymodem/config.py`
- Test: `05_Tools/Ymodem/tests/test_crc16.py`
- Test: `05_Tools/Ymodem/tests/test_config.py`

**Interfaces:**
- Produces `crc16_xmodem(data: bytes) -> int`.
- Produces configuration constants for baud rate, timeout, retry count, packet sizes, default port filters, and exit codes.

- [ ] **Step 1: Write the failing CRC vector test**

```python
def test_crc16_xmodem_known_vector():
    self.assertEqual(crc16_xmodem(b"123456789"), 0x31C3)
```

- [ ] **Step 2: Run the CRC test to verify it fails**

Run:

```powershell
python -B -m unittest 05_Tools/Ymodem/tests/test_crc16.py -v
```

Expected: FAIL because `crc16.py` and `crc16_xmodem` do not exist yet.

- [ ] **Step 3: Write the minimal CRC implementation**

Implement the non-reflected XMODEM loop with polynomial `0x1021`, initial value `0`, and return a masked 16-bit integer.

- [ ] **Step 4: Run the CRC test to verify it passes**

Run the same command and expect PASS.

- [ ] **Step 5: Write configuration tests**

Assert the defaults are `115200` baud, `1.0` second control timeout, `5` maximum retries, 128-byte Block 0, 1024-byte data packets, and exit code constants `0..6` with the specified meanings.

- [ ] **Step 6: Run the configuration tests to verify they fail**

Run:

```powershell
python -B -m unittest 05_Tools/Ymodem/tests/test_config.py -v
```

Expected: FAIL because `config.py` does not exist yet.

- [ ] **Step 7: Add minimal configuration constants and pass the tests**

Define only the values used by the CLI and protocol. Keep default VID/PID and match patterns empty so an unconfigured automatic scan never guesses among multiple ports.

- [ ] **Step 8: Run both tests and commit**

```powershell
python -B -m unittest 05_Tools/Ymodem/tests/test_crc16.py 05_Tools/Ymodem/tests/test_config.py -v
git diff --check -- 05_Tools/Ymodem
git add -- 05_Tools/Ymodem/crc16.py 05_Tools/Ymodem/config.py 05_Tools/Ymodem/tests/test_crc16.py 05_Tools/Ymodem/tests/test_config.py
git commit -m "feat: add YMODEM sender CRC and config"
```

Expected: all tests PASS and only the listed new files are staged.

### Task 2: YMODEM Packet Builder and Protocol State Machine

**Files:**
- Create: `05_Tools/Ymodem/ymodem_protocol.py`
- Test: `05_Tools/Ymodem/tests/test_ymodem_protocol.py`

**Interfaces:**
- Produces `build_block0(filename, file_size, modification_time, file_mode, serial_number) -> bytes`.
- Produces `build_packet(block_number, data, packet_size) -> bytes`.
- Produces `YModemSender(transport, timeout, max_retries, logger).send(file_path) -> TransferStats`.
- Consumes the transport interface `write(data: bytes) -> None` and `read_byte(timeout: float) -> int`.

- [ ] **Step 1: Write failing packet-structure tests**

Test that Block 0 is exactly 133 bytes on the wire, starts with `SOH`, includes the filename, decimal size, octal metadata separated by spaces, and is NUL padded. Test that a data packet is exactly 1029 bytes, starts with `STX`, contains block number and complement, pads the payload with `0x1A`, and places CRC high byte before low byte.

- [ ] **Step 2: Run packet tests to verify they fail**

```powershell
python -B -m unittest 05_Tools/Ymodem/tests/test_ymodem_protocol.py -v
```

Expected: FAIL because `ymodem_protocol.py` does not exist yet.

- [ ] **Step 3: Implement the minimal packet builders**

Validate non-empty ASCII filename, 128-byte Block 0 capacity, valid packet sizes, and data length. Use fixed regular-file mode `0o100644` by default from the caller and encode modification time and mode as octal text.

- [ ] **Step 4: Run packet tests to verify they pass**

Run the same command and expect PASS.

- [ ] **Step 5: Write a scripted-transport normal-flow test**

Use a temporary non-empty file and a real `ScriptedTransport` test object whose response sequence is:

```text
C, ACK, C, ACK, NAK, ACK, ACK, C, ACK
```

Assert that the sender writes Block 0, one padded 1K data packet, `EOT`, a second `EOT`, and an all-zero end Block 0 in that order; assert the returned byte count equals the source file size.

- [ ] **Step 6: Run the normal-flow test to verify it fails**

Run the focused test and expect FAIL because `YModemSender` does not exist yet.

- [ ] **Step 7: Implement the minimal send state machine**

Implement initial `'C'` wait, Block 0 `ACK + C`, streaming 1K data packets, standard two-EOT handshake, empty Block 0, control-byte handling, and file-size-based final padding. Keep all retries bounded by `max_retries`; send double `CAN` best-effort only while preserving the original failure.

- [ ] **Step 8: Run the normal-flow test to verify it passes**

Run the focused test and expect PASS.

- [ ] **Step 9: Add retry, timeout, and cancel tests**

Cover one NAK followed by ACK, repeated NAK until transfer failure, initial no-`C` timeout, and CAN during data transfer. Assert the correct protocol exception or returned status and that retry count is bounded.

- [ ] **Step 10: Implement only the behavior needed by those tests and run the full protocol test file**

```powershell
python -B -m unittest 05_Tools/Ymodem/tests/test_ymodem_protocol.py -v
```

Expected: all protocol tests PASS with no warnings.

- [ ] **Step 11: Commit the protocol unit**

```powershell
git diff --check -- 05_Tools/Ymodem/ymodem_protocol.py 05_Tools/Ymodem/tests/test_ymodem_protocol.py
git add -- 05_Tools/Ymodem/ymodem_protocol.py 05_Tools/Ymodem/tests/test_ymodem_protocol.py
git commit -m "feat: add YMODEM sender protocol state machine"
```

### Task 3: Serial Transport and Automatic COM Detection

**Files:**
- Create: `05_Tools/Ymodem/serial_transport.py`
- Create: `05_Tools/Ymodem/port_detector.py`
- Test: `05_Tools/Ymodem/tests/test_serial_transport.py`
- Test: `05_Tools/Ymodem/tests/test_port_detector.py`

**Interfaces:**
- Produces `SerialTransport(port, baudrate, write_timeout)` with `open()`, `write(data)`, `read_byte(timeout)`, and `close()`.
- Produces `PortCriteria(vid, pid, matches)` and `PortDetector(provider).scan(criteria)`, `select(criteria)`, and `describe(info)`.
- Consumes `serial.tools.list_ports.comports()` only in production; tests inject a provider and serial factory.

- [ ] **Step 1: Write failing port-selection tests**

Use simple fake port records to test: VID/PID match, case-insensitive description match, one candidate selection, zero candidates, and multiple candidates with a rendered list of port names and descriptions.

- [ ] **Step 2: Run the port tests to verify they fail**

```powershell
python -B -m unittest 05_Tools/Ymodem/tests/test_port_detector.py -v
```

Expected: FAIL because `port_detector.py` does not exist yet.

- [ ] **Step 3: Implement the detector with injected provider**

Match VID and PID exactly when configured; match any configured text pattern against port, name, description, manufacturer, product, interface, or HWID. Return the single candidate, otherwise raise a selection error containing all observed ports.

- [ ] **Step 4: Run the port tests to verify they pass**

Run the same command and expect PASS.

- [ ] **Step 5: Write failing serial transport tests**

Inject a fake serial factory and verify `open()` passes the requested port and baud, `write()` forwards all bytes, `read_byte()` returns a byte and raises the transport timeout when no byte arrives, and `close()` closes the handle safely.

- [ ] **Step 6: Implement the minimal pyserial adapter**

Import `pyserial` lazily so unit tests and `--help` do not fail before serial access is needed. Map open errors to a transport exception for the CLI.

- [ ] **Step 7: Run transport and detector tests**

```powershell
python -B -m unittest 05_Tools/Ymodem/tests/test_serial_transport.py 05_Tools/Ymodem/tests/test_port_detector.py -v
```

Expected: all tests PASS.

- [ ] **Step 8: Commit the transport unit**

```powershell
git diff --check -- 05_Tools/Ymodem/serial_transport.py 05_Tools/Ymodem/port_detector.py 05_Tools/Ymodem/tests/test_serial_transport.py 05_Tools/Ymodem/tests/test_port_detector.py
git add -- 05_Tools/Ymodem/serial_transport.py 05_Tools/Ymodem/port_detector.py 05_Tools/Ymodem/tests/test_serial_transport.py 05_Tools/Ymodem/tests/test_port_detector.py
git commit -m "feat: add YMODEM serial transport and port detection"
```

### Task 4: AI-Callable CLI, JSON Result, Batch Entry, and Documentation

**Files:**
- Create: `05_Tools/Ymodem/ymodem_sender.py`
- Create: `05_Tools/Ymodem/README.md`
- Create: `05_Tools/Ymodem/requirements.txt`
- Create: `05_Tools/Scripts/send_ymodem_python.bat`
- Modify: `05_Tools/Config/toolchain.local.example.bat`
- Test: `05_Tools/Ymodem/tests/test_cli.py`

**Interfaces:**
- Produces `main(argv: list[str] | None = None) -> int` with `devices` and `send` subcommands.
- `send` accepts a positional file path plus optional `--port`, `--baud`, `--timeout`, `--retries`, `--vid`, `--pid`, repeatable `--match`, and `--json`; `devices` accepts the filters and `--json`.
- Emits stable `[PORT]`, `[WAIT]`, `[RX]`, `[TX]`, `[RETRY]`, `[DONE]`, and `[ERROR]` lines; default progress goes to stdout, while `--json` keeps stdout machine-readable and sends trace to stderr.
- Exit codes are exactly those defined in `config.py` and the design spec.

- [ ] **Step 1: Write failing CLI parsing and exit-mapping tests**

Test `devices` and `send` parsing, missing file, non-positive baud, invalid COM name, help returning `0`, one-object `--json` result output, and mapping of port selection, serial-open, receiver-timeout, transfer-failed, and receiver-cancelled exceptions to codes `2..6`.

- [ ] **Step 2: Run CLI tests to verify they fail**

```powershell
python -B -m unittest 05_Tools/Ymodem/tests/test_cli.py -v
```

Expected: FAIL because `ymodem_sender.py` does not exist yet.

- [ ] **Step 3: Implement the CLI minimally**

Use a custom `ArgumentParser` error path returning `1`, validate the file before opening the port, log every auto-detected port, prefer explicit `--port`, build `YModemSender`, and always close the transport. In `--json` mode collect the final result fields and write one JSON object to stdout while routing protocol trace to stderr. Never call `input()` or pause for a choice.

- [ ] **Step 4: Run CLI tests to verify they pass**

Run the same command and expect PASS.

- [ ] **Step 5: Add the Python dependency and BAT wrapper**

Put exactly `pyserial` in `requirements.txt`. The BAT script may load the ignored local config, defaults to `python`, forwards all arguments unchanged, and returns the Python process exit code. Add an empty `PYTHON_EXE` setting to the example config without changing any real local path.

- [ ] **Step 6: Write README usage and automation contract**

Document direct Python and BAT examples, auto-detection rules, optional VID/PID/description filters, exit codes, `.bin`/`.img` transparency, installation of `pyserial`, and the distinction between Host Test and COM9 hardware verification.

- [ ] **Step 7: Run CLI tests and a no-port smoke check**

```powershell
python -B -m unittest 05_Tools/Ymodem/tests/test_cli.py -v
python -B 05_Tools/Ymodem/ymodem_sender.py --help
python -B 05_Tools/Ymodem/ymodem_sender.py devices --json
```

Expected: CLI tests PASS; help exits `0`, contains the two subcommands, and `devices --json` emits one parseable JSON object.

- [ ] **Step 8: Commit the CLI unit**

```powershell
git diff --check -- 05_Tools/Ymodem 05_Tools/Scripts/send_ymodem_python.bat 05_Tools/Config/toolchain.local.example.bat
git add -- 05_Tools/Ymodem 05_Tools/Scripts/send_ymodem_python.bat 05_Tools/Config/toolchain.local.example.bat
git commit -m "feat: add AI-callable YMODEM sender CLI"
```

### Task 5: Full Host Verification and Current S05 Integration Check

**Files:**
- Modify: `docs/superpowers/plans/2026-09-15-ymodem-sender-v1.md` to record completed verification only if the plan itself is tracked as implementation evidence.
- Potentially create: `04_Test/Reports/Stages/S05_UART_Ymodem/ymodem_sender_host_verification.md` only for final evidence; do not overwrite the existing user-modified verification report.

- [ ] **Step 1: Run the complete Sender Host Test suite**

```powershell
python -B -m unittest discover -s 05_Tools/Ymodem/tests -v
```

Expected: all Sender tests PASS with no test-generated files.

- [ ] **Step 2: Run Python syntax and diff checks**

```powershell
python -B -m py_compile 05_Tools/Ymodem/*.py
git diff --check -- 05_Tools/Ymodem 05_Tools/Scripts/send_ymodem_python.bat 05_Tools/Config/toolchain.local.example.bat
```

Expected: syntax check and whitespace check PASS. Remove any generated `__pycache__` only if it was created by a command in this task and is untracked; do not remove the existing `05_Tools/Firmware/__pycache__` user file.

- [ ] **Step 3: Check local dependency and serial visibility without modifying hardware**

```powershell
python -B -c "import serial; from serial.tools import list_ports; print(serial.VERSION); print([p.device for p in list_ports.comports()])"
```

Expected: `pyserial` imports and the current COM list is printed. If no dependency or port is available, record that fact as an environment limitation rather than changing code.

- [ ] **Step 4: Use the existing S05 environment for a real `.img` attempt only when the COM9 physical path is available**

Run:

```powershell
python -B 05_Tools/Ymodem/ymodem_sender.py send <existing-s05-img> --port COM9 --baud 115200
```

Expected success evidence requires `[DONE] Transfer successful`, S05 RTT evidence for Block 0/data/EOT/end Block 0, and final Slot B image validation. A timeout, open failure, or `rx_bytes=0` is recorded as hardware `PENDING`/`FAIL` with the exact exit code; it is not converted into Host Test failure.

- [ ] **Step 5: Run the BAT help/argument-forwarding check**

```powershell
05_Tools\Scripts\send_ymodem_python.bat devices --json
```

Expected: the wrapper forwards the subcommand and returns the Python exit code; with a working pyserial installation, output is one parseable JSON object.

- [ ] **Step 6: Review scope and existing user changes**

```powershell
git status --short
git diff --stat
```

Expected: only the Sender files and explicitly updated Sender docs are attributed to this task; all pre-existing S05 modifications remain intact.

- [ ] **Step 7: Commit final verification evidence if created**

Use a single-purpose commit containing only the new Sender evidence file or plan completion update. Do not stage the existing dirty S05 design, firmware, test, or verification files unless the user separately requests that scope.
