# S02 External Flash Driver Review

## Metadata

- Stage: `S02_External_Flash_Driver`
- Review State: `PASS`
- Stage Status: `CLOSED`
- Branch: `main`
- Design Commit: `44fddb1484441a98d336df7783170267c166f4af`
- Plan Commit: `aee30916c5c784828269668d99a2b4e63689f80e`
- Baseline Code Commit: `5ac069f19c7f401f56e8fa5aad00c92a76aaaedf`
- Implementation Branch Tip: `5bc4ccf7d0b367c37dfca45b5d3fbff83d2a1bed`
- Merge Commit: `c6c77a240fce463afa4c86d797bb52d2781fb651`
- Rework Commit: `42c02b891d7f32b728857c44024c3c91a15ea604`
- Verification Commit: `c3527b3cd2fc10c3dd7fef89a0757f1199cd7c5d`
- Previous Review Commit: `3154c0c07f5041eb1ea2a2e9fdf524fe7ecba26a`
- Reviewed At: `2026-09-12`

## Final Review Scope

本次为 S02 的最终 Review。审核范围包括：

- 已批准的 `design.md` 与冻结的 `implementation_plan.md`；
- 原始实现及 Verification 证据；
- Previous Review `CHANGES_REQUESTED` 中唯一阻塞 Finding；
- Rework Commit `42c02b8...` 的 SPI 大长度传输修正；
- Host Test、Keil Build/Clean-Rebuild 与返工后真实硬件最小回归；
- Rework Commit 到当前 Review 前 HEAD 的后续差异，确认没有再次修改生产 SPI/W25Q64 行为；
- destructive test、machine-local toolchain 配置与阶段边界的最终清理状态。

## Previously Passed Items

以下 S02 主体能力在第一次 Review 中已经通过，本次复核后继续成立：

1. Platform SPI 保留 Bus / Device / Transaction 模型，只增加同步 `read()`；
2. SPI1/SPI2 共用 STM32 SPI Impl，SPI1 使用 PCLK2、SPI2 使用 PCLK1；
3. W25Q64 Driver 未直接依赖 HAL、`hspi2`、`SPI2` 或 Service Log；
4. JEDEC ID、SR1、Read、Page Program、Sector Erase 的 Opcode 与 24-bit 地址序正确；
5. 原子 Page Program 严格禁止跨 256 Byte Page；
6. 连续 Write 按 Page 拆分且不隐式 Erase；
7. Sector Erase 要求 4 KiB 对齐，不自动修正错误地址；
8. Program/Erase 使用 WREN、WEL 检查和 BUSY 轮询；
9. transaction begin 成功后的失败路径会尽力 transaction end，避免 CS/activeDevice 泄漏；
10. 地址范围校验使用减法形式避免加法溢出；
11. destructive board test 仅使用 `0x7FF000 ~ 0x7FFFFF`，且已从生产 Application/Keil 工程移除；
12. Erase / Program / Cross-page / Boundary / Reset Persistence 均由真实 Read Back / Compare 支撑；
13. `app_system` 启动修正未引入 App → CMSIS-RTOS 直接依赖；
14. `05_Tools/Scripts/build_app.bat` 已形成可复用 Keil Build 入口，`toolchain.local.bat` 当前不再由 Git 跟踪；
15. SFUD 仅完成边界评估，没有提前扩大 S02 实现范围。

## Finding 1 Closure — SPI HAL 0xFFFF Length Limit

### Previous Finding

Previous Review 发现 STM32 HAL SPI 的 `uint16_t Size` 限制被直接暴露成 Platform/W25Q64 公共接口的隐式 `65535 Byte` 上限，导致地址合法的 `>0xFFFF` Read 被拒绝，与冻结 Design 不一致。

Previous Review Result：`CHANGES_REQUESTED`。

### Rework Implementation

返工后 STM32 SPI Impl 在内部处理 HAL 单次传输限制：

```text
platform_size_t request
      ↓
while remaining > 0
    chunk = min(remaining, 0xFFFF)
    HAL_SPI_Transmit / HAL_SPI_Receive(chunk)
    data += chunk
    remaining -= chunk
```

审核确认：

- `stm32_spi_write()` 与 `stm32_spi_read()` 对称分块；
- `dataLength > 0xFFFF` 不再直接返回 `PLATFORM_ERR_OVERFLOW`；
- `0xFFFF` 仅作为 STM32 HAL Impl 内部 chunk 上限；
- Platform SPI 公共接口、`platform_size_t`、W25Q64 Driver、Bus/Device/Transaction 模型均未修改；
- HAL chunk 之间不会结束 Platform transaction，CS 继续由上层事务保持；
- 任一 chunk 的 `HAL_BUSY / HAL_TIMEOUT / HAL_ERROR` 会立即停止后续传输并保持原有错误映射；
- 未引入 DMA、Interrupt SPI、动态内存或范围外重构。

Finding 1：`CLOSED`。

## Rework Verification Review

### Host / Code Evidence

Host Test 对生产 `impl_platform_spi.c` 直接验证，18 项检查 `PASS`，覆盖：

```text
length = 1
length = 0xFFFF
length = 0x10000
length = 0x30000
HAL_TIMEOUT
HAL_BUSY
HAL_ERROR
NULL
length = 0
```

关键 chunk 结果：

```text
0xFFFF  → 0xFFFF
0x10000 → 0xFFFF + 1
0x30000 → 0xFFFF + 0xFFFF + 0xFFFF + 3
```

同一测试对返工前实现能够复现 Finding 1，因此该测试具备针对性的回归价值。

### Build Evidence

- Code Verification：`PASS`；
- Keil Normal Build：`PASS`；
- Keil Clean/Rebuild：`PASS`；
- 未发现返工引入的新编译错误；
- 既有 Warning 作为非阻塞技术债务保留。

### Hardware Regression Evidence

Project Owner 在真实硬件完成返工后的最小回归，RTT 证据包括：

```text
W25Q64 Init                      PASS
JEDEC ID = EF 40 17             PASS
SR1 Read                        PASS
Ordinary Read                   PASS
Persistence Marker Read         PASS
0x7FF0F0 / 300 Byte Read Back   PASS
```

该回归确认 SPI Impl 的返工没有破坏真实 W25Q64 链路。原始 S02 已通过的完整 destructive test 无需重复执行。

Hardware Regression：`PASS`。

## Final Difference / Scope Review

从 Rework Commit `42c02b8...` 到最终 Review 前的后续仓库变更仅涉及：

- S02 Verification / Handoff / Status / Project Context 的状态同步；
- machine-local `toolchain.local.bat` 停止跟踪；
- `toolchain.local.example.bat` 泛化；
- 临时收口计划清理。

未发现后续再次修改生产 SPI/W25Q64 实现的情况，因此 Finding 1 的关闭证据仍适用于当前生产代码。

## Non-blocking Observations

以下内容不阻塞 S02 关闭：

- `project_config.h` / `app_main.c` 个别文件头仍有 S01 文案，可后续统一清理；
- `.uvoptx` IDE 状态 churn 后续继续避免无意义提交；
- Storage SPI 跨 transaction 的 RTOS 并发锁应在正式并发/OTA 阶段设计；
- 既有 ARMCC Warning 继续作为技术债务管理；
- SFUD 实际集成继续延后到确有需求的后续阶段。

## Final Review Result

`PASS`

S02 已满足批准 Design 和 Acceptance Criteria：

- External SPI Flash 基础链路可用；
- W25Q64 Raw Driver V1 的 Read / Program / Erase / JEDEC / WEL / BUSY / Boundary 行为已实现并验证；
- Cross-page Write、Reset Persistence 和错误路径已有真实硬件证据；
- Previous Review Finding 1 已完成代码修正、Host 回归、Keil 构建和真实硬件回归；
- destructive test 已退出生产启动路径；
- 本阶段未越界实现 OTA、Bootloader 或 SFUD Middleware 集成。

## Closure Decision

`S02_External_Flash_Driver`：`CLOSED`

下一阶段按 Roadmap 进入 `S03_EEPROM_Storage` 的设计准备。S03 尚未正式启动前，不在本次 Review 中预先冻结其实现方案。
