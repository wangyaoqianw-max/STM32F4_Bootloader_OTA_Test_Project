# S00 Template Restructure Handoff

## Metadata

- Stage: `S00_Template_Restructure`
- Status: `IN_PROGRESS`
- Branch: `main`
- Design Commit: `2c696d4`
- Baseline Commit: `e9b71e4`
- Implementation Commit: `Not created yet`
- Verification Commit: `Not created yet`

## Implementation Input

### Goal

将原始工程骨架整理为以 RTOS 固件开发为核心、支持跨工具接续的通用模板。

### Required Reading

1. `PROJECT_CONTEXT.md`
2. `00_Project/03_Stages/S00_Template_Restructure/design.md`
3. `00_Project/03_Stages/S00_Template_Restructure/implementation_plan.md`

### Allowed Changes

- 按冻结设计迁移现有文档；
- 重写根目录导航和 Agent 规则；
- 创建阶段、测试、工具和输出目录模板；
- 删除已经确认由新结构替代的空目录和空占位文件。

### Prohibited Changes

- 不得丢失任何原有非空文档内容；
- 不得修改 STM32F411 OTA 项目的具体功能需求；
- 不得开始实现 Bootloader 或 Application 代码；
- 不得将工具名称写成不可替换的强制角色。

### Acceptance Criteria

以 `design.md` 第 12 节的八项验收条件为准。

### Required Verification

- 必需文件存在性检查；
- 陈旧路径扫描；
- 上下文字段一致性检查；
- Git 差异与工作区检查。

## Implementation Output

- Status: `IN_PROGRESS`

### Completed Work

- 已迁移项目需求、固件架构和嵌入式 C 代码规范。
- 已规范化迁移文档的 LF 行尾。

### Changed Files

参见实施完成后的最终文件清单。

### Deviations From Plan

- 用户明确授权直接在 `main` 分支施工，因此未创建隔离工作树。
- 源文档存在 Git 尾随空白检查问题，增加了一次 LF 行尾规范化提交。

### Verification Results

Task 1 文件存在性和陈旧路径扫描通过；全量结构验证尚未执行。

### Known Issues

旧目录尚未清理，两级 Agent 规则尚未建立。

### Review Focus

确认最终目录与冻结设计一致，并检查任意工具能否从 `PROJECT_CONTEXT.md` 恢复当前阶段。
