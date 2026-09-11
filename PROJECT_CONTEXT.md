# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S00_Template_Restructure`
- Status: `CHANGES_REQUESTED`
- Branch: `main`
- Baseline Commit: `e9b71e4`
- Current Role: `Implementation`
- Updated At: `2026-09-11`

## Current Goal

将现有目录整理为以 RTOS 固件开发为核心、支持跨工具交接的通用工程模板。

## Next Action

按 `review.md` 的返工要求补充 Keil 构建输出和 I/O 故障诊断规范，完成验证后重新交回审核。

## Required Reading

1. `AGENTS.md`
2. `README.md`
3. `00_Project/WORKFLOW.md`
4. `00_Project/03_Stages/S00_Template_Restructure/design.md`
5. `00_Project/03_Stages/S00_Template_Restructure/implementation_plan.md`
6. `00_Project/03_Stages/S00_Template_Restructure/handoff.md`
7. `00_Project/05_Status/current_status.md`

## Blockers

无。

## Latest Verification

- Verification Commit: `015c2aa`
- 13 个核心文件存在且非空。
- 顶层目录、上下文字段、陈旧路径和 Git 差异检查全部通过。
- 完整报告：`04_Test/Reports/Stages/S00_Template_Restructure/verification_report.md`

## Prohibited Actions

- 返工验证完成前不得将阶段状态改为 `READY_FOR_REVIEW` 或 `CLOSED`。
- 不得在审核阶段顺便实现 Bootloader 或 Application 功能。
- 不得把聊天中的未确认结论直接写成冻结设计。
- 不得把代码验证描述为硬件验证。
