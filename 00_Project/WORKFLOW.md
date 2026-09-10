# 阶段式工程工作流

## 1. 目的

本工作流通过仓库内的固定文档和 Git Commit 保存工程上下文，使人工、网页版 AI、本地 Agent 或其他工具能够在切换后可靠继续工程。

工作流只定义角色、动作和交付物，不绑定具体工具。

## 2. 正式上下文

以下内容构成正式工程上下文：

```text
PROJECT_CONTEXT.md
项目需求
开发路线图
当前阶段 design.md
当前阶段 implementation_plan.md
当前阶段 handoff.md
当前阶段 review.md
ADR
验证报告
Git Commit
```

未写入仓库并提交的聊天结论不视为正式工程决定。

## 3. 阶段文件

每个阶段保存在 `00_Project/03_Stages/<stage_id>_<topic>/`，并包含：

- `design.md`：阶段目标、边界、方案和验收条件；
- `implementation_plan.md`：可执行、可验证的施工步骤；
- `handoff.md`：上游施工输入和下游施工输出；
- `review.md`：审核结论、返工要求和关闭决定。

阶段关闭后目录不移动、不改名。需要修订时通过新的 Git Commit 保留历史。

## 4. 状态机

正常状态转换：

```text
DRAFT
→ DESIGN_APPROVED
→ READY_FOR_IMPLEMENTATION
→ IN_PROGRESS
→ READY_FOR_VERIFICATION
→ READY_FOR_REVIEW
→ CLOSED
```

异常状态转换：

```text
DRAFT / DESIGN_APPROVED / READY_FOR_IMPLEMENTATION / IN_PROGRESS
→ BLOCKED

READY_FOR_REVIEW
→ CHANGES_REQUESTED
→ IN_PROGRESS
```

`00_Project/05_Status/current_status.md` 保存唯一活动阶段状态。`PROJECT_CONTEXT.md` 必须与其保持一致。

## 5. 角色合同

### 5.1 Design Role

进入条件：项目需求和上游约束已经明确。

必须读取：

- `PROJECT_CONTEXT.md`；
- 项目需求；
- 开发路线图；
- 相关架构、接口和 ADR；
- 上一阶段交接与审核结果。

允许修改：阶段 `design.md`、`implementation_plan.md`、`handoff.md` 的施工输入，以及必要的需求和 ADR。

禁止修改：未获授权的生产代码、验证结论和已关闭阶段的历史结论。

输出：冻结设计、实施计划、施工输入、设计基准 Commit。

退出条件：Project Owner 批准设计，状态进入 `READY_FOR_IMPLEMENTATION`。

### 5.2 Implementation Role

进入条件：状态为 `READY_FOR_IMPLEMENTATION` 或 `CHANGES_REQUESTED`。

必须读取：

```text
AGENTS.md
→ README.md
→ PROJECT_CONTEXT.md
→ design.md
→ implementation_plan.md
→ handoff.md
→ 目标目录 AGENTS.md
→ 相关文档和代码
```

允许修改：计划明确列出的代码、测试、文档、施工输出和状态文件。

禁止修改：冻结设计、范围外模块、Vendor 原始代码，以及未经确认的硬件事实。

输出：实现、必要测试、文档同步、施工 Commit 和 `handoff.md` 施工输出。

退出条件：计划内实现完成，状态进入 `READY_FOR_VERIFICATION`；设计冲突或缺失信息导致无法继续时进入 `BLOCKED`。

### 5.3 Verification Role

进入条件：状态为 `READY_FOR_VERIFICATION`。

必须读取：阶段验收条件、实现提交、测试方案和施工交接。

允许修改：`04_Test/Reports/Stages/<stage>/` 下的验证报告、必要的测试代码以及验证状态。

禁止修改：通过调整验收条件掩盖实现问题，或用编译结果替代硬件验证结果。

输出：编译、静态检查、Host Test、Board Test 或集成测试证据，并明确区分代码验证与硬件验证。

退出条件：验证证据完整时进入 `READY_FOR_REVIEW`；验证失败时记录问题并回到 `IN_PROGRESS` 或进入 `BLOCKED`。

### 5.4 Review Role

进入条件：状态为 `READY_FOR_REVIEW`。

必须读取：需求、冻结设计、实施计划、代码差异、施工交接和验证报告。

允许修改：阶段 `review.md`、当前状态、必要的返工说明。

禁止修改：在缺少验证证据时直接关闭阶段。

输出：通过、返工或阻塞结论，以及对应 Commit。

退出条件：通过时进入 `CLOSED`；有明确问题时进入 `CHANGES_REQUESTED`；缺少外部信息时进入 `BLOCKED`。

### 5.5 Project Owner

Project Owner 负责：

- 确认需求和设计；
- 解决范围与硬件事实冲突；
- 确认真实硬件验收；
- 批准不可逆或超范围操作；
- 最终关闭阶段并决定下一阶段。

## 6. 工具切换协议

### 离开当前工具前

1. 将已确认结论写入正式文档。
2. 更新当前阶段 `handoff.md`。
3. 同步更新 `PROJECT_CONTEXT.md` 和 `current_status.md`。
4. 记录当前分支和 Commit。
5. 提交并推送远程仓库。

### 进入下一个工具后

1. 拉取远程最新版本。
2. 核对分支、工作区和 Commit。
3. 读取 `PROJECT_CONTEXT.md`。
4. 按其中的 Required Reading 顺序恢复上下文。
5. 在修改前确认当前目标、范围、状态和下一步。
6. 实际 `HEAD` 与交接记录不一致时，先检查差异，不直接施工。

## 7. Commit 约定

阶段交接至少记录：

- Design Commit；
- Baseline Commit；
- Implementation Commit；
- Verification Commit；
- Review Commit。

尚未产生的 Commit 明确写 `Not created yet`，不得预填或猜测哈希。

## 8. 上下文维护约束

- `PROJECT_CONTEXT.md` 只保存当前入口和摘要，不复制详细设计。
- `current_status.md` 只保存唯一活动阶段状态，不追加历史流水账。
- `handoff.md` 保存工具切换所需的输入和输出，不复制完整日志。
- `review.md` 保存审核结论，不承担施工记录职责。
- 跨阶段长期有效的技术决定写入 ADR。
- 完整测试证据写入 `04_Test/Reports`，交接文件只引用报告。
