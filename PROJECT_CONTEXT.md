# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S05_UART_Ymodem`
- Active Stage Status: `READY_FOR_IMPLEMENTATION`
- Branch: `codex/s05-uart-ymodem`
- S05 Baseline Commit: `8173a3da2c174294350e47d8e889cf22b9066e23`
- S05 Design Commit: `400b8b4f4cb50672faea2c332379466bb637b3a6`
- S05 Implementation Plan Commit: `a5417e47dc86546176ec87dff5f6c58ddfb14260`
- S05 Design Approval Commit: `b62bdad9d1279158d4925a417ab5a0e1b4668db3`
- Last Closed Stage: `S04_Firmware_Image_Storage`
- S04 Final Result: `CLOSED / PASS`
- Current Role: `Implementation Role / Ready to execute Task 1`
- Updated At: `2026-09-14`

## Current Goal

按照 Project Owner 已批准的 S05 Design + Implementation Plan 构建：

```text
Tera Term Ymodem Sender
        ↓
service_uart
        ↓
ymodem_parser
        ↓
ymodem_receiver
        ↓
ymodem_sink
        ↓
S05 Flash Sink
        ↓
firmware_storage
        ↓
W25Q64 Slot B
```

S05 建立可靠文件传输能力，不实现正式 OTA Service。

设计门禁已经通过：

```text
DRAFT
→ DESIGN_APPROVED
→ READY_FOR_IMPLEMENTATION
```

## Required Reading

进入 S05 Implementation Role 后按顺序读取：

1. `AGENTS.md`
2. `README.md`
3. `PROJECT_CONTEXT.md`
4. `00_Project/WORKFLOW.md`
5. `00_Project/01_Requirements/项目需求V1.md`
6. `00_Project/02_Roadmap/development_roadmap.md`
7. `00_Project/03_Stages/S05_UART_Ymodem/design.md`
8. `00_Project/03_Stages/S05_UART_Ymodem/implementation_plan.md`
9. `00_Project/03_Stages/S05_UART_Ymodem/handoff.md`
10. `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`
11. `03_Firmware/AGENTS.md`
12. current `service_uart`
13. current `service_common/crc`
14. current `service_firmware`

## Stable S04 Inputs

### External Flash

```text
Slot A: 0x000000 ~ 0x07FFFF
Slot B: 0x080000 ~ 0x0FFFFF

Per Slot:
+0x0000 ~ +0x0FFF : Header Sector
+0x1000 ~          : Firmware Payload
Payload capacity  : 508 KiB
```

### Firmware Image V1

- Header fixed 64 Byte；
- Header / Payload CRC32；
- Image Header is Version / Size / CRC authority；
- `firmware_storage_validate_image()` is read-only validation；
- CRC-16/XMODEM already exists in `service_common/crc`；
- `pack_firmware.py` output is compact `[64 Byte Header][Payload]`。

Important S05 mapping rule：

```text
compact .img
Header[64] + Payload

must become

Slot Header @ +0x0000
Slot Payload @ +0x1000
```

Do not write compact `.img` linearly from Slot Base.

## S05 Approved Design Summary

### Protocol Scope

Supported：

- Receiver only；
- Single File；
- SOH 128 Byte / STX 1024 Byte；
- Block 0 filename/filesize；
- CRC-16/XMODEM；
- ACK / NAK；
- duplicate / sequence handling；
- timeout / retry；
- CAN cancel；
- EOT + Empty Block 0 end；
- Parser / Receiver / Sink separation。

Excluded：

- STM32 Ymodem Sender；
- Batch multi-file business；
- OTA PENDING / inactive slot / reset；
- FreeRTOS background OTA；
- Bootloader installation；
- Trial/Confirm/Rollback。

### Firmware Storage Extension

Approved APIs：

```c
firmware_storage_write_payload(...)
firmware_storage_write_header(...)
```

S05 Flash Sink caches/validates Header, writes Payload first, commits Header last.

### Config

Ymodem timeout/retry/filename limits go to:

```text
03_Firmware/Application/OTA_APP/00_Config/ymodem_config.h
```

Protocol byte constants remain in `ymodem_def.h`.

## PC / Local Tooling

Reference Sender：Tera Term 5 YMODEM。

Current machine executable confirmed by Project Owner：

```text
E:\APP\ProgramFile\tera_term\teraterm5\ttermpro.exe
```

This path must only be configured in ignored:

```text
05_Tools/Config/toolchain.local.bat
```

Planned committed tools：

```text
05_Tools/TeraTerm/send_ymodem.ttl
05_Tools/Scripts/send_ymodem.bat
```

Existing stable toolchain remains the board-test base：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Scripts/flash_app.bat
05_Tools/Scripts/rtt_capture.bat
05_Tools/Scripts/run_app_cycle.bat
05_Tools/Firmware/pack_firmware.py
```

S05 board flow：

```text
Build
→ Flash
→ Reset / Run
→ RTT Capture
→ Tera Term Ymodem Send
→ RTT protocol/storage evidence
→ firmware_storage_validate_image(Slot B)
```

## Verification Direction

Host Test：Parser framing/fragmentation/CRC, Block 0 parsing, Receiver state machine, duplicate/sequence/retry/cancel, Firmware Storage write boundary mapping.

Board Test：real Tera Term transfer, Slot B validation, interruption/cancel recovery, second transfer after failure.

RTT must provide deterministic evidence for filename/filesize, progress, retry/error summary, EOT/session completion, Header/Payload commit and final Firmware validation.

## Deferred Regression

S04 cross-stage items remain:

```text
Reset Persistence       PENDING / DEFERRED
Power-cycle Persistence PENDING / DEFERRED
```

They do not block S05/S06 and must be completed before S07 closure.

## Formal S05 Documents

- Design: `00_Project/03_Stages/S05_UART_Ymodem/design.md`
- Implementation Plan: `00_Project/03_Stages/S05_UART_Ymodem/implementation_plan.md`
- Handoff: `00_Project/03_Stages/S05_UART_Ymodem/handoff.md`

## Next Action

Implementation Role begins with:

```text
Task 1: Tera Term Ymodem sender automation entry
```

Task 1 verifies local macro invocation and parameters first; a transfer timeout before MCU Receiver implementation is an expected intermediate result, not a hardware PASS.
