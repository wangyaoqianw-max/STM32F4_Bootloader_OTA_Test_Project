# S08 Bootloader Foundation Implementation Plan

## Metadata

- Stage: `S08_Bootloader_Foundation`
- Design: `00_Project/03_Stages/S08_Bootloader_Foundation/design.md`
- Baseline Commit: `fba93ad7ea36321d218790b50bcb493e95574b32`
- Status: `CLOSED / PASS`
- Branch: `main`

## Global Constraints

- 本阶段只做 Flash Layout、诊断基础、APP 跳转三项能力。
- Bootloader 保持 bare-metal，不引入 FreeRTOS。
- MCU 普通外设继续 HAL；Cortex-M 核心操作使用 CMSIS/必要寄存器。
- 不移植 EasyLogger。
- RTT / CmBacktrace Vendor 可直接复制 Application 当前已验证版本。
- CmBacktrace 必须改为 Bare-metal Cortex-M4 配置。
- 不实现 W25Q64、AT24C02、Software I2C、PENDING、Firmware Install、Trial/Confirm/Rollback。
- 不复制 Application 五层架构。
- 不在 `main.c` 堆积 Bootloader 核心实现。
- `__set_MSP()` 之后不得调用普通 C/HAL/日志函数。
- 修改 Keil XML 时必须修改真实 Target memory 配置，不得只替换 Device 描述字符串。
- 每个关键任务完成后 Build / diff check；最终必须真实板验证。

## Task 0: Baseline and Project Audit

**Goal:** 固定新建 Bootloader 工程的实施基线。

- [ ] 记录 HEAD / branch / worktree。
- [ ] 检查 `OTA_Bootloader.ioc`、Keil project、startup、system file、main。
- [ ] 确认无 FreeRTOS。
- [ ] 确认 SPI2 与 PB6/PB7 只是预配置，不作为 S08 依赖。
- [ ] Bootloader 默认工程 Clean Build baseline。
- [ ] Application 当前工程 Clean Build baseline。
- [ ] 记录两个工程当前 .map/.bin/AXF 状态。
- [ ] 提交/记录 Baseline evidence；不得改业务功能。

## Task 1: Freeze Internal Flash Layout

**Goal:** 将 Bootloader / Application 地址合同写入工程并可由 linker 证明。

冻结：

```text
Bootloader : 0x08000000 / 0x00010000
Application: 0x08010000 / 0x00070000
```

- [ ] 新增 Bootloader `Config/memory_layout.h` 或等价单一地址合同。
- [ ] 修改 Bootloader Keil IROM 为 64 KiB。
- [ ] 修改 Application Keil IROM 为 `0x08010000 / 448 KiB`。
- [ ] 修改 Application Vector Table 到 `0x08010000`。
- [ ] 确认 Application linker / VTOR 一致。
- [ ] Clean Build 两工程。
- [ ] 解析 .map/.hex/axf，确认无地址重叠。
- [ ] 记录 Flash size。
- [ ] 提交并写回 handoff。

## Task 2: Establish Minimal Bootloader Structure

**Goal:** 将自研逻辑从 CubeMX Core 中分离。

目标最小目录：

```text
Boot/
Config/
Diagnostics/
Vendor/
```

- [ ] 新增 Boot Core 文件骨架。
- [ ] 新增 Config。
- [ ] 新增 Diagnostics。
- [ ] 更新 Keil Groups / Include Paths。
- [ ] CubeMX 生成文件保持可识别边界。
- [ ] 不建立 App/Service/Platform/Impl 五层。
- [ ] Clean Build。
- [ ] 提交并写回 handoff。

## Task 3: Integrate SEGGER RTT and Lightweight boot_log

**Goal:** Bootloader 在无 EasyLogger/RTOS 情况下具备稳定普通日志。

- [ ] 从 Application 复制当前已验证 RTT Vendor 文件。
- [ ] 添加 Keil source/include。
- [ ] 实现 `boot_log` 最小 INFO/WARN/ERROR API。
- [ ] 底层仅使用 SEGGER RTT。
- [ ] 日志支持编译期开关/最小等级裁剪。
- [ ] main 仅调用诊断初始化与 Boot Core 入口。
- [ ] Build。
- [ ] Flash 后通过现有 RTT capture 看到 `BOOT start`。
- [ ] 提交并写回 handoff。

## Task 4: Integrate Bare-metal CmBacktrace

**Goal:** 复用已有 CMB 能力，但去除 Application/FreeRTOS 依赖。

- [ ] 从 Application 复制 CmBacktrace Vendor。
- [ ] 创建 Bootloader 自己的 `cmb_user_cfg.h`。
- [ ] 使用 `CMB_USING_BARE_METAL_PLATFORM`。
- [ ] CPU = `CMB_CPU_ARM_CORTEX_M4`。
- [ ] Fault raw output = SEGGER RTT。
- [ ] `cm_backtrace_init()` 改为 Bootloader identity。
- [ ] 建立 HardFault/MemManage/BusFault/UsageFault handoff。
- [ ] 解决 CubeMX `stm32f4xx_it.c` 重复符号问题。
- [ ] 不移植 Application 的 FreeRTOS diagnostics framework。
- [ ] Build。
- [ ] 至少一个受控 Fault 完成 RTT/CMB 真实板证据。
- [ ] 提交并写回 handoff。

## Task 5: Implement APP Vector Validation

**Goal:** 在跳转前拒绝明显非法 Application vector。

- [ ] 读取 `APP_BASE_ADDR + 0x00` MSP。
- [ ] 读取 `APP_BASE_ADDR + 0x04` Reset_Handler。
- [ ] MSP SRAM range/alignment check。
- [ ] Reset_Handler Thumb bit check。
- [ ] Reset_Handler Application Flash range check。
- [ ] invalid/erased vector reject。
- [ ] 返回明确 validation result/reason。
- [ ] 使用 boot_log 输出必要诊断。
- [ ] Host/纯函数测试可行处增加边界测试。
- [ ] Build。
- [ ] 提交并写回 handoff。

## Task 6: Implement Runtime Cleanup and APP Jump

**Goal:** 完成 Cortex-M handoff，不把 Bootloader 状态污染带入 APP。

- [ ] 完成所有日志后再进入不可返回 jump path。
- [ ] disable global IRQ。
- [ ] stop / clear SysTick。
- [ ] disable NVIC enable bits。
- [ ] clear NVIC pending bits。
- [ ] 清理由 Bootloader 实际初始化的必要 peripheral/core state。
- [ ] `SCB->VTOR = APP_BASE_ADDR`。
- [ ] 必要 barrier。
- [ ] 设置 MSP。
- [ ] 立即 branch to APP Reset_Handler。
- [ ] `__set_MSP()` 后不得再调用 C/HAL/log。
- [ ] Build。
- [ ] GDB 检查 jump 前 vector / VTOR / MSP。
- [ ] 提交并写回 handoff。

## Task 7: Real-board Verification

**Goal:** 证明 Bootloader 能可靠启动已重定位的 S07A Application。

### Normal

- [ ] Program Bootloader。
- [ ] Program Application 到 `0x08010000`。
- [ ] Reset / Power-cycle。
- [ ] RTT 观察 Bootloader startup / validation / jump。
- [ ] Application 正常启动。
- [ ] FreeRTOS tick 正常。
- [ ] appMainTask / otaWorker / displayTask Runtime 正常。
- [ ] LED / LCD 基础行为正常。
- [ ] 至少一个 Application interrupt path 正常。
- [ ] 重复 Reset 验证稳定性。

### Negative

- [ ] invalid MSP reject。
- [ ] invalid Reset_Handler reject。
- [ ] erased/invalid APP vector reject。
- [ ] reject 后保持稳定诊断状态，不随机跳转。

### Diagnostics

- [ ] CMB controlled Fault evidence。
- [ ] GDB snapshot 证明 APP Runtime 正常。
- [ ] J-Link ownership / cleanup 遵守现有 Toolkit contract。

## Task 8: Tooling and Documentation Closure Preparation

**Goal:** 让现有 Toolkit 能同时服务 Bootloader/Application，并准备 Verification/Review。

- [ ] 评估现有 `toolkit.bat build/flash/run/rtt/snapshot` 对双工程的配置能力。
- [ ] 只做 S08 必需的最小 target/config 扩展；不得复制第二套工具链。
- [ ] 保持 Legacy wrappers 为薄包装。
- [ ] 更新 handoff implementation output。
- [ ] 新建/更新 S08 verification report。
- [ ] 记录 Bootloader .map/.bin 使用量。
- [ ] 记录 Implementation Commits。
- [x] 状态已推进到 `READY_FOR_VERIFICATION`，由 Verification Role 完成正式证据。
- [x] Verification / Review 已通过，阶段关闭为 `CLOSED / PASS`。

## Final Verification Target

```text
Reset
↓
Bootloader @ 0x08000000
↓
RTT/CmBacktrace diagnostics available
↓
Read APP vector @ 0x08010000
↓
Validate MSP + Reset_Handler
├─ invalid → reject + diagnostic state
└─ valid
    ↓
    cleanup SysTick/NVIC/peripheral state
    ↓
    VTOR = 0x08010000
    ↓
    MSP = APP initial MSP
    ↓
    APP Reset_Handler
    ↓
    SystemInit / C runtime / main / HAL / FreeRTOS
    ↓
    S07A Application Runtime normal
```

## Completion Condition

必须同时满足：

- Flash Layout 有 linker/map 证据；
- RTT / boot_log PASS；
- Bare-metal CmBacktrace PASS；
- 合法 APP jump PASS；
- 非法 APP reject PASS；
- APP 跳转后 interrupts / FreeRTOS Runtime PASS；
- Bootloader <= 64 KiB；
- 无 S09/S10 范围蔓延；
- Verification / Review 完整后再关闭阶段。
