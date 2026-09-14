# S05 UART Ymodem Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现可复用的 Application Ymodem Receiver，并通过 Tera Term 5 自动化 Sender 将 S04 Firmware Image V1 `.img` 可靠接收、按冻结的 Slot 物理布局写入 W25Q64 Slot B，并由现有 Firmware Storage 验证为 VALID。

**Architecture:** 复用现有 `service_uart` 作为 UART DMA/RingBuffer Transport Owner，新增 `service_ymodem`，内部拆分 Packet Parser、Receiver State Machine 和 Sink Contract。S05 Board Test Flash Sink 将紧凑 `[64B Header][Payload]` 文件转换为 S04 已冻结的 Slot 布局：Payload 写 `slot + 0x1000`，Header 最后提交；Ymodem 本身不感知 Slot、Metadata 或 OTA PENDING。

**Tech Stack:** STM32F411CEU6, C99, Keil MDK, existing Platform/Impl/HAL, existing `service_uart`, CRC-16/XMODEM, W25Q64, Firmware Storage Service, SEGGER RTT + EasyLogger, Tera Term 5 TTL Macro, Windows BAT, host GCC.

**Spec:** `00_Project/03_Stages/S05_UART_Ymodem/design.md`

## Metadata

- Stage: `S05_UART_Ymodem`
- Baseline Commit: `8173a3da2c174294350e47d8e889cf22b9066e23`
- Design Commit: `400b8b4f4cb50672faea2c332379466bb637b3a6`
- Status: `IN_PROGRESS`
- Branch: `codex/s05-uart-ymodem`

## Global Constraints

- 不引入黑盒第三方 Ymodem 库；参考协议和成熟实现，但 S05 自己实现 Receiver 子集。
- S05 仅实现 Receiver / Single File；不实现 STM32 Sender、多文件业务、Bluetooth、USB CDC。
- `service_uart` 继续拥有 UART DMA、RingBuffer、ISR callback 和 TX transaction；Ymodem 不直接依赖 HAL UART。
- Packet CRC 固定复用现有 CRC-16/XMODEM；不得复制第二份 CRC 实现。
- Parser、Receiver State Machine、Sink Contract 必须分离。
- 协议常量 SOH/STX/EOT/ACK/NAK/CAN/`'C'` 不进入 Config；Timeout/Retry/Filename Limit 等编译期策略进入 `00_Config/ymodem_config.h`。
- PC 发送文件固定使用 S04 `pack_firmware.py` 生成的 `.img = [64 Byte Header][Payload]`。
- `.img` 紧凑布局不得连续写到 Slot Base；Payload 必须写 `slot_base + FIRMWARE_PAYLOAD_OFFSET`，Header 最后提交。
- S05 Test Sink 固定 Slot B；不得更新 EEPROM Metadata、PENDING、Active Slot 或触发 Reset。
- 现有 `build_app.bat`、`flash_app.bat`、`rtt_capture.bat`、`run_app_cycle.bat` 作为板测基础工具链；S05 只扩展 Tera Term Sender 与必要组合脚本。
- Tera Term 本机路径只允许写入被 Git 忽略的 `05_Tools/Config/toolchain.local.bat`；不得写死到提交脚本。
- 修改 `03_Firmware` 前必须读取 `03_Firmware/AGENTS.md`、嵌入式 C 规范和相关接口文档。
- 每个任务结束执行 `git diff --check`；Host Test 能覆盖的模块先 Host Test，再 Keil Build；编译通过不等于硬件验收通过。

---

## Planned File Structure

```text
03_Firmware/Application/OTA_APP/00_Config/
└─ ymodem_config.h

03_Firmware/Application/OTA_APP/02_Service/
├─ service_firmware/
│  ├─ firmware_storage.h          # modify
│  └─ firmware_storage.c          # modify
└─ service_ymodem/
   ├─ ymodem_def.h
   ├─ ymodem_parser.h
   ├─ ymodem_parser.c
   ├─ ymodem_sink.h
   ├─ ymodem_receiver.h
   └─ ymodem_receiver.c

04_Test/Host/S05_UART_Ymodem/
├─ README.md
├─ s05_ymodem_parser_host_test.c
├─ s05_ymodem_receiver_host_test.c
└─ s05_firmware_storage_write_host_test.c

04_Test/Board/S05_UART_Ymodem/
├─ app_s05_ymodem_test.h
├─ app_s05_ymodem_test.c
├─ s05_ymodem_flash_sink.h
└─ s05_ymodem_flash_sink.c

05_Tools/Config/
└─ toolchain.local.example.bat    # add TERA_TERM_EXE placeholder

05_Tools/TeraTerm/
└─ send_ymodem.ttl

05_Tools/Scripts/
├─ send_ymodem.bat
└─ run_ymodem_cycle.bat           # optional integration wrapper after basic flow passes
```

`toolchain.local.bat` remains ignored and machine-local. Current machine observed Tera Term executable path is `E:\APP\ProgramFile\tera_term\teraterm5\ttermpro.exe`; implementation must place this value only in the ignored local file.

---

### Task 1: Add Tera Term Ymodem sender automation entry

**Files:**
- Modify: `05_Tools/Config/toolchain.local.example.bat`
- Create: `05_Tools/TeraTerm/send_ymodem.ttl`
- Create: `05_Tools/Scripts/send_ymodem.bat`

**Interfaces:**
- Consumes: `toolchain.local.bat` machine-local `TERA_TERM_EXE`, COM port, baud rate, firmware path.
- Produces: stable command `send_ymodem.bat <COMx> <baud> <firmware.img>` with process exit success/failure suitable for Codex/local Agent invocation.

- [x] **Step 1: Extend local tool template with Tera Term placeholder.**

```bat
REM Tera Term 5
set "TERA_TERM_EXE="
```

Do not place the actual machine path in the committed example.

- [x] **Step 2: Add `send_ymodem.bat` argument/config validation.**

Required behavior:

```text
missing toolchain.local.bat -> exit nonzero with clear message
TERA_TERM_EXE missing/not existing -> exit nonzero
missing COM/baud/file -> usage + exit nonzero
firmware file missing -> exit nonzero
otherwise invoke Tera Term macro and propagate success/failure
```

- [x] **Step 3: Add Tera Term 5 TTL macro.**

Macro must:

```text
accept COM / baud / firmware file arguments
open serial connection
invoke ymodemsend for exactly one file
wait until transfer returns
map Tera Term result=1 to success and result=0 to failure
close connection
exit deterministically
```

Do not automate GUI mouse clicks.

- [x] **Step 4: Configure the ignored local file on the real machine.**

```bat
set "TERA_TERM_EXE=E:\APP\ProgramFile\tera_term\teraterm5\ttermpro.exe"
```

This step is local-only and must not be committed.

- [x] **Step 5: Smoke-test invocation before MCU Receiver exists.**

Expected:

```text
script launches Tera Term successfully
serial arguments are accepted
macro executes
Ymodem transfer eventually fails/times out because no valid Receiver handshake exists yet
failure is reported as expected, not treated as PASS
```

- [x] **Step 6: Run `git diff --check` and commit.**

Suggested commit: `tools: add Tera Term ymodem sender entry`

---

### Task 2: Extend Firmware Storage with contract-aware write APIs

**Files:**
- Modify: `03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_storage.h`
- Modify: `03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_storage.c`
- Create: `04_Test/Host/S05_UART_Ymodem/s05_firmware_storage_write_host_test.c`
- Update: `04_Test/Host/S05_UART_Ymodem/README.md`

**Interfaces:**

```c
platform_error_t firmware_storage_write_payload(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    uint32_t payloadOffset,
    const uint8_t *data,
    uint32_t length);

platform_error_t firmware_storage_write_header(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    const uint8_t rawHeader[FIRMWARE_IMAGE_HEADER_SIZE]);
```

- [x] **Step 1: Write failing Host Tests.**

Cover:

```text
Slot A/B address translation
payloadOffset=0 -> slotBase + 0x1000
payload boundary exact fit -> PASS
payload offset > capacity -> reject
length > capacity-offset -> reject without uint32 addition overflow
null/uninitialized/invalid slot -> reject
header write -> exactly 64 bytes at slotBase
```

- [x] **Step 2: Compile tests and confirm failure before implementation.**

Use the same host stub pattern already established by S04 Firmware Storage tests.

- [x] **Step 3: Implement minimal write APIs using existing W25Q64 Raw Driver.**

Rules:

```text
no erase inside write APIs
no Metadata update
no image validation side effect
no Ymodem concepts
```

- [x] **Step 4: Run Host Tests and S04 Firmware Storage regression tests.**

Expected: S05 tests PASS and existing S04 storage tests remain PASS.

- [x] **Step 5: Run `git diff --check` and commit.**

Suggested commit: `feat: add firmware storage write paths`

---

### Task 3: Implement Ymodem definitions, configuration, and incremental Packet Parser

**Files:**
- Create: `03_Firmware/Application/OTA_APP/00_Config/ymodem_config.h`
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_ymodem/ymodem_def.h`
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_ymodem/ymodem_parser.h`
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_ymodem/ymodem_parser.c`
- Create: `04_Test/Host/S05_UART_Ymodem/s05_ymodem_parser_host_test.c`

**Interfaces:**

```c
typedef enum {
    YMODEM_PARSER_EVENT_NONE = 0,
    YMODEM_PARSER_EVENT_PACKET,
    YMODEM_PARSER_EVENT_EOT,
    YMODEM_PARSER_EVENT_CAN,
    YMODEM_PARSER_EVENT_PACKET_ERROR
} ymodem_parser_event_t;

typedef struct {
    uint8_t blockNumber;
    const uint8_t *data;
    uint16_t dataSize;
} ymodem_packet_t;

void ymodem_parser_init(ymodem_parser_t *parser);
void ymodem_parser_reset(ymodem_parser_t *parser);
platform_error_t ymodem_parser_feed_byte(
    ymodem_parser_t *parser,
    uint8_t byte,
    ymodem_parser_event_t *event,
    ymodem_packet_t *packet);
```

- [x] **Step 1: Write parser Host Tests first.**

Vectors must include:

```text
valid SOH 128-byte packet
valid STX 1024-byte packet
fragmented packet over many feed calls
SOH/STX/EOT/CAN values inside payload remain data
block complement mismatch
CRC mismatch
EOT at packet boundary
CAN at packet boundary
noise bytes before packet
CRC error resets whole candidate packet without searching payload for new STX
```

Use existing `crc16_xmodem_calculate()` to construct valid packets in the test.

- [x] **Step 2: Confirm test compile/run fails before parser implementation.**

- [x] **Step 3: Implement protocol constants and static config.**

Initial config constants must be named, not magic literals. Timeout/retry values are provisional until Tera Term board integration validates them.

- [x] **Step 4: Implement two-state incremental parser.**

```text
WAIT_START
COLLECT_PACKET
```

Maximum packet buffer = 1029 bytes. CRC bytes are big-endian on wire. CRC covers Data only.

- [x] **Step 5: Run parser Host Tests.**

Expected: all vectors PASS.

- [x] **Step 6: Run `git diff --check` and commit.**

Suggested commit: `feat: add ymodem packet parser`

---

### Task 4: Implement Ymodem Receiver state machine with stub Sink/UART tests

**Files:**
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_ymodem/ymodem_sink.h`
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_ymodem/ymodem_receiver.h`
- Create: `03_Firmware/Application/OTA_APP/02_Service/service_ymodem/ymodem_receiver.c`
- Create: `04_Test/Host/S05_UART_Ymodem/s05_ymodem_receiver_host_test.c`

**Interfaces:**

```c
typedef struct {
    platform_error_t (*begin)(void *context,
                              const char *filename,
                              uint32_t fileSize);
    platform_error_t (*write)(void *context,
                              const uint8_t *data,
                              uint32_t length);
    platform_error_t (*end)(void *context);
    void (*abort)(void *context);
    void *context;
} ymodem_sink_t;
```

Receiver must expose init/start/process-or-feed/cancel/status/statistics APIs consistent with `design.md`; exact signatures are frozen in the implementation before production code changes and then used unchanged by Board Test.

- [x] **Step 1: Write Receiver Host Test harness with fake Transport TX capture and fake Sink.**

Test normal single-file flow:

```text
start -> emits 'C'
Block0(filename,size) -> sink.begin -> ACK + 'C'
Block1..N -> sink.write valid bytes -> ACK
last packet padding not passed to sink
EOT -> NAK
second EOT -> ACK + 'C'
empty Block0 -> sink.end -> ACK -> FINISHED
```

- [x] **Step 2: Add failure-path tests.**

Cover:

```text
bounded filename parsing
missing NUL
invalid/nondecimal/overflow/zero file size
duplicate previous Block -> ACK without second sink.write
future/wrong Block -> NAK + retry
CRC/parser packet error -> NAK + retry
retry success resets consecutive retry counter
retry exceeded -> sink.abort + ERROR
remote CAN -> abort
local cancel -> CAN sequence + abort + ABORTED
sink.begin failure
sink.write failure before ACK
received bytes < file size when EOT arrives
single-file mode rejects second non-empty Block0
```

- [x] **Step 3: Implement minimum Receiver state machine.**

Frozen states:

```text
UNINITIALIZED
IDLE
WAIT_HEADER
RECEIVE_DATA
WAIT_EOT_CONFIRM
WAIT_END_HEADER
FINISHED
ABORTED
ERROR
```

ACK must only be emitted after successful Sink commit for the current data chunk.

- [x] **Step 4: Implement timeout hooks/counters without binding to HAL tick.**

Use existing project time/OS abstraction available to Application; do not introduce a second timer subsystem. Packet collection timeout must reset an incomplete parser candidate before Receiver requests retransmission.

- [x] **Step 5: Run Receiver + Parser Host Tests.**

Expected: all normal and fault vectors PASS.

- [x] **Step 6: Run `git diff --check` and commit.**

Suggested commit: `feat: add ymodem receiver state machine`

---

### Task 5: Build S05 Flash Sink that maps compact `.img` into the frozen Slot layout

**Files:**
- Create: `04_Test/Board/S05_UART_Ymodem/s05_ymodem_flash_sink.h`
- Create: `04_Test/Board/S05_UART_Ymodem/s05_ymodem_flash_sink.c`

**Interfaces:**
- Consumes: `ymodem_sink_t`, `firmware_storage_erase_slot()`, `firmware_storage_write_payload()`, `firmware_storage_write_header()`, `firmware_image_validate_header()`.
- Produces: S05 Board-only sink fixed to Slot B.

- [x] **Step 1: Define Sink context.**

Context must track:

```text
firmware_storage_t *storage
slot = FIRMWARE_SLOT_B
Ymodem expected file size
received file bytes
64-byte header buffer and header fill count
decoded header
payload written bytes
started/failed state
```

- [x] **Step 2: Implement `begin()`.**

Rules:

```text
filename recorded for diagnostics only
fileSize must be >= 65 bytes and <= 64 + payload capacity
do not mark Metadata VALID/PENDING
prepare context; actual erase can wait until Header is fully parsed so payload size is known
```

- [x] **Step 3: Implement `write()` as a stream transformer.**

Behavior:

```text
first 64 file bytes -> fill header buffer only
when header complete -> validate header contract
require ymodem fileSize == 64 + header.imageSize
erase Slot B using header.imageSize
subsequent bytes -> firmware_storage_write_payload(payloadOffset,...)
never write padding beyond Ymodem fileSize
```

If one incoming chunk crosses byte 63/64 boundary, split it correctly between Header accumulation and Payload write.

- [x] **Step 4: Implement `end()`.**

Only if:

```text
receivedFileBytes == expectedFileSize
payloadWritten == header.imageSize
```

then write the 64-byte Header last using `firmware_storage_write_header()`.

- [x] **Step 5: Implement `abort()`.**

Do not commit Header. Do not modify Metadata. Clear runtime context so a new transfer can start.

- [x] **Step 6: Review failure atomicity and commit.**

Expected invariant: any failure before `end()` leaves no newly committed valid Header for the incomplete transfer.

Suggested commit: `test: add S05 ymodem flash sink`

---

### Task 6: Integrate the Receiver into a dedicated S05 Board Test and Keil target path

**Files:**
- Create: `04_Test/Board/S05_UART_Ymodem/app_s05_ymodem_test.h`
- Create: `04_Test/Board/S05_UART_Ymodem/app_s05_ymodem_test.c`
- Modify only the minimal Application/Keil test integration files required by the repository's established stage-test pattern.

**Interfaces:**
- Consumes: existing communication UART construction, `service_uart`, `service_ymodem`, S05 Flash Sink, RTT/EasyLogger, Firmware Storage.
- Produces: board endpoint that waits for Tera Term Ymodem transfer and logs deterministic verification evidence.

- [x] **Step 1: Initialize existing board services using S04 patterns.**

Reuse existing communication UART, Storage SPI/W25Q64, EEPROM/Firmware Storage and owner thread conventions. Do not duplicate HAL handles or invent a second UART architecture.

- [x] **Step 2: Start Ymodem Receiver and print one deterministic readiness marker.**

Example semantic marker:

```text
[S05] YMODEM_READY
```

The PC-side operator/automation can use this as evidence that MCU reached receiver-ready state; do not require RTT and COM port to share the same channel.

- [x] **Step 3: Add meaningful protocol logs without per-byte spam.**

Required evidence:

```text
filename / fileSize
accepted packet/progress summary
retry/CRC/sequence/duplicate counters
cancel/error reason
EOT/session completion
Flash payload/header commit result
firmware_storage_validate_image() result
```

- [x] **Step 4: On FINISHED, validate Slot B using existing `firmware_storage_validate_image()`.**

A Ymodem FINISHED event alone is not stage PASS. Board path must distinguish transport success from Firmware Image validation success.

- [x] **Step 5: Build using the existing unified tool.**

```bat
05_Tools\Scripts\build_app.bat
```

Expected: normal build and clean rebuild succeed with no unexpected target drift.

- [x] **Step 6: Flash/capture smoke using existing toolchain.**

```bat
05_Tools\Scripts\run_app_cycle.bat 10
```

Expected: Build/Flash/RTT tooling works and logs show `YMODEM_READY`; this is toolchain evidence, not full Ymodem PASS.

- [x] **Step 7: Run `git diff --check` and commit.**

Suggested commit: `test: integrate S05 ymodem board endpoint`

---

### Task 7: Perform first real Tera Term ↔ STM32 end-to-end transfer

**Files:**
- Use: `05_Tools/Firmware/pack_firmware.py`
- Use: `05_Tools/Scripts/build_app.bat`
- Use: `05_Tools/Scripts/flash_app.bat`
- Use: `05_Tools/Scripts/rtt_capture.bat`
- Use: `05_Tools/Scripts/send_ymodem.bat`
- Evidence later goes to `04_Test/Reports/Stages/S05_UART_Ymodem/verification.md` during Verification Role, not during implementation.

- [x] **Step 1: Generate a known S04 Firmware Image V1 `.img`.**

Use the existing packer; record source `.bin`, version and resulting file size.

- [x] **Step 2: Build and flash S05 Board Test with existing tools.**

- [x] **Step 3: Start RTT capture and confirm `YMODEM_READY`.**

- [ ] **Step 4: Invoke the new Tera Term sender tool.**

```bat
05_Tools\Scripts\send_ymodem.bat COMx 115200 path\to\firmware.img
```

Expected: Tera Term completes with sender success.

- [ ] **Step 5: Inspect RTT evidence.**

Required:

```text
Block0 filename and file size match sender
received file bytes == .img file size
payload bytes == Header.imageSize
Ymodem state FINISHED
no UART data-loss flag
Flash Header committed last
firmware_storage_validate_image(Slot B) == VALID
```

- [x] **Step 6: Cross-check file/image CRC semantics.**

Do not compare PC CRC32 of compact `.img` directly to the non-contiguous Slot address range as if they were identical layouts. Use S04 Header/Payload validation contract: Header CRC valid and Payload CRC32 from Header matches Flash Payload.

- [ ] **Step 7: If interoperability differs at EOT/CAN details, adjust only inside frozen Receiver compatibility boundary and rerun Host regression + board transfer.**

- [ ] **Step 8: Commit any integration-only fixes after regression passes.**

Suggested commit: `fix: stabilize Tera Term ymodem interoperability`

---

### Task 8: Verify failure recovery and add an optional one-command S05 cycle

**Files:**
- Modify as needed within planned S05 modules/tests only.
- Create optionally: `05_Tools/Scripts/run_ymodem_cycle.bat`

**Interfaces:**
- Produces: repeatable local sequence built on existing Build/Flash/RTT tools plus `send_ymodem.bat`.

- [ ] **Step 1: Test local/remote cancel path.**

Expected:

```text
Receiver leaves active transfer
Sink aborts
Header is not committed
next new transfer can start without reset-induced corruption
```

- [ ] **Step 2: Test transfer interruption/timeout.**

Stop sender mid-transfer or otherwise create a repeatable interruption. Expected: bounded retry/timeout, no endless wait, no valid Header commit.

- [ ] **Step 3: Test second transfer after a failed transfer.**

Expected: subsequent normal Tera Term transfer succeeds and validates Slot B.

- [x] **Step 4: Confirm duplicate/retry counters through Host Test; do not require unsafe manual serial corruption if Tera Term cannot inject it deterministically.**

Hardware evidence is required for interruption/cancel/recovery; exact CRC-corruption injection may remain Host Test evidence if no deterministic sender facility exists.

- [ ] **Step 5: Add `run_ymodem_cycle.bat` only after individual tools are stable.**

Recommended sequence:

```text
build_app.bat
→ flash_app.bat
→ start RTT capture
→ wait/observe YMODEM_READY by documented operator/automation step
→ send_ymodem.bat
→ collect logs
```

Do not claim the wrapper itself interprets protocol correctness unless it actually parses deterministic result markers.

- [x] **Step 6: Run full Host regression, Keil clean rebuild and `git diff --check`.**

- [ ] **Step 7: Remove S05 Board Test from normal production startup/target if the repository stage-test convention requires test-only integration after verification.**

- [ ] **Step 8: Commit final implementation state.**

Suggested commit: `feat: complete S05 UART ymodem receiver`

---

## Implementation Completion Gate

Implementation may move to `READY_FOR_VERIFICATION` only when all are true:

```text
Tera Term automation tool can be invoked through repository scripts
Parser Host Tests PASS
Receiver Host Tests PASS
Firmware Storage write Host Tests PASS
S04 relevant regression tests PASS
Keil normal + clean build PASS
real Tera Term -> STM32 -> Slot B transfer completes
RTT logs show expected file/session evidence
firmware_storage_validate_image(Slot B) == VALID
interrupted/canceled transfer does not commit a valid new Header
new transfer after failure succeeds
```

The following are explicitly not S05 completion requirements:

```text
EEPROM PENDING
inactive slot selection
Application OTA Service
FreeRTOS background OTA concurrency
Bootloader install
Trial/Confirm/Rollback
```

## Self-Review

- Spec coverage: Parser, Receiver, Block 0, SOH/STX, CRC-16, ACK/NAK, duplicate/sequence, timeout/retry/cancel, EOT, Sink, Firmware Storage mapping, Tera Term tool, existing RTT toolchain and board verification are each mapped to tasks.
- Layout consistency: compact `.img` is explicitly transformed to Header Sector + Payload Offset; no task writes compact `.img` linearly into the Slot.
- Authority consistency: S05 writes/validates image data but does not commit OTA Metadata or PENDING state.
- Tooling consistency: machine paths remain local-only; committed scripts use `toolchain.local.bat`.
- No new third-party Ymodem runtime dependency is introduced.
