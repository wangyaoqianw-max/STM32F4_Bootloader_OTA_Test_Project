# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S01_Application_Foundation`
- Status: `READY_FOR_VERIFICATION`
- Branch: `main`
- Baseline Commit: `0a3f97493560fbd3ff3a220dc7d0a433e348a95e`
- Design Commit: `1345db1d5e6cb72a81cd30255bbfaa2d25d54bc2`
- Current Role: `Verification`
- Updated At: `2026-09-11`

## Current Goal

按已批准的 `S01_Application_Foundation` 设计和实施计划，将当前 `OTA_APP` 收口为稳定、可复用的 STM32F411 Application 基础工程。

S01 已冻结范围：

- 清理 `00_Config` 中上一项目产品级参数；
- 对当前 CubeMX 工程适配 BSP/Impl；
- 接入 RTT + EasyLogger 与统一 Service Log；
- 保留 FreeRTOS 基础运行环境，不提前冻结最终 Task/IPC 架构；
- 建立最小 App 入口和 LED Blink 验证；
- 按仓库规范统一 Keil `Objects/`、`Listings/` 构建输出；
- 完成 CubeMX regenerate、Clean Rebuild、J-Link、LED 和 RTT 板级验证。

## Current Design Baseline

Project Owner 已批准：

- `00_Project/03_Stages/S01_Application_Foundation/design.md`
- `00_Project/03_Stages/S01_Application_Foundation/implementation_plan.md`

批准的设计基线 Commit：`1345db1d5e6cb72a81cd30255bbfaa2d25d54bc2`。

核心约束：

- Application 继续使用 `App -> Service -> Platform -> Impl -> Vendor`；
- 主要代码适配范围为 `00_Config`、`04_Impl/impl_bsp` 和必要的工程接线；
- Platform/Service/Vendor 以复用和验证为主，不做无关重构；
- 当前 CubeMX 已启用的 FreeRTOS/UART/DMA/SPI 等视为个人常用基础能力，不代表后续模块参数已经冻结；
- S01 不实现 W25Q64、AT24C02、Ymodem、OTA Service、LCD UI、Bootloader 或 Security；
- 日常 Build Artifact 进入 `MDK-ARM/Objects/` 与 `MDK-ARM/Listings/`，`06_Output/` 只用于需要交付、打包或临时导出的制品。

## Next Action


Task 1–5 的实现、代码验证和硬件板测已完成，Task 1–4 实现提交为 `35af9fb`、`95231a1`、`52c51d4`、`dea56a9`，验证报告提交为 `f9497ac`，最终交接同步为 `06ddb9f`。下一步由 Verification Role / Project Owner 完成 CubeMX regenerate 和 Listings 输出确认。

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
10. `03_Firmware/AGENTS.md`
11. `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`
12. `03_Firmware/00_Doc/Standards/Keil工程与构建输出规范.md`
13. `03_Firmware/Application/OTA_APP/OTA_APP.ioc`
14. `03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx`

## Blockers

无阻塞代码实施的问题；J-Link、FreeRTOS、LED 和 RTT 硬件验证已通过，CubeMX regenerate 和 Listings 稳定输出仍待完成。

## Latest Review / Verification

- `S00_Template_Restructure` 已 `CLOSED`。
- S01 Design 已由 Project Owner 批准。
- S01 当前状态为 `READY_FOR_VERIFICATION`。
- Keil Clean/Rebuild 已完成：0 Error、6 Warning，AXF/HEX/MAP 已生成。
- UV4 命令行未稳定输出 `MDK-ARM/Listings/`；CubeMX regenerate 仍为 PENDING。
- 用户已提供板测结论和 RTT Viewer 截图：LED 正常闪烁，J-Link 烧录、FreeRTOS 启动、RTT + EasyLogger 输出均记录为 PASS。
- 详细证据见 `04_Test/Reports/Stages/S01_Application_Foundation/verification.md`。

## Prohibited Actions

- 不提前实现 S02+ 的 W25Q64、EEPROM、Firmware Image、Ymodem、OTA、Bootloader、Rollback 或 Security。
- 不为了兼容复制进来的旧模块，把上一项目产品配置继续保留在基础 Config 中。
- 不大规模重构稳定 Platform/Impl 接口。
- 不修改 Vendor 原始库以规避工程接线错误。
- 不把未确认的硬件事实写成 `CONFIRMED`。
