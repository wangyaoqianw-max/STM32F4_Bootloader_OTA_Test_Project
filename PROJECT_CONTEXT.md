# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S00_Template_Restructure`
- Status: `READY_FOR_REVIEW`
- Branch: `main`
- Baseline Commit: `e9b71e4`
- Current Role: `Review`
- Updated At: `2026-09-11`

## Current Goal

将现有目录整理为以 RTOS 固件开发为核心、支持跨工具交接的通用工程模板。

## Next Action

由 Review Role 复核 Keil 构建输出和 I/O 故障诊断返工，决定关闭或继续返工。

## Required Reading

1. `AGENTS.md`
2. `README.md`
3. `00_Project/WORKFLOW.md`
4. `00_Project/03_Stages/S00_Template_Restructure/design.md`
5. `00_Project/03_Stages/S00_Template_Restructure/implementation_plan.md`
6. `00_Project/03_Stages/S00_Template_Restructure/handoff.md`
7. `00_Project/03_Stages/S00_Template_Restructure/review.md`
8. `00_Project/05_Status/current_status.md`

## Blockers

无。

## Latest Verification

- Verification Commit: `762f6fa`
- Keil 规范相关 4 个文件存在且非空，9 项内容要求和 2 个入口检查通过。
- 3 个模拟构建输出路径的目录级忽略检查通过，未发现过宽的固件扩展名忽略规则。
- 完整报告：`04_Test/Reports/Stages/S00_Template_Restructure/verification_report.md`

## Prohibited Actions

- 复核前不得将阶段状态改为 `CLOSED`。
- 不得在审核阶段顺便实现 Bootloader 或 Application 功能。
- 不得把聊天中的未确认结论直接写成冻结设计。
- 不得把代码验证描述为硬件验证。
