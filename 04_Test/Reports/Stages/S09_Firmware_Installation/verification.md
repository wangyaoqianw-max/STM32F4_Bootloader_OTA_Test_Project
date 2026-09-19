# S09 Firmware Installation Verification Evidence

## Report Status

- Stage: `S09_Firmware_Installation`
- Report type: Implementation evidence / board verification handoff
- Workflow status: `IN_PROGRESS`
- Branch: `main`
- Evidence date: `2026-09-19`
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
| Application Keil build | PASS | 正式工程重新编译并烧录；恢复正式 Application 后 RTT 初始化 PASS |
| Bootloader size | PASS | Map ROM `21104 bytes / 20.61 KiB`；BIN `11688 bytes / 11.41 KiB`；limit `65536 bytes` |
| Diff/style checks | PASS | `git diff --check`；新增 C 文件注释审查完成 |
| W25Q64 write/erase scope | PASS | Bootloader production driver仅保留 init/JEDEC/status/read；无写入/擦除 API |

## Board Verification Evidence

本轮已取得以下真实硬件证据：

| 场景 | 结果 | 证据 |
| --- | --- | --- |
| Factory Restore baseline | PASS | `06_Output/Logs/S09_Factory_Restore/provision_rtt_raw.log`；Slot A v1.0 VALID、Slot B EMPTY、Metadata NONE |
| v1.1 YMODEM transfer | PASS | 现有 YMODEM 记录：80 blocks / 81412 bytes；存在重试但最终完成 |
| PENDING → install → TRIAL | PASS | `S09_install_rtt_output.log`：CRC/vector PASS、Metadata PENDING → TRIAL、sequence=62 |
| TRIAL reset | PASS | `S09_trial_reset_output.log`：`TRIAL state: skip reinstall` |
| Commit 前复位注入 | PASS | `S09_pretrial_reset_output.log` 命中 `boot_metadata_commit_trial` 入口并注入 reset；`S09_post_pretrial_capture_output.log` 随后恢复为 TRIAL 并跳转 |
| v1.1 运行现象 | PASS | 用户确认 LED 闪烁变慢；LCD V1.0 为既有硬编码文本，不作为 S09 阻塞项 |

最新 Factory Restore 重试仍未完成：PC 未收到初始 `C`，板端最终 `worker result=2`；该失败不覆盖前述成功 baseline。失败日志为：

```text
06_Output/Logs/S09_Factory_Restore/ymodem.log
06_Output/Logs/OTA_APP_rtt.log
```

失败路径已补充工具恢复动作：Factory Restore 失败后恢复源文件、重建并烧录正式 Application，避免临时测试固件继续留在板上；脚本语法检查 PASS，恢复后的正式 Application RTT 已确认正常初始化。

仍未取得真实板级证据的 Fault Injection 点：erase 后、program 约 25%/50%、program 完成、Internal CRC 前后、Metadata body 后/commit marker 前后、Power Loss。当前不能把未执行点描述为 PASS。

## Verification Decision

```text
代码验证：PASS
硬件验证：PENDING（部分板级场景 PASS）
阶段状态：IN_PROGRESS
```

未满足 `implementation_plan.md` 的 Task 8 / Task 9 Completion Gate，不推进 `CLOSED`，也不修改 S10 ownership。后续验证应从稳定 Factory baseline 开始，补齐 erase/program/CRC/Metadata marker/Power Loss checkpoints。
