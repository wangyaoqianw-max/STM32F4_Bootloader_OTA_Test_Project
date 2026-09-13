# S04 Firmware Image Storage Handoff

## Metadata

- Stage: `S04_Firmware_Image_Storage`
- Status: `DESIGN_APPROVED`
- Branch: `codex/s04-firmware-image-storage`
- Baseline Commit: `245cd3ee550a2c2cc016e6a197609d712ef1e893`
- Design Commit: `e701910a0452952c632ad8352c6973eedf06b283`
- Implementation Plan Commit: `Not created yet`
- Implementation Commit: `Not created yet`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Updated At: `2026-09-13`

## Current Objective

S04 已完成设计讨论并获 Project Owner 批准。当前目标是基于冻结设计生成 `implementation_plan.md`，再进入生产代码实施。

在 implementation plan 创建和批准前，不得开始 S04 生产代码施工。

## Required Reading

按顺序读取：

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
12. `03_Firmware/Application/OTA_APP/02_Service/service_uart/service_uart.h/.c`
13. `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_w25q64.h/.c`
14. `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/at24c02/platform_at24c02.h/.c`

## Frozen Design Summary

### External Flash

```text
Slot A: 0x000000 ~ 0x07FFFF, 512 KiB
Slot B: 0x080000 ~ 0x0FFFFF, 512 KiB
Reserved: 0x100000 ~ 0x7FFFFF
```

每个 Slot：

```text
+0x0000 ~ +0x0FFF : Header Sector, 4 KiB
+0x1000 ~          : Firmware Payload
```

Payload Capacity = `508 KiB`。

### Firmware Header V1

- 固定 64 Byte；
- little-endian；
- Magic = `0x4D495746` (`FWIM`)；
- Format Version = `1`；
- Header Size = `64`；
- Version = major/minor/patch/reserved，各 `uint16_t`；
- Image Size = Payload 实际字节数；
- Payload CRC32 = CRC-32/ISO-HDLC；
- Header CRC32 覆盖 Header `0x00~0x3B`；
- V1 Reserved 全 `0x00`；
- 不允许直接持久化 C struct layout。

### CRC Common

S04 新增通用 CRC 能力：

- CRC-8/SMBUS；
- CRC-16/XMODEM；
- CRC-32/ISO-HDLC；
- one-shot + streaming；
- V1 使用纯软件 bitwise implementation；
- 不依赖 HAL / RTOS / STM32 CRC Peripheral。

### AT24C02 Metadata

```text
0x00 ~ 0x7F : Copy A
0x80 ~ 0xFF : Copy B
```

每份 128 Byte：

- Magic = `FWMD`；
- Format Version = 1；
- Metadata Size = 128；
- sequence；
- active_slot；
- confirmed_slot；
- slot_a_state；
- slot_b_state；
- confirmed_version；
- reserved；
- metadata_crc32；
- commit_marker = `CMIT`。

采用：

```text
Double Copy
+ sequence
+ CRC32
+ commit marker
```

提交时必须先让目标副本失效，写入并 read-back 验证后，最后提交 commit marker。

### State Boundary

S04 Slot State 只定义：

```text
EMPTY
VALID
INVALID
```

不提前引入：

```text
PENDING
INSTALLING
TRIAL
CONFIRMED
ROLLBACK
```

### Authority Boundary

```text
W25Q64 Image Header
→ Firmware 真实 Version / Size / CRC 的权威来源

AT24C02 Metadata
→ 系统当前 active / confirmed / slot state / confirmed_version 的权威来源
```

EEPROM 不重复保存每个 Slot 的 image_size / payload_crc32。

### Application Module Boundary

当前建议：

```text
02_Service/
├─ service_common/
│  └─ crc/
│
└─ service_firmware/
   ├─ firmware_def
   ├─ firmware_version
   ├─ firmware_image
   ├─ firmware_metadata
   └─ firmware_storage
```

当前只有 Application 一个真实消费者，因此不提前把代码放入 `03_Firmware/Shared`。

S08 Bootloader 成为第二个消费者后，再评估将稳定的纯格式/算法代码抽入 `03_Firmware/Shared/Firmware`。

### Validation Boundary

Image Validation 必须区分：

```text
Image INVALID
≠
Validation I/O Failure
```

W25Q64 / SPI 访问失败时返回底层错误，Validation Result 保持 UNKNOWN，不得自动把 EEPROM Slot State 改成 INVALID。

`firmware_storage_validate_image()` 为只读验证操作，不自动提交 Metadata。

## S04 Board Test Contract

### Test Input

PC 侧新增 Firmware pack tool，输入 APP `.bin` 与 version，输出：

```text
[64 Byte Header][Payload]
```

测试文件通过串口助手 raw binary send。

### Fixed Target

S04 板测固定目标：`Slot B`。

```text
Slot B Base    = 0x080000
Payload Address= 0x081000
```

不增加 Slot 选择 UART 命令协议。

### Existing UART Reuse

复用已经存在的 `service_uart`：

- DMA RX；
- RingBuffer；
- wait event；
- read；
- data loss detection；
- UART error handling。

S04 不实现 Ymodem。

### Image Commit Rule

写入顺序固定：

```text
Header 在 RAM 中先接收/验证
→ erase Slot B required sectors
→ Payload first
→ Payload CRC success
→ Header last
```

只有 Header 最后写入后，Image 才被视为已提交。

传输中断、UART Data Loss、CRC mismatch 时不得写 Header；Header 保持擦除态，Slot 识别为 EMPTY。

### Diagnostics

统一使用：

```text
SEGGER RTT + EasyLogger
```

至少输出：

- erase start/result；
- Header fields；
- Firmware version；
- image size；
- expected/calculated CRC32；
- receive progress；
- Header commit result；
- image validation result；
- Metadata Copy A/B validity；
- selected sequence；
- metadata commit result；
- final board test result。

### Retained Test Location

板测源码最终保留：

```text
04_Test/Board/S04_Firmware_Image_Storage/
```

验收后必须从生产 Application 启动路径和正式 Keil target 中移除。

## Required Verification Cases

至少覆盖：

1. Valid image；
2. Payload one-byte corruption；
3. Header one-byte corruption；
4. erased Header → EMPTY；
5. invalid image_size；
6. UART interrupted transfer → Header not committed；
7. UART data loss/error → abort；
8. Metadata A valid / B invalid；
9. Metadata A invalid / B valid；
10. Metadata A/B valid sequence selection；
11. target Metadata copy incomplete commit → previous copy recovery；
12. Reset Persistence；
13. Power-cycle Persistence；
14. Keil build / clean rebuild；
15. RTT real-board evidence。

## Allowed Changes For Implementation Plan

实施计划可以规划以下范围：

- `02_Service/service_common/crc/`；
- 新增 `02_Service/service_firmware/`；
- 必要的 `project_config.h` 固定配置；
- Keil project source/include entries；
- `05_Tools/Firmware/pack_firmware.py`；
- `04_Test/Board/S04_Firmware_Image_Storage/`；
- 必要的临时 Board Test 启动接入；
- S04 文档 / 状态文件；
- 后续 Verification Report。

## Prohibited Changes

- 不返工 S02/S03 已关闭 Raw Driver，除非发现明确阻塞 Bug 并升级设计；
- 不把 Firmware / Metadata 业务语义塞入 Raw Driver；
- 不实现 Ymodem；
- 不实现正式 OTA Service；
- 不实现 Bootloader 安装；
- 不实现 Trial / Confirm / Rollback；
- 不引入大型通用 Storage / Device Manager；
- 不提前把单一消费者代码移入 Shared；
- 不因一次 I/O Error 将 Slot 永久标记为 INVALID；
- 不将临时破坏性板测长期留在生产启动路径。

## Current Status

```text
Design Discussion  PASS
Design Approval    PASS
Design Document    CREATED
Implementation Plan NOT_CREATED
Production Code    NOT_STARTED
Verification       NOT_STARTED
Review             NOT_STARTED
```

## Next Action

读取 `design.md` 与本 handoff，生成 S04 `implementation_plan.md`。计划完成并由 Project Owner 接受后，再将 Stage 推进至 `READY_FOR_IMPLEMENTATION`。
