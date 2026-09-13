# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Last Closed Stage: `S03_EEPROM_Storage`
- Last Closed Stage Status: `CLOSED`
- Branch: `main`
- S03 Merge Commit: `4d14973c86e02a3855cb6ca62b8d995c621de4a5`
- S03 Baseline Code Commit: `b590b3cad3c04292c41130b78cfb737d3898dd30`
- S03 Design Commit: `a2a77a6a01d3219f8a1a095ce922b5a81cb6d771`
- S03 Plan Commit: `b67a7b7c1375522b1c74fcc5290ffb10ce5a7bb8`
- S03 Handoff Commit: `ffbfcdde51e4b2d1aa6e20ffbaebfd8c59741dd5`
- S03 Verification Commit: `eb511a4ea0e1d443a422518bc2a0588df9cde0e5`
- S03 Review Commit: `3c827293332e10dd660361470555bc453450430c`
- Next Stage: `S04_Firmware_Image_Storage`
- Next Stage Roadmap State: `PLANNED`
- Current Role: `Project Owner / S04 Design Preparation`
- Updated At: `2026-09-13`

## Current Goal

`S01_Application_Foundation`、`S02_External_Flash_Driver` 与 `S03_EEPROM_Storage` 均已关闭。

S03 已完成实现、用户真实板测、Verification Report、Review，并通过 PR #4 合并回 `main`。当前不再扩大 S03 范围；下一步进入 `S04_Firmware_Image_Storage` Design Preparation，建立 Firmware Image、A/B Slot 和 Metadata 的基础存储模型。

## Stable Storage Baseline

### External Flash — W25Q64

```text
Application
   ↓
W25Q64 Raw Driver
   ↓
Platform SPI Device / Bus
   ↓
STM32 SPI2 Impl
   ↓
W25Q64JV
```

已验证能力：

- JEDEC / SR1 / Read；
- Page Program / Cross-page Write；
- 4 KiB Sector Erase；
- WREN / WEL / BUSY；
- Page / Sector / 地址边界保护；
- Reset Persistence；
- 真实硬件板测；
- 统一 Keil Build 工具入口。

### EEPROM — AT24C02

```text
PB6 / PB7
    ↓
Platform GPIO
    ↓
Software I2C
    └─ platform_i2c_probe()
         ↓
AT24C02 Raw Driver
    ├─ init / deinit
    ├─ read
    ├─ write
    ├─ 8 Byte Page Split
    └─ ACK Polling
```

已验证能力：

- 7-bit address `0x50` probe；
- Single-byte / In-page / Cross-page Read-Write；
- `0xFF` 地址边界；
- 越界请求提前拒绝；
- 1 ms ACK Polling 间隔、10 ms 软件超时；
- Reset Persistence；
- 实际 Power-cycle Persistence；
- RTT + EasyLogger 真实板测；
- Driver 不拥有共享 I2C Bus 生命周期。

## Closed S03 Documents

- Design: `00_Project/03_Stages/S03_EEPROM_Storage/design.md`
- Implementation Plan: `00_Project/03_Stages/S03_EEPROM_Storage/implementation_plan.md`
- Handoff: `00_Project/03_Stages/S03_EEPROM_Storage/handoff.md`
- Verification: `04_Test/Reports/Stages/S03_EEPROM_Storage/verification.md`
- Review: `00_Project/03_Stages/S03_EEPROM_Storage/review.md`
- Hardware/Software Reference: `02_Hardware/Hardware_Software_Interface/AT24C02_硬件软件接口参考.md`

## S04 Design Preparation Boundary

S04 Roadmap 目标：建立 Firmware Image、A/B Slot 和 Metadata 的基础存储模型。

当前进入设计阶段，应讨论并冻结：

- W25Q64 External Flash A/B Slot 分区；
- Firmware Image Header；
- Firmware Version / Image Size / CRC；
- Slot Identity / Slot State 的基础表示；
- AT24C02 Metadata Contract 与地址布局；
- External Flash Image Data 与 EEPROM Metadata 的职责边界；
- Application 与未来 Bootloader 共用的数据格式；
- 有效镜像 / 损坏镜像的识别和验证流程；
- 人工写入 Firmware Image 的板测与验收方式。

S04 设计阶段暂不直接实现：

- UART / Ymodem Firmware Transport；
- FreeRTOS OTA 并发模型；
- Application OTA Service；
- Bootloader Internal Flash 安装流程；
- Trial / Confirm / Rollback 状态机；
- AES / SHA / HMAC / Digital Signature；
- Device Manager / 通用 Storage Device / NVM Manager 架构重构。

## Explicitly Deferred Architecture Work

以下架构工作继续延后到 Bootloader + OTA 主项目完成后的专项重构：

- Device Manager；
- `platform_storage_device_t`；
- 通用 NVM Manager；
- W25Q64 EEPROM Emulation。

S04 可以定义真实的跨存储数据契约，但不借机扩张成大型设备管理框架。

## Reusable Tooling

```text
05_Tools/Scripts/build_app.bat
05_Tools/Config/toolchain.local.example.bat
```

`toolchain.local.bat` 为 machine-local 配置，不由 Git 跟踪。

## Required Reading for S04 Design

1. `AGENTS.md`
2. `README.md`
3. `PROJECT_CONTEXT.md`
4. `00_Project/WORKFLOW.md`
5. `00_Project/01_Requirements/项目需求V1.md`
6. `00_Project/02_Roadmap/development_roadmap.md`
7. `00_Project/03_Stages/S02_External_Flash_Driver/design.md`
8. `00_Project/03_Stages/S02_External_Flash_Driver/handoff.md`
9. `00_Project/03_Stages/S03_EEPROM_Storage/design.md`
10. `00_Project/03_Stages/S03_EEPROM_Storage/handoff.md`
11. 当前 W25Q64 / AT24C02 Raw Driver 与相关 Platform 接口

## Next Action

进入 `S04_Firmware_Image_Storage` Design Preparation：先基于当前真实存储能力讨论 W25Q64 A/B Slot、Firmware Image Header、CRC 和 EEPROM Metadata Contract，再冻结 S04 Design。设计批准前不进入生产代码施工。

## Prohibited Actions

- 不修改已关闭 S02 / S03 的功能结论；
- 不把 Firmware Metadata 塞入 W25Q64 / AT24C02 Raw Driver；
- 不在 S04 Design Approval 前直接施工生产代码；
- 不提前实现 Ymodem、OTA Service、Bootloader 安装或 Trial/Rollback；
- 不借 S04 引入 Device Manager / 通用 Storage/NVM 架构。
