# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S04_Firmware_Image_Storage`
- Active Stage Status: `DESIGN_APPROVED`
- Branch: `codex/s04-firmware-image-storage`
- S04 Baseline Commit: `245cd3ee550a2c2cc016e6a197609d712ef1e893`
- S04 Design Commit: `e701910a0452952c632ad8352c6973eedf06b283`
- S04 Handoff Commit: `46f19c7a9624a7109b69d9133124e0fee505b08f`
- S04 Status Commit: `84eb9a11b1b3ab94a004a2b91f614d889c240723`
- S04 Implementation Plan Commit: `Not created yet`
- Last Closed Stage: `S03_EEPROM_Storage`
- Last Closed Stage Status: `CLOSED`
- Current Role: `Project Owner / S04 Design Approved`
- Updated At: `2026-09-13`

## Current Goal

S04 已完成设计讨论并获 Project Owner 批准。当前已冻结 Firmware Image、A/B Slot、Firmware Header、Version、CRC、AT24C02 Metadata 双副本以及 S04 Test-only UART 注入和 RTT 板测方案。

当前尚未创建 Implementation Plan，因此不得进入生产代码施工。下一步是创建 `00_Project/03_Stages/S04_Firmware_Image_Storage/implementation_plan.md`，审核通过后再进入 `READY_FOR_IMPLEMENTATION`。

## Stable Storage Baseline

### W25Q64

已关闭并验证：

- 8 MiB address space；
- JEDEC / SR1 / Read；
- Page Program / Cross-page Write；
- 4 KiB Sector Erase；
- WREN / WEL / BUSY；
- Page / Sector / address boundary protection；
- Reset Persistence；
- Real-board verification。

### AT24C02

已关闭并验证：

- 256 Byte capacity；
- 7-bit address `0x50`；
- read / write；
- 8 Byte Page Split；
- bounded ACK Polling；
- address boundary protection；
- Reset Persistence；
- Power-cycle Persistence。

## Frozen S04 Contracts

### External Flash A/B

```text
Slot A: 0x000000 ~ 0x07FFFF  (512 KiB)
Slot B: 0x080000 ~ 0x0FFFFF  (512 KiB)
Reserved: 0x100000 ~ 0x7FFFFF
```

Per Slot：

```text
+0x0000 ~ +0x0FFF : Header Sector
+0x1000 ~          : Firmware Payload
```

Payload capacity = 508 KiB。

### Firmware Header V1

- 64 Byte；
- little-endian；
- Magic = `FWIM`；
- Header Format Version = 1；
- Firmware Version = major/minor/patch/reserved；
- Image Size；
- Payload CRC32；
- Header CRC32；
- fixed binary offsets；
- Header CRC covers `0x00~0x3B`；
- Payload CRC only covers Firmware Payload；
- V1 Reserved all zero；
- C struct layout 不是持久化协议。

### CRC Common

冻结：

- CRC-8/SMBUS；
- CRC-16/XMODEM；
- CRC-32/ISO-HDLC；
- one-shot + streaming；
- software bitwise V1 implementation。

CRC 不绑定 Firmware、HAL、RTOS 或 STM32 CRC Peripheral。

### Metadata V1

AT24C02：

```text
0x00 ~ 0x7F : Copy A
0x80 ~ 0xFF : Copy B
```

采用：

```text
Double Copy
+ uint32_t sequence
+ CRC-32/ISO-HDLC
+ commit marker
```

基础字段包括：

- active_slot；
- confirmed_slot；
- slot_a_state；
- slot_b_state；
- confirmed_version。

Slot State V1：

```text
EMPTY
VALID
INVALID
```

### Authority Boundary

```text
Image Header
→ Firmware 实际 Version / Size / CRC 的权威来源

EEPROM Metadata
→ active / confirmed / slot state / confirmed_version 的系统状态权威来源
```

EEPROM 不重复保存每个 Slot 的 image size / payload CRC。

### Validation Boundary

明确区分：

```text
Image Invalid
≠
Validation I/O Failure
```

I/O / SPI / Flash read failure 时 Validation Result 为 UNKNOWN，返回底层错误，不自动修改 EEPROM Slot State。

Image Validation 为只读操作，不自动 commit Metadata。

## Application Module Direction

当前建议：

```text
03_Firmware/Application/OTA_APP/02_Service/
├─ service_common/
│  └─ crc/
└─ service_firmware/
   ├─ firmware_def
   ├─ firmware_version
   ├─ firmware_image
   ├─ firmware_metadata
   └─ firmware_storage
```

当前只有 Application 一个真实消费者，因此不提前把代码放入 `03_Firmware/Shared`。

等 S08 Bootloader 成为第二个消费者后，再评估将稳定的纯格式与算法代码抽取到 `03_Firmware/Shared/Firmware`。

## S04 Board Test Contract

S04 可以使用现有 `service_uart` 做 test-only Firmware Injection，但不实现正式 Ymodem。

固定目标：`Slot B`。

```text
PC pack_firmware.py
→ [Header][Payload]
→ Serial Assistant raw binary send
→ existing service_uart
→ Payload first
→ CRC PASS
→ Header commit last
→ image validation
→ RTT + EasyLogger evidence
```

传输中断、UART Data Loss、UART Error 或 CRC mismatch 时，不写 Header，因此未完成镜像保持 EMPTY。

Board test 最终保留在：

```text
04_Test/Board/S04_Firmware_Image_Storage/
```

验收后退出生产启动路径和正式 Keil target。

## Formal S04 Documents

- Design: `00_Project/03_Stages/S04_Firmware_Image_Storage/design.md`
- Handoff: `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`
- Implementation Plan: `Not created yet`
- Verification: `Not created yet`
- Review: `Not created yet`

## Required Reading For Next Step

1. `AGENTS.md`
2. `README.md`
3. `PROJECT_CONTEXT.md`
4. `00_Project/WORKFLOW.md`
5. `00_Project/01_Requirements/项目需求V1.md`
6. `00_Project/02_Roadmap/development_roadmap.md`
7. `00_Project/03_Stages/S04_Firmware_Image_Storage/design.md`
8. `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`
9. `03_Firmware/AGENTS.md`
10. `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`
11. `03_Firmware/00_Doc/Standards/Keil工程与构建输出规范.md`
12. 当前 `service_uart`、W25Q64 Raw Driver、AT24C02 Raw Driver。

## Explicitly Deferred

- UART / Ymodem 正式 Firmware Transport；
- Application OTA Service；
- RTOS OTA concurrency；
- Bootloader Internal Flash installation；
- `PENDING / TRIAL / CONFIRMED / ROLLBACK`；
- IWDG / failure counter；
- AES / SHA / HMAC / Digital Signature；
- Device Manager / generic Storage / NVM Manager。

## Next Action

生成并审核 S04 `implementation_plan.md`。设计本身已经批准，不再重新展开架构讨论，除非实施计划发现与冻结设计存在明确冲突。

## Prohibited Actions

- 不修改已关闭 S02 / S03 的功能结论；
- 不把 Firmware / Metadata 语义写进 Raw Driver；
- 不在 Implementation Plan 形成前施工生产代码；
- 不提前实现 Ymodem / OTA Service / Bootloader Installation / Trial-Rollback；
- 不提前把单一消费者代码移入 Shared；
- 不借 S04 引入 Device Manager / 通用 Storage / NVM 架构。
