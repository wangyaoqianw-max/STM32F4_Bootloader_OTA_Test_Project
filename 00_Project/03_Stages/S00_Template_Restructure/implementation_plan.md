# 嵌入式软件工程模板重组实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将现有工程骨架整理为以 RTOS 固件开发为核心、通过 Git 仓库保存跨工具上下文的通用模板。

**Architecture:** 仓库根目录提供人工入口、通用上下文入口和 Agent 规则入口；`00_Project` 保存需求、路线图、阶段、决策和当前状态；固件、测试、工具和输出分别拥有独立职责。每个阶段通过设计、实施计划、交接和审核四份文档闭环，Git Commit 作为状态锚点。

**Tech Stack:** Markdown、Git、PowerShell、Codex `AGENTS.md`、STM32/RTOS 工程目录约定。

**Spec:** `00_Project/03_Stages/S00_Template_Restructure/design.md`

## Global Constraints

- 仓库是工程上下文的唯一正式载体。
- 未写入仓库并提交的聊天结论不视为正式工程决定。
- `PROJECT_CONTEXT.md` 是所有工具恢复当前上下文的统一入口。
- 工作流使用角色名称，不绑定具体 AI 产品。
- Application 是默认主工程，Bootloader 按项目需要启用。
- 默认面向 RTOS 项目，不为纯裸机场景增加额外结构。
- 所有现有非空文档必须保留内容或将有效规则迁入明确的新位置。
- 不修改 `.git` 内部文件，不提交生成日志、构建缓存或固件二进制。
- 每个任务结束后只提交该任务职责范围内的变更。

---

## 目标文件职责

### 根目录入口

- `README.md`：模板定位、目录导航、初始化步骤和当前推荐工作方式。
- `PROJECT_CONTEXT.md`：当前阶段、状态、分支、Commit、下一步和必读文件。
- `AGENTS.md`：仓库级长期规则、上下文恢复流程和阶段门禁。
- `.gitignore`：忽略 `06_Output` 生成物、IDE 缓存、构建输出和临时日志，同时保留说明文件。

### 项目上下文

- `00_Project/WORKFLOW.md`：角色、状态、允许动作、退出条件和工具切换协议。
- `00_Project/01_Requirements/项目需求V1.md`：保留当前 STM32F411 OTA 项目需求。
- `00_Project/01_Requirements/README.md`：说明需求文档的职责和冻结方式。
- `00_Project/02_Roadmap/development_roadmap.md`：长期阶段路线图模板。
- `00_Project/03_Stages/_Template/*.md`：新阶段的四份标准模板。
- `00_Project/03_Stages/S00_Template_Restructure/*.md`：本次目录重组阶段记录。
- `00_Project/04_Decisions/README.md`：ADR 命名和内容约定。
- `00_Project/05_Status/current_status.md`：唯一活动阶段状态。

### 固件与验证

- `03_Firmware/AGENTS.md`：固件专项规则。
- `03_Firmware/README.md`：Application、Bootloader、Shared 和固件文档导航。
- `03_Firmware/00_Doc/Architecture/固件软件架构.md`：通用 RTOS 固件分层说明。
- `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`：详细 C 代码规范唯一正文。
- `04_Test/Reports/Stages/S00_Template_Restructure/verification_report.md`：本阶段结构验证证据。

---

### Task 1: 安全盘点并迁移现有非空文档

**Files:**

- Move: `00_Doc/01_项目需求/项目需求V1.md` → `00_Project/01_Requirements/项目需求V1.md`
- Move: `00_Doc/03_架构设计/嵌入式项目C代码设计规范.md` → `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`
- Move: `00_Doc/03_架构设计/软件架构说明.md` → `03_Firmware/00_Doc/Architecture/固件软件架构.md`
- Preserve: `00_Project/03_Stages/S00_Template_Restructure/design.md`
- Preserve: `00_Project/03_Stages/S00_Template_Restructure/implementation_plan.md`

**Interfaces:**

- Consumes: 当前仓库文件清单和冻结设计。
- Produces: 三份内容完整、路径明确的正式工程文档，供后续入口文件引用。

- [x] **Step 1: 核对所有现有非空文件**

运行：

```powershell
Get-ChildItem -File -Recurse -Force |
    Where-Object { $_.FullName -notmatch '\\.git(\\|$)' -and $_.Length -gt 1 } |
    Select-Object FullName, Length
```

预期：除已提交的阶段设计和计划外，至少识别出根目录 `README.md`、`AGENTS.md` 以及上述三份需要迁移的非空文档和 `execution_rules.md`。发现其他非空文件时，先为其确定归属，不得删除。

- [x] **Step 2: 建立迁移目标父目录**

创建：

```text
00_Project/01_Requirements/
03_Firmware/00_Doc/Architecture/
03_Firmware/00_Doc/Standards/
```

预期：目标目录均位于仓库根目录内，不覆盖同名文件。

- [x] **Step 3: 逐个移动三份正式文档**

使用 PowerShell `Move-Item -LiteralPath` 逐个移动，不进行递归批量移动。移动前后分别检查源路径和目标路径。

预期：三个源文件不存在，三个目标文件存在且长度大于 100 字节。

- [x] **Step 4: 校验迁移内容未丢失**

运行：

```powershell
Get-Item `
    '.\00_Project\01_Requirements\项目需求V1.md', `
    '.\03_Firmware\00_Doc\Architecture\固件软件架构.md', `
    '.\03_Firmware\00_Doc\Standards\嵌入式C代码规范.md' |
    Select-Object FullName, Length
```

预期：三份文件均存在，需求文档、架构文档和代码规范分别保持原有主体内容。

- [x] **Step 5: 修正迁移文档中的旧路径和模板限定**

修改：

- `固件软件架构.md`：将目录示例调整为 `Application` 默认、`Bootloader` 可选的 RTOS 通用结构。
- `嵌入式C代码规范.md`：正文保留为唯一详细规范，不复制到 Agent 文件。
- 所有文档：删除 `00_Doc/02_架构设计` 等不存在的旧路径引用。

验证：

```powershell
rg -n "00_Doc/02_架构设计|00_Doc\\02_架构设计|04_Agent" `
    00_Project/01_Requirements 03_Firmware/00_Doc
```

预期：无匹配。

- [x] **Step 6: 提交文档迁移**

```powershell
git add -- 00_Project/01_Requirements 03_Firmware/00_Doc
git diff --cached --check
git commit -m "docs: relocate requirements and firmware guidance"
```

预期：提交只包含三份正式文档的迁移及必要内容修订。

---

### Task 2: 建立工具无关的项目上下文合同

**Files:**

- Create: `PROJECT_CONTEXT.md`
- Create: `00_Project/WORKFLOW.md`
- Create: `00_Project/01_Requirements/README.md`
- Create: `00_Project/02_Roadmap/development_roadmap.md`
- Create: `00_Project/03_Stages/_Template/design.md`
- Create: `00_Project/03_Stages/_Template/implementation_plan.md`
- Create: `00_Project/03_Stages/_Template/handoff.md`
- Create: `00_Project/03_Stages/_Template/review.md`
- Create: `00_Project/03_Stages/S00_Template_Restructure/handoff.md`
- Create: `00_Project/03_Stages/S00_Template_Restructure/review.md`
- Create: `00_Project/04_Decisions/README.md`
- Create: `00_Project/05_Status/current_status.md`

**Interfaces:**

- Consumes: 冻结设计、迁移后的正式需求和阶段目录。
- Produces: 任意工具均可读取的确定入口、阶段状态机和双向交接合同。

- [x] **Step 1: 编写 `PROJECT_CONTEXT.md`**

必须包含以下固定字段：

```text
Active Stage
Status
Branch
Baseline Commit
Current Role
Updated At
Current Goal
Next Action
Required Reading
Blockers
Latest Verification
Prohibited Actions
```

本阶段值设为 `S00_Template_Restructure`，状态设为 `IN_PROGRESS`，下一步指向当前实施计划。Commit 尚未产生的字段明确写 `Not created yet`，不得伪造哈希。

- [x] **Step 2: 编写 `00_Project/WORKFLOW.md`**

完整定义以下状态及合法转换：

```text
DRAFT
DESIGN_APPROVED
READY_FOR_IMPLEMENTATION
IN_PROGRESS
READY_FOR_VERIFICATION
READY_FOR_REVIEW
CHANGES_REQUESTED
BLOCKED
CLOSED
```

为 Design、Implementation、Verification、Review、Project Owner 五种角色分别定义进入条件、必读文件、允许修改、禁止修改、输出和退出条件。

- [x] **Step 3: 编写阶段模板**

四份模板使用明确的模板字段，例如 `{{stage_id}}`、`{{stage_title}}`、`{{baseline_commit}}`。模板必须覆盖：

- 设计边界和验收条件；
- 可验证的施工步骤；
- 施工输入和施工输出；
- 审核结论和返工要求。

模板中不得包含模糊的“适当处理”“自行补充”等指令。

- [x] **Step 4: 编写路线图、需求和 ADR 说明**

- `development_roadmap.md` 提供阶段编号、目标、前置条件、状态和完成标准的表格。
- 需求目录 `README.md` 说明需求是设计和验收的上游真值。
- ADR `README.md` 规定文件名 `ADR-NNNN-topic.md`，以及 Context、Decision、Consequences、Status 四个固定部分。

- [x] **Step 5: 初始化本阶段交接和审核文件**

- `handoff.md` 填入本阶段目标、设计基准、允许修改范围、禁止事项和验收条件；施工输出保持 `NOT_COMPLETED`。
- `review.md` 状态设为 `NOT_REVIEWED`，说明只有进入 `READY_FOR_REVIEW` 后才能填写审核结论。

- [x] **Step 6: 编写唯一活动状态文件**

`current_status.md` 必须与 `PROJECT_CONTEXT.md` 一致，记录当前阶段为 `S00_Template_Restructure`、状态为 `IN_PROGRESS`，并链接到本阶段四份文档。

- [x] **Step 7: 验证上下文合同完整性**

运行：

```powershell
$required = @(
    'PROJECT_CONTEXT.md',
    '00_Project/WORKFLOW.md',
    '00_Project/03_Stages/_Template/design.md',
    '00_Project/03_Stages/_Template/implementation_plan.md',
    '00_Project/03_Stages/_Template/handoff.md',
    '00_Project/03_Stages/_Template/review.md',
    '00_Project/05_Status/current_status.md'
)
$required | ForEach-Object {
    if (-not (Test-Path -LiteralPath $_)) { throw "Missing required file: $_" }
}
```

预期：命令无异常退出。

- [x] **Step 8: 提交上下文工作流**

```powershell
git add -- PROJECT_CONTEXT.md 00_Project
git diff --cached --check
git commit -m "docs: add repository context workflow"
```

预期：提交包含上下文入口、工作流、阶段模板和项目状态，不包含固件代码。

---

### Task 3: 拆分仓库级与固件级 Agent 规则

**Files:**

- Modify: `AGENTS.md`
- Create: `03_Firmware/AGENTS.md`
- Read: `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`
- Remove after migration: `00_Doc/04_Agent/execution_rules.md`

**Interfaces:**

- Consumes: 原根目录 `AGENTS.md`、旧 `execution_rules.md`、详细 C 规范和工作流。
- Produces: 简短的仓库级规则，以及聚焦固件架构与实时性约束的专项规则。

- [x] **Step 1: 重写根目录 `AGENTS.md`**

根文件必须覆盖：

```text
模板定位和语言
指令优先级
PROJECT_CONTEXT 上下文恢复流程
阶段状态门禁
修改范围和安全边界
设计、实施、验证和审核职责
文档同步要求
代码验证与硬件验证区分
Git 提交边界
固件任务的专项规则入口
```

根文件不得重复 ISR、DMA、RingBuffer、FreeRTOS IPC 或具体 C 命名规则全文。

- [x] **Step 2: 创建 `03_Firmware/AGENTS.md`**

固件规则必须覆盖：

```text
APP → Service → Platform → Impl → Vendor/HAL 依赖方向
Application、Bootloader、Shared 的边界
CubeMX USER CODE 区规则
Vendor 不直接修改原则
ISR、DMA、RingBuffer 和 RTOS 并发边界
静态内存、错误处理和日志要求
详细 C 规范读取门禁
Coding Standard Review
编译验证和真实硬件验证区分
```

详细格式和命名只链接到 `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`。

- [x] **Step 3: 检查规则冲突和重复**

运行：

```powershell
rg -n "00_Doc/02_架构设计|00_Doc/04_Agent|04_Agent" AGENTS.md 03_Firmware/AGENTS.md
```

预期：无匹配。

检查两个 `AGENTS.md` 的职责：根文件负责跨仓库流程，固件文件负责嵌入式开发约束；同一详细章节不得在两处重复。

- [x] **Step 4: 删除已完成规则迁移的旧文件**

删除前确认 `execution_rules.md` 的所有有效要求已分别进入两个 `AGENTS.md` 或详细 C 规范。然后仅删除：

```text
00_Doc/04_Agent/execution_rules.md
```

预期：不删除其他非空文件。

- [x] **Step 5: 提交 Agent 规则拆分**

```powershell
git add -- AGENTS.md 03_Firmware/AGENTS.md 00_Doc/04_Agent/execution_rules.md
git diff --cached --check
git commit -m "docs: split repository and firmware agent rules"
```

预期：提交只包含两级 Agent 规则和已迁移旧规则文件的删除。

---

### Task 4: 建立精简工程骨架并清理旧空目录

**Files:**

- Modify: `README.md`
- Create: `.gitignore`
- Create: domain `README.md` files under `01_Reference`, `02_Hardware`, `03_Firmware`, `04_Test`, `05_Tools`, and `06_Output`
- Remove: 已被新结构替代且确认为空的旧目录

**Interfaces:**

- Consumes: 目标目录设计、已经迁移的正式文档和两级 Agent 规则。
- Produces: Git 可追踪、职责清晰、无无效空目录的通用工程骨架。

- [x] **Step 1: 编写根目录 `README.md`**

README 依次包含：

```text
模板定位
快速恢复项目上下文
顶层目录职责
Application 与可选 Bootloader 说明
阶段工作流入口
初始化新项目步骤
当前工具分工示例
构建与测试入口
```

“当前工具分工示例”可以写网页版 GPT 与本地 Codex，但必须注明它只是可替换的当前选择。

- [x] **Step 2: 创建可追踪的领域目录**

为设计中要求保留的目录创建简短 `README.md`，说明该目录保存什么、不得保存什么。至少覆盖：

```text
01_Reference/{Datasheets,Reference_Manuals,Application_Notes,Protocols,Other}
02_Hardware/{Schematic,Pinout,Hardware_Software_Interface}
03_Firmware/00_Doc/{Module_Design,Interface,RTOS,Memory}
03_Firmware/{Application,Bootloader,Shared}
04_Test/{Host,Board,Integration,Test_Plans,Reports/Stages}
05_Tools/{Scripts,Packaging,Debug,CI}
06_Output/{Firmware,Packages,Logs}
```

每份说明保持简短，不复制根 README 内容。

- [x] **Step 3: 创建 `.gitignore`**

至少忽略：

```text
06_Output/Firmware/*
06_Output/Packages/*
06_Output/Logs/*
*.uvguix.*
*.user
*.tmp
*.log
03_Firmware/**/MDK-ARM/Objects/
03_Firmware/**/MDK-ARM/Listings/
03_Firmware/**/Debug/
03_Firmware/**/Release/
**/build/
```

为保证目录说明仍被跟踪，对 `06_Output` 下各目录的 `README.md` 使用否定规则保留。

- [x] **Step 4: 只读确认旧目录剩余内容**

检查：

```powershell
Get-ChildItem -Recurse -Force `
    00_Doc,00_Project_Management,00_Reference,01_Function_Map,02_Hardware,03_Firmware,04_Software,05_Mechanical,06_FTC,07_Tools |
    Select-Object FullName, PSIsContainer, Length
```

预期：需要保留的内容均已迁移；旧目录中只剩空目录、空占位文件或已明确替代的结构。若发现未计划的非空文件，停止删除并更新实施计划。

- [x] **Step 5: 删除已确认无内容的旧骨架**

仅对检查确认为空的目录使用 `Remove-Item -LiteralPath`，从最深层开始删除。允许删除的旧职责目录为：

```text
00_Project_Management
00_Reference
01_Function_Map
04_Software
05_Mechanical
06_FTC
07_Tools
00_Doc
```

`02_Hardware` 和 `03_Firmware` 是目标目录，只删除其中已被替代且为空的旧子目录，不删除目标根目录。

- [x] **Step 6: 验证目录树和旧路径清理**

运行：

```powershell
rg --files -g '!/.git/**' | Sort-Object
rg -n "00_Project_Management|00_Reference/01_DataSheet|01_Function_Map|04_Agent|07_Tools" `
    README.md PROJECT_CONTEXT.md AGENTS.md 00_Project/WORKFLOW.md `
    00_Project/01_Requirements 00_Project/02_Roadmap `
    00_Project/03_Stages/_Template 00_Project/04_Decisions `
    00_Project/05_Status 03_Firmware
```

预期：文件清单与冻结设计一致；第二条命令无陈旧路径匹配。

- [x] **Step 7: 提交工程骨架**

```powershell
git add -- README.md .gitignore 01_Reference 02_Hardware 03_Firmware 04_Test 05_Tools 06_Output
git add -u
git diff --cached --check
git commit -m "chore: establish firmware-centered project template"
```

预期：提交包含新骨架、README、忽略规则和旧空结构清理，不包含生成物。

---

### Task 5: 执行结构验收并完成本阶段交接

**Files:**

- Create: `04_Test/Reports/Stages/S00_Template_Restructure/verification_report.md`
- Modify: `00_Project/03_Stages/S00_Template_Restructure/handoff.md`
- Modify: `00_Project/05_Status/current_status.md`
- Modify: `PROJECT_CONTEXT.md`

**Interfaces:**

- Consumes: 完成后的模板目录、上下文合同、Agent 规则和 Git 历史。
- Produces: 可供 Review Role 审核的验证证据和最终交接状态。

- [x] **Step 1: 验证必需文件存在**

运行一个 PowerShell 检查数组，至少覆盖：

```text
README.md
PROJECT_CONTEXT.md
AGENTS.md
00_Project/WORKFLOW.md
00_Project/01_Requirements/项目需求V1.md
00_Project/03_Stages/_Template/design.md
00_Project/03_Stages/_Template/implementation_plan.md
00_Project/03_Stages/_Template/handoff.md
00_Project/03_Stages/_Template/review.md
00_Project/05_Status/current_status.md
03_Firmware/AGENTS.md
03_Firmware/00_Doc/Architecture/固件软件架构.md
03_Firmware/00_Doc/Standards/嵌入式C代码规范.md
```

预期：全部存在且非空。

- [x] **Step 2: 验证上下文一致性**

核对 `PROJECT_CONTEXT.md` 与 `current_status.md` 的以下字段完全一致：

```text
Active Stage
Status
Branch
Current Role
Next Action
```

预期：不存在两个活动阶段或相互冲突的状态。

- [x] **Step 3: 验证路径和占位错误**

运行：

```powershell
rg -n "00_Doc/02_架构设计|00_Doc/04_Agent|Memorym|Application/L|TBD|TODO" `
    README.md PROJECT_CONTEXT.md AGENTS.md 00_Project/WORKFLOW.md `
    00_Project/01_Requirements 00_Project/02_Roadmap `
    00_Project/03_Stages/_Template 00_Project/04_Decisions `
    00_Project/05_Status 03_Firmware 04_Test
```

预期：无非模板语义的错误匹配。阶段模板使用的是明确的 `{{field_name}}` 模板字段，不使用 `TBD` 或 `TODO`。

- [x] **Step 4: 验证 Git 状态和提交边界**

运行：

```powershell
git status --short
git log --oneline --decorate -8
git diff --check
```

预期：没有意外未跟踪文件，没有格式错误；历史能够分别看出设计、文档迁移、上下文工作流、Agent 规则和目录骨架提交。

- [x] **Step 5: 编写验证报告**

`verification_report.md` 记录：

```text
验证日期
验证 Commit
文件存在性结果
陈旧路径扫描结果
上下文一致性结果
Git 工作区结果
代码验证状态：NOT_APPLICABLE
硬件验证状态：NOT_APPLICABLE
```

- [x] **Step 6: 更新施工交接和当前状态**

- 在 `handoff.md` 填写实际迁移内容、删除的空目录、提交列表、验证结果、已知问题和审核重点。
- 将 `PROJECT_CONTEXT.md` 与 `current_status.md` 状态同步改为 `READY_FOR_REVIEW`。
- `Next Action` 设为由 Review Role 检查本阶段设计、差异和验证报告。

- [x] **Step 7: 提交验证与交接**

```powershell
git add -- PROJECT_CONTEXT.md 00_Project/03_Stages/S00_Template_Restructure/handoff.md `
    00_Project/05_Status/current_status.md `
    04_Test/Reports/Stages/S00_Template_Restructure/verification_report.md
git diff --cached --check
git commit -m "docs: hand off project template for review"
```

预期：阶段状态为 `READY_FOR_REVIEW`，审核文件仍保持 `NOT_REVIEWED`，等待独立审核。
