# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Last Closed Stage: `S02_External_Flash_Driver`
- Status: `CLOSED`
- Branch: `main`
- S02 Design Commit: `44fddb1484441a98d336df7783170267c166f4af`
- S02 Plan Commit: `aee30916c5c784828269668d99a2b4e63689f80e`
- S02 Merge Commit: `c6c77a240fce463afa4c86d797bb52d2781fb651`
- S02 Rework Commit: `42c02b891d7f32b728857c44024c3c91a15ea604`
- S02 Verification Commit: `c3527b3cd2fc10c3dd7fef89a0757f1199cd7c5d`
- S02 Final Review Commit: `b5e5e8a00ba4a9c08b5677d364883e3722bfa258`
- Next Stage: `S03_EEPROM_Storage`
- Next Stage Roadmap State: `PLANNED`
- Current Role: `Project Owner / Design Preparation`
- Updated At: `2026-09-12`

## Current Goal

`S01_Application_Foundation` 与 `S02_External_Flash_Driver` 均已关闭。

下一步准备 `S03_EEPROM_Storage`。S03 尚未正式启动，当前应先读取仓库现状、AT24C02 相关硬件资料与现有 Software I2C 实现，再讨论阶段设计；未获 Design Approval 前不进入生产代码施工。

## Stable Baseline from S02

后续阶段可以直接依赖以下已验证能力：

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

已稳定验证：

- Platform SPI 同步 `read()`；
- SPI1/SPI2 共用 Impl；
- SPI1 使用 PCLK2、SPI2 使用 PCLK1；
- W25Q64 JEDEC / SR1 / Read / Page Program / Cross-page Write / 4 KiB Sector Erase；
- WREN / WEL / BUSY；
- Page/Sector/地址边界保护；
- Raw Driver 不隐式 Erase；
- STM32 HAL 单次 `0xFFFF` Size 限制由 Impl 内部 chunking 吸收；
- Erase / Program / Cross-page / Boundary / Reset Persistence 真实板测 `PASS`；
- Review Finding 1 的 Host/Build/Hardware Regression 均 `PASS`；
- destructive test 已退出生产启动路径；
- SFUD 只完成边界评估，未实际集成。

S02 正式文档：

- Design: `00_Project/03_Stages/S02_External_Flash_Driver/design.md`
- Implementation Plan: `00_Project/03_Stages/S02_External_Flash_Driver/implementation_plan.md`
- Handoff: `00_Project/03_Stages/S02_External_Flash_Driver/handoff.md`
- Verification: `04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md`
- SFUD Evaluation: `00_Project/03_Stages/S02_External_Flash_Driver/sfud_evaluation.md`
- Final Review: `00_Project/03_Stages/S02_External_Flash_Driver/review.md`

## Reusable Tooling

保留跨阶段使用：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Config/toolchain.local.example.bat
```

`toolchain.local.bat` 为 machine-local 配置，当前不由 Git 跟踪。

## Next Stage Preparation — S03 EEPROM Storage

Roadmap 目标：建立掉电后可保存的小容量状态存储能力。

当前只作为 Design Preparation 输入，不在本文件预先冻结实现：

- AT24C02 Driver；
- Software I2C 复用边界；
- Byte/Page Read/Write；
- 跨页处理；
- EEPROM 写周期等待；
- 地址和容量边界；
- 基础 NVM 数据访问接口；
- Reset / 掉电数据保持验证。

正式设计必须以届时仓库代码和硬件资料为准。

## Required Reading for Next Conversation

1. `AGENTS.md`
2. `README.md`
3. `PROJECT_CONTEXT.md`
4. `00_Project/WORKFLOW.md`
5. `00_Project/02_Roadmap/development_roadmap.md`
6. `00_Project/03_Stages/S02_External_Flash_Driver/handoff.md`
7. `00_Project/03_Stages/S02_External_Flash_Driver/review.md`
8. AT24C02 / Software I2C 对应硬件与代码资料

## Next Action

进入 S03 Design Preparation：先检查当前仓库中的 AT24C02、Software I2C、Platform GPIO/Time 等可复用基础，再讨论并冻结 S03 Design。

## Prohibited Actions

- 不再向已关闭 S02 追加范围外功能；
- 不在 S03 Design Approval 前直接施工 EEPROM 生产代码；
- 不提前把 Firmware Metadata、OTA 状态机或 Bootloader 业务塞入 S03 EEPROM Driver；
- S03 具体接口和存储布局以正式设计为准，不由本上下文文件预先决定。
