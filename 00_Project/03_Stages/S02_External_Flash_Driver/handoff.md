# S02 External Flash Driver Handoff

## Metadata

- Stage: `S02_External_Flash_Driver`
- Status: `READY_FOR_REVIEW`
- Branch: `main`
- Design Commit: `44fddb1484441a98d336df7783170267c166f4af`
- Plan Commit: `aee30916c5c784828269668d99a2b4e63689f80e`
- Baseline Code Commit: `5ac069f19c7f401f56e8fa5aad00c92a76aaaedf`
- Implementation Branch Tip: `5bc4ccf7d0b367c37dfca45b5d3fbff83d2a1bed`
- Merge Commit: `c6c77a240fce463afa4c86d797bb52d2781fb651`
- Verification Commit: `Not created yet`
- Review Commit: `3154c0c07f5041eb1ea2a2e9fdf524fe7ecba26a`
- Rework Commit: `42c02b891d7f32b728857c44024c3c91a15ea604`
- Current Role: `Review`

## Goal

建立并验证 W25Q64 Raw Driver V1。主体实现、真实硬件验证和 Keil Build/Clean-Rebuild 已完成。
SPI 大长度返工代码、Host/Build 验证和最小真实硬件回归均已完成，返工硬件证据为 PASS。
阶段现进入 `READY_FOR_REVIEW`。`review.md` 的 `CHANGES_REQUESTED` 历史结论
继续保留，直到下一次正式 Review。

## Rework Input（来自 review.md Finding 1）

```text
Design:
platform_w25q64_read() 只受整个 8 MiB 合法地址范围限制

Current Impl:
dataLength > 0xFFFF
→ PLATFORM_ERR_OVERFLOW
```

STM32 HAL SPI 的 `Size` 参数是 16-bit，Impl 不能把这个限制暴露给 32-bit 的
`platform_size_t` Platform API。`platform_w25q64_read(&flash, 0x000000U, buffer, 65536U)`
地址合法，但返工前会失败。

## Rework Output

### 修改的文件

```text
03_Firmware/Application/OTA_APP/04_Impl/impl_mcu/impl_platform_spi.c   （修正）
04_Test/Host/S02_External_Flash_Driver/s02_spi_chunking_host_test.c   （新增 Host Test）
04_Test/Host/S02_External_Flash_Driver/stubs/spi.h                    （新增 HAL 替身）
04_Test/Host/S02_External_Flash_Driver/README.md                      （新增运行说明）
04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md      （返工验证记录）
00_Project/03_Stages/S02_External_Flash_Driver/handoff.md             （本文件）
00_Project/05_Status/current_status.md
PROJECT_CONTEXT.md
```

本次收口还修正了工程状态和配置跟踪：`toolchain.local.bat` 属于 machine-local 配置，
当前版本不再由 Git 跟踪，本机文件可以保留；仓库只保留
`toolchain.local.example.bat` 作为模板。该文件曾出现在 Git 历史，本次不重写历史。

### Impl 内的拆分方式

```text
static platform_size_t stm32_spi_get_transfer_chunk_length(remaining)
    → (remaining > 0xFFFF) ? 0xFFFF : remaining

stm32_spi_write() / stm32_spi_read():
    currentData = data; remaining = dataLength;
    while (remaining > 0) {
        chunkLength = stm32_spi_get_transfer_chunk_length(remaining);
        HAL_SPI_Transmit/_Receive(chunkLength);  失败立即返回映射后的错误
        currentData += chunkLength; remaining -= chunkLength;
    }
```

约束遵守情况：

- 未修改 `platform_size_t`，未给 Platform SPI 公共 API 增加 65535 Byte 上限；
- 未在 W25Q64 Driver 内做 HAL chunk，未新增 `transfer()`，未改动 Bus / Device / Transaction 模型与 CS 逻辑；
- chunk 之间不调用 `transaction_end()`，CS 仍由上层 `transaction_begin/end` 保持；
- 任一 chunk 失败立即停止剩余传输并返回映射后的错误（`HAL_BUSY→BUSY`、`HAL_TIMEOUT→TIMEOUT`、`HAL_ERROR→IO`）；
- 未使用动态内存，未引入 DMA / Interrupt SPI，未重构无关 SPI / W25Q64 代码。

### 冻结 Implementation Plan 的偏离记录

`implementation_plan.md` Task 2 Step 4 仍写着 `dataLength > STM32_SPI_HAL_MAX_TRANSFER_SIZE → PLATFORM_ERR_OVERFLOW`。
该片段已被 Review Finding 1 判定为实现约束泄漏，本轮按 Review 要求删除该分支；
`implementation_plan.md` 作为冻结计划文档未改动，偏离在此记录并交回 Review/Project Owner 确认。

## Verification Evidence

| 项目 | 结果 | 证据位置 |
| --- | --- | --- |
| Host Test（HAL 替身） | `PASS`，18 项检查 | `04_Test/Host/S02_External_Flash_Driver/README.md` + verification.md |
| 返工前文件对照 | `FAIL`，10 项，复现 Finding 1 | verification.md |
| 静态语法检查 | `PASS`，`gcc -std=c99 -Wall -Wextra` | verification.md |
| Keil Normal Build | `PASS`，0 Error / 1 Warning | `06_Output/Logs/OTA_APP_build.log` |
| Keil Clean/Rebuild | `PASS`，0 Error / 8 Warning | `06_Output/Logs/OTA_APP_rebuild.log` |
| Hardware Regression | `PASS` | Project Owner 提供 RTT 实机日志 |

硬件最小回归范围（不需要重跑整个 destructive 套件）：

1. W25Q64 Init / JEDEC ID；
2. 普通 Read；
3. 至少一个 Read Back / Compare Case。

Sector Erase 全 4 KiB、300 Byte Cross-page Write、Reset Persistence 和 destructive 边界用例
在返工前已 PASS，本次未改动其代码路径，除非板测发现异常否则无需重做。

可选的针对性板级证据（本次未实现）：真实 `> 0xFFFF` Byte 读取需要在测试代码里准备
64 KiB 以上 Buffer，会显著占用 STM32F411 的 128 KiB SRAM；是否值得占用该 RAM 属于
Project Owner 的决策，不在本轮返工范围内自动实施。

## Pending / Not Verified

- `review.md` 仍记录 `CHANGES_REQUESTED`，正式 Review 需要再次独立执行，S02 尚未关闭。

## Coding Standard Review

```text
Coding Standard:
03_Firmware/00_Doc/Standards/嵌入式C代码规范.md
Status: READ
```

```text
Coding Standard Review: PASS
```

- 命名与文件组织沿用 `impl_platform_spi.c` 现有风格（`g_stm32SpiOps`、`stm32_spi_*`、`chunkLength`）；
- NULL / 长度 0 / Context 无效 / HAL Handle 未绑定均保持显式校验，返回码可诊断；
- chunk 拆分不引入新 Buffer、不复制数据，不使用动态内存；
- HAL 返回值继续经 `stm32_spi_map_hal_status()` 映射，不吞 `HAL_BUSY / HAL_TIMEOUT / HAL_ERROR`；
- 未修改 Vendor / 生成代码，未新增 Compiler Warning（Normal Build 1 Warning、Clean Rebuild 8 Warning 均为既有项）。

## Next Action

Project Owner 已完成以下最小板级回归并提供 RTT 日志：

1. W25Q64 Init；
2. JEDEC ID == `EF 40 17`；
3. 普通 Read 成功；
4. 既有持久化数据 Read Back / Compare 成功。

硬件回归结果为 `PASS`。最小回归测试代码已在板测完成后从 Application/Keil 工程移除，
生产启动路径不会自动执行该测试。下一步由 Review Role 独立复核并决定是否关闭 S02。

## Non-blocking Notes

- `project_config.h` / `app_main.c` 文件头仍有 S01 文案，可后续整理；
- `.uvoptx` IDE 状态 churn 后续尽量减少；
- Storage SPI RTOS 并发锁留待正式并发阶段设计；
- 既有 Warning 不属于本轮返工。
