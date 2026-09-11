# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S01_Application_Foundation`
- Status: `CLOSED`
- Branch: `main`
- Baseline Commit: `0a3f97493560fbd3ff3a220dc7d0a433e348a95e`
- Design Commit: `1345db1d5e6cb72a81cd30255bbfaa2d25d54bc2`
- Current Role: `Project Owner / Design`
- Updated At: `2026-09-11`

## Current Goal

`S01_Application_Foundation` 已完成并关闭。当前从 Application 基础工程建设切换到 `S02_External_Flash_Driver` 的设计讨论。

S01 已建立的稳定基线包括：

- `App -> Service -> Platform -> Impl -> Vendor` Application 分层；
- 收口后的基础 Config；
- STM32F411 当前板级 Status LED / Software I2C 绑定；
- FreeRTOS 基础运行环境；
- RTT + EasyLogger + Service Log；
- 最小 `app_main` 和 LED Blink；
- Keil `Objects/` / `Listings/` 构建输出规范；
- CubeMX regenerate、Clean/Rebuild、J-Link、LED、RTT 与连续 Reset 板测基线。

## Latest Review / Verification

- S01 Code Verification：`PASS`。
- S01 Hardware Verification：`PASS`。
- Keil Clean/Rebuild：0 Error、6 个已解释既有 Warning。
- CubeMX regenerate：`PASS`。
- J-Link、FreeRTOS、LED Blink、RTT + EasyLogger：`PASS`。
- Project Owner 连续 Reset 4 次：`PASS (4/4)`，满足至少 3 次稳定复现要求。
- S01 Review Result：`PASS`。
- S01 Status：`CLOSED`。
- 完整证据见 `04_Test/Reports/Stages/S01_Application_Foundation/verification.md` 和 `00_Project/03_Stages/S01_Application_Foundation/review.md`。

## Next Action

开始 `S02_External_Flash_Driver` Design Discussion，不直接编码：

1. 读取 W25Q64 Datasheet、开发板原理图/Pinout 与工程准备数据；
2. 检查当前 SPI2 CubeMX 配置和 SPI Platform/Impl 代码；
3. 讨论 W25Q64 Raw Driver 与 SFUD 的职责分工；
4. 定义 Read / Page Program / Sector Erase / JEDEC ID / Busy Wait / 地址边界 / 错误返回；
5. 设计跨页写入和板级验证方案；
6. 冻结 S02 设计后再创建实施计划并进入施工。

## Required Reading

1. `AGENTS.md`
2. `README.md`
3. `00_Project/WORKFLOW.md`
4. `00_Project/01_Requirements/项目需求V1.md`
5. `00_Project/02_Roadmap/development_roadmap.md`
6. `00_Project/03_Stages/S01_Application_Foundation/review.md`
7. `00_Project/03_Stages/S01_Application_Foundation/handoff.md`
8. `00_Project/05_Status/current_status.md`
9. `03_Firmware/AGENTS.md`
10. `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`
11. `03_Firmware/Application/OTA_APP/OTA_APP.ioc`
12. W25Q64 / SPI2 相关硬件资料和当前 SPI Platform/Impl 代码

## Blockers

无阻塞 S02 设计讨论的问题。

## Prohibited Actions

- S02 设计获批前，不直接开始 W25Q64/SFUD 功能实现。
- 不在 Raw External Flash Driver 阶段提前冻结 Firmware Image、A/B Slot、Metadata、Ymodem、OTA Service 或 Bootloader 安装策略。
- 不把 S01 已关闭结论重新解释为仍在施工。
- 不把 6 个已解释既有 Warning 误写为 S01 新增缺陷；后续按技术债务单独处理。
