# Development Roadmap

路线图描述长期阶段顺序，不替代阶段实施计划。正式 Stage 必须在项目级规划讨论后确定，不根据早期设想直接冻结。

## Current Roadmap

| Stage | Goal | Prerequisites | Status | Completion Criteria |
| --- | --- | --- | --- | --- |
| `S00_Template_Restructure` | 建立通用工程模板、跨工具上下文合同和工程准备机制 | 目录与工作流设计获批 | `CLOSED` | 结构与构建规范验证通过、Project Owner 审核通过，并已被当前项目实际采用 |

## Next Planning Checkpoint

当前尚未创建 `S01`。

下一次项目级 Design Discussion 需要基于以下输入正式拆分 Bootloader/OTA 开发路线：

- `00_Project/01_Requirements/项目需求V1.md`
- `00_Project/00_Preparation/Engineering_Preparation_Bilingual.xlsx`
- `01_Reference/`
- `02_Hardware/`
- `03_Firmware/00_Doc/`

规划讨论至少需要确定：

1. 整个 Bootloader/OTA 项目拆分为哪些可独立验收的 Stage；
2. 每个 Stage 的学习目标、工程交付物和前置依赖；
3. 哪些开放资料可以延后到对应 Stage 再补充；
4. 第一个正式功能 Stage 的范围与验收条件。

在该讨论完成前，不预先给 `S01` 分配实现范围，也不直接进入功能编码。

允许的正式 Stage 状态值参见 `00_Project/WORKFLOW.md`。
