# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S01_Application_Foundation`
- Status: `DRAFT`
- Branch: `main`
- Baseline Commit: `0a3f97493560fbd3ff3a220dc7d0a433e348a95e`
- Design Commit: `1345db1d5e6cb72a81cd30255bbfaa2d25d54bc2`
- Current Role: `Project Owner / Design`
- Updated At: `2026-09-11`

## Current Goal

完成 `S01_Application_Foundation` 设计审核，将当前已经创建并复制部分通用代码的 `OTA_APP` 收口为稳定、可复用的 STM32F411 Application 基础工程。

S01 重点包括：

- 清理 `00_Config` 中上一项目产品级参数；
- 对当前 CubeMX 工程适配 BSP/Impl；
- 接入 RTT + EasyLogger 与统一 Service Log；
- 保留 FreeRTOS 基础运行环境，不提前冻结最终 Task/IPC 架构；
- 建立最小 App 入口和 LED Blink 验证；
- 按仓库规范统一 Keil `Objects/`、`Listings/` 构建输出；
- 完成 CubeMX regenerate、Clean Rebuild、J-Link、LED 和 RTT 板级验证。

## Current Design Baseline

S01 正式阶段文档已创建：

- `00_Project/03_Stages/S01_Application_Foundation/design.md`
- `00_Project/03_Stages/S01_Application_Foundation/implementation_plan.md`
- `00_Project/03_Stages/S01_Application_Foundation/handoff.md`
- `00_Project/03_Stages/S01_Application_Foundation/review.md`

当前设计结论：

- Application 继续使用 `App -> Service -> Platform -> Impl -> Vendor`；
- 主要代码适配范围为 `00_Config`、`04_Impl/impl_bsp` 和必要的工程接线；
- Platform/Service/Vendor 以复用和验证为主，不做无关重构；
- 当前 CubeMX 已启用的 FreeRTOS/UART/DMA/SPI 等视为个人常用基础能力，不代表后续模块参数已经冻结；
- S01 不实现 W25Q64、AT24C02、Ymodem、OTA Service、LCD UI、Bootloader 或 Security；
- 日常 Build Artifact 进入 `MDK-ARM/Objects/` 与 `MDK-ARM/Listings/`，`06_Output/` 只用于需要交付、打包或临时导出的制品。

## Next Action

Project Owner 审核：

1. `S01_Application_Foundation/design.md`；
2. `S01_Application_Foundation/implementation_plan.md`；
3. 确认范围、验收条件和构建输出边界；
4. 审核通过后将状态推进到 `DESIGN_APPROVED` / `READY_FOR_IMPLEMENTATION`；
5. 再交由 Implementation Role 按计划施工。

## Required Reading

1. `AGENTS.md`
2. `README.md`
3. `00_Project/WORKFLOW.md`
4. `00_Project/01_Requirements/项目需求V1.md`
5. `00_Project/02_Roadmap/development_roadmap.md`
6. `00_Project/03_Stages/S01_Application_Foundation/design.md`
7. `00_Project/03_Stages/S01_Application_Foundation/implementation_plan.md`
8. `00_Project/03_Stages/S01_Application_Foundation/handoff.md`
9. `00_Project/05_Status/current_status.md`
10. `03_Firmware/00_Doc/Standards/Keil工程与构建输出规范.md`

## Blockers

无阻塞 S01 设计审核的问题。

## Latest Review / Verification

- `S00_Template_Restructure` 已 `CLOSED`。
- S01 尚未开始实现或功能级硬件验证。
- 当前只完成 S01 Design/Plan 建档，不得把文档创建视为实现完成。

## Prohibited Actions

- S01 设计获批前，不修改生产代码进入正式施工。
- 不提前实现 S02+ 的 W25Q64、EEPROM、Firmware Image、Ymodem、OTA、Bootloader、Rollback 或 Security。
- 不为了兼容复制进来的旧模块，把上一项目产品配置继续保留在基础 Config 中。
- 不把未确认的硬件事实写成 `CONFIRMED`。
