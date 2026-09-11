# RTOS 嵌入式软件工程模板

## 1. 模板定位

本仓库用于组织以 RTOS 为基础的嵌入式软件学习和项目开发。固件是主要工作区，硬件接口、外部资料、测试和工具作为固件开发的输入与验证支撑。

模板通过仓库内的阶段文档和 Git Commit 保存上下文，使人工、网页版 AI、本地 Agent 或其他工具能够可靠接续同一工程。

Application 是默认主工程；只有项目存在启动管理、固件升级或恢复需求时才启用 Bootloader。

## 2. 快速恢复上下文

进入仓库后按以下顺序读取：

1. `PROJECT_CONTEXT.md`：当前阶段、状态、分支、Commit 和下一步；
2. `00_Project/WORKFLOW.md`：阶段状态、角色和工具切换协议；
3. `PROJECT_CONTEXT.md` 的 Required Reading：当前阶段设计、计划和交接；
4. 目标目录的 `AGENTS.md`、架构、接口和相关代码。

正式工程结论必须写入仓库并提交。聊天记录可以用于讨论，但不能单独承担工程交接。

## 3. 目录结构

```text
Project/
├── AGENTS.md
├── PROJECT_CONTEXT.md
├── README.md
├── 00_Project/
├── 01_Reference/
├── 02_Hardware/
├── 03_Firmware/
├── 04_Test/
├── 05_Tools/
└── 06_Output/
```

| 目录 | 职责 |
| --- | --- |
| `00_Project` | 工程准备、需求、路线图、阶段文档、ADR 和当前状态 |
| `01_Reference` | Datasheet、Reference Manual、应用笔记和协议等外部原始资料 |
| `02_Hardware` | 固件需要使用的原理图及其他硬件输入资料 |
| `03_Firmware` | Application、可选 Bootloader、共享代码和固件设计文档 |
| `04_Test` | Host、Board、Integration 测试及正式验证报告 |
| `05_Tools` | 项目脚本、打包、调试和 CI 工具 |
| `06_Output` | 固件、升级包和日志等生成物，默认不提交 Git |

## 4. 工程准备阶段

新项目开始正式 Design Stage 前，先执行 `00_Project/00_Preparation/README.md` 中定义的 Engineering Preparation Stage。

准备阶段由项目维护者人工收集或确认以下输入：

- MCU、外部器件和开发板的 Datasheet、Reference Manual、Errata、Application Note、协议规范、原理图与 Pinout；
- 板级硬件资源、器件型号、接口、电平、引脚、IRQ、DMA、Reset、Debug 等事实；
- IDE、编译器、SDK、调试器和脚本环境版本；
- 当前无法确认的硬件问题及其影响。

准备阶段结构化数据统一填写在双语工作簿：

```text
00_Project/00_Preparation/
├── README.md
└── Engineering_Preparation.xlsx
```

工作簿包含：

```text
00_说明_Instructions
01_资料_References
02_资源_Hardware
03_引脚_Pinout
04_接口_HSI
05_问题_Issues
06_环境_Environment
```

原始 Datasheet、Reference Manual、协议、原理图等文件仍分别保存在 `01_Reference` 和 `02_Hardware`。Excel 保存索引和准备阶段结构化事实，不代替原始资料。

准备阶段只建立“事实输入”，不提前冻结 Flash 软件分区、Boot 决策、OTA 状态机、Metadata 格式等软件设计。

## 5. 固件结构

```text
03_Firmware/
├── AGENTS.md
├── README.md
├── 00_Doc/
├── Application/
├── Bootloader/
└── Shared/
```

- `Application`：默认 RTOS 固件主工程。
- `Bootloader`：可选工程，不机械复制 Application 的完整分层。
- `Shared`：仅保存两个固件工程已经共同使用的稳定代码。
- `00_Doc`：固件架构、模块、接口、RTOS、Memory 和代码规范。

修改固件前必须读取 `03_Firmware/AGENTS.md` 和详细 C 代码规范。

## 6. 阶段工作流

每个正式开发阶段保存在：

```text
00_Project/03_Stages/<stage_id>_<topic>/
├── design.md
├── implementation_plan.md
├── handoff.md
└── review.md
```

标准状态流转：

```text
DRAFT
→ DESIGN_APPROVED
→ READY_FOR_IMPLEMENTATION
→ IN_PROGRESS
→ READY_FOR_VERIFICATION
→ READY_FOR_REVIEW
→ CLOSED
```

新阶段从 `00_Project/03_Stages/_Template` 复制四份模板，填写实际字段后再提交设计基线。

工程准备阶段不是一个普通功能 `Sxx` Stage；它是项目实例化时的前置输入阶段。准备完成后，再开始第一个正式设计阶段。

## 7. 初始化新项目

1. 复制本模板或使用 GitHub Template 创建仓库。
2. 删除 `S00_Template_Restructure` 示例阶段，保留 `_Template`。
3. 用新项目需求替换 `00_Project/01_Requirements/项目需求V1.md`。
4. 执行 `00_Project/00_Preparation/README.md`：收集原始资料并填写 `Engineering_Preparation.xlsx`。
5. 更新 `PROJECT_CONTEXT.md`、路线图和 `current_status.md`。
6. 在 `03_Firmware/Application` 创建或导入主工程。
7. 仅在实际需要时启用 Bootloader 和 Shared。
8. 提交项目初始化基线。
9. 从 `_Template` 创建第一个正式 Design Stage，再开始功能设计与实现。

## 8. 当前工具分工示例

当前可以采用：

```text
网页版 GPT：Design Role / Review Role
本地 Codex：Implementation Role / Verification Role
项目维护者：Project Owner
```

这只是可替换的当前选择。任何工具只要能读取和提交仓库，并遵守 `00_Project/WORKFLOW.md`，都可以承担相应角色。

## 9. 构建与测试入口

模板本身不预设编译命令。实例化项目后，在以下位置补充真实信息：

- 工程准备数据与开发环境基线：`00_Project/00_Preparation/Engineering_Preparation.xlsx`；
- Application 构建：`03_Firmware/Application/README.md`；
- Bootloader 构建：`03_Firmware/Bootloader/README.md`；
- 测试方法：`04_Test/README.md`；
- 当前阶段验证：`04_Test/Reports/Stages/<stage>/verification_report.md`。
