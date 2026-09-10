# 嵌入式软件工程模板与跨工具上下文工作流设计

## 1. 文档状态

- Stage：S00_Template_Restructure
- Status：REVIEW_REQUIRED
- Scope：通用 RTOS 嵌入式软件工程模板
- Date：2026-09-10

## 2. 目标

将当前工程骨架整理为以 RTOS 嵌入式软件开发为核心的通用模板，并把项目上下文持久化在 Git 仓库中，使人工、网页版 AI、本地 Agent 或后续其他工具能够可靠接续同一工程。

模板不绑定特定 AI 产品。当前可以由网页版 GPT 负责设计和审核、Codex 本地负责实现和验证；更换工具后，目录结构和交接协议保持不变。

## 3. 核心原则

1. 仓库是工程上下文的唯一正式载体。
2. 未写入仓库并提交的聊天结论不视为正式工程决定。
3. `PROJECT_CONTEXT.md` 是所有工具恢复当前上下文的统一入口。
4. 阶段文档分别承担设计、施工、交接和审核职责，避免内容重复。
5. Git Commit 是设计、实现和审核状态的版本锚点。
6. 固件开发是模板核心；硬件和产品资料只保留固件开发需要的输入。
7. Application 是默认主工程，Bootloader 按项目需要启用。
8. 默认面向 RTOS 项目，不为纯裸机场景增加额外结构。

## 4. 目标目录结构

```text
Embedded_Project_Template/
├── AGENTS.md
├── PROJECT_CONTEXT.md
├── README.md
│
├── 00_Project/
│   ├── WORKFLOW.md
│   ├── 01_Requirements/
│   ├── 02_Roadmap/
│   ├── 03_Stages/
│   │   ├── _Template/
│   │   │   ├── design.md
│   │   │   ├── implementation_plan.md
│   │   │   ├── handoff.md
│   │   │   └── review.md
│   │   └── Sxx_topic/
│   ├── 04_Decisions/
│   └── 05_Status/
│       └── current_status.md
│
├── 01_Reference/
│   ├── Datasheets/
│   ├── Reference_Manuals/
│   ├── Application_Notes/
│   ├── Protocols/
│   └── Other/
│
├── 02_Hardware/
│   ├── Schematic/
│   ├── Pinout/
│   └── Hardware_Software_Interface/
│
├── 03_Firmware/
│   ├── AGENTS.md
│   ├── README.md
│   ├── 00_Doc/
│   │   ├── Architecture/
│   │   ├── Module_Design/
│   │   ├── Interface/
│   │   ├── RTOS/
│   │   ├── Memory/
│   │   └── Standards/
│   ├── Application/
│   ├── Bootloader/
│   └── Shared/
│
├── 04_Test/
│   ├── Host/
│   ├── Board/
│   ├── Integration/
│   ├── Test_Plans/
│   └── Reports/
│       └── Stages/
│
├── 05_Tools/
│   ├── Scripts/
│   ├── Packaging/
│   ├── Debug/
│   └── CI/
│
└── 06_Output/
    ├── Firmware/
    ├── Packages/
    └── Logs/
```

`06_Output` 中的生成物和日志默认不纳入 Git；需要长期留存的验证结论写入 `04_Test/Reports`。

## 5. 现有文档迁移

| 当前文件 | 目标位置 | 处理方式 |
| --- | --- | --- |
| `00_Doc/01_项目需求/项目需求V1.md` | `00_Project/01_Requirements/项目需求V1.md` | 保留为当前 STM32F411 OTA 项目内容 |
| `00_Doc/03_架构设计/嵌入式项目C代码设计规范.md` | `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md` | 保留并修正引用路径 |
| `00_Doc/03_架构设计/软件架构说明.md` | `03_Firmware/00_Doc/Architecture/固件软件架构.md` | 改为通用 RTOS 固件架构 |
| `00_Doc/04_Agent/execution_rules.md` | 根目录与固件目录两级 `AGENTS.md` | 拆分有效规则后取消重复文件 |
| `00_Doc/04_Agent/development_roadmap.md` | `00_Project/02_Roadmap/development_roadmap.md` | 用有意义的路线图模板替换空文件 |
| `00_Doc/04_Agent/implementation_plan.md` | 阶段目录中的 `implementation_plan.md` | 用阶段模板替换空文件 |
| `00_Doc/04_Agent/handoff.md` | 阶段目录中的 `handoff.md` | 用双向交接模板替换空文件 |
| `00_Doc/04_Agent/architecture.md` | 不迁移 | 与正式架构文档职责重复且当前为空 |
| `00_Doc/04_Agent/requirements.md` | 不迁移 | 与正式需求目录职责重复且当前为空 |

## 6. 通用上下文入口

根目录保留三个入口文件：

```text
README.md             项目或模板说明，以及人工导航
PROJECT_CONTEXT.md    当前阶段、状态、Commit、下一步和必读文件
AGENTS.md             Agent 必须遵守的仓库级规则
```

`PROJECT_CONTEXT.md` 保持简短，至少记录：

- 当前阶段；
- 当前状态；
- 当前分支；
- 基准 Commit；
- 当前执行角色；
- 当前目标；
- 下一步动作；
- 必读文件；
- 阻塞项；
- 最近验证结果；
- 明确禁止事项。

详细设计、计划、验证记录不得复制到 `PROJECT_CONTEXT.md`，只通过链接或路径引用正式文件。

## 7. 阶段工作流

每个大任务拆分为一个或多个可独立验收的阶段。每个阶段包含：

```text
design.md
implementation_plan.md
handoff.md
review.md
```

阶段状态按以下顺序流转：

```text
DRAFT
→ DESIGN_APPROVED
→ READY_FOR_IMPLEMENTATION
→ IN_PROGRESS
→ READY_FOR_VERIFICATION
→ READY_FOR_REVIEW
→ CLOSED
```

异常状态：

```text
任意阶段 → BLOCKED
审核失败 → CHANGES_REQUESTED → IN_PROGRESS
```

`00_Project/05_Status/current_status.md` 保存唯一的活动阶段状态。阶段文档可以记录自身文档状态，但不得维护另一套相互冲突的项目状态。

## 8. 角色与动作合同

工作流使用角色名称，不绑定具体工具。

### 8.1 Design Role

读取：

- 项目需求；
- 路线图；
- 当前状态；
- 相关架构和历史 ADR。

输出：

- `design.md`；
- `implementation_plan.md`；
- `handoff.md` 的施工输入；
- 更新后的 `PROJECT_CONTEXT.md`。

设计确认并提交后，状态进入 `READY_FOR_IMPLEMENTATION`。

### 8.2 Implementation Role

读取顺序：

```text
AGENTS.md
→ README.md
→ PROJECT_CONTEXT.md
→ 当前阶段 design.md
→ 当前阶段 implementation_plan.md
→ 当前阶段 handoff.md
→ 目标目录 AGENTS.md
→ 相关工程文档和代码
```

输出：

- 实现代码；
- 必要测试；
- 同步更新的正式文档；
- `handoff.md` 的施工输出；
- 更新后的 `PROJECT_CONTEXT.md`。

不得擅自修改冻结设计。设计与代码现状存在实质冲突时，状态改为 `BLOCKED` 并交回确认。

### 8.3 Verification Role

读取验收条件、实现提交和测试方案，执行适用的编译、静态检查、Host Test、Board Test 或集成测试。

验证证据保存到：

```text
04_Test/Reports/Stages/Sxx_topic/
```

必须区分：

```text
代码验证：PASS / FAIL
硬件验证：PASS / PENDING / FAIL
```

### 8.4 Review Role

对照需求、冻结设计、施工计划、代码差异和验证证据进行审核。

审核结论写入 `review.md`：

- 通过时进入 `CLOSED`；
- 存在问题时进入 `CHANGES_REQUESTED`；
- 信息不足时进入 `BLOCKED`。

### 8.5 Project Owner

负责批准设计、解决阻塞、确认硬件验收和关闭阶段。角色可以由项目维护者本人承担。

## 9. 工具切换协议

### 9.1 离开当前工具前

1. 将已确认的结论写入正式工程文档。
2. 更新当前阶段 `handoff.md`。
3. 更新 `PROJECT_CONTEXT.md`。
4. 记录当前分支和 Commit。
5. 提交并推送远程仓库。

### 9.2 进入下一个工具后

1. 拉取远程最新版本。
2. 核对当前分支、工作区和 Commit。
3. 读取 `PROJECT_CONTEXT.md`。
4. 按其中顺序读取当前阶段资料。
5. 在修改前复述当前目标、范围、状态和下一步。
6. 如果实际 `HEAD` 与交接记录不一致，先检查差异，不直接施工。

## 10. 两级 Agent 规则

### 10.1 根目录 `AGENTS.md`

只保存仓库长期有效的规则：

- 模板定位；
- 指令和文档优先级；
- 上下文恢复与阶段状态规则；
- 修改权限和阶段门禁；
- 验证、文档和 Git 基本要求；
- 固件任务必须继续读取 `03_Firmware/AGENTS.md`。

### 10.2 `03_Firmware/AGENTS.md`

保存固件专项规则：

- APP、Service、Platform、Impl 分层；
- CubeMX 和 Vendor 边界；
- ISR、DMA、RTOS 和并发要求；
- 静态内存、错误处理和日志规则；
- C 代码规范入口；
- Coding Standard Review；
- 代码验证和硬件验证的区分。

详细 C 规范只保留在 `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`，`AGENTS.md` 引用它而不重复全文。

## 11. 模板裁剪规则

1. 通用模板提供 Application 主工程位置。
2. Bootloader 目录允许保留，但仅在项目需要启动管理或升级功能时启用。
3. Shared 仅在 Application 与 Bootloader 确实共享代码时使用。
4. 机械、法规、生产和移动端等领域不建立默认维护目录。
5. 外部资料统一归档到 `01_Reference`，项目内部形成的硬件接口事实写入 `02_Hardware`。
6. 不为了目录完整创建大量空文件；需要保持的空目录使用职责明确的 `README.md`。

## 12. 验收条件

整理完成后应满足：

1. 根目录可通过三个入口文件恢复项目与当前阶段上下文。
2. 所有原有非空文档均有明确归属，内容不丢失。
3. 所有文档内部路径与实际目录一致。
4. 根目录和固件目录 Agent 规则职责无大段重复。
5. 阶段模板完整覆盖设计、施工、验证、审核和交接。
6. 新工具只读取仓库即可确定当前阶段、下一步和验收条件。
7. 生成物与正式验证报告分离。
8. README 能作为人工和 AI 的统一导航页。
