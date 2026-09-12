# AGENTS.md

本文件定义仓库级长期规则。它面向人工和不同 Agent 的连续协作，不绑定具体 AI 产品。

## 1. 模板定位

- 本仓库以 RTOS 嵌入式软件开发为核心。
- `03_Firmware/Application` 是默认主工程。
- `03_Firmware/Bootloader` 仅在项目需要启动管理、升级或恢复能力时启用。
- 硬件与外部资料作为固件开发输入，不建立无实际需求的产品管理层级。
- 默认使用中文沟通和编写项目自研文档，已有术语与代码命名保持原风格。

## 2. 指令优先级

发生冲突时按以下顺序处理：

1. 用户当前明确指令；
2. 项目正式需求与已冻结阶段设计；
3. 本 `AGENTS.md` 及目标目录更具体的 `AGENTS.md`；
4. 当前阶段实施计划；
5. 目标模块已有稳定接口与代码风格；
6. Agent 自身推断。

无法依据高优先级信息消除实质冲突时，停止修改，记录冲突、影响和需要确认的问题。

## 3. 上下文恢复

开始分析、设计、实现、验证或审核前必须：

1. 读取 `PROJECT_CONTEXT.md`；
2. 核对 Active Stage、Status、Branch、Baseline Commit、Current Role 和 Next Action；
3. 按 Required Reading 顺序读取当前阶段文档；
4. 读取目标目录适用的 `AGENTS.md`、架构、接口和代码；
5. 在修改前确认当前目标、范围、验收条件和禁止事项。

如果实际分支或 `HEAD` 与上下文、交接记录不一致，先调查差异。不得假定聊天记录能够代替仓库上下文。

正式工程结论必须写入仓库并提交；未落盘的聊天结论不视为冻结设计或完成证据。

## 4. 阶段门禁

阶段状态及合法转换以 `00_Project/WORKFLOW.md` 为准。

- Design Role 负责设计和实施计划，不直接进入未获批准的施工。
- Implementation Role 只在状态为 `READY_FOR_IMPLEMENTATION`、`IN_PROGRESS` 或 `CHANGES_REQUESTED` 时修改计划范围内文件。
- Verification Role 以验收条件为依据生成可回读的验证证据。
- Review Role 对照需求、设计、差异和验证报告决定通过、返工或阻塞。
- Project Owner 负责设计批准、外部阻塞、硬件验收和阶段关闭。

不得通过修改验收条件掩盖实现失败，不得在缺少必要证据时将阶段标记为 `CLOSED`。

## 5. 修改范围与安全

- 默认先理解再修改，只处理当前任务明确范围。
- 不顺便重构、重命名、格式化或优化无关模块。
- 不删除、覆盖或迁移未确认归属的非空文件。
- 不打印密钥、Token 或其他敏感凭证。
- 涉及不可逆操作、外部系统写入或范围扩张时，先取得明确授权。
- 发现未计划的用户修改时予以保留；与当前任务冲突时停止并说明。

## 6. 开发过程

非简单任务遵循：

```text
需求
→ 现状调查
→ 设计确认
→ Implementation Plan
→ 小步实现
→ 验证
→ Review
→ 交接或关闭
```

如果当前阶段仅允许设计或计划，不得提前修改生产代码。实施过程中需要偏离冻结设计时，先在 `handoff.md` 记录原因并交回确认。

## 7. 文档职责

- `README.md`：项目定位、目录和使用入口。
- `PROJECT_CONTEXT.md`：当前阶段的统一上下文入口。
- `00_Project/WORKFLOW.md`：角色、状态和工具切换协议。
- `design.md`：阶段冻结设计。
- `implementation_plan.md`：当前阶段施工步骤。
- `handoff.md`：施工输入、输出和工具切换信息。
- `review.md`：审核、返工或关闭结论。
- `00_Project/04_Decisions`：跨阶段长期有效的技术决策。
- `04_Test/Reports`：完整验证证据。

代码改变 API、架构、模块职责、硬件资源、任务、IPC、数据流、状态机、Memory Layout 或 Flash Layout 时，必须检查并同步相应正式文档。

## 8. 验证与完成声明

“代码看起来正确”不等于完成。根据变更范围执行适用的格式检查、编译、静态检查、Host Test、集成测试和板级测试。

必须明确区分：

```text
代码验证：PASS / FAIL / NOT_APPLICABLE
硬件验证：PASS / PENDING / FAIL / NOT_APPLICABLE
```

Agent 无法操作真实硬件时，不得把编译或模拟结果描述为硬件验证通过。阶段完成前将验证命令、结果、Commit 和未验证项写入验证报告及交接文件。

### 8.1 统一本地工具入口

本仓库通过 `05_Tools` 对本机开发工具提供稳定入口，避免不同 Agent 重复探测本机安装路径。

Application 固件默认编译命令：

```text
05_Tools\Scripts\build_app.bat
```

规则：

1. 修改 Application 固件后，优先执行上述脚本，不自行搜索 `UV4.exe`、ARMCC 或 `.uvprojx`；
2. 本机工具路径只写入 `05_Tools\Config\toolchain.local.bat`，该文件不得提交；
3. 新环境从 `toolchain.local.example.bat` 复制本地配置后再调整路径；
4. 脚本报告本地配置缺失或无效时，才允许调查本机工具安装位置；
5. 编译失败时读取脚本输出和 `06_Output/Logs/OTA_APP_build.log`，修复后重新执行；
6. 编译成功不等于硬件验证通过，板级验证仍按当前 Stage 的 Verification 要求执行。

后续增加 J-Link、RTT、打包等自动化时，继续通过 `05_Tools` 提供统一入口，不把机器相关路径写入生产代码或阶段设计。

## 9. Git 规则

- 一个 Commit 保持单一职责，并能解释修改原因。
- 不提交构建缓存、临时文件、调试输出或无意义 IDE 变化。
- 提交前检查 `git diff --check` 和任务要求的验证结果。
- 不擅自重写、丢弃或覆盖用户历史。
- 工具切换前更新 `PROJECT_CONTEXT.md` 和 `handoff.md`，记录实际 Commit 并推送远程仓库。

## 10. 目录专项规则

修改 `03_Firmware` 下的任何文件前，必须读取：

1. `03_Firmware/AGENTS.md`；
2. `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`；
3. 当前任务相关的固件架构、接口和硬件软件接口文档。

如果 Codex 或其他 Agent 从仓库根目录启动，不得假定它会自动发现更深层的规则文件，必须显式读取。
