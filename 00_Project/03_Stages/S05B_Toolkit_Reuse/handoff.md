# S05B Tools Toolkit Reuse Handoff

## Metadata

- Stage: `S05B_Toolkit_Reuse`
- Status: `READY_FOR_IMPLEMENTATION`
- Branch: `main`
- Baseline Commit: `9208cfd`
- Previous Design Commit: `9d6b037` (`docs(s05b): add reusable tools toolkit design`)
- Design Approval Commit: `5622b63a7cb3d532aab55de73eaf88d823b9acb9`
- Implementation Plan Commit: `3a055a84397ab5eab6dcddf2dea0590c97daff4c`
- Implementation Commit: `Not created yet`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Current Role: `S05B Implementation Role`
- Updated At: `2026-09-16`

## Input

- S05A 已关闭并完成 GDB、CmBacktrace、RTT、Fault Capture 和真实板测；
- S04 Persistence 补充回归已在 `6f2fad5` 完成；
- 当前 `05_Tools` 已包含 Keil、J-Link、GDB、RTT、Firmware Image、Ymodem、Tera Term、CmBacktrace 和测试脚本，但配置、公共能力、工具调用和项目专用测试仍存在职责混合；
- Project Owner 已确认 S05B 的核心目的为：整合现有工具，建立便于后续扩展、升级和跨工程复用的工具框架；
- 第一版仍以当前已经验证的 `STM32 + Keil + J-Link + GNU Arm GDB` 工具组合为基线，不提前实现其他编译链或 Probe。

## Frozen Design Output

正式详细设计：

```text
00_Project/03_Stages/S05B_Toolkit_Reuse/design.md
```

正式实施计划：

```text
00_Project/03_Stages/S05B_Toolkit_Reuse/implementation_plan.md
```

不使用 `docs/superpowers/specs/` 或 `docs/superpowers/plans/` 作为本项目阶段正式文档落点。

设计已冻结：

1. `Config + Core + Adapters + Workflows + Project Tests + Legacy Wrappers` 分层；
2. Adapter 按 `Build / Probe / Debug` 变化轴拆分；
3. 三层配置：`toolchain.local.bat`（machine, ignored）+ `project.defaults.bat`（project, committed）+ `project.local.bat`（machine override, ignored）；
4. `toolkit.bat` / `toolkit.ps1` 作为统一 Human / Agent Router；
5. 旧 `Scripts/*.bat` 最终只做薄包装，禁止双轨业务实现；
6. Firmware / Ymodem / TeraTerm 第一版继续作为独立工具能力；
7. S04 Persistence 收敛为 `Tests/S04_Persistence` 项目扩展；
8. 稳定 Exit Code 分类；
9. 不提前实现动态插件、GCC/CMake、OpenOCD、GD32 等未使用能力；
10. S05B 关闭前必须完成跨工程复用证明。

## Implementation Plan Summary

实施顺序固定为：

```text
Task 1  三层配置 + Config/Path/Logging Core
Task 2  Process / Lock / Cleanup Core
Task 3  Keil + J-Link Adapter 与 Application Workflows
Task 4  GDB Adapter 与 Debug Workflows
Task 5  toolkit Router + Legacy Wrappers
Task 6  S04 Persistence Project Test Extension
Task 7  Firmware / Ymodem Router 接入 + README/占位目录清理
Task 8  全量回归 + 第二工程复用演练 + Verification Handoff
```

第二工程复用首选 `wangyaoqianw-max/stm32f4_DMA_UART_ring_RTOS`，在临时 clone 中只替换配置，不修改通用 Core / Adapter / Workflow。

## Scope Boundary

- 不修改生产固件、Flash/Memory Layout、RTOS 接口、Firmware Image Contract 或 MCU 侧 Ymodem；
- 不提交本机工具路径，不修改系统 `PATH`；
- 不提交临时板测代码、构建缓存或调试输出；
- 每个 Task 先测试再迁移，失败时停在当前 Task；
- Host PASS 不替代 Board PASS；无 USB/J-Link 的执行环境必须明确记录硬件项 `PENDING`。

## Acceptance Direction

```text
Level 1  现有 Build / Flash / RTT / GDB / Fault / Firmware / Ymodem 能力无退化
Level 2  当前工程配置驱动，通用实现无工程专用硬编码
Level 3  Workflow / Adapter / Core 可独立演化，旧入口只做 wrapper
Level 4  第二同类 STM32 + Keil + J-Link 工程只改配置即可复用
```

## Tool Switch Information

当前状态：

```text
DESIGN_APPROVED
  → implementation_plan.md complete
  → plan self-review complete
  → READY_FOR_IMPLEMENTATION
  → Implementation Role
```

实施时必须先读取：

```text
AGENTS.md
PROJECT_CONTEXT.md
00_Project/WORKFLOW.md
00_Project/03_Stages/S05B_Toolkit_Reuse/design.md
00_Project/03_Stages/S05B_Toolkit_Reuse/implementation_plan.md
00_Project/03_Stages/S05B_Toolkit_Reuse/handoff.md
05_Tools/README.md
05_Tools/Scripts/README.md
```

## Next Action

从 `implementation_plan.md` Task 1 开始施工：先建立三层配置合同和 Core Config/Path/Logging 测试，不得先移动现有 Build/GDB 脚本。Implementation 完成后进入 `READY_FOR_VERIFICATION`，不得直接关闭 S05B。
