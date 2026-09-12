# S02 External Flash Driver Review

## Metadata

- Stage: `S02_External_Flash_Driver`
- Review State: `CHANGES_REQUESTED`
- Stage Status: `CHANGES_REQUESTED`
- Branch: `main`
- Design Commit: `44fddb1484441a98d336df7783170267c166f4af`
- Plan Commit: `aee30916c5c784828269668d99a2b4e63689f80e`
- Implementation Branch Tip: `5bc4ccf7d0b367c37dfca45b5d3fbff83d2a1bed`
- Merge Commit: `c6c77a240fce463afa4c86d797bb52d2781fb651`
- Verification Commit: `df9ec99d411f83b3a2f5c21ccc9f13c6b5e0ac64`
- Review Commit: `This commit`
- Reviewed At: `2026-09-12`

## Review Entry Conditions

Review 入口条件均已满足：批准设计、冻结实施计划、实现合并、Code Verification、Keil Build、Keil Clean/Rebuild、真实 W25Q64 板测和 destructive-test 清理均已有证据。

## Review Scope

本轮对照以下内容进行审核：

- `design.md` 与 `implementation_plan.md`；
- Baseline `5ac069f...` 到 Merge `c6c77a2...` 的代码差异；
- Platform SPI / STM32 SPI Impl；
- W25Q64 Raw Driver / BSP；
- Application 启动修正；
- S02 Board Test 与 Verification Report；
- `05_Tools` Keil Build 入口；
- SFUD Boundary Evaluation。

## Passed Review Items

以下项目与冻结设计一致，可接受：

1. Platform SPI 保留 Bus / Device / Transaction 模型，只新增同步 `read()`；
2. SPI1/SPI2 共用同一个 STM32 SPI Impl，SPI1 使用 PCLK2、SPI2 使用 PCLK1；
3. W25Q64 Driver 未直接依赖 HAL、`hspi2`、`SPI2` 或 Service Log；
4. JEDEC ID、SR1、Read、Page Program、Sector Erase 的 Opcode 和 24-bit 地址顺序正确；
5. `platform_w25q64_page_program()` 严格禁止跨 256 Byte Page；
6. `platform_w25q64_write()` 按 Page 剩余空间拆分，未隐式 Erase；
7. Sector Erase 要求 4 KiB 对齐，不自动向下修正地址；
8. Program/Erase 每笔重新 Write Enable，并检查 WEL；完成条件通过 BUSY 轮询而非固定 Delay；
9. transaction begin 成功后，Read/Write/Program/Erase 路径均通过 finish/end 逻辑尽力释放 CS 和 `activeDevice`；
10. 地址范围校验采用 `length <= TOTAL_SIZE - address`，避免加法溢出；
11. destructive board test 只使用 `0x7FF000 ~ 0x7FFFFF`，并已从生产 Application/Keil 工程移除；
12. Hardware PASS 由真实 Read Back / Compare 支撑，包含 Erase、单页 Program、跨页 Write、边界拒绝和 Reset Persistence；
13. `app_system` 修正没有让 App 层直接依赖 CMSIS-RTOS，属于可接受的启动基础设施修正；
14. `build_app.bat + toolchain.local.bat` 将机器路径与仓库脚本隔离，适合作为跨阶段工程资产保留；
15. SFUD 只完成边界评估，没有提前扩大 S02 为 Middleware 集成阶段。

## Finding 1 — Important: SPI 单次 0xFFFF 限制违反 W25Q64 Read 冻结语义

### Evidence

冻结设计规定：

```text
platform_w25q64_read()
- Read 可跨 Page/Sector
- 只受整个 8 MiB 地址范围限制
```

同时：

- `platform_size_t` 为 32-bit；
- `platform_spi_read()` / `platform_spi_write()` 的 Platform 公共接口没有声明 `65535 Byte` 上限；
- STM32 SPI Impl 内部定义 `STM32_SPI_HAL_MAX_TRANSFER_SIZE = 0xFFFF`；
- 当 `dataLength > 0xFFFF` 时，`stm32_spi_read()` / `stm32_spi_write()` 直接返回 `PLATFORM_ERR_OVERFLOW`；
- `platform_w25q64_read()` 当前把完整 `dataLength` 一次传给 `platform_spi_read()`，没有拆分。

因此类似以下调用虽然完全位于 W25Q64 8 MiB 合法地址空间内：

```c
platform_w25q64_read(&flash, 0x000000U, buffer, 65536U);
```

仍会因为 STM32 HAL 的 `uint16_t Size` 实现细节返回 `PLATFORM_ERR_OVERFLOW`。

这使 Impl 层限制泄漏到 Platform/W25Q64 公共语义，与已批准 Design 不一致。

### Required Change

不得通过降低或修改冻结 Design 来规避问题。建议优先在 STM32 SPI Impl 内部把 `platform_size_t` 请求拆成多个 `<= 0xFFFF` 的 HAL blocking transfer，使 Platform API 对调用者继续保持 32-bit 长度语义；`write()` 与 `read()` 应保持对称行为。

如果选择在 W25Q64 Driver 内拆分，也必须保证：

- 单次 `platform_w25q64_read()` 的 CS 在整笔 Read Data 命令期间保持 Low；
- 后续 chunk 继续时不重新发送错误地址或提前结束 transaction；
- HAL 的 16-bit Size 限制不暴露到 W25Q64 公共接口。

### Required Verification

修正后至少补充：

1. `> 0xFFFF` Byte 的 Platform SPI/Flash Read 长度路径验证，证明不会直接 `OVERFLOW`；
2. Keil Build / Clean-Rebuild；
3. 现有 W25Q64 基础板测无需全部重做，但至少重新验证 JEDEC/普通 Read 和一个真实 Read Back Case，确认拆分修改没有破坏现有 SPI transaction 行为；
4. 更新 `verification.md` 与 `handoff.md`，记录修正 Commit 和验证结果。

## Non-blocking Observations

- `project_config.h`、`app_main.c` 的文件头 `@brief` 仍保留 S01 文案，后续文档/代码质量整理时可更新，不阻塞本次 Review；
- `.uvoptx` 存在较大 IDE 状态变动，后续提交应继续遵守“避免无意义 IDE churn”的 Git 规则；本轮未发现它造成 S02 功能错误；
- 当前 SPI Bus 没有跨多 transaction 的 RTOS 锁；S02 单线程/同步使用场景可接受，正式后台 OTA 并发策略应在 S06/SFUD 实际集成前设计。

## Review Result

`CHANGES_REQUESTED`

原因不是现有 W25Q64 板测失败，而是公共 Read 接口存在一个可复现的冻结设计不一致：合法的 `> 65535 Byte` Read 会被 STM32 Impl 的 HAL Size 限制拒绝。

本轮返工范围仅限于该长度语义及其验证，不要求重做 W25Q64 其他已通过能力，不扩大到 SFUD、OTA、Bootloader 或其他后续功能。

修正并完成针对性 Verification 后，重新进入 `READY_FOR_REVIEW`，Review Role 只需复核该 Finding 的关闭证据以及是否引入回归。