# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S04_Firmware_Image_Storage`
- Active Stage Status: `CLOSED`
- Branch: `main`
- S04 Baseline Commit: `245cd3ee550a2c2cc016e6a197609d712ef1e893`
- S04 Design Commit: `e701910a0452952c632ad8352c6973eedf06b283`
- S04 Implementation Plan Commit: `bc5360fa40188c189a9e19b91a29ad5d266d8220`
- S04 Implementation Commit: `647f32f`
- S04 Toolchain Commit: `1f756f0`
- S04 Verification Commit: `72a7403`
- S04 Scoped Review Commit: `f2b6ed9`
- S04 Final Result: `CLOSED / PASS`
- Last Closed Stage: `S04_Firmware_Image_Storage`
- Next Planned Stage: `S05_UART_Ymodem`
- Current Role: `Ready for S05 Design`
- Updated At: `2026-09-14`

## Current Goal

S04 已正式关闭。下一轮工作从 `S05_UART_Ymodem` Design Stage 开始。

S05 尚未创建正式 Stage 文档，因此当前不要直接施工 Ymodem 代码；应先读取现有 UART、S04 Firmware Contract、Roadmap 和协议参考，再进行设计讨论。

## Stable Storage Baseline

### W25Q64

已关闭并验证：

- 8 MiB address space；
- JEDEC / SR1 / Read；
- Page Program / Cross-page Write；
- 4 KiB Sector Erase；
- WREN / WEL / BUSY；
- boundary protection；
- real-board verification。

### AT24C02

已关闭并验证：

- 256 Byte；
- 7-bit address `0x50`；
- read / write；
- 8 Byte Page Split；
- bounded ACK Polling；
- boundary protection；
- Raw Driver reset / power-cycle persistence 已在 S03 验证。

## Stable S04 Firmware Contract

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
Payload capacity  : 508 KiB
```

### Firmware Header V1

- 64 Byte；
- little-endian；
- Magic = `FWIM`；
- Format Version = 1；
- Firmware Version = major/minor/patch/reserved；
- Image Size；
- Payload CRC32；
- Header CRC32；
- Header CRC covers `0x00~0x3B`；
- Payload CRC only covers actual Firmware Payload；
- V1 reserved all zero；
- persistent binary format uses fixed offsets, not raw C struct layout。

### CRC Common

```text
CRC-8/SMBUS
CRC-16/XMODEM
CRC-32/ISO-HDLC
```

支持 one-shot + streaming；V1 software bitwise implementation。

### Metadata V1

```text
AT24C02
0x00 ~ 0x7F : Copy A
0x80 ~ 0xFF : Copy B
```

机制：

```text
Double Copy
+ uint32_t sequence
+ CRC-32/ISO-HDLC
+ commit marker
```

S04 Slot State：

```text
EMPTY
VALID
INVALID
```

Authority：

```text
Image Header
→ actual Firmware Version / Size / CRC

EEPROM Metadata
→ active / confirmed / slot state / confirmed_version
```

`firmware_storage_validate_image()` 为只读验证；I/O failure 不等于 image invalid。

## Application Modules Available To S05

```text
03_Firmware/Application/OTA_APP/02_Service/
├─ service_uart/
├─ service_common/
│  └─ crc/
└─ service_firmware/
   ├─ firmware_def
   ├─ firmware_version
   ├─ firmware_image
   ├─ firmware_metadata
   └─ firmware_storage
```

S04 没有把单一 Application 消费者提前移动到 `03_Firmware/Shared`。等 S08 Bootloader 成为第二个真实消费者后，再评估抽取稳定纯格式/算法代码。

## Application Toolchain

统一入口：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Scripts/flash_app.bat
05_Tools/Scripts/rtt_capture.bat
05_Tools/Scripts/run_app_cycle.bat
05_Tools/Firmware/pack_firmware.py
```

机器相关路径由被 Git 忽略的 `05_Tools/Config/toolchain.local.bat` 管理。

## S04 Verification / Review Result

已完成并通过：

- CRC Host Test；
- Firmware Format Host Test；
- Firmware Storage Host Test；
- Python pack tool test；
- Python ↔ C binary contract；
- Keil normal / clean rebuild；
- Slot B Firmware 主流程真实板测；
- Payload streaming CRC；
- Header-last commit；
- full image re-read validation；
- Metadata double-copy commit and recovery；
- J-Link flash / RTT tool smoke；
- Review。

正式文件：

- Design: `00_Project/03_Stages/S04_Firmware_Image_Storage/design.md`
- Implementation Plan: `00_Project/03_Stages/S04_Firmware_Image_Storage/implementation_plan.md`
- Handoff: `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`
- Review: `00_Project/03_Stages/S04_Firmware_Image_Storage/review.md`
- Verification: `04_Test/Reports/Stages/S04_Firmware_Image_Storage/verification.md`

## Deferred Regression

以下两项仍未执行，必须保持真实状态：

```text
Reset Persistence       PENDING / DEFERRED
Power-cycle Persistence PENDING / DEFERRED
```

Project Owner 已批准将其从 S04 关闭阻塞项调整为跨阶段延期回归项。

硬门禁：

```text
Must be completed before:
S07_OTA_Service_V1 stage closure
```

S05 / S06 可正常推进。

## S05 Design Entry

下一阶段：

`S05_UART_Ymodem`

第一轮 Design Discussion 优先读取：

1. `AGENTS.md`
2. `README.md`
3. `PROJECT_CONTEXT.md`
4. `00_Project/WORKFLOW.md`
5. `00_Project/01_Requirements/项目需求V1.md`
6. `00_Project/02_Roadmap/development_roadmap.md`
7. `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`
8. `00_Project/03_Stages/S04_Firmware_Image_Storage/review.md`
9. `03_Firmware/AGENTS.md`
10. current `service_uart`；
11. current `service_common/crc`；
12. current `service_firmware`；
13. Ymodem 原始协议或高可信参考资料。

S05 需要正式讨论：

- Ymodem protocol boundary；
- module layer / ownership；
- Block 0 / SOH / STX / EOT / ACK / NAK / CAN；
- CRC-16/XMODEM reuse；
- timeout / retry / cancel；
- 128 Byte / 1 KiB packet；
- raw `.bin` vs S04 `.img`；
- Ymodem 与 `firmware_storage` 的边界；
- PC test tool；
- board verification cases。

## Explicitly Deferred Beyond S05

- Application OTA Service；
- Bootloader Internal Flash installation；
- `PENDING / TRIAL / CONFIRMED / ROLLBACK` OTA workflow；
- IWDG / failure counter；
- AES / SHA / HMAC / Digital Signature；
- Device Manager / generic Storage / NVM Manager。

## Next Action

开启新对话，进入 `S05_UART_Ymodem` Design Stage。

不要直接施工代码；先按仓库当前状态和 Ymodem 参考资料完成设计讨论。
