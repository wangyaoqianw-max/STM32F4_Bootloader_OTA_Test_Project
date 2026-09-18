# S08 Bootloader Foundation Review

## Metadata

- Stage: `S08_Bootloader_Foundation`
- Status: `CLOSED / PASS`
- Branch: `main`
- Baseline Commit: `f164f3c`
- Design Commit: `03406dcba9d57191bb1109da52fc23ab7a8c0d4f`
- Implementation Plan Commit: `05cb274ca3454f53ed47b577e5c72232a2d484ee`
- Implementation Commit: `8ec0045`
- Verification Commits: `573312a`, `ac9abec`, `0c32781`
- Review Commit: `pending`
- Reviewed Head: `0c32781`
- Review Date: `2026-09-18`
- Reviewer Role: `Review Role`

## Review Scope

本次为 S08 阶段关闭前的轻量 Review。对照冻结的 Design、Implementation Plan、最终代码差异、Verification Report 和 Handoff，检查 Bootloader 基础能力、Application Jump 合同、验证证据以及阶段边界。

重点检查：

- Bootloader / Application Flash Layout、链接地址和 VTOR 是否一致；
- Bootloader 是否保持精简 Bare-metal，未引入 FreeRTOS / EasyLogger；
- APP Vector 校验、Runtime Cleanup 和 MSP / Reset_Handler 跳转路径；
- RTT / CmBacktrace 配置及受控 Fault 证据；
- Keil Build、Map、Size、RTT、GDB 和真实板测证据；
- 是否发生 S09 / S10 范围蔓延；
- Handoff、Verification、Project Context 和 current_status 是否一致。

## Findings

### Blocking Findings

无。

### Important Findings

无。

### Resolved Findings

1. 早期 GDB 跳转断点测试曾遗留 FPB comparator，产生 `HFSR=0x80000000` 的 `DEBUGEVT`。已读取并清除 `E0002008`–`E0002024` 及 `DEMCR` 后重新执行最终 Build / Flash / Jump / RTT / GDB 验证；该现象确认为调试器状态残留，不作为生产 Fault 证据。
2. Review 发现缺少 `.bin` 证据；已从最终 AXF 生成并回读 Bootloader `11688 bytes`、Application `81348 bytes` 的 `.bin`，均未超过对应 Flash 分区。
3. Review 发现阶段状态残留；已同步 `implementation_plan.md`、`handoff.md`、`verification.md`、`PROJECT_CONTEXT.md` 和 `current_status.md`。

### Non-blocking Notes

1. Bootloader 的 `boot_jump_set_msp_and_branch()` 使用独立汇编完成 `MSR MSP` 后立即 `BX`，其后没有普通 C、HAL 或日志调用，满足 S08 的 Cortex-M handoff 约束。
2. `DIAG_FAULT_TEST_ENABLE` 最终配置为关闭；受控 Fault 仅用于验证，未作为正式运行路径交付。
3. S08 只冻结 Flash Layout、诊断基础和 Application Jump；External Flash / EEPROM、Firmware Installation、PENDING 消费、Trial / Confirm / Rollback 仍属于后续阶段，未在本次实现中引入。

## Review Results

| Review Area | Result | Evidence |
| --- | --- | --- |
| Frozen Flash Layout | PASS | Shared `memory_layout.h`、两工程 linker/map：Boot `0x08000000 / 64 KiB`，APP `0x08010000 / 448 KiB`，区间相邻且不重叠 |
| Application VTOR | PASS | Application `SystemInit()` 使用共享 `APP_BASE_ADDR`，map `__Vectors = 0x08010000` |
| Bootloader architecture | PASS | Boot Core / Config / Diagnostics / Vendor 边界清晰，未复制 Application 五层架构 |
| Bare-metal dependency boundary | PASS | Bootloader 工程无 FreeRTOS / EasyLogger 依赖，CmBacktrace 使用 Bare-metal Cortex-M4 配置 |
| Vector validation | PASS | MSP、Thumb Reset_Handler、Application Flash range 和 erased vector 均有拒绝证据 |
| APP Jump cleanup | PASS | IRQ、SysTick、NVIC、PendSV/PendST 清理后设置 VTOR；汇编路径设置 MSP 并立即 branch |
| Diagnostics | PASS | SEGGER RTT boot log、CmBacktrace controlled Fault 的 RTT/GDB 交叉证据 |
| Build / map / bin / size | PASS | Bootloader/Application 均 0 error / 0 warning；Boot LR size `0x2da8`，Boot `.bin` `11688 bytes`，均小于 64 KiB |
| Board verification | PASS | Final Flash、合法 Jump、RTT、GDB、SysTick、3 次 Reset、断电上电、LED、LCD、Application 外设中断均确认通过 |
| Scope isolation | PASS | 未实现 S09/S10 的安装、元数据消费、回滚和确认功能 |
| Documentation consistency | PASS | Verification、Handoff、Implementation Plan、PROJECT_CONTEXT、current_status 和 Review 已同步到关闭状态 |

## Review Decision

```text
Implementation:       PASS
Architecture:         PASS
Code Verification:    PASS
Hardware Verification: PASS
Review:               PASS
Stage:                CLOSED / PASS
Closure:              APPROVED
```

实现本身无 Critical / Important 问题；S08 满足冻结设计和实施计划的验收条件，关闭为 `CLOSED / PASS`。下一计划阶段为 `S09_Firmware_Installation`；S09 功能不属于本次交付。
