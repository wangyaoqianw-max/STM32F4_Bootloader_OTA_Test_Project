# S09 Firmware Installation Verification Evidence

## Report Status

- Stage: `S09_Firmware_Installation`
- Report type: Implementation evidence / verification handoff draft
- Workflow status: `IN_PROGRESS`
- Branch: `main`
- Evidence date: `2026-09-18`
- Verification result: `PENDING`

本文件保存已完成的自动化和代码证据，不把未完成的真实板级安装、复位、断电和人工按键场景描述为通过。S09 不实现 S10 的 Confirm、Watchdog、Failure Counter 或 Rollback。

## Implementation Commits

```text
6e3fef5 test: add S09 factory restore workflow
c8e0c07 feat: add S09 bootloader firmware contracts
1f5b4d9 feat: add S09 bootloader storage drivers
fbe8e37 feat: add S09 flash gate and prevalidation
e4d6816 docs: complete S09 bootloader comments
69c426a feat: add S09 installer transaction
28feb5a feat: add S09 atomic trial commit
53acd8e feat: integrate S09 boot decision flow
```

## Code Verification

| Item | Result | Evidence |
| --- | --- | --- |
| Firmware Header / Metadata / CRC compatibility | PASS | `s09_contract_host_test.exe` → `S09 Bootloader contract host test passed.` |
| Candidate pre-validation gate | PASS | `s09_prevalidate_host_test.exe` → `no destructive API is linked.` |
| Installer transaction and fault injection | PASS | `s09_txn_host_test.exe` → `destructive gate and read-back verified.` |
| Atomic Metadata commit | PASS | `s09_metadata_commit_host_test.exe` → `atomic marker ordering verified.` |
| Bootloader Keil build | PASS | `OTA_Bootloader_build.log`，0 error / 0 warning |
| Application Keil build | PASS | 最终正式工程 0 error / 0 warning |
| Bootloader size | PASS | Map ROM `21104 bytes / 20.61 KiB`；BIN `11688 bytes / 11.41 KiB`；limit `65536 bytes` |
| Diff/style checks | PASS | `git diff --check`；新增 C 文件注释审查完成 |
| W25Q64 write/erase scope | PASS | Bootloader production driver仅保留 init/JEDEC/status/read；无写入/擦除 API |

## Existing Board Evidence

此前真实板自动流程已保存以下证据：

- `06_Output/Logs/S09_boot_rtt_gdb_client.log`：Bootloader `TRIAL` 状态跳过重复安装；
- `06_Output/Logs/S09_app_key_gdb_client.log`、`S09_app_confirm_gdb_client.log`：使用现有 GDB 合同触发 Application 事件，未修改生产代码；
- `06_Output/Logs/OTA_Bootloader_rtt.log`：`W25Q64 JEDEC EF 40 17`、外部设备初始化和 `NONE` Metadata 基线；
- `06_Output/Logs/S09_Factory_Restore/provision_rtt_raw.log`：一次历史 Factory Restore baseline PASS。

上述证据证明了相应单点行为，但不能替代当前完整 S09 正常安装验收。

## Current Board Result

今晚按“无需人工按键”的范围自动重试 Factory Restore 两次，均未完成：

```text
YMODEM sender: receiver did not send initial C
Board RTT:    worker result=2
```

`platform_error_t` 中 `2` 为 `PLATFORM_ERR_TIMEOUT`。板端 RTT 在 `destructive erase Slot A/B start` 后未到达 `YMODEM_READY`，因此本次不能证明 Factory baseline，也不能继续宣称当前板处于 v1.0 baseline。原始证据已另存：

```text
06_Output/Logs/S09_Factory_Restore/provision_rtt_failure_20260918.log
06_Output/Logs/S09_Factory_Restore/ymodem_failure_20260918.log
06_Output/Logs/S09_Factory_Restore/provision_flash_failure_20260918.log
```

需要 Project Owner/人工配合的按键、复位时序、断电和 Fault Injection 场景延期到下一次板测。

## Verification Decision

```text
代码验证：PASS
硬件验证：PENDING
阶段状态：IN_PROGRESS
```

未满足 `implementation_plan.md` 的 Task 8 / Task 9 Completion Gate，不推进 `CLOSED`。下一次验证从 Factory Restore 重新建立的 baseline 开始，并补齐 `PENDING → install → TRIAL` 以及 marker 前后 reset/power-loss 证据。
