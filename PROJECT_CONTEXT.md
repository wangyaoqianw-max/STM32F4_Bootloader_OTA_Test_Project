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
- Verification Commit: `Not created yet`
- Review Commit: `3154c0c07f5041eb1ea2a2e9fdf524fe7ecba26a`
- Rework Commit: `42c02b891d7f32b728857c44024c3c91a15ea604`
- Current Role: `Review`
- Updated At: `2026-09-12`

## Current Goal

`S01_Application_Foundation` 已关闭。

`S02_External_Flash_Driver` 已完成主体实现、真实硬件验证和 Keil Build/Clean-Rebuild。

正式 Review 提出的唯一 Finding 已完成返工：STM32 HAL SPI 的 `uint16_t Size` 限制不再泄漏为
Platform/W25Q64 公共 API 的 `65535 Byte` 隐式上限。

SPI 大长度返工代码、Host/Build 验证和最小真实硬件回归均已完成，返工硬件证据为 PASS。
阶段现进入 `READY_FOR_REVIEW`。`review.md` 仍记录 `CHANGES_REQUESTED`，S02
关闭必须由 Review Role 独立重新执行。

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

返工前 STM32 Impl 对 `dataLength > 0xFFFF` 直接返回 `PLATFORM_ERR_OVERFLOW`，但
`platform_size_t` 是 32-bit，Platform SPI 公共接口也没有声明 65535 Byte 上限。

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

修正内容与证据：

1. `impl_platform_spi.c` 内新增 `stm32_spi_get_transfer_chunk_length()`，`stm32_spi_write()` /
   `stm32_spi_read()` 改为在 Impl 内按 `<= 0xFFFF` 拆分 HAL blocking transfer，删除
   `dataLength > 0xFFFF` 直接返回 `PLATFORM_ERR_OVERFLOW` 的分支；
2. 新增 Host Test `04_Test/Host/S02_External_Flash_Driver/`，覆盖 1 / 0xFFFF / 0x10000 / 0x30000
   四个长度、chunk 数据覆盖、失败即停和错误映射，18 项检查 `PASS`；同一测试对返工前文件为
   `FAIL`，可直接复现 Finding；
3. Keil Normal Build：`0 Error`、`1 Warning`（增量构建）；Keil Clean/Rebuild：`0 Error`、
   `8 Warning`，与返工前已记录的 warning 基线一致；
4. 板级最小回归已由 Project Owner 在真实硬件上确认 PASS；返工 Commit 已记录为
   `42c02b891d7f32b728857c44024c3c91a15ea604`。

## Reusable Keil Tooling

保留：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Config/toolchain.local.example.bat
```

其已作为 Agent/Developer 的统一 Keil Build 入口写入 `AGENTS.md`。`toolchain.local.bat`
属于 machine-local 配置，当前版本不再由 Git 跟踪，本机文件可以保留；仓库只保留
`toolchain.local.example.bat` 作为模板。

## Current Stage Documents

- Design: `00_Project/03_Stages/S02_External_Flash_Driver/design.md`
- Implementation Plan: `00_Project/03_Stages/S02_External_Flash_Driver/implementation_plan.md`
- Handoff: `00_Project/03_Stages/S02_External_Flash_Driver/handoff.md`
- Verification: `04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md`
- SFUD Evaluation: `00_Project/03_Stages/S02_External_Flash_Driver/sfud_evaluation.md`
- Review: `00_Project/03_Stages/S02_External_Flash_Driver/review.md`
- Status: `00_Project/05_Status/current_status.md`

## Next Action

1. Review Role 独立复核 Finding 1 的代码、Host/Build 和硬件回归证据；
2. `review.md` 继续记录 `CHANGES_REQUESTED`，待正式 Review 后决定 `CLOSED`、再次返工或阻塞。

返工证据见 `04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md` 的
“Rework Verification” 章节和 `04_Test/Host/S02_External_Flash_Driver/README.md`。

## Prohibited Actions

- 不修改冻结 Design 来声明 65535 Byte 为新上限；
- 不重做或重构已经通过的 W25Q64 主体功能；
- 不顺带引入 DMA/Interrupt SPI、SFUD、OTA、Bootloader；
- 不因为已有硬件板测 PASS 而忽略公共接口契约不一致。
