# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S02_External_Flash_Driver`
- Status: `CHANGES_REQUESTED`
- Branch: `main`
- Baseline Code Commit: `5ac069f19c7f401f56e8fa5aad00c92a76aaaedf`
- Design Commit: `44fddb1484441a98d336df7783170267c166f4af`
- Plan Commit: `aee30916c5c784828269668d99a2b4e63689f80e`
- Implementation Branch Tip: `5bc4ccf7d0b367c37dfca45b5d3fbff83d2a1bed`
- Merge Commit: `c6c77a240fce463afa4c86d797bb52d2781fb651`
- Verification Commit: `df9ec99d411f83b3a2f5c21ccc9f13c6b5e0ac64`
- Review Commit: `3154c0c07f5041eb1ea2a2e9fdf524fe7ecba26a`
- Current Role: `Implementation`
- Updated At: `2026-09-12`

## Current Goal

`S01_Application_Foundation` 已关闭。

`S02_External_Flash_Driver` 已完成主体实现、真实硬件验证和 Keil Build/Clean-Rebuild，但正式 Review 发现一个需要返工的 Platform/Impl 长度语义问题，因此当前不能关闭阶段。

当前只修正这一项：STM32 HAL SPI 的 `uint16_t Size` 限制不能直接泄漏为 Platform/W25Q64 公共 API 的 `65535 Byte` 隐式上限。

## S02 Stable Results

以下能力已经实现并通过验证，返工时应保持：

- SPI Bus / Device / Transaction 模型；
- 同步 `platform_spi_read()`；
- SPI1/SPI2 共用 Impl；
- SPI1→PCLK2、SPI2→PCLK1；
- W25Q64 JEDEC/SR1/Read/Page Program/Cross-page Write/4 KiB Sector Erase；
- WEL + BUSY；
- Page/Sector/地址边界保护；
- destructive test 仅使用 `0x7FF000 ~ 0x7FFFFF`；
- Erase / Program / Cross-page / Boundary / Reset Persistence 板测 PASS；
- destructive test 已从生产工程移除；
- `app_system` Application Thread 启动修正；
- SFUD 只完成边界评估，未实际集成。

## Review Finding

冻结 Design 对 W25Q64 Read 的语义：

```text
Read 可跨 Page / Sector
只受整个 8 MiB 地址范围限制
```

当前链路：

```text
platform_w25q64_read()
  → platform_spi_read()
  → stm32_spi_read()
```

STM32 Impl 对 `dataLength > 0xFFFF` 直接返回 `PLATFORM_ERR_OVERFLOW`，但 `platform_size_t` 是 32-bit，Platform SPI 公共接口也没有声明 65535 Byte 上限。

所以一个地址合法的 `65536 Byte` W25Q64 Read 会失败，属于冻结设计与实现不一致。

正式结论见：

`00_Project/03_Stages/S02_External_Flash_Driver/review.md`

## Required Rework

推荐在 STM32 SPI Impl 内部对较长请求进行 HAL chunking：

```text
platform_size_t request
      ↓
while remaining > 0
    chunk = min(remaining, 0xFFFF)
    HAL_SPI_Transmit / HAL_SPI_Receive(chunk)
```

整个 Platform transaction 的 CS 仍由上层 `transaction_begin/end` 管理，不因 HAL chunk 被切断。

`read()` 与 `write()` 建议保持对称，避免相同的 Impl 限制继续从另一个方向泄漏。

修正后：

1. 增加 `>0xFFFF` 长度路径的针对性验证；
2. Keil Build / Clean-Rebuild；
3. 至少重新跑 JEDEC / 普通 Read / 一个真实 Read Back Case；
4. 更新 `verification.md`、`handoff.md`；
5. 返回 `READY_FOR_REVIEW`。

## Reusable Keil Tooling

保留：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Config/toolchain.local.example.bat
```

其已作为 Agent/Developer 的统一 Keil Build 入口写入 `AGENTS.md`，本机实际路径继续只保存在被忽略的 `toolchain.local.bat`。

## Current Stage Documents

- Design: `00_Project/03_Stages/S02_External_Flash_Driver/design.md`
- Implementation Plan: `00_Project/03_Stages/S02_External_Flash_Driver/implementation_plan.md`
- Handoff: `00_Project/03_Stages/S02_External_Flash_Driver/handoff.md`
- Verification: `04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md`
- SFUD Evaluation: `00_Project/03_Stages/S02_External_Flash_Driver/sfud_evaluation.md`
- Review: `00_Project/03_Stages/S02_External_Flash_Driver/review.md`
- Status: `00_Project/05_Status/current_status.md`

## Next Action

Implementation Role 只处理 Review Finding，不扩大功能范围。修正和验证完成后重新提交 Review。

## Prohibited Actions

- 不修改冻结 Design 来声明 65535 Byte 为新上限；
- 不重做或重构已经通过的 W25Q64 主体功能；
- 不顺带引入 DMA/Interrupt SPI、SFUD、OTA、Bootloader；
- 不因为已有硬件板测 PASS 而忽略公共接口契约不一致。
