# S02 External Flash Driver Handoff

## Metadata

- Stage: `S02_External_Flash_Driver`
- Status: `CLOSED`
- Branch: `main`
- Design Commit: `44fddb1484441a98d336df7783170267c166f4af`
- Plan Commit: `aee30916c5c784828269668d99a2b4e63689f80e`
- Baseline Code Commit: `5ac069f19c7f401f56e8fa5aad00c92a76aaaedf`
- Implementation Branch Tip: `5bc4ccf7d0b367c37dfca45b5d3fbff83d2a1bed`
- Merge Commit: `c6c77a240fce463afa4c86d797bb52d2781fb651`
- Rework Commit: `42c02b891d7f32b728857c44024c3c91a15ea604`
- Verification Commit: `c3527b3cd2fc10c3dd7fef89a0757f1199cd7c5d`
- Previous Review Commit: `3154c0c07f5041eb1ea2a2e9fdf524fe7ecba26a`
- Final Review Commit: `b5e5e8a00ba4a9c08b5677d364883e3722bfa258`
- Closed At: `2026-09-12`

## Goal

建立并验证 STM32F411 Application 侧 W25Q64 External SPI Flash Raw Driver V1，为后续 Firmware Image Storage、OTA 下载和 Bootloader 安装提供可靠的原始非易失存储基础。

## Final Implementation Output

S02 已完成并关闭，最终交付能力包括：

- Platform SPI 同步 `read()`；
- SPI1/SPI2 共用 STM32 Impl；
- SPI1 PCLK2 / SPI2 PCLK1 时钟来源修正；
- Storage SPI Bus 与 Flash CS BSP；
- W25Q64 Init/Deinit；
- JEDEC ID / SR1；
- Continuous Read；
- WREN / WEL / BUSY；
- 严格 Page Program；
- 自动跨 Page 的连续 Write；
- 4 KiB Sector Erase；
- 地址、Page、Sector 边界保护；
- RTT + EasyLogger 板级诊断；
- SFUD Boundary Evaluation；
- Agent/Developer 统一 Keil Build 入口。

## Final Verification Output

原始 S02 验证：

```text
Code Verification             PASS
Keil Normal Build             PASS
Keil Clean/Rebuild            PASS
JEDEC ID = EF 40 17           PASS
Sector Erase + 4 KiB FF       PASS
Single-page Program           PASS
Cross-page Write              PASS
Boundary Cases                PASS
Reset Persistence             PASS
Production Test Cleanup       PASS
```

Previous Review Finding 1 的返工验证：

```text
SPI >0xFFFF Host Regression    PASS
HAL Error Mapping Regression  PASS
Keil Build / Clean-Rebuild    PASS
Real Hardware Regression      PASS
```

真实硬件最小回归确认：

- W25Q64 Init：`PASS`；
- JEDEC ID `EF 40 17`：`PASS`；
- SR1：`PASS`；
- 普通 Read：`PASS`；
- Persistence Marker Read：`PASS`；
- `0x7FF0F0` / 300 Byte Read Back Compare：`PASS`。

完整 Verification 证据：

`04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md`

## Final Review Output

第一次正式 Review 发现：

```text
STM32 HAL uint16_t Size
→ 0xFFFF 单次传输限制
→ 泄漏为 Platform/W25Q64 API 隐式长度上限
```

返工后改为 STM32 SPI Impl 内部 chunking：

```text
while remaining > 0
    chunk = min(remaining, 0xFFFF)
    HAL_SPI_Transmit / HAL_SPI_Receive
```

Platform SPI 公共接口、W25Q64 Driver 和 Transaction/CS 模型保持不变。

最终 Review：

```text
Finding 1     CLOSED
Review Result PASS
Stage         CLOSED
```

正式结论：

`00_Project/03_Stages/S02_External_Flash_Driver/review.md`

## Reusable Engineering Assets

S02 形成并保留以下跨阶段资产：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Config/toolchain.local.example.bat
```

规则：

- Agent/Developer 优先通过统一脚本调用 Keil；
- `toolchain.local.bat` 为 machine-local 文件，当前不由 Git 跟踪；
- 仓库只保留通用模板；
- 后续 J-Link、RTT、Firmware Packaging 等工具可以沿用同一模式扩展。

## Stable Design Decisions for Downstream Stages

后续阶段可以依赖以下 S02 稳定结论：

- W25Q64 = 8 MiB，24-bit Address；
- Page = 256 Byte；
- Sector = 4 KiB；
- Erased Byte = `0xFF`；
- Raw Driver 不隐式 Erase；
- 原子 Page Program 不跨 Page；
- 连续 Write 可以自动拆 Page；
- Sector Erase 要求调用者提供 4 KiB 对齐地址；
- Program/Erase 使用 WREN/WEL/BUSY；
- Platform SPI 请求长度不受 STM32 HAL `uint16_t Size` 公共语义限制；
- 当前 Raw Driver 是后续 Storage / SFUD / OTA 方案评估的已验证基线。

## Non-blocking Technical Debt

以下内容不影响 S02 关闭，后续按实际需要处理：

- 个别文件头仍有旧阶段文案；
- `.uvoptx` IDE 状态 churn 应继续避免无意义提交；
- 既有 ARMCC Warning；
- Storage SPI 跨 transaction 的 RTOS 并发锁；
- SFUD 实际 Middleware 集成。

## Next Stage

Roadmap 下一阶段：

`S03_EEPROM_Storage`

S03 当前仍为 `PLANNED`。下一步应先进入 Design Role，读取当前仓库、AT24C02 硬件资料和现有 Software I2C 基线，讨论并冻结阶段设计后再生成实施计划；S02 不再继续追加功能。
