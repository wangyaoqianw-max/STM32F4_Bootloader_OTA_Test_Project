# S05B Tools Toolkit Reuse Design

## Metadata

- Stage: `S05B_Toolkit_Reuse`
- Status: `DRAFT`
- Branch: `main`
- Baseline Commit: `9208cfd`
- Current Role: `S05B Design Role`
- Design Specification: `docs/superpowers/specs/2026-09-16-s05b-toolkit-reuse-design.md`
- Updated At: `2026-09-16`

## Objective

将 `05_Tools` 重塑为面向同类 `STM32 + Keil + J-Link` 工程的可复用 PC 工具包。工具包通过“公共核心 + 平台适配器 + 工作流 + 项目测试扩展”分层，并将机器工具路径与项目/目标参数分离。

## Frozen Design Summary

- `Config`：`toolchain.local.bat` 只放本机工具路径；`project.local.bat` 放项目路径、芯片、J-Link、GDB、RTT 和串口参数；两个本地文件均不得提交。
- `Core`：提供公共路径、进程生命周期、日志和 J-Link 占用辅助能力。
- `Adapters/STM32_Keil_JLink`：只封装当前 STM32 + Keil + J-Link 工具链，不写入本工程名称和 S04 专用符号。
- `Workflows`：提供 Application、Debug、Test 编排入口。
- `Firmware`、`Ymodem`、`TeraTerm`：继续作为稳定格式/协议包；现有稳定职责目录第一轮不强制物理迁移。
- `Tests/S04_Persistence`：保留 S04 专用 AXF、解析器和板测 runner，不能污染通用工作流。
- `Scripts`：保留旧 BAT 入口，迁移后只做薄包装，不保留重复业务实现。
- 不在 S05B 实现 GD 或其他编译链适配器，不修改 `03_Firmware`。

## Acceptance Direction

复制 `05_Tools` 到另一个同类工程后，只填写两个本地配置文件即可调用编译、烧录、RTT、GDB、固件打包和 Ymodem 能力；旧 `Scripts` 入口继续可用；README、阶段验证报告和交接文件对目录与配置保持一致。

## Required Review

本设计进入 `DESIGN_APPROVED` 前，需要 Project Owner 审阅并确认：

1. 当前复用边界是否限定为 `STM32 + Keil + J-Link`；
2. `Config / Core / Adapter / Workflow / Project Test` 分层是否符合后续复制使用方式；
3. 是否接受保留 `Scripts` 兼容入口，以及第一轮不强制迁移稳定协议包目录；
4. 是否接受“至少一次第二同类工程配置替换演练”作为 S05B 关闭条件。
