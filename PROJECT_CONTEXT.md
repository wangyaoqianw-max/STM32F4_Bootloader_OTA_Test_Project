# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S00_Template_Restructure`
- Status: `CLOSED`
- Branch: `main`
- Planning Handoff Baseline: `b094d3a`
- Current Role: `Project Owner / Design`
- Updated At: `2026-09-11`

## Current Goal

从“通用模板建设 + 工程准备”切换到 STM32F411 Bootloader/OTA 项目的正式项目规划。

下一步先做项目级任务拆分，不直接编码：根据 V1 需求和当前已确认硬件输入拆分正式开发 Stage，明确学习目标、交付物、前置依赖和验收条件，并确定第一个正式功能 Stage。

## Current Planning Baseline

当前资料已经足以进入项目规划，不要求 Preparation 阶段所有字段 100% 完成。

已确认的关键输入：

- MCU：STM32F411CEU6，Internal Flash 512 KB；
- External SPI NOR：W25Q64JVSSIQ，SPI2，3.3 V；
- EEPROM：AT24C02，Software I2C，PB6/PB7，3.3 V，7-bit Address `0x50`，WP 接 GND；
- Debug：J-Link + SWD；日志方向为 SEGGER RTT + EasyLogger；
- 项目主线：Boot Process、Flash Management、Firmware Transfer/Storage、Image Validation、OTA State Machine、Watchdog、Rollback/Recovery、Security Basics、Logging/Diagnostics。

仍存在的开放资料项按“需要时补充”处理，不阻塞当前规划：CK02AT Datasheet/API、Ymodem 原始协议资料、HC-05 实测参数、LCD/CTP 细节、部分工具链精确版本。

## Next Action

开启新的 Design Discussion，执行以下顺序：

1. 读取需求、工程准备数据和当前路线图；
2. 将完整 Bootloader/OTA 项目拆分为若干可独立验收的正式 Stage；
3. 确定每个 Stage 的学习目标、工程输出、依赖和完成标准；
4. 判断哪些开放资料必须前置、哪些可以延后；
5. 选定第一个正式功能 Stage；
6. 再从 `00_Project/03_Stages/_Template` 创建该 Stage 的 `design.md`、`implementation_plan.md`、`handoff.md` 和 `review.md`。

## Required Reading

1. `AGENTS.md`
2. `README.md`
3. `00_Project/WORKFLOW.md`
4. `00_Project/01_Requirements/项目需求V1.md`
5. `00_Project/00_Preparation/README.md`
6. `00_Project/00_Preparation/Engineering_Preparation_Bilingual.xlsx`
7. `00_Project/02_Roadmap/development_roadmap.md`
8. `00_Project/03_Stages/S00_Template_Restructure/handoff.md`
9. `00_Project/03_Stages/S00_Template_Restructure/review.md`
10. `00_Project/05_Status/current_status.md`

## Blockers

无阻塞项目级规划的问题。

## Latest Review / Verification

- `S00_Template_Restructure` 已通过最终审核并 `CLOSED`。
- Closing Review Commit: `9968c8b`。
- 模板结构与 Keil 构建规范复验已经通过，Verification Commit: `762f6fa`。
- 工程准备阶段已经能够承载当前 STM32F411 Bootloader/OTA 项目的真实资料输入。
- 当前尚未进行任何新的 Bootloader/OTA 功能代码实现，因此不存在可声明的功能级硬件验证结果。

## Prohibited Actions

- 在项目级 Stage 拆分和第一个 Stage 设计获批前，不直接开始功能代码实现。
- 不提前冻结未经讨论的 Internal/External Flash 软件分区、Firmware Metadata、OTA 状态机或 Rollback 策略。
- 不把 CK02AT、LCD 等当前非阻塞开放项升级成首个 Stage 的强制前置条件，除非新的设计讨论明确要求。
- 不把未确认的硬件事实写成 `CONFIRMED`；缺失资料在对应 Stage 需要时继续补充。
