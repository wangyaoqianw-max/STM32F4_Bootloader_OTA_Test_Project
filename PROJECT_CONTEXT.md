# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S00_Template_Restructure`
- Status: `IN_PROGRESS`
- Branch: `main`
- Baseline Commit: `e9b71e4`
- Current Role: `Implementation`
- Updated At: `2026-09-10`

## Current Goal

将现有目录整理为以 RTOS 固件开发为核心、支持跨工具交接的通用工程模板。

## Next Action

继续执行当前阶段实施计划，从上下文合同完成后进入两级 `AGENTS.md` 拆分。

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

- 三份原有非空工程文档已经迁移且内容存在。
- 迁移文档已规范化为 LF 行尾。
- 陈旧路径初步扫描无匹配。

## Prohibited Actions

- 不得删除尚未确认归属的非空文件。
- 不得把聊天中的未确认结论直接写成冻结设计。
- 不得把代码验证描述为硬件验证。
- 不得在未更新交接文档和 Commit 信息时切换执行工具。
