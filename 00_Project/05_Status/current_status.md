# Current Project Status

## Context Metadata

- Active Stage: `S04_Firmware_Image_Storage`
- Status: `DESIGN_APPROVED`
- Branch: `codex/s04-firmware-image-storage`
- Baseline Commit: `245cd3ee550a2c2cc016e6a197609d712ef1e893`
- Design Commit: `e701910a0452952c632ad8352c6973eedf06b283`
- Handoff Commit: `46f19c7a9624a7109b69d9133124e0fee505b08f`
- Implementation Plan Commit: `Not created yet`
- Implementation Commit: `Not created yet`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Last Closed Stage: `S03_EEPROM_Storage`
- Last Closed Stage Status: `CLOSED`
- Current Role: `Project Owner / S04 Design Approved`
- Updated At: `2026-09-13`

## Current Goal

`S04_Firmware_Image_Storage` 已完成设计讨论并获得 Project Owner 批准。

本阶段冻结了 Firmware Image、External Flash A/B Slot、Image Header、Firmware Version、CRC、AT24C02 Metadata 双副本以及 S04 UART test-only 注入与 RTT 板测方案。

当前尚未创建 `implementation_plan.md`，因此不得开始生产代码施工。下一步生成并审核 S04 Implementation Plan，之后再进入 `READY_FOR_IMPLEMENTATION`。

## Frozen S04 Design Summary

### External Flash Layout

```text
Slot A: 0x000000 ~ 0x07FFFF, 512 KiB
Slot B: 0x080000 ~ 0x0FFFFF, 512 KiB
Reserved: 0x100000 ~ 0x7FFFFF
```

每个 Slot：

```text
+0x0000 ~ +0x0FFF : Header Sector
+0x1000 ~          : Firmware Payload
```

S04 板测固定使用 Slot B。

### Firmware Header V1

- 64 Byte fixed binary format；
- little-endian；
- Magic `FWIM`；
- Header Format Version = 1；
- Firmware Version = major/minor/patch；
- Image Size；
- Payload CRC32；
- Header CRC32；
- fixed offsets，不直接持久化 C struct layout。

### CRC Common

冻结算法：

- CRC-8/SMBUS；
- CRC-16/XMODEM；
- CRC-32/ISO-HDLC；
- one-shot + streaming；
- V1 为纯软件 bitwise implementation。

### AT24C02 Metadata

```text
0x00 ~ 0x7F : Metadata Copy A
0x80 ~ 0xFF : Metadata Copy B
```

采用：

```text
Double Copy
+ uint32_t sequence
+ Metadata CRC32
+ Commit Marker
```

S04 基础 Slot State：

```text
EMPTY
VALID
INVALID
```

不提前引入 OTA `PENDING / TRIAL / CONFIRMED / ROLLBACK` 状态机。

### Authority Boundary

```text
W25Q64 Image Header
→ Firmware Version / Size / CRC 的权威来源

AT24C02 Metadata
→ active / confirmed / slot state / confirmed_version 的系统状态权威来源
```

### Module Boundary

当前实现目标位于 Application：

```text
02_Service/
├─ service_common/crc/
└─ service_firmware/
```

`03_Firmware/Shared` 仍保持“Application 与 Bootloader 已经共同使用后再抽取”的现有约束。

## S04 Board Test Boundary

允许复用现有 `service_uart` 作为 test-only raw binary injection，但不实现 Ymodem。

测试链：

```text
PC pack_firmware.py
        ↓
[64 Byte Header][Payload]
        ↓
Serial Assistant raw binary send
        ↓
existing service_uart
        ↓
Slot B Payload first
        ↓
Payload CRC PASS
        ↓
Header commit last
        ↓
firmware_storage_validate_image(SLOT_B)
        ↓
RTT + EasyLogger
```

中断、UART Data Loss 或 CRC mismatch 时不得提交 Header，因此半成品不得成为 VALID Image。

## Formal Documents

- Design: `00_Project/03_Stages/S04_Firmware_Image_Storage/design.md`
- Handoff: `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`
- Implementation Plan: `Not created yet`
- Verification: `Not created yet`
- Review: `Not created yet`

## Explicitly Deferred

- Ymodem；
- Application OTA Service；
- FreeRTOS OTA 并发；
- Bootloader Firmware Installation；
- Trial / Confirm / Rollback；
- Watchdog Boot Failure；
- Security mechanisms；
- Device Manager / 通用 Storage / NVM Manager。

## Next Action

创建并审核：

```text
00_Project/03_Stages/S04_Firmware_Image_Storage/implementation_plan.md
```

计划批准后，将 Stage 从 `DESIGN_APPROVED` 推进至 `READY_FOR_IMPLEMENTATION`。

## Blockers

当前无已知设计阻塞项。
