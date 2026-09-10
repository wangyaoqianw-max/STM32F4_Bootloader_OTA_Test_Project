# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S00_Template_Restructure`
- Status: `READY_FOR_REVIEW`
- Branch: `main`
- Baseline Commit: `e9b71e4`
- Current Role: `Review`
- Updated At: `2026-09-10`

## Current Goal

将现有目录整理为以 RTOS 固件开发为核心、支持跨工具交接的通用工程模板。

## Next Action

由 Review Role 对照冻结设计、实施差异和验证报告审核本阶段，决定关闭或返工。

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

- 审核前不得将阶段状态改为 `CLOSED`。
- 不得在审核阶段顺便实现 Bootloader 或 Application 功能。
- 不得把聊天中的未确认结论直接写成冻结设计。
- 不得把代码验证描述为硬件验证。
