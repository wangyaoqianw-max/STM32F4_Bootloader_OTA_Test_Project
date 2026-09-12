# S02 External Flash Driver Handoff

## Metadata

- Stage: `S02_External_Flash_Driver`
- Status: `CHANGES_REQUESTED`
- Branch: `main`
- Design Commit: `44fddb1484441a98d336df7783170267c166f4af`
- Plan Commit: `aee30916c5c784828269668d99a2b4e63689f80e`
- Baseline Code Commit: `5ac069f19c7f401f56e8fa5aad00c92a76aaaedf`
- Implementation Branch Tip: `5bc4ccf7d0b367c37dfca45b5d3fbff83d2a1bed`
- Merge Commit: `c6c77a240fce463afa4c86d797bb52d2781fb651`
- Verification Commit: `df9ec99d411f83b3a2f5c21ccc9f13c6b5e0ac64`
- Review Commit: `3154c0c07f5041eb1ea2a2e9fdf524fe7ecba26a`

## Goal

建立并验证 W25Q64 Raw Driver V1。主体实现和硬件闭环已经完成；当前 handoff 仅记录 Review 返工输入。

## Completed Implementation / Verification

以下结果保持有效：

- Platform SPI 同步 `read()`；
- SPI1/SPI2 多实例和 PCLK2/PCLK1 判断；
- Storage SPI Bus / Flash CS BSP；
- W25Q64 Init/Deinit、JEDEC ID、SR1、Read、WEL/BUSY、Page Program、跨页 Write、4 KiB Sector Erase；
- 地址范围、Page、Sector 对齐保护；
- JEDEC ID=`EF 40 17`；
- Sector Erase + 4 KiB Read Back 全 `0xFF`；
- Single-page Program + Compare；
- `0x7FF0F0` 300 Byte Cross-page Write + Compare；
- 边界拒绝与 Reset Persistence；
- Normal Keil Build / Clean-Rebuild；
- destructive board test 已从生产工程移除；
- `app_system` 启动修正；
- SFUD Boundary Evaluation；
- `05_Tools/Scripts/build_app.bat` 统一 Keil Build 入口。

完整原始证据：

`04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md`

## Review Output

Review Result：`CHANGES_REQUESTED`

唯一阻塞 Finding：

```text
Design:
platform_w25q64_read() 只受 8 MiB 合法地址范围限制

Current Impl:
dataLength > 0xFFFF
→ PLATFORM_ERR_OVERFLOW
```

原因是 STM32 HAL SPI 的 Size 参数为 16-bit，而当前 Impl 直接把这个限制暴露给了 32-bit `platform_size_t` Platform API。

例如：

```c
platform_w25q64_read(&flash, 0x000000U, buffer, 65536U);
```

地址范围合法，但当前实现会失败。

详见：

`00_Project/03_Stages/S02_External_Flash_Driver/review.md`

## Rework Input

推荐在 STM32 SPI Impl 中拆分 HAL 调用：

```text
remaining = dataLength
while remaining > 0
    chunk = min(remaining, 0xFFFF)
    HAL_SPI_Transmit / HAL_SPI_Receive(chunk)
    advance buffer
```

约束：

- 不改变 Platform SPI Bus / Device / Transaction 架构；
- HAL chunk 期间不能结束上层 transaction，CS 必须保持原事务语义；
- `read()` / `write()` 建议对称处理；
- 不通过修改 Design 或公共接口文档来降低原要求；
- 不扩展 DMA/Interrupt SPI、SFUD、OTA 或 Bootloader。

## Required Re-verification

修正后至少提供：

1. `>0xFFFF` Byte 长度路径不再直接 `OVERFLOW` 的针对性证据；
2. Code Verification；
3. Keil Build / Clean-Rebuild；
4. JEDEC ID / 普通 Read / 一个真实 Read Back Case；
5. `verification.md` 更新；
6. 新的修正 Commit；
7. 状态恢复到 `READY_FOR_REVIEW` 后重新审核。

不要求重新执行全部 destructive Flash 板测，除非修正过程中改变了 W25Q64 transaction 或擦写逻辑。

## Non-blocking Notes

- `project_config.h` / `app_main.c` 文件头仍有 S01 文案，可后续整理；
- `.uvoptx` IDE 状态 churn 后续尽量减少；
- Storage SPI RTOS 并发锁留待正式并发阶段设计；
- 既有 Warning 不属于本轮返工。

## Next Action

Implementation Role 根据本 handoff 和 `review.md` 完成最小返工，验证后重新交 Review Role。
