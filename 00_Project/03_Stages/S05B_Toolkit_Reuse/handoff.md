# S05B Tools Toolkit Reuse Handoff

## Metadata

- Stage: `S05B_Toolkit_Reuse`
- Status: `DESIGN_APPROVED`
- Branch: `main`
- Baseline Commit: `9208cfd`
- Previous Design Commit: `9d6b037` (`docs(s05b): add reusable tools toolkit design`)
- Design Approval Commit: `Pending current documentation commit`
- Implementation Plan Commit: `Not created yet`
- Implementation Commit: `Not created yet`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Current Role: `S05B Design Role`
- Updated At: `2026-09-16`

## Input

- S05A 已关闭并完成 GDB、CmBacktrace、RTT、Fault Capture 和真实板测；
- S04 Persistence 补充回归已在 `6f2fad5` 完成；
- 当前 `05_Tools` 已包含 Keil、J-Link、GDB、RTT、Firmware Image、Ymodem、Tera Term、CmBacktrace 和测试脚本，但配置、公共能力、工具调用和项目专用测试仍存在职责混合；
- Project Owner 已确认 S05B 的核心目的为：整合现有工具，建立便于后续扩展、升级和跨工程复用的工具框架；
- 第一版仍以当前已经验证的 `STM32 + Keil + J-Link + GNU Arm GDB` 工具组合为基线，不提前实现其他编译链或 Probe。

## Frozen Design Output

正式详细设计仅保存在：

```text
00_Project/03_Stages/S05B_Toolkit_Reuse/design.md
```

不再使用 `docs/superpowers/specs/` 作为本项目阶段设计的正式落点。

设计已冻结以下决策：

1. `Config + Core + Adapters + Workflows + Project Tests + Legacy Wrappers` 分层；
2. Adapter 按 `Build / Probe / Debug` 变化轴拆分，而不是使用 `STM32_Keil_JLink` 组合 Adapter；
3. 配置采用三层模型：
   - `toolchain.local.bat`：本机工具安装事实，ignored；
   - `project.defaults.bat`：工程固有事实，committed；
   - `project.local.bat`：本机工程覆盖，ignored；
4. 增加统一 `toolkit.bat` Router，Human / Agent 使用稳定入口；
5. 旧 `Scripts/*.bat` 保留兼容，但最终只做薄包装，不保留第二套业务实现；
6. Firmware / Ymodem / TeraTerm 第一版继续作为独立格式/协议工具，不强制抽象为 Transport Adapter；
7. S04 Persistence 收敛到 `Tests/S04_Persistence` 项目扩展边界；
8. S05B 至少冻结稳定 Exit Code 分类，并为后续 JSON Result 留出方向；
9. 不建立动态插件框架，不为尚未使用的 GCC/CMake、OpenOCD、GD32 等提前实现 Adapter；
10. S05B 关闭前必须证明跨工程复用，而不是只证明目录更整齐。

## Scope Boundary

- 只重塑 `05_Tools`、配置模板、公共工具能力、调用契约和相关文档；
- 不修改生产固件、Flash/Memory Layout、RTOS 接口、Firmware Image Contract 或 MCU 侧 Ymodem；
- 不提交本机工具路径，不修改系统 `PATH`；
- 不提交临时板测代码、构建缓存或调试输出；
- 未进入 `READY_FOR_IMPLEMENTATION` 前，不迁移脚本、不删除旧入口、不修改工具实现。

## Acceptance Direction

S05B 验收分四层：

```text
Level 1  现有 Build / Flash / RTT / GDB / Fault / Firmware / Ymodem 能力无退化
Level 2  当前工程实现配置驱动，通用实现无工程专用硬编码
Level 3  Workflow / Adapter / Core 边界可独立演化
Level 4  第二同类 STM32 + Keil + J-Link 工程只改配置即可复用
```

旧入口继续可用，但必须转发到统一 Workflow，禁止长期双轨维护。

## Tool Switch Information

当前已完成 Design Review，工作流状态为 `DESIGN_APPROVED`：

```text
Design approved
  → create implementation_plan.md
  → review plan consistency
  → READY_FOR_IMPLEMENTATION
  → Implementation Role
```

## Next Action

基于已冻结的 `design.md` 创建正式 `implementation_plan.md`。计划需要按迁移顺序拆分任务，并明确每一步的回归、兼容验证和停止条件；在计划完成并确认前不开始移动 `05_Tools` 实现。