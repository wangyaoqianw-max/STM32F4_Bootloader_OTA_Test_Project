# S07 OTA Service V1 Review

## Metadata

- Stage: `S07_OTA_Service_V1`
- Status: `IN_PROGRESS / HARDWARE_ACCEPTANCE_PENDING`
- Branch: `main`
- Design Commit: `a5c4c1b`
- Implementation Head Reviewed: `37a0d09`
- Verification Commit: `ade703e0d72bdef4a26cabd2afe36ecb86a09fa6`
- Review Commit: `9734a18`
- Review Date: `2026-09-18`
- Reviewer Role: `Review Role`

## Review Scope

本次 Review 对照 S07 冻结设计、implementation plan、实现差异、Task 10 verification、handoff、ADR 和工程 C 代码规范，检查 Metadata 语义、Service/worker 边界、Key 分层、生产依赖隔离、S06 Runtime 兼容性和真实硬件证据是否一致。

## Findings

### Blocking for Stage Closure

1. LCD `RECEIVING → VERIFYING → READY/FAILED` 全流程仍缺少操作者肉眼确认记录。
2. 最终 Project Owner 硬件验收/签字尚未完成。

这些是验收证据缺失，不是通过修改代码或文档可以消除的实现问题。因此阶段不得进入 `CLOSED / PASS`。

### Important Findings

无新增架构级 Important Finding。Task 10 发现的 100% 进度事件洪泛问题已在 `2ab34f1` 修复，并由 Service Host Test、Keil link 和 COM9 传输复核。

### Non-blocking Notes

1. Keil clean rebuild 当前为 `0 errors, 13 warnings`；警告来自既有 GPIO/UART/FreeRTOS 枚举边界检查，Toolkit 因 warning policy 返回 WARN。未将无关底层格式化或重构带入 S07。
2. `service_ota_init()` 只初始化 RAM Service 对象，Metadata 在 `service_ota_start()` 时加载；S07 不消费启动时的 `PENDING`，原始 EEPROM 持久化已通过临时只读板测回读。该边界与后续 Bootloader consumption 分离并已记录在 handoff。
3. 当前测试设备在重复 KEY 验收后保持 `READY_TO_INSTALL`，Metadata 为 `pendingSlot=NONE / upgradeState=NONE`；未执行 S09/S10 consume 或安装路径。

## Review Results

| Review Area | Result | Evidence |
| --- | --- | --- |
| Frozen A/B and Metadata semantics | PASS | ADR-0001、design、Metadata V2 tests |
| Service / worker boundary | PASS | `service_ota` 承担业务状态；`app_ota_worker` 保持 RTOS execution shell |
| Key ownership | PASS | Key 位于 `platform_bsp/key` / `impl_bsp`；MCU 层仅保留 IRQ |
| Production/test separation | PASS | Production 无 S05 Board Test sink 依赖 |
| Header-last and failure safety | PASS (code) | Production sink Host tests、Service Host tests |
| C code style | PASS for S07 changes | 已按 `嵌入式C代码规范.md` 复核新增/修改文件 |
| Host / Toolkit / Keil regression | PASS | verification.md；Keil 0 errors |
| COM9 transfer / durable PENDING | PASS | 真实 PA0 启动/确认、COM9 传输和 Metadata PENDING 回读均有证据 |
| READY reset / interrupted / bad CRC / duplicate KEY | PASS | 真实板 GDB 状态回读分别证明安全复位、Slot B INVALID、FAILED/no PENDING 和重复键忽略 |
| Physical LCD full-flow observation | PENDING | RTT/Display 初始化通过，RECEIVING/VERIFYING/READY/FAILED 肉眼记录待补 |
| Physical board acceptance | PENDING | 等 Project Owner 确认 |
| S09/S10 boundary | PASS | 未实现 Internal Flash Installation、Trial、Confirm、Rollback execution |

## Decision

```text
Implementation: PASS
Code Verification: PASS
Architecture: PASS
Regression: PASS
Hardware Verification: PARTIAL / PENDING
Stage: IN_PROGRESS
Closure: BLOCKED until the listed physical board evidence is completed
```

S07 代码和文档具备后续板测条件，但当前不能关闭阶段，也不能声明 `CLOSED / PASS`。
