# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S02_External_Flash_Driver`
- Status: `READY_FOR_REVIEW`
- Branch: `main`
- Baseline Code Commit: `5ac069f19c7f401f56e8fa5aad00c92a76aaaedf`
- Design Commit: `44fddb1484441a98d336df7783170267c166f4af`
- Plan Commit: `aee30916c5c784828269668d99a2b4e63689f80e`
- Implementation Branch Tip: `5bc4ccf7d0b367c37dfca45b5d3fbff83d2a1bed`
- Merge Commit: `c6c77a240fce463afa4c86d797bb52d2781fb651`
- Verification Commit: `df9ec99d411f83b3a2f5c21ccc9f13c6b5e0ac64`
- Current Role: `Review`
- Updated At: `2026-09-12`

## Current Goal

`S01_Application_Foundation` 已完成并关闭。

`S02_External_Flash_Driver` 已完成设计、实现、真实硬件板测和 Keil Build/Clean-Rebuild 验证，当前进入正式 Review。Review 只审核已冻结设计、实际代码差异、验证证据和范围边界，不继续新增功能。

S02 已建立：

```text
Application
   ↓
W25Q64 Raw Driver
   ↓
Platform SPI Device
   ↓
Platform SPI Bus
   ↓
STM32 SPI2 Impl
   ↓
W25Q64JV
```

并已通过真实 Read Back / Compare 证明基础 External SPI Flash 能力可用。

## S02 Completed Capabilities

### SPI Platform / Impl

- 新增同步阻塞 `platform_spi_read()`；
- 保持 Bus / Device / Transaction 模型；
- SPI1/SPI2 共用 `impl_platform_spi.c`；
- SPI1 使用 PCLK2，SPI2 使用 PCLK1；
- SPI1/SPI2 当前 SCK 均为 12.5 MHz；
- 第一版仍为阻塞 SPI，不引入 DMA/Interrupt/Async SPI。

### W25Q64 Raw Driver

V1 Opcode：

```text
0x9F Read JEDEC ID
0x05 Read Status Register-1
0x06 Write Enable
0x03 Read Data
0x02 Page Program
0x20 Sector Erase 4 KiB
```

已验证行为：

- JEDEC ID=`EF 40 17`；
- Read 可跨 Page/Sector；
- 原子 Page Program 不得跨 256 Byte Page；
- 连续 Write 自动 Page 拆分但不自动 Erase；
- Sector Erase 要求 4 KiB 对齐；
- Program/Erase 使用 Write Enable、WEL 和 BUSY 轮询；
- 无 Chip Erase 公共 API；
- Driver 不直接依赖 HAL 或 Service Log。

### Board Verification

S02 destructive test Sector：

```text
0x7FF000 ~ 0x7FFFFF
```

已通过：

- JEDEC / SR1；
- Sector Erase 后完整 4 KiB Read Back 为 `0xFF`；
- Single-page Program + Compare；
- `0x7FF0F0`、300 Byte Cross-page Write + Compare；
- 越界 Read/Write 拒绝；
- Atomic Page Program 跨页拒绝；
- Unaligned Sector Erase 拒绝；
- Reset Persistence；
- RTT + EasyLogger 运行时观测。

RTT PASS/FAIL 来自测试代码真实 Read Back / Compare，不是仅打印预设文本。

### Application Startup Correction

验证过程中修正了 Application 启动基础设施：

- `service_log_init()` 在 FreeRTOS 初始化阶段提前完成；
- CubeMX `defaultTask` 只启动 `app_system` 后退出；
- `app_system` 通过 Platform Thread API 使用独立任务栈运行 `app_main()`；
- App 层不直接依赖 CMSIS-RTOS。

### Reusable Keil Tooling

新增：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Config/toolchain.local.example.bat
```

其作用是给人工和 Agent 提供稳定的 Keil Build 入口：

```text
Agent / Developer
        ↓
build_app.bat
        ↓
toolchain.local.bat
        ↓
Keil UV4
        ↓
OTA_APP.uvprojx
        ↓
06_Output/Logs/OTA_APP_build.log
```

规则已写入 `AGENTS.md`。机器相关工具路径只存在被忽略的 `toolchain.local.bat` 中。该能力作为跨阶段工程资产保留。

## Verification Summary

- Code Verification：`PASS`；
- Normal Keil Build：`PASS`，0 Error、8 Warning；
- Keil Clean/Rebuild：`PASS`，Project Owner 已于 `2026-09-12` 实际执行并确认成功；
- Hardware Verification：`PASS`；
- Production destructive-test cleanup：`PASS`；
- SFUD boundary evaluation：完成，实际接入延后。

完整证据：

`04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md`

## Current Stage Documents

- Design: `00_Project/03_Stages/S02_External_Flash_Driver/design.md`
- Implementation Plan: `00_Project/03_Stages/S02_External_Flash_Driver/implementation_plan.md`
- Handoff: `00_Project/03_Stages/S02_External_Flash_Driver/handoff.md`
- Verification: `04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md`
- SFUD Evaluation: `00_Project/03_Stages/S02_External_Flash_Driver/sfud_evaluation.md`
- Review: `00_Project/03_Stages/S02_External_Flash_Driver/review.md`

## Required Reading

1. `AGENTS.md`
2. `README.md`
3. `00_Project/WORKFLOW.md`
4. `00_Project/01_Requirements/项目需求V1.md`
5. `00_Project/02_Roadmap/development_roadmap.md`
6. `00_Project/03_Stages/S02_External_Flash_Driver/design.md`
7. `00_Project/03_Stages/S02_External_Flash_Driver/implementation_plan.md`
8. `00_Project/03_Stages/S02_External_Flash_Driver/handoff.md`
9. `04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md`
10. `00_Project/03_Stages/S02_External_Flash_Driver/sfud_evaluation.md`
11. `00_Project/03_Stages/S02_External_Flash_Driver/review.md`
12. `00_Project/05_Status/current_status.md`

## Blockers

无 Verification 阻塞项。

## Next Action

执行 S02 Review：

1. 对照冻结 Design 和实际实现；
2. 检查 Baseline `5ac069f...` 到 Merge `c6c77a2...` 的差异；
3. 核查 SPI transaction、WEL/BUSY、Page/Sector 边界和分层依赖；
4. 核查真实硬件与 Keil Build/Clean-Rebuild 证据；
5. 确认 destructive test 已从生产工程移除；
6. 在 `review.md` 写入正式结论。

Review PASS 后再由 Project Owner 将 S02 标记为 `CLOSED`，然后进入 Roadmap 下一阶段。

## Prohibited Actions

- Review 前不直接关闭 S02；
- 不在 Review 阶段新增 SFUD、OTA、Bootloader 或其他后续实现；
- 不重新解释或降低已批准的 Acceptance Criteria；
- 不把非阻塞 Warning 误写成新的 S02 功能失败；
- 如发现实质设计偏差，应进入 `CHANGES_REQUESTED`，而不是修改冻结 Design 来迁就实现。
