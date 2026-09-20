# S10 Trial Confirm Rollback Verification Report

## Metadata

- Stage: `S10_Trial_Confirm_Rollback`
- Status: `READY_FOR_VERIFICATION`
- Branch: `main`
- Plan Baseline: `651b3001b4c23cf4162e3367a91ae43307207bce`
- Actual Implementation Baseline: `79b95d1f4681c2f7b5f961785079a112a3c62492`
- Implementation Commits: `dfb012b`, `cd15bad`, `e245e21`, `26ce0ee`, `1ae57ef`, `d1b08b2`, `a43086c`, `352ee62`, `c237361`, `54c9827`, `b40d1b4`, `a5295c2`
- Verification follow-up commits: `5ca2d14`, `f0e5d88`, `25d2acc`, `0ee7f5d`
- Code Verification: `PASS`
- Hardware Verification: `PENDING`

## Coding Standard

```text
Coding Standard:
03_Firmware/00_Doc/Standards/嵌入式C代码规范.md
Status: READ
```

本阶段未重新运行 CubeMX。IWDG 通过 HAL module/source、Keil 工程、Platform/Impl abstraction 和 DBGMCU debug freeze 手工接入；`otaWorker` 继续持有 Firmware Storage I/O ownership。

## Implementation Result

Implementation Plan Task 0→10 已完成，核心结果如下：

- Application：Watchdog capability、Runtime Health 状态机、strict Trial Confirm、Runtime Ready/Feed/Confirm handshake。
- Metadata：保留 `NONE / PENDING / TRIAL / ROLLBACK`，强化 A/B identity 和 state invariants；Confirm 与 Rollback 均保持原子提交边界。
- Bootloader：Pending install 与 Confirmed restore 共用验证/擦写/回读核心；未 Confirm 的 Trial boot 先持久化 `ROLLBACK`，再执行 confirmed image restore。
- Reset Cause：仅用于诊断记录，不替代 Trial/Rollback 状态决策。
- 临时生产测试代码：`NONE`。最终代码中没有 TEST ONLY hook、强制失败或强制复位行为。

## Automated Verification

### Host and Contract Tests

- S02/S04/S05/S07/S07A/S09 regression Host Tests：`PASS`。
- S10 Runtime Health、Lifecycle Confirm、Metadata Invariant、Boot Recovery、Rollback Transaction Host Tests：`PASS`。
- `05_Tools/Contracts` 中 Application/Bootloader/兼容性/调试/工具流程 contract：`PASS`；本轮重新执行的 Application workflow、Runtime Health、Watchdog、Task Lifecycle、Boot Decision、Transport、Compatibility、Core、Debug、Logic Analyzer 和 S04 isolation contract 均通过。
- S10 Boot Decision contract、GDB automation、CmBacktrace/fault diagnostics、tool sequence contract：`PASS`。
- S05C parser fixture：`PASS`。
- S05C SPI/I²C 项目断言：使用解析器夹具结果执行，`test_spi.ps1` 两套 SPI 夹具 `PASS`，`test_i2c.ps1` I²C 夹具 `PASS`；真实逻辑分析仪采集仍待板测。

### Python Tests

- Firmware pack tests：`2/2 PASS`。
- YMODEM tests：`24/24 PASS`。
- S04 persistence tests：`15/15 PASS`。
- 本轮重新执行结果保持一致：Firmware pack `2/2`、YMODEM `24/24`、S04 persistence `15/15`。

### Build Verification

- Application `05_Tools\\toolkit.bat build application`：`PASS`，0 errors / 0 warnings。
- Bootloader `05_Tools\\toolkit.bat build bootloader`：`PASS`，0 errors / 0 warnings。
- Application true clean raw UV4 build：`PASS`，0 errors / 0 warnings。
- Application Total ROM：`84616 bytes (82.63 KiB)`；`configTOTAL_HEAP_SIZE=24576`，startup stack `1024 bytes`。
- Bootloader Total ROM：`22372 bytes (21.85 KiB)`，低于 `64 KiB` 限制。

## Board Verification Boundary

本轮已取得部分真实目标板证据，但尚未完成完整 S10 板级验收。以下状态只依据本轮可回读的 RTT/GDB/传输结果，不把历史日志或发送端单方面成功当作通过：

| 项目 | 状态 |
| --- | --- |
| Factory Restore / Flash | `PARTIAL; S09 baseline PASS evidence exists; latest retry is blocked by Bootloader Soft-I2C BUSY/fault` |
| RTT capture / Reset Cause | `PARTIAL; formal NONE and Trial/Confirm RTT captured; latest Bootloader capture halts at Soft-I2C init` |
| GDB target session / snapshot | `PARTIAL; stable NONE and Trial/Confirm symbols read; latest snapshot stops in diagnostics_fault_entry` |
| IWDG timeout / debug freeze | `PENDING / NOT_EXECUTED` |
| Trial runtime Ready / strict Confirm | `PASS for runtime handshake; persistent post-power-cycle state pending` |
| software reset / IWDG reset / power-cycle | `PENDING / NOT_EXECUTED` |
| interrupted rollback and restart-from-zero | `PENDING / NOT_EXECUTED` |
| PA0 OTA/install input | `PASS; two PA0 actions reached install and Confirm flow` |
| LED / LCD manual observation | `PENDING / NOT_EXECUTED` |
| S05C real I2C/SPI capture | `PENDING / NOT_EXECUTED` |

### Real Target Evidence Collected

- Formal Application `NONE` startup after the Event Flags mapping fix: startup result fields are `PLATFORM_ERR_OK`, system state is `RUNNING`, Health is `STABLE`, and `trial=0`; GDB stopped at the FreeRTOS idle path without the previous controlled-reset loop.
- `app_v1.1.img` was rebuilt as `84680 bytes` (`84616-byte payload`, `83` YMODEM blocks). The real target transfer completed with `84680 bytes`, `retries=2`, sender exit code `0`; RTT reached `READY_TO_INSTALL` with `84680/84680` bytes.
- After the second PA0 action, GDB read `g_appHealthContext.readyMask=0x7`, `state=APP_HEALTH_STATE_STABLE`, `trial=1`, `g_appHealthConfirmResult=PLATFORM_ERR_OK`, and `g_appStartupContext.systemState=APP_SYSTEM_STATE_RUNNING`. This proves the runtime Ready/strict Confirm handshake, but does not by itself prove the persisted Metadata after a later power-cycle.
- The latest S09 Factory Restore retry reported sender-side success but the target receiver remained `ERROR/TIMEOUT` with `packetReceivedCount=0`; it is not counted as a new Factory Restore PASS. The earlier S09 baseline evidence remains S09-owned.
- Verification follow-up on 2026-09-20 reran the S10 Host Tests, static contracts, Python regressions and both Keil builds successfully; no production source was changed in this follow-up. Application ROM remains `84616 bytes (82.63 KiB)` and Bootloader ROM remains `22372 bytes (21.85 KiB)`.
- The Factory Restore workflow sequencing/recovery fix is committed as `0ee7f5d`. A subsequent clean Bootloader flash still produced fresh RTT `Soft-I2C init FAIL: BUSY` / `BOOT halt: external device init`; GDB stopped in `diagnostics_fault_entry` with the backtrace reaching `boot_soft_i2c_init()` line 287. `DIAG_FAULT_TEST_ENABLE` remains `0` and the clean Bootloader disassembly contains no test-trigger call, so this is recorded as a current board startup fault rather than S10 Fault Injection evidence.

Verification Role still needs a physical power-cycle/reset to recover the board, then the consolidated IWDG, software-reset, Trial/Rollback, rollback-interruption, and visual LED/LCD scenarios; Review Role must decide closure afterward. Until the board recovers, these remain `PENDING / NOT_EXECUTED`.

## S09 Deferred Boundary

S09 Deferred Fault Injection（erase/program 分段、Internal CRC、Metadata body/marker 和 Power Loss）本次未完成。最新 Factory Restore 重试的目标端 `worker result=2` / `ERROR/TIMEOUT` 证据不覆盖此前 baseline PASS，也不计入 S10；这些项目仍归属 S09 supplementary verification。

## Handoff Result

代码和自动化证据满足 Implementation Plan 的实现退出条件，阶段保持 `READY_FOR_VERIFICATION`。硬件验证仍为 `PARTIAL / PENDING`，阶段不得标记 `CLOSED`。本轮生成的构建缓存和 Python cache 仍可能存在于本地未跟踪状态，但未加入提交；本报告和 `verification_matrix.md` 是当前可回读证据入口。
