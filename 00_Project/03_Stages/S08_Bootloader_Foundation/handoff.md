# S08 Bootloader Foundation Handoff

## Metadata

- Stage: `S08_Bootloader_Foundation`
- Workflow Status: `READY_FOR_VERIFICATION`
- Implementation Status: `COMPLETE`
- Branch: `main`
- Baseline Commit: `fba93ad7ea36321d218790b50bcb493e95574b32`
- Implementation Baseline Commit: `f164f3c`
- Design Commit: `03406dcba9d57191bb1109da52fc23ab7a8c0d4f`
- Implementation Plan Commit: `05cb274ca3454f53ed47b577e5c72232a2d484ee`
- Implementation Commits: `8ec0045`
- Verification Commit: `pending commit`
- Review Commit: `Not created yet`
- Updated At: `2026-09-18`

## Required Reading

施工前按顺序读取：

```text
AGENTS.md
README.md
PROJECT_CONTEXT.md
00_Project/WORKFLOW.md
00_Project/02_Roadmap/development_roadmap.md
00_Project/03_Stages/S08_Bootloader_Foundation/design.md
00_Project/03_Stages/S08_Bootloader_Foundation/implementation_plan.md
00_Project/03_Stages/S08_Bootloader_Foundation/handoff.md
03_Firmware/AGENTS.md
03_Firmware/00_Doc/Standards/嵌入式C代码规范.md
相关 Keil / Build Output 规范
```

并读取 S07A handoff/review 作为 Application Startup Contract 输入。

## Upstream Input

S07A 已 `CLOSED / PASS`。

当前 Application Startup Contract：

```text
defaultTask (temporary)
→ app_system_bootstrap()
→ appMainTask + otaWorker + displayTask
→ RUNNING / DEGRADED / FAILED
→ steady FreeRTOS runtime
```

S07 已建立 OTA 下载与 durable PENDING，但 S08 不消费 PENDING。

已验证工具能力：

- Keil Build；
- J-Link Flash/Run；
- RTT capture；
- GDB runtime snapshot；
- CmBacktrace / Fault capture；
- Toolkit unified entry；
- Logic Analyzer SPI/I2C evidence。

## Current Bootloader Baseline

独立工程已创建：

```text
03_Firmware/Bootloader/OTA_Bootloader/
```

当前事实：

```text
MCU           STM32F411CEU6
Runtime       Bare-metal
RTOS          None
Library       STM32 HAL + CMSIS
Clock target  100 MHz generated baseline
SPI2          configured
FLASH_CS      PB12
SPI2_SCK      PB13
SPI2_MISO     PB14
SPI2_MOSI     PB15
Soft I2C SCL  PB6 GPIO Open-Drain
Soft I2C SDA  PB7 GPIO Open-Drain
```

SPI2 / PB6 / PB7 只为后续阶段预留；S08 不实现存储驱动。

## Frozen S08 Scope

只有三项：

```text
1 Flash Layout
2 Diagnostics
3 APP Jump
```

### Flash Layout

```text
Bootloader  0x08000000 / 0x00010000
Application 0x08010000 / 0x00070000
```

必须同步：

- Bootloader Keil IROM；
- Application Keil IROM；
- Application VTOR；
- `APP_BASE_ADDR`；
- map/hex/axf validation。

### Diagnostics

```text
SEGGER RTT
+
lightweight boot_log
+
CmBacktrace bare-metal
```

不使用 EasyLogger，不依赖 FreeRTOS。

RTT 与 CMB Vendor 可直接复制 Application 当前版本；CMB 配置和工程 Port 必须为 Bootloader 重写。

### APP Jump

```text
read MSP / Reset_Handler
→ validate
→ cleanup
→ VTOR
→ MSP
→ branch Reset_Handler
```

## Architecture Decision

Bootloader 不复制 Application 五层架构。

目标边界：

```text
Boot/
Diagnostics/
Config/
Vendor/
CubeMX Core/Drivers
```

后续 S09 存储驱动可加入 Drivers，但不得反向污染 S08 Boot Core。

## Non-goals

S08 禁止顺带实现：

```text
W25Q64
AT24C02
Software I2C
PENDING consume
Internal Flash installation
Trial
Confirm
Rollback
Watchdog
LCD
Ymodem
Security
FreeRTOS
EasyLogger
```

## Verification Expectations

实施完成后至少交付以下证据：

```text
Bootloader Clean Build
Application Clean Build
Flash layout / map evidence
Bootloader size evidence
RTT boot log
CmBacktrace controlled Fault
valid APP jump
invalid MSP reject
invalid Reset_Handler reject
Application FreeRTOS tick/interrupt/runtime after jump
repeat reset/power-cycle stability
GDB snapshot
```

## Toolkit Note

当前 Toolkit 主要围绕 Application 默认目标建立。S08 实施时允许做“支持 Bootloader/Application 双目标”所必需的最小配置扩展，但不得复制第二套 Build/Flash/GDB/RTT 工具链。

## Implementation Output

当前状态：

```text
READY_FOR_VERIFICATION
```

### Implementation Summary

已完成 S08 Task 1–8 的实施范围：

- 新增共享 `03_Firmware/Shared/memory_layout.h`，冻结 Bootloader `0x08000000 / 64 KiB` 与 Application `0x08010000 / 448 KiB`；
- 修正两个 Keil 工程的 CPU IROM、实际生效的 `OCR_RVCT4`、IRAM、Objects/Listings 输出目录和必要 Include/Define；
- Application `VTOR` 使用共享 `APP_BASE_ADDR`；
- Bootloader 建立精简 `Boot / Config / Diagnostics / Vendor` 目录，不引入 FreeRTOS 或 EasyLogger；
- 集成 SEGGER RTT、轻量 `boot_log`、Bare-metal CmBacktrace、Fault context 和受控 Fault 入口；
- 完成 APP MSP / Reset_Handler 合法性检查、SysTick/NVIC cleanup、VTOR barrier、MSP 设置和立即 Branch；
- 扩展现有 Toolkit 为双目标入口，并增加 CubeMX 重新生成后的 `sync-s08` 恢复流程；
- 已清理临时 Fault Injection、向量测试宏和构建调试产物，用户创建的 `MDK-ARM/Objects`、`Listings` 目录保留。

正式验证报告：`04_Test/Reports/Stages/S08_Bootloader_Foundation/verification.md`。

### Implementation Evidence

```text
Bootloader Build       PASS, 0 error / 0 warning
Application Build      PASS, 0 error / 0 warning
Bootloader map         LR 0x08000000, size 0x2DA8 / max 0x10000
Application map        LR 0x08010000, size 0x13F20 / max 0x70000
RTT / valid jump       PASS
CmBacktrace Fault      PASS
Invalid vector reject  PASS: MSP / Reset_Handler / ERASED
GDB APP snapshot       PASS: FreeRTOS prvIdleTask
```

当前需 Project Owner 补齐真实断电上电、LED/LCD 现场行为和一个 Application 外设中断路径确认；在这些证据完成前不得将 S08 标记为 `CLOSED`。

### Task 0 Baseline Evidence

采集日期：`2026-09-18`，Implementation Baseline Commit：`f164f3c`。

```text
Branch                  main
Worktree                clean before baseline build
Application Build       PASS, 0 error / 0 warning, 05_Tools/toolkit.bat build
Bootloader Build        PASS, 0 error / 0 warning, Keil V5.06 update 7
Bootloader Program Size Code=3514, RO-data=458, RW-data=16, ZI-data=1720
Bootloader Runtime      Bare-metal CubeMX baseline; no FreeRTOS or EasyLogger target dependency
Bootloader Scope        SPI2/PB6/PB7 preconfigured only; no storage driver implementation
```

Baseline output log：`06_Output/Logs/S08_bootloader_baseline_build.log`。

当前发现：Bootloader 工程仍为 CubeMX 空壳，IROM 尚未冻结；其输出目录为 `MDK-ARM/OTA_Bootloader/`，需要在 Task 1/2 修正为 `Objects/` 与 `Listings/`。

施工时持续填写：

- Baseline evidence；
- Design/Plan commits；
- 每个 Task 的 implementation commit；
- Build / RTT / GDB result；
- Flash layout evidence；
- Fault / jump evidence；
- Bootloader .map/.bin size；
- 已知问题；
- Verification handoff。

## Next Stage Contract

S08 完成后向 S09 提供：

```text
Independent Bootloader
+
Fixed Internal Flash Layout
+
RTT/CmBacktrace Diagnostics
+
APP Vector Validation
+
Reliable APP Jump
```

S09 再实现 External Flash / EEPROM / Metadata / Internal Flash Installation。
