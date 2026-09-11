# S00 Template Restructure Handoff

## Metadata

- Stage: `S00_Template_Restructure`
- Status: `READY_FOR_REVIEW`
- Branch: `main`
- Design Commit: `2c696d4`
- Baseline Commit: `e9b71e4`
- Implementation Commits: `91ad2df`, `9a059a2`, `1c81bb9`, `878d0a1`, `7bee8e5`, `eaa64b8`
- Review Feedback Commit: `894ac40`
- Verification Commits: `015c2aa`, `762f6fa`

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

- Status: `COMPLETED`

### Completed Work

- 已迁移项目需求、固件架构和嵌入式 C 代码规范，并修正架构适用范围。
- 已规范化迁移文档的 LF 行尾。
- 已建立 `PROJECT_CONTEXT.md`、阶段状态机、路线图、ADR 约定和四份阶段模板。
- 已将原根 Agent 规则拆分为仓库级 `AGENTS.md` 与 `03_Firmware/AGENTS.md`。
- 已建立 Reference、Hardware、Firmware、Test、Tools 和 Output 精简骨架。
- 已删除确认无内容的旧项目管理、Function Map、Software、Mechanical、FTC、旧 Doc 和旧命名空目录。
- 已根据审核反馈增加适配当前目录的 Keil 构建输出管理与 I/O 故障诊断规范。
- 已在固件 Agent 规则和固件 README 中建立规范入口，保留根 `.gitignore` 的目录级忽略策略。

### Changed Files

- 根入口：`README.md`、`PROJECT_CONTEXT.md`、`AGENTS.md`、`.gitignore`；
- 项目上下文：`00_Project`；
- 参考与硬件输入：`01_Reference`、`02_Hardware`；
- 固件规则和文档：`03_Firmware`；
- 测试、工具和生成物说明：`04_Test`、`05_Tools`、`06_Output`。

### Deviations From Plan

- 用户明确授权直接在 `main` 分支施工，因此未创建隔离工作树。
- 源文档的 CRLF 行尾触发 Git 尾随空白检查，增加 `9a059a2` 进行 LF 规范化。
- 为避免验证报告自引用无法产生的 Commit，先提交报告 `015c2aa`，再提交本交接状态。
- 外部 Zettelkasten 文档仅作为审核参考；新规范已移除旧工程根目录、项目内 Tests 和项目内 Release 等不适配内容，仓库运行不依赖外部文件。
- 未加入仓库级 `*.hex`、`*.bin`、`*.map` 忽略规则，以便通过 `git status` 暴露错误输出位置。

### Verification Results

原全量结构验证与 Keil 规范返工复验均通过。代码验证和硬件验证均为 `NOT_APPLICABLE`；详见 `04_Test/Reports/Stages/S00_Template_Restructure/verification_report.md`。

### Known Issues

- Keil 规范返工等待 Review Role 复核。
- 远程仓库推送状态尚未验证。

### Review Focus

- 检查 `PROJECT_CONTEXT.md` 是否足以让新工具恢复当前阶段；
- 检查两级 Agent 规则是否职责清晰且无关键规则遗漏；
- 检查阶段模板是否足以支持设计、施工、验证和审核闭环；
- 检查 Keil 规范是否正确区分工程输入、构建输出、正式制品和 I/O 故障；
- 检查目录级忽略策略是否既排除正常输出，又能暴露错误输出位置；
- 确认是否接受在 `main` 上产生的本阶段提交序列。
