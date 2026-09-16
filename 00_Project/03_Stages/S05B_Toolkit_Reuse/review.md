# S05B Tools Toolkit Reuse Review

## Metadata

- Stage: `S05B_Toolkit_Reuse`
- Status: `CHANGES_REQUESTED`
- Branch: `main`
- Baseline Commit: `5c26fe63`
- Verification Commit: `98efc77`
- Review Commit: `ec90dbb`
- Closure Decision: `CHANGES_REQUESTED`
- Updated At: `2026-09-16`

## Review Scope

本次 Review 对照 S05B 冻结设计、实施计划、最终差异、交接和验证报告，检查架构边界、API/契约一致性、编译与测试结果、板级证据、第二工程隔离，以及文档与代码的一致性。

## Review Findings

### 1. [P1] 通用 J-Link ownership 锁没有接入实际 Workflow

结论：`NEEDS_FIX`

- `05_Tools/Core/Lock.ps1` 实现并导出了 `Enter-ToolkitLock` / `Exit-ToolkitLock`，但 `05_Tools/Adapters/Probe/JLink/jlink_flash.ps1`、`jlink_rtt.ps1`、`Adapters/Debug/GDB/gdb_session.ps1` 及其调用的 Workflow 没有获取该锁；当前调用点只有 Core 合同测试和 S04 专用 Python runner。
- 因此两个 `flash`、`rtt`、`snapshot` 或 `fault` Workflow 可以同时启动各自的 J-Link client/server。J-Link 设备拒绝其中一个不等于 Toolkit 已在 Workflow 层保证单 owner，也不能覆盖 `run` 和 Fault 的多步骤持锁边界。
- 这与设计中的 J-Link ownership、失败清理和“同一时刻只能有一个工具持有 J-Link”要求不一致；现有合同测试只验证锁原语，没有验证公共 Workflow 实际使用锁。

返工要求：由 Implementation Role 建立共享的 Toolkit J-Link ownership 边界，覆盖 Flash/RTT/GDB 及 `run`、Fault 的完整多步骤生命周期，在 `finally` 中可靠释放；补充并运行并发/锁冲突合同测试，证明失败时不启动第二个 Probe owner 且不残留 owned process。

### 2. [P1] Unified Exit Code 对外部失败码 `1` 的映射不一致

结论：`NEEDS_FIX`

- `05_Tools/toolkit.ps1` 的 `ConvertTo-ToolkitExitCode` 将原始 `1` 直接返回，但冻结文档只定义 `0/10/20/30/40/50/60` 类别，未定义公共 `1` 类别。
- Python YMODEM 的 `EXIT_INVALID_ARGUMENT` 明确为 `1`。实际执行 `toolkit.bat ymodem python send __review_missing__.img --port COM999` 返回 `[ERROR] firmware file does not exist` 和 `EXIT=1`，没有映射为 `50 TRANSFER_ERROR`。Firmware pack 的输入校验异常同样可能由 Python 返回 `1`。
- 当前 Build warning 使用 `1` 是可以识别的本地 Keil 结果，但该值不能同时作为 YMODEM/Firmware 失败的公共成功/警告码，否则 Agent 无法按统一契约分类失败。

返工要求：明确 `1` 是否为仅 Build warning；按 Workflow 对 Firmware/YMODEM 的所有非零外部失败码映射到 `50 TRANSFER_ERROR`，或补充正式的 warning 类别并同步所有文档；增加 Router 失败路径合同测试，覆盖 Python 返回 `1` 和工具参数错误。

## Review Summary

```text
代码验证：PASS（13 组 Host/Contract 回归和 git diff --check）
硬件验证：PASS（当前工程 smoke、真实 YMODEM、Fault、S04 Reset/Power-cycle、第二工程 Flash/RTT）
架构边界：NEEDS_FIX（通用 J-Link ownership 未落到 Workflow）
API / Exit Code 契约：NEEDS_FIX（外部失败码 1 穿透统一映射）
文档与代码一致性：NEEDS_FIX（上述两项需修复并回归）
```

## Review Decision

验证证据足以证明已执行的板级和回归测试结果，但上述两项是关闭前必须修复的架构与公共 API 契约问题。决定：`CHANGES_REQUESTED`。

阶段返回 Implementation Role；修复后需重新执行相关 Host/Contract 回归，并由 Verification Role 更新证据，再次进入 Review Role。当前不得进入 `CLOSED`。
