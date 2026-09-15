# S05A Debug & Crash Diagnostics Implementation Plan

## Metadata

- Stage: `S05A_Debug_Crash_Diagnostics`
- Status: `CLOSED`
- Branch: `main`
- Baseline Commit: `ec6119f64306d027c86208329823cf42b77ceebf`
- Plan Basis: Project Owner supplied S05A implementation plan, 2026-09-15
- Implementation Plan Commit: `32f3368`
- Updated At: `2026-09-15`

## Execution Steps

| Step | Scope | Result |
| --- | --- | --- |
| 1 | 建立 GDB/J-Link 本地配置和 Runtime Snapshot 脚本 | PASS |
| 2 | 建立 PowerShell/BAT Agent 入口、超时和进程清理 | PASS |
| 3 | 验证 resume/halt、失败路径、J-Link 释放和禁止 `load` | PASS |
| 4 | 实现 Invalid Address、Undefined Instruction、Divide by Zero 受控 Fault | PASS |
| 5 | 接入 CmBacktrace、FreeRTOS 当前任务栈信息和 raw RTT | PASS |
| 6 | 建立 Fault Capture GDB 脚本和 GDB/RTT 交叉核对 | PASS |
| 7 | 固定 Build → prepare → 提前启动调试工具 → reset/continue → RTT 顺序 | PASS |
| 8 | 执行 Keil、契约测试、真实板级 Fault 和正常固件回归 | PASS |

## Implemented Entry Points

```text
05_Tools/Scripts/build_app.bat
05_Tools/Scripts/flash_app.bat run|prepare
05_Tools/Scripts/gdb_runtime_snapshot.bat resume|halt
05_Tools/Scripts/gdb_fault_capture.bat capture|trigger
05_Tools/Scripts/rtt_capture.bat
```

## Verification Commands

```text
05_Tools/Debug/GDB/test_gdb_automation.ps1
05_Tools/Debug/test_tool_sequence.ps1
05_Tools/Debug/CmBacktrace/test_cm_backtrace_integration.ps1
05_Tools/Debug/CmBacktrace/test_fault_diagnostics.ps1
```

补充验证：PowerShell Parser、Keil project XML、Keil full rebuild、真实
J-Link / GDB / RTT 板测以及最终正常固件恢复。

## Deferred Work

S04 Reset Persistence / Power-cycle Persistence 不在本次实现中补测，继续
作为 S07 关闭前的硬件延期回归。
