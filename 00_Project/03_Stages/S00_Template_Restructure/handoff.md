# S00 Template Restructure Handoff

## Metadata

- Stage: `S00_Template_Restructure`
- Status: `CLOSED`
- Branch: `main`
- Design Commit: `2c696d4`
- Baseline Commit: `e9b71e4`
- Implementation Commits: `91ad2df`, `9a059a2`, `1c81bb9`, `878d0a1`, `7bee8e5`, `eaa64b8`
- Review Feedback Commit: `894ac40`
- Verification Commits: `015c2aa`, `762f6fa`
- Closing Review Commit: `9968c8b`
- Review Metadata Sync Commit: `2405709`
- Closed At: `2026-09-11`

## Stage Goal

将原始工程骨架整理为以 RTOS 固件开发为核心、支持跨工具接续的通用模板，并建立 Design → Implementation → Verification → Review 的工程闭环。

## Final Output

- Status: `COMPLETED`
- Review Result: `APPROVED`

### Completed Work

- 已迁移项目需求、固件架构和嵌入式 C 代码规范，并修正架构适用范围。
- 已建立 `PROJECT_CONTEXT.md`、阶段状态机、路线图、ADR 约定和四份阶段模板。
- 已将原根 Agent 规则拆分为仓库级 `AGENTS.md` 与 `03_Firmware/AGENTS.md`。
- 已建立 Reference、Hardware、Firmware、Test、Tools 和 Output 精简骨架。
- 已增加适配当前目录的 Keil 构建输出管理与 I/O 故障诊断规范。
- 已建立 Engineering Preparation Stage，并将结构化准备数据收束到双语 Excel 工作簿。
- 当前 STM32F411 Bootloader/OTA 项目已经使用该模板继续收集需求、硬件资料和准备阶段事实。

### Verification Result

模板结构验证与 Keil 规范返工复验均通过；代码验证和硬件验证对 S00 为 `NOT_APPLICABLE`。完整证据见：

`04_Test/Reports/Stages/S00_Template_Restructure/verification_report.md`

## Downstream Handoff

S00 关闭后，项目从“模板建设”切换为“真实 Bootloader/OTA 项目规划”。

### 已具备的输入

- 正式需求：`00_Project/01_Requirements/项目需求V1.md`
- 工程准备说明：`00_Project/00_Preparation/README.md`
- 工程准备数据：`00_Project/00_Preparation/Engineering_Preparation_Bilingual.xlsx`
- 已收集的 MCU、W25Q64JVSSIQ、AT24C02、HC-05、LCD 等资料：`01_Reference/`
- 开发板原理图、Pinout 和硬件接口资料：`02_Hardware/`
- 固件架构和代码规范：`03_Firmware/00_Doc/`

### 当前已经确认、足以开始规划的关键事实

- MCU：STM32F411CEU6，Internal Flash 512 KB。
- 外部 SPI NOR：W25Q64JVSSIQ，SPI2，3.3 V。
- EEPROM：AT24C02，Software I2C，PB6/PB7，3.3 V，7-bit 地址 0x50，WP 接 GND。
- 调试：J-Link + SWD，日志方向为 SEGGER RTT + EasyLogger。
- 项目目标：Bootloader + OTA、External Flash A/B、Firmware Version/Rollback、CRC→Hash→AES 等逐步学习与实现。

### 已知但不阻塞项目规划的开放项

- CK02AT 缺少可靠公开 Datasheet/API；V1 不依赖其进入正式安全链。
- Ymodem 原始协议资料需要在协议设计阶段前补齐。
- HC-05、LCD/CTP 的部分参数和实测信息可在对应功能阶段补充。
- 工具链部分精确版本仍需继续完善。

## Next Action

下一次 Design Discussion 先做项目级任务拆分和路线图规划，不直接开始功能代码：

1. 根据 V1 需求和准备阶段输入拆分正式开发 Stage；
2. 明确各 Stage 的学习目标、交付物、前置条件和验收条件；
3. 确定第一个正式 Bootloader/OTA Stage；
4. 再创建并填写该 Stage 的 `design.md`、`implementation_plan.md`、`handoff.md` 和 `review.md`。

## Prohibited Before Planning Approval

- 不直接开始 Ymodem、SFUD、Rollback、AES 等实现；
- 不提前冻结未经讨论的 Flash 软件分区、Metadata 格式或 OTA 状态机；
- 不把 CK02AT、LCD 等非阻塞开放项升级为当前阶段前置依赖；
- 不为了“补全资料”而阻塞已经具备条件的项目级规划。
