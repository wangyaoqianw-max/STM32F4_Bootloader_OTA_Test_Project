# S04 Firmware Image Storage Handoff

## Metadata

- Stage: `S04_Firmware_Image_Storage`
- Status: `READY_FOR_IMPLEMENTATION`
- Branch: `codex/s04-firmware-image-storage`
- Baseline Commit: `245cd3ee550a2c2cc016e6a197609d712ef1e893`
- Design Commit: `e701910a0452952c632ad8352c6973eedf06b283`
- Implementation Plan Commit: `bc5360fa40188c189a9e19b91a29ad5d266d8220`
- Review Skeleton Commit: `39602db5997c3277ff764d4a85ac029799b7ee85`
- Plan Owner Acceptance: `PASS`
- Implementation Commit: `Not created yet`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Updated At: `2026-09-13`

## Current Objective

S04 设计与 `implementation_plan.md` 均已由 Project Owner 批准，阶段正式进入 `READY_FOR_IMPLEMENTATION`。

Implementation Role 可以开始施工，但必须严格遵循冻结设计、计划任务顺序与范围边界。若实现发现设计冲突，不得自行改协议或扩展 OTA 范围，应先记录并回到设计/阻塞处理。

## Required Reading

实施前按顺序读取：

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
13. `03_Firmware/Application/OTA_APP/02_Service/service_uart/service_uart.h/.c`
14. `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_w25q64.h/.c`
15. `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/at24c02/platform_at24c02.h/.c`
16. `04_Test/Board/S02_External_Flash_Driver/` 与 `04_Test/Board/S03_EEPROM_Storage/` 现有板测模式。

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

S04 新增：

- CRC-8/SMBUS；
- CRC-16/XMODEM；
- CRC-32/ISO-HDLC；
- one-shot + streaming；
- V1 软件 bitwise implementation；
- 不依赖 HAL / RTOS / STM32 CRC Peripheral。

### AT24C02 Metadata

```text
0x00 ~ 0x7F : Copy A
0x80 ~ 0xFF : Copy B
```

每份 128 Byte，采用：

```text
Double Copy
+ uint32_t sequence
+ CRC-32/ISO-HDLC
+ commit marker
```

S04 基础字段：

- active_slot；
- confirmed_slot；
- slot_a_state；
- slot_b_state；
- confirmed_version。

S04 Slot State 只定义：

```text
EMPTY
VALID
INVALID
```

不提前引入 `PENDING / INSTALLING / TRIAL / CONFIRMED / ROLLBACK`。

### Authority Boundary

```text
W25Q64 Image Header
→ Firmware 实际 Version / Size / CRC

AT24C02 Metadata
→ active / confirmed / slot state / confirmed_version
```

EEPROM 不重复保存每个 Slot 的 image size / payload CRC。

### Application Module Boundary

计划新增：

```text
02_Service/
├─ service_common/
│  └─ crc/
└─ service_firmware/
   ├─ firmware_def
   ├─ firmware_version
   ├─ firmware_image
   ├─ firmware_metadata
   └─ firmware_storage
```

当前只有 Application 一个真实消费者，因此不提前把代码放入 `03_Firmware/Shared`。S08 Bootloader 成为第二个消费者后，再评估将稳定纯格式/算法代码抽入 `03_Firmware/Shared/Firmware`。

### Validation Boundary

必须区分：

```text
Image INVALID
≠
Validation I/O Failure
```

W25Q64 / SPI 访问失败时返回底层错误，Validation Result 保持 UNKNOWN，不得自动把 EEPROM Slot State 改成 INVALID。

`firmware_storage_validate_image()` 为只读验证操作，不自动提交 Metadata。

## Implementation Plan Task Map

正式计划：`00_Project/03_Stages/S04_Firmware_Image_Storage/implementation_plan.md`

任务顺序：

```text
Task 1  CRC Common + standard host vectors
Task 2  Firmware Version + Header V1 format
Task 3  Metadata V1 codec + double-copy selection
Task 4  Firmware Storage service + storage host stubs
Task 5  PC pack_firmware.py + cross-language contract test
Task 6  Keil production integration
Task 7  UART → fixed Slot B isolated board test
Task 8  Remove destructive test from production path + verification handoff
```

实现原则：Host Test 能覆盖的格式、CRC、Metadata 与 Storage 编排先在 PC 上验证；真实 Flash / EEPROM / UART / persistence 再由板测确认。

## S04 Board Test Contract

### Test Input

PC 工具：

```text
05_Tools/Firmware/pack_firmware.py
```

输入 APP `.bin` + version，输出：

```text
[64 Byte Header][Payload]
```

通过串口助手 raw binary send。

### Fixed Target

```text
Slot B Base     = 0x080000
Payload Address = 0x081000
```

不增加 Slot 选择 UART 命令协议。

### Existing UART Reuse

复用现有 `service_uart`：DMA RX、RingBuffer、wait event、read、data loss detection、UART error handling。

Board Test 在当前 `appSystem` Task 中执行时，通过 `platform_thread_get_current()` 获取 owner thread，不新增为了测试暴露的系统线程接口。

### Image Commit Rule

```text
Receive/validate Header in RAM
→ erase Slot B required sectors
→ write Payload first
→ streaming Payload CRC PASS
→ write Header last
→ full re-read validate
```

只有 Header 最后写入后，Image 才被视为已提交。

传输中断、UART Data Loss、UART Error、CRC mismatch 时不得写 Header；Header 保持擦除态，Slot 识别为 EMPTY。

### Diagnostics

统一：`SEGGER RTT + EasyLogger`。

至少输出：erase、Header fields、Version、Size、Expected/Calculated CRC32、receive progress、Header commit、full image validation、Metadata Copy A/B、selected sequence、metadata commit、final result。

### Retained Test Location

```text
04_Test/Board/S04_Firmware_Image_Storage/
```

验收后必须从 production `app_main()` 和正式 Keil target 移除；测试源码继续留在 `04_Test/Board`。

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
11. sequence wrap-around；
12. target Metadata copy incomplete commit → previous copy recovery；
13. Reset Persistence；
14. Power-cycle Persistence；
15. PC pack tool ↔ C decoder compatibility；
16. Keil normal build / clean rebuild；
17. RTT real-board evidence；
18. destructive board-test cleanup。

## Allowed Changes

- `02_Service/service_common/crc/`；
- `02_Service/service_firmware/`；
- 必要的 `project_config.h` test gate；
- `MDK-ARM/OTA_APP.uvprojx`；
- `05_Tools/Firmware/`；
- `04_Test/Host/S04_Firmware_Image_Storage/`；
- `04_Test/Board/S04_Firmware_Image_Storage/`；
- 必要的临时 `app_main.c` board-test 接入；
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
- 不将临时破坏性板测长期留在 production startup / Keil target。

## Current Status

```text
Design Discussion      PASS
Design Approval        PASS
Design Document        CREATED
Implementation Plan    CREATED
Plan Owner Acceptance  PASS
Stage Status           READY_FOR_IMPLEMENTATION
Production Code        IN_PROGRESS (Task 1 complete)
Verification           NOT_STARTED
Review                 NOT_STARTED
```

## Implementation Progress

- Task 1 — CRC Common：已新增 CRC-8/SMBUS、CRC-16/XMODEM、CRC-32/ISO-HDLC 的软件 bitwise 实现和 Host Test；标准向量与流式分块计算已验证。提交记录见当前阶段分支历史。
- Task 2 — Firmware Version / Header V1：已新增固定 Slot 合同、Version 比较与校验、64 Byte Header 固定偏移编码/解码和 Header 校验；Host Test 已覆盖 fixed-offset、little-endian、erased、reserved、size 与 CRC 路径。提交记录见当前阶段分支历史。
- Task 3 — Metadata V1：已新增 128 Byte 双副本 fixed-offset 编解码、CRC32、commit marker、字段范围校验与 wrap-around-safe sequence 选择；Host Test 已覆盖 A/B 有效副本选择、序列回绕、保留区篡改、CRC 与未提交 marker 恢复。提交记录见当前阶段分支历史。
- Task 4 — Firmware Storage Service：已新增 W25Q64/AT24C02 存储编排、整镜像流式 CRC 验证、I/O 与镜像无效区分、Metadata 双副本原子提交；Host Stub 已覆盖提交中断后的旧副本恢复与 sequence 递增。提交记录见当前阶段分支历史。
- Task 5 — PC Firmware Pack Tool：已新增固定偏移 little-endian 打包工具与 Python 单元测试；Python 生成 `.img` 已由 C Host Test 验证 Header、Version、长度及 Payload CRC 合同一致。提交记录见当前阶段分支历史。
- Task 6 — Keil Production Integration：已将 CRC 与 4 个 Firmware production source 加入独立 Keil Group，并追加对应 include path；normal build 与 Clean/Rebuild 均为 0 errors、8 个既有 warning，S04 source 无新增 warning。提交记录见当前阶段分支历史。

## Next Action

Implementation Role 按 `implementation_plan.md` 继续 Task 7，并在每个 Task 结束后执行对应 Host Test / Build / `git diff --check` / Commit。

Task 1 → Task 8 完成后更新 Implementation Output，并将阶段推进至 `READY_FOR_VERIFICATION`。Implementation Role 不得自行填写最终 Verification PASS 或关闭 Stage。
