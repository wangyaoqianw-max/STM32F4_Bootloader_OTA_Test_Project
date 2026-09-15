# S05A Debug & Crash Diagnostics Review

## Metadata

- Stage: `S05A_Debug_Crash_Diagnostics`
- Status: `CLOSED`
- Branch: `main`
- Baseline Commit: `ec6119f64306d027c86208329823cf42b77ceebf`
- Implementation Commit: `bd8883d`
- Verification Commit: `bd8883d`
- Review Commit: `Pending documentation commit`
- Closure Decision: `PASS`
- Updated At: `2026-09-15`

## Review Scope

本次 Review 对照 S05A 设计、实施计划、代码差异、工具契约、Keil 构建、
真实板测和验证报告，检查功能正确性、内存与现场安全、硬件与实时性、
资源释放、ISR/并发边界、API、可移植性、C 代码规范和文档一致性。

## Review Findings

### 1. 功能与架构

结论：`PASS`

- GDB Runtime Snapshot 和 Fault Capture 分开管理运行/暂停生命周期；
- GDB 不执行 `load`，Flash 仍由 `flash_app.bat` 管理；
- Fault 入口由项目自有汇编适配器统一接管，避免 CmBacktrace Vendor
  handler 与工程 Handler 重复定义；
- CmBacktrace 保留在 Vendor 目录，工程配置、RTT 和 Fault 适配位于
  Config / Impl，依赖方向清晰；
- Fault 测试入口默认关闭，不进入正常固件执行路径。

### 2. 内存、现场和错误路径

结论：`PASS`

- Fault Context 使用固定宽度类型和 `volatile` 全局快照，供 Fault Handler
  写入、GDB 读取；
- 自动压栈的 R0/R1/R2/R3/R12/LR/PC/xPSR 与 MSP/PSP/EXC_RETURN、SCB
  Fault 寄存器均已保存；
- Fault 路径未引入动态内存、Mutex、Queue、普通日志服务或阻塞 UART；
- PowerShell 只终止当前调用创建的 Server/GDB/RTT 进程，不使用全局进程名
  清理；异常、超时和工具执行错误均返回非零并写日志；
- `diagnostics_fault_handler()` 的 stacked SP 来源是硬件 Fault 入口汇编，
  不是一般业务调用者提供的地址；按 Fault Handler 最小依赖原则保留该
  内部硬件接口，不增加可能二次 Fault 的复杂范围探测。

### 3. 硬件、实时性和工具顺序

结论：`PASS`

- `SCB_CCR_DIV_0_TRP_Msk` 在 Divide-by-zero 测试前启用；
- `target remote`、断点设置均早于 `monitor reset`；
- J-Link GDB Server 早于 GDB 客户端启动；GDB detach 和 Server 退出早于
  RTT Logger 启动；
- Fault Capture 在 breakpoint 后停止并保持 MCU halted，不执行恢复；
- 正常 Resume 仍使用 `continue& → disconnect → quit`。

### 4. C 代码设计规范审查

结论：`PASS`

依据：`03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`。

- 自研文件使用统一文件头、Header Guard、模块前缀、snake_case 函数名、
  lowerCamelCase 局部变量和 `g_` 全局变量；
- 公共枚举、现场结构体和适配 API 已补充必要接口说明；
- include 顺序、宏位置、4 空格缩进、控制语句括号、单语句行和显式
  `(void)` 返回值处理已检查；
- Fault 相关硬件访问集中在 Impl，Vendor 源码没有做风格重排；
- 未发现自研文件中的 TAB 或超过 120 列；没有新增 Keil warning；
- `stm32f4xx_it.c` 为 CubeMX 生成文件，删除重复 Fault Handler 是为了
  让项目汇编适配器独占四个向量；`FreeRTOS/tasks.c` 为第三方源码，
  仅保留受 `CMB_USER_CFG` 保护的最小兼容补丁。这两项作为明确例外记录，
  未做无关格式化。

### 5. 文档一致性

结论：`PASS`

- S05A `design.md`、`implementation_plan.md`、`handoff.md`、`review.md`
  和验证报告已补齐并相互引用；
- `PROJECT_CONTEXT.md`、`current_status.md` 和路线图已同步当前关闭状态；
- S04 Reset / Power-cycle Persistence 保留为 `PENDING / DEFERRED`，没有
  被错误报告为 PASS；
- 工具说明明确了 `flash_app.bat prepare` 以及调试工具必须在 reset 前
  完成启动和断点设置的调用顺序。

## Verification Summary

```text
代码验证：PASS
硬件验证：PASS（GDB / Runtime Snapshot / Fault / CmBacktrace 范围）
Keil full rebuild：PASS，0 errors，14 个既有 warning
Contract / parser / XML / diff checks：PASS
S04 Reset Persistence：PENDING / DEFERRED
S04 Power-cycle Persistence：PENDING / DEFERRED
```

真实板测已覆盖 Invalid Address、Undefined Instruction、Divide by Zero，
并核对 GDB 与 RTT 的 Fault PC、EXC_RETURN、Stack、SCB 寄存器和核心寄存器。

## Review Decision

S05A 当前实现、代码风格、工具顺序、验证证据和文档一致性满足本阶段
关闭条件。决定：`PASS / CLOSED`。

S04 两项 Persistence 回归不在本次 S05A 关闭结论中，仍必须在
`S07_OTA_Service_V1` 关闭前完成真实硬件验证。

下一阶段：`S06_RTOS_Runtime` Design Discussion。
