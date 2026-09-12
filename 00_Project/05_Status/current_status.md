# Current Project Status

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

S02 的主体实现、Keil Build/Clean-Rebuild 和真实 W25Q64 板测均已完成。正式 Review 发现一个需要在关闭阶段前修正的接口语义问题，因此状态由 `READY_FOR_REVIEW` 进入 `CHANGES_REQUESTED`。

返工只处理 SPI 大长度传输语义，不重做已经通过的 W25Q64 功能，不扩大到 SFUD、OTA、Bootloader 或其他后续阶段。

## Review Finding

冻结 Design 规定 `platform_w25q64_read()` 可跨 Page/Sector，合法读取只受整个 8 MiB Flash 地址范围限制。

当前实现存在：

```text
platform_size_t = uint32
        ↓
platform_w25q64_read(dataLength)
        ↓
platform_spi_read(dataLength)
        ↓
stm32_spi_read()
        ↓
if dataLength > 0xFFFF
    PLATFORM_ERR_OVERFLOW
```

因此合法的 `65536 Byte` Read 会因为 HAL `uint16_t Size` 的 Impl 细节失败，和冻结 Platform/W25Q64 公共语义不一致。

详细 Review：

`00_Project/03_Stages/S02_External_Flash_Driver/review.md`

## Previously Verified S02 Capabilities

以下结果保持有效，不因本次 Review Finding 被否定：

- Platform SPI `read()` 和 SPI2 multi-instance；
- SPI1 PCLK2 / SPI2 PCLK1；
- W25Q64 JEDEC ID=`EF 40 17`；
- SR1 / WEL / BUSY；
- 4 KiB Sector Erase + 全量 Read Back `0xFF`；
- Single-page Program + Compare；
- `0x7FF0F0` 300 Byte Cross-page Write + Compare；
- 越界、跨页原子 Program、未对齐 Erase 拒绝；
- Reset Persistence；
- Code Verification / Normal Keil Build / Keil Clean-Rebuild；
- destructive board test 已从生产启动路径移除；
- SFUD Boundary Evaluation 已完成，实际集成延后。

## Reusable Tooling

以下工程资产继续保留：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Config/toolchain.local.example.bat
```

Agent/Developer 应继续优先通过统一脚本调用 Keil，本机路径仅放在被忽略的 `toolchain.local.bat`。

## Required Rework

优先修正 STM32 SPI Impl，使 `platform_spi_read()` / `platform_spi_write()` 的 32-bit `platform_size_t` 请求可在 Impl 内拆成多个 `<= 0xFFFF` 的 HAL blocking transfer，而不是把 HAL Size 限制泄漏给 Platform 调用者。

修正后至少验证：

1. `> 0xFFFF` Byte 请求不再直接返回 `PLATFORM_ERR_OVERFLOW`；
2. Keil Build / Clean-Rebuild；
3. W25Q64 JEDEC/普通 Read + 一个真实 Read Back Case，确认 transaction 没有回归；
4. 更新 `verification.md` 和 `handoff.md`；
5. 状态重新进入 `READY_FOR_REVIEW`。

## Non-blocking Items

- `project_config.h` / `app_main.c` 的文件头仍有 S01 文案，可后续清理；
- `.uvoptx` 有较大的 IDE 状态 churn，后续提交应尽量避免无意义变动；
- Storage SPI 跨 transaction 的 RTOS lock 留待正式并发阶段设计；
- 既有 Warning 继续按技术债务处理，不属于本轮返工。

## Blockers

- Review Finding：SPI Impl 单次 `0xFFFF` 长度限制与冻结 W25Q64 Read 语义不一致。

## Next Action

Implementation Role 针对 Review Finding 做最小修正和针对性验证。不得修改冻结 Design 来降低接口要求，也不得顺带扩展 SFUD、DMA SPI、OTA 或 Bootloader。
