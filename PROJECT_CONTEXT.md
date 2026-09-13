# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Last Closed Stage: `S03_EEPROM_Storage`
- Active Stage: `S03_EEPROM_Storage`
- Active Stage Status: `CLOSED`
- Branch: `codex/s03-eeprom-storage`
- S03 Baseline Code Commit: `b590b3cad3c04292c41130b78cfb737d3898dd30`
- S03 Design Commit: `a2a77a6a01d3219f8a1a095ce922b5a81cb6d771`
- S03 Plan Commit: `b67a7b7c1375522b1c74fcc5290ffb10ce5a7bb8`
- S03 Handoff Commit: `ffbfcdd`
- S03 Review Skeleton Commit: `982b8afb715e47808ea5731d81f2ca5cd49b7a22`
- S03 Verification Commit: `eb511a4ea0e1d443a422518bc2a0588df9cde0e5`
- S03 Review Commit: `3c827293332e10dd660361470555bc453450430c`
- Current Role: `Review Role complete`
- Updated At: `2026-09-13`

## Current Goal

`S01_Application_Foundation` 与 `S02_External_Flash_Driver` 已关闭。

`S03_EEPROM_Storage` 实现、用户真实板测、Verification Report 和 Review 已完成，结果为 `PASS`，阶段已 `CLOSED`；临时板测源已保留在 `04_Test/Board/S03_EEPROM_Storage` 并移出生产 Application/Keil 工程。后续如继续推进，需单独创建 S04 Design Stage；仍禁止把 S03 扩大到 Firmware Metadata、OTA 状态机或完整 Device/Manager/Storage 架构。

## Stable Baseline from S02

后续阶段继续依赖已经真实验证的 W25Q64 Raw Driver、Platform SPI 和统一 Keil Build 工具链。

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

S02 正式文档：

- `00_Project/03_Stages/S02_External_Flash_Driver/design.md`
- `00_Project/03_Stages/S02_External_Flash_Driver/implementation_plan.md`
- `00_Project/03_Stages/S02_External_Flash_Driver/handoff.md`
- `04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md`
- `00_Project/03_Stages/S02_External_Flash_Driver/review.md`

## Active Stage — S03 EEPROM Storage

目标：建立 AT24C02 小容量掉电存储 Raw Driver。

冻结后的主链：

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
         ↓
RTT + EasyLogger Board Verification
```

已确认的硬件与协议事实：

- AT24C02 = 256 Byte；
- Page Size = 8 Byte；
- A0/A1/A2 = GND → 7-bit address `0x50`；
- WP = GND；
- PB6 = SCL，PB7 = SDA；
- SCL/SDA 外部各有 4.7 kΩ 上拉至 `FLASH_VCC`；
- PB6/PB7 当前生成配置为 Open Drain + `GPIO_NOPULL` + initial HIGH；
- 当前 Software I2C 名义约 100 kHz；
- `tWR` 最大 5 ms；
- ACK Polling 使用 1 ms 间隔、10 ms 软件保护窗口；
- Raw Driver 不拥有共享 I2C Bus 生命周期。

S03 正式文档：

- Design: `00_Project/03_Stages/S03_EEPROM_Storage/design.md`
- Implementation Plan: `00_Project/03_Stages/S03_EEPROM_Storage/implementation_plan.md`
- Handoff: `00_Project/03_Stages/S03_EEPROM_Storage/handoff.md`
- Review: `00_Project/03_Stages/S03_EEPROM_Storage/review.md`
- Hardware/Software Reference: `02_Hardware/Hardware_Software_Interface/AT24C02_硬件软件接口参考.md`

## Explicitly Deferred from S03

以下内容等主链出现真实需求后再设计：

- Firmware Metadata 具体布局；
- CRC / 双副本 / Sequence / Journal；
- OTA `PENDING / TRIAL / CONFIRMED / ROLLBACK`；
- Device Manager；
- `platform_storage_device_t`；
- 通用 NVM Manager；
- EEPROM Emulation on W25Q64；
- RTOS I2C 互斥。

Device / Manager / Storage 的完整统一架构，计划在 Bootloader + OTA 主项目完成后作为单独架构重构专题处理。

## Reusable Tooling

```text
05_Tools/Scripts/build_app.bat
05_Tools/Config/toolchain.local.example.bat
```

`toolchain.local.bat` 为 machine-local 配置，不由 Git 跟踪。

## Required Reading for Next Tool / Agent

1. `AGENTS.md`
2. `README.md`
3. `PROJECT_CONTEXT.md`
4. `00_Project/WORKFLOW.md`
5. `00_Project/02_Roadmap/development_roadmap.md`
6. `00_Project/03_Stages/S03_EEPROM_Storage/design.md`
7. `00_Project/03_Stages/S03_EEPROM_Storage/implementation_plan.md`
8. `00_Project/03_Stages/S03_EEPROM_Storage/handoff.md`
9. `02_Hardware/Hardware_Software_Interface/AT24C02_硬件软件接口参考.md`
10. 当前 `platform_i2c`、Platform GPIO/Time、W25Q64 Raw Driver 代码

## Next Action

当前下一步：如继续推进，由 Project Owner 决定是否创建 `S04_Firmware_Image_Storage` Design Stage；S03 不再追加范围外实现。

实现完成后必须经过真实硬件 RTT 测试、Reset Persistence、实际 Power-cycle Persistence，再移交独立 Verification / Review；S03 的硬件证据已由用户提供并记录在 Verification Report。

## Prohibited Actions

- 不修改 S02 已关闭结论；
- 不绕过 Platform 在 AT24C02 Driver 中直接调用 HAL GPIO；
- 不把 Firmware Metadata 或 OTA 业务状态塞入 Raw Driver；
- 不提前实现 Device Manager / 通用 Storage/NVM 架构；
- 不在缺少真实硬件证据时声明 S03 PASS 或 CLOSED。
