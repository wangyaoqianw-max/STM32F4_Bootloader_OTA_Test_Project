# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S04_Firmware_Image_Storage`
- Active Stage Status: `DESIGN_APPROVED`
- Branch: `codex/s04-firmware-image-storage`
- S04 Baseline Commit: `245cd3ee550a2c2cc016e6a197609d712ef1e893`
- S04 Design Commit: `e701910a0452952c632ad8352c6973eedf06b283`
- S04 Implementation Plan Commit: `bc5360fa40188c189a9e19b91a29ad5d266d8220`
- S04 Handoff Sync Commit: `b02f6b39e043ee6fef743e9b900e349712aae5df`
- S04 Review Skeleton Commit: `39602db5997c3277ff764d4a85ac029799b7ee85`
- S04 Status Commit: `53f332b5931b659fbedfe02337bd8e0d47ade198`
- Last Closed Stage: `S03_EEPROM_Storage`
- Last Closed Stage Status: `CLOSED`
- Current Role: `Project Owner / S04 Plan Review`
- Updated At: `2026-09-13`

## Current Goal

S04 的设计合同已经冻结，正式 `implementation_plan.md` 已创建并自检完成。

当前等待 Project Owner 接受实施计划。Owner Acceptance 前保持 `DESIGN_APPROVED`，不得施工生产代码；接受后同步到 `READY_FOR_IMPLEMENTATION`，再由 Codex / Implementation Role 按 Task 1~8 执行。

## Stable Storage Baseline

### W25Q64

已关闭并验证：8 MiB address space、JEDEC/SR1/Read、Page Program/Cross-page Write、4 KiB Sector Erase、WREN/WEL/BUSY、边界保护、Reset Persistence、Real-board verification。

### AT24C02

已关闭并验证：256 Byte、7-bit address `0x50`、read/write、8 Byte Page Split、bounded ACK Polling、边界保护、Reset Persistence、Power-cycle Persistence。

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
- Header CRC covers `0x00~0x3B`；
- Payload CRC only covers Firmware Payload；
- V1 Reserved all zero；
- C struct layout 不是持久化协议。

### CRC Common

- CRC-8/SMBUS；
- CRC-16/XMODEM；
- CRC-32/ISO-HDLC；
- one-shot + streaming；
- software bitwise V1 implementation；
- CRC 不绑定 Firmware、HAL、RTOS 或 STM32 CRC Peripheral。

### Metadata V1

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

基础字段：active_slot、confirmed_slot、slot_a_state、slot_b_state、confirmed_version。

Slot State V1：`EMPTY / VALID / INVALID`。

### Authority Boundary

```text
Image Header
→ Firmware 实际 Version / Size / CRC

EEPROM Metadata
→ active / confirmed / slot state / confirmed_version
```

EEPROM 不重复保存每个 Slot 的 image size / payload CRC。

### Validation Boundary

```text
Image Invalid
≠
Validation I/O Failure
```

I/O / SPI / Flash read failure 时 Validation Result 为 UNKNOWN，返回底层错误，不自动修改 EEPROM Slot State。Image Validation 为只读操作，不自动 commit Metadata。

## Application Module Direction

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

当前只有 Application 一个真实消费者，因此不提前把代码放入 `03_Firmware/Shared`。S08 Bootloader 成为第二个消费者后，再评估抽取稳定纯格式/算法代码。

## Implementation Plan

正式计划：

`00_Project/03_Stages/S04_Firmware_Image_Storage/implementation_plan.md`

Task Map：

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

计划要求先用 Host Test 验证 CRC / Binary Contract / Metadata / Storage 编排，再进入 Keil Build 和真实硬件验证。

## S04 Board Test Contract

S04 使用现有 `service_uart` 做 test-only Firmware Injection，但不实现正式 Ymodem。

固定目标：`Slot B`。

```text
PC pack_firmware.py
→ [64 Byte Header][Payload]
→ Serial Assistant raw binary send
→ existing service_uart
→ Payload first
→ streaming CRC PASS
→ Header commit last
→ full re-read image validation
→ Metadata double-copy checks
→ RTT + EasyLogger evidence
```

传输中断、UART Data Loss、UART Error 或 CRC mismatch 时不写 Header，因此未完成镜像保持 EMPTY。

Board Test 在当前 appSystem Task 中可通过 `platform_thread_get_current()` 获取 `service_uart` 所需 owner thread，不为了测试暴露 `app_system.c` static object。

Board test 最终保留：

```text
04_Test/Board/S04_Firmware_Image_Storage/
```

验收后退出 production startup 和正式 Keil target。

## Formal S04 Documents

- Design: `00_Project/03_Stages/S04_Firmware_Image_Storage/design.md`
- Implementation Plan: `00_Project/03_Stages/S04_Firmware_Image_Storage/implementation_plan.md`
- Handoff: `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`
- Review Skeleton: `00_Project/03_Stages/S04_Firmware_Image_Storage/review.md`
- Verification: `Not created yet`

## Required Reading For Implementation

1. `AGENTS.md`
2. `README.md`
3. `PROJECT_CONTEXT.md`
4. `00_Project/WORKFLOW.md`
5. `00_Project/01_Requirements/项目需求V1.md`
6. `00_Project/02_Roadmap/development_roadmap.md`
7. `00_Project/03_Stages/S04_Firmware_Image_Storage/design.md`
8. `00_Project/03_Stages/S04_Firmware_Image_Storage/implementation_plan.md`
9. `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`
10. `03_Firmware/AGENTS.md`
11. `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`
12. `03_Firmware/00_Doc/Standards/Keil工程与构建输出规范.md`
13. current `service_uart`, W25Q64 Raw Driver, AT24C02 Raw Driver。

## Explicitly Deferred

- UART / Ymodem 正式 Firmware Transport；
- Application OTA Service；
- Bootloader Internal Flash installation；
- `PENDING / TRIAL / CONFIRMED / ROLLBACK`；
- IWDG / failure counter；
- AES / SHA / HMAC / Digital Signature；
- Device Manager / generic Storage / NVM Manager。

## Next Action

Project Owner 审阅并接受 S04 `implementation_plan.md`。

接受后：

```text
DESIGN_APPROVED
→ READY_FOR_IMPLEMENTATION
```

然后按 Task 1 → Task 8 进入 Implementation Role。

## Prohibited Actions

- 不修改已关闭 S02 / S03 的功能结论；
- 不把 Firmware / Metadata 语义写进 Raw Driver；
- 不在 Plan Owner Acceptance 前施工生产代码；
- 不提前实现 Ymodem / OTA Service / Bootloader Installation / Trial-Rollback；
- 不提前把单一消费者代码移入 Shared；
- 不借 S04 引入 Device Manager / 通用 Storage / NVM 架构。
