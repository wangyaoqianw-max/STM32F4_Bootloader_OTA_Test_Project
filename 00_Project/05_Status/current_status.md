# Current Project Status

## Context Metadata

- Active Stage: `S04_Firmware_Image_Storage`
- Status: `DESIGN_APPROVED`
- Branch: `codex/s04-firmware-image-storage`
- Baseline Commit: `245cd3ee550a2c2cc016e6a197609d712ef1e893`
- Design Commit: `e701910a0452952c632ad8352c6973eedf06b283`
- Implementation Plan Commit: `bc5360fa40188c189a9e19b91a29ad5d266d8220`
- Handoff Sync Commit: `b02f6b39e043ee6fef743e9b900e349712aae5df`
- Review Skeleton Commit: `39602db5997c3277ff764d4a85ac029799b7ee85`
- Implementation Commit: `Not created yet`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Last Closed Stage: `S03_EEPROM_Storage`
- Last Closed Stage Status: `CLOSED`
- Current Role: `Project Owner / S04 Plan Review`
- Updated At: `2026-09-13`

## Current Goal

`S04_Firmware_Image_Storage` 的设计与正式 `implementation_plan.md` 已创建。

当前不再补充新的架构范围；Project Owner 需要审核并接受 Implementation Plan。接受前 Stage 保持 `DESIGN_APPROVED`，生产代码不得施工。接受后推进：

```text
DESIGN_APPROVED
→ READY_FOR_IMPLEMENTATION
```

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

- CRC-8/SMBUS；
- CRC-16/XMODEM；
- CRC-32/ISO-HDLC；
- one-shot + streaming；
- V1 软件 bitwise implementation。

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

### Module Boundary

```text
02_Service/
├─ service_common/crc/
└─ service_firmware/
```

当前不提前把单一 Application 消费者代码放入 `03_Firmware/Shared`。

## Implementation Plan Task Map

```text
Task 1  CRC Common + standard host vectors
Task 2  Firmware Version + Header V1 format
Task 3  Metadata V1 codec + double-copy selection
Task 4  Firmware Storage service + storage host stubs
Task 5  PC pack_firmware.py + cross-language contract test
Task 6  Keil production integration
Task 7  UART → fixed Slot B isolated board test
Task 8  Board-test cleanup + verification handoff
```

实施采用 Host Test → Keil Build → Real-board Verification 的顺序；格式和状态算法不能只靠板测观察。

## S04 Board Test Boundary

允许复用现有 `service_uart` 作为 test-only raw binary injection，不实现 Ymodem。

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
full image validation
        ↓
RTT + EasyLogger
```

中断、UART Data Loss、UART Error 或 CRC mismatch 时不得提交 Header。

## Formal Documents

- Design: `00_Project/03_Stages/S04_Firmware_Image_Storage/design.md`
- Implementation Plan: `00_Project/03_Stages/S04_Firmware_Image_Storage/implementation_plan.md`
- Handoff: `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`
- Review Skeleton: `00_Project/03_Stages/S04_Firmware_Image_Storage/review.md`
- Verification: `Not created yet`

## Explicitly Deferred

- Ymodem；
- Application OTA Service；
- Bootloader Firmware Installation；
- Trial / Confirm / Rollback；
- Watchdog Boot Failure；
- Security mechanisms；
- Device Manager / 通用 Storage / NVM Manager。

## Next Action

Project Owner 审阅并接受 S04 `implementation_plan.md`。

Plan 接受后同步状态为 `READY_FOR_IMPLEMENTATION`，再交给 Codex / Implementation Role 按 Task 1~8 执行。

## Blockers

当前无已知技术阻塞；唯一门禁为 Implementation Plan Owner Acceptance。
