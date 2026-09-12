# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S02_External_Flash_Driver`
- Status: `READY_FOR_VERIFICATION`
- Branch: `codex/s02-external-flash-driver`
- Baseline Code Commit: `5ac069f19c7f401f56e8fa5aad00c92a76aaaedf`
- Design Commit: `44fddb1484441a98d336df7783170267c166f4af`
- Plan Commit: `aee30916c5c784828269668d99a2b4e63689f80e`
- Implementation Commit: `82573b2532cc2ca7645d9a6698eefcca3234f867`
- Current Role: `Verification`
- Updated At: `2026-09-12`

## Current Goal

`S01_Application_Foundation` 已完成并关闭。`S02_External_Flash_Driver` 设计已批准，当前进入施工准备完成状态。

S02 要建立：

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

阶段完成后应具备经过真实硬件验证的 External SPI Flash 原始访问能力，为后续 Firmware Image Storage、Ymodem 和 OTA Service 提供存储基础。

当前实现已完成 Task 1-5，代码验证和自动 Keil Build 已通过；真实硬件验证、Keil Clean/Rebuild 和阶段 Review 尚未完成。默认破坏性板测门禁为 `0U`。

## Stable Baseline From S01

- `App -> Service -> Platform -> Impl -> Vendor` Application 分层；
- 收口后的 Config；
- STM32F411 当前板级 Status LED / Software I2C 绑定；
- FreeRTOS 基础运行环境；
- RTT + EasyLogger + Service Log；
- 最小 `app_main` 和 LED Blink；
- Keil `Objects/` / `Listings/` 构建输出规范；
- CubeMX regenerate、Clean/Rebuild、J-Link、LED、RTT 与连续 Reset 板测基线。

## S02 Frozen Design

### Hardware

- W25Q64JVSSIQ，8 MiB；地址 `0x000000 ~ 0x7FFFFF`。
- Page=256 Byte；Sector=4096 Byte；Erase value=`0xFF`。
- SPI2：PB13 SCK、PB14 MISO、PB15 MOSI。
- PB12=`FLASH_CS`，Low Active。
- SPI2 Mode 0、MSB First、8-bit、Full Duplex、Software NSS。
- APB1=50 MHz，SPI2 Prescaler=/4，SCK=12.5 MHz。
- SPI1 APB2=100 MHz，Prescaler=/8，SCK=12.5 MHz。

### SPI Platform/Impl

- 保留现有 Bus / Device / Transaction 架构。
- 仅增加同步阻塞 `platform_spi_read()`；不增加 `transfer()`。
- SPI1/SPI2 共用 `impl_platform_spi.c`，使用不同 Context 绑定 `hspi1/hspi2`。
- SPI1 实际时钟取 PCLK2；SPI2 取 PCLK1。
- 第一版不使用 DMA/Interrupt SPI，也不运行时重配 SPI。

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

约束：

- JEDEC ID 预期 `EF 40 17`。
- Read 可跨 Page/Sector。
- 原子 Page Program 不得跨 256 Byte Page。
- 连续 Write 自动 Page 拆分，但不自动 Erase。
- Sector Erase 地址必须 4 KiB 对齐，不自动向下对齐。
- 每次 Program/Erase 前 Write Enable，并确认 WEL。
- Program/Erase 通过 SR1.BUSY 轮询完成。
- V1 不提供 Chip Erase。
- Driver 不直接依赖 HAL 或 Service Log。

### Board Verification

专用可破坏测试区：

```text
0x7FF000 ~ 0x7FFFFF
```

RTT + EasyLogger 是主要运行时观测接口，但 PASS/FAIL 必须来自真实数据检查：

- Erase PASS → 4 KiB Read Back 全 `0xFF`；
- Program PASS → Read Back 与源 Buffer Compare 一致；
- Cross-page PASS → 从 `0x7FF0F0` 写 300 Byte 后完整 Compare；
- Reset persistence PASS → Reset 后重新读取仍一致。

逻辑分析仪仅在 SPI transaction 诊断需要时使用。

## Implementation Plan

按以下顺序执行：

1. Platform SPI `read()` + SPI2 multi-instance + PCLK1/PCLK2 修正；
2. Flash CS / Storage SPI BSP + W25Q64 init/JEDEC/SR1/Read；
3. Write Enable/BUSY + Page Program + Sector Erase；
4. 连续跨页 Write + 边界负向测试；
5. RTT 板测入口 + Reset persistence + destructive test 收口；
6. 记录 Verification 输入并评估 SFUD 边界。

逐步细节只以：

`00_Project/03_Stages/S02_External_Flash_Driver/implementation_plan.md`

为准。

## Current Stage Documents

- Design: `00_Project/03_Stages/S02_External_Flash_Driver/design.md`
- Implementation Plan: `00_Project/03_Stages/S02_External_Flash_Driver/implementation_plan.md`
- Handoff: `00_Project/03_Stages/S02_External_Flash_Driver/handoff.md`
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
9. `00_Project/05_Status/current_status.md`
10. `03_Firmware/AGENTS.md`
11. `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`
12. `02_Hardware/Hardware_Software_Interface/W25Q64JVSSIQ_外部Flash硬件软件接口参考.md`
13. `03_Firmware/Application/OTA_APP/OTA_APP.ioc`
14. 当前 SPI Platform/Impl、BSP SPI/GPIO 与 `app_main.c`。

## Blockers

实现无已知代码阻塞；当前仍等待 Keil Clean/Rebuild 和真实开发板验证证据。

## Next Action

进入 S02 Verification Role：补做 Keil Clean/Rebuild 和两次启动板测，回填 `04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md`；证据完整后交 Review Role，由 Review Role 决定 `PASS / CHANGES_REQUESTED / BLOCKED`，不得跳过 Verification 直接关闭 Stage。

## Prohibited Actions

- 不重新解释或绕过已批准的 S02 Design。
- 不新增 Chip Erase、Dual/Quad、DMA/Interrupt SPI 或异步 API。
- 不让 Raw Driver 自动 Erase 或自动修正 Sector 地址。
- 不提前实现 OTA Slot、Firmware Metadata、Ymodem、OTA Service、Bootloader 或 Security。
- 不把 RTT 文本本身当作数据校验结果。
- 如 SFUD 实际适配要求改变批准的 SPI 抽象，必须返回 Design Role，而不是在施工阶段私自扩展架构。
