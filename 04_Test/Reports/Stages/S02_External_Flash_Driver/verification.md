# S02 External Flash Driver Verification Input

## Verification Boundary

- Stage: `S02_External_Flash_Driver`
- Verification input status: `READY_FOR_VERIFICATION`
- Code verification: `PASS` for the host-side static checks listed below
- Hardware verification: `PENDING`
- Keil/MDK Clean/Rebuild: `NOT_RUN`
- Board, RTT and logic-analyzer evidence: `NOT_AVAILABLE`

本文件记录 Implementation Role 已完成的可回读证据和下一步 Verification Role 输入，不替代真实开发板验收，也不把静态编译结果描述为硬件通过。

## Implementation Commits

| Commit | 内容 |
| --- | --- |
| `f89a394678b42199d73972100fe3636c81b6fe80` | Platform SPI read、SPI2 Storage Bus、SPI1/SPI2 PCLK 实现 |
| `70f179ebf0e571430cc93101b998371c552837ef` | W25Q64 BSP、初始化、JEDEC/SR1/Read 和板测骨架 |
| `42217e2baf193a3e52a77be983bda44d81afd1c8` | WEL/BUSY、Page Program、Sector Erase |
| `86ecbb322819d726f18026b6cc248aed50832b60` | 跨页 Write 和边界负向测试 |
| `82573b2532cc2ca7645d9a6698eefcca3234f867` | App 编排、默认门禁和双启动持久化板测 |

## Host-side Code Verification

执行命令：

```text
gcc -std=c99 -Wall -Wextra -Werror=implicit-function-declaration \
    -fsyntax-only -DSTM32F411xE -DUSE_HAL_DRIVER <S02 include paths> <Task1-5 changed C files>
```

结果：`PASS`。以下 8 个源文件均通过：

- `platform_spi.c`
- `impl_platform_spi.c`
- `impl_platform_bsp_spi.c`
- `impl_platform_bsp_gpio.c`
- `platform_bsp_w25q64.c`
- `platform_w25q64.c`
- `app_main.c`
- `s02_flash_board_test.c`

其他静态检查：

- `git diff --check`：`PASS`；仓库工程使用 CRLF，检查使用 `cr-at-eol` 解释配置。
- `OTA_APP.uvprojx` XML 解析及工程内全部 `FilePath` 存在性：`PASS`。
- W25Q64 Raw Driver 直接引用 HAL、`hspi2` 或 `SPI2`：未发现，`PASS`。
- Chip Erase `0x60/0xC7` 或 Chip Erase 公共 API：未发现，`PASS`。
- `PROJECT_S02_FLASH_BOARD_TEST_ENABLE`：提交值为 `0U`，正常启动破坏性板测门禁关闭，`PASS`。

## MDK Build Verification

主机未发现 `UV4`、`UV5`、`armcc` 或 `armclang` 命令，因此无法执行 Keil Clean/Rebuild。

- Clean/Rebuild：`NOT_RUN`
- 新 S02 warning 与既有 S01 warning 区分：`NOT_APPLICABLE`，没有 MDK 输出可供比较
- 生成的 `.axf/.hex/.bin`：无本次验证产物
- 已知既有 warning：`platform_gpio.c` 5 个、Vendor `elog_port.c` 文件末尾换行 warning，沿用项目状态记录，未将其伪装成 S02 新验证结果

## Hardware Case Matrix

所有硬件栏位均保持 `PENDING`，原因是当前会话没有真实开发板、J-Link、RTT 终端或逻辑分析仪。

| Case | 目标证据 | 当前状态 |
| --- | --- | --- |
| JEDEC ID | 实际读取 `EF 40 17` | `PENDING` |
| SR1 | 实际读取稳定且可解释 BUSY/WEL | `PENDING` |
| Test Sector | 仅使用 `0x7FF000 ~ 0x7FFFFF` | 代码范围已检查；硬件 `PENDING` |
| Sector Erase | 擦除后完整 4096 Byte 逐字节为 `0xFF` | `PENDING` |
| Single-page Program | `0x7FF000`、64 Byte、真实 Read Back Compare | `PENDING` |
| Cross-page Write | `0x7FF0F0`、300 Byte、预期拆分 `16 + 256 + 28`、真实 Compare | `PENDING` |
| Boundary Read/Write | 越界读写被拒绝，合法尾地址读成功 | `PENDING` |
| Atomic Page Program | `0x...F0` + 32 Byte 跨页被拒绝 | `PENDING` |
| Unaligned Erase | `0x7FF001` 被拒绝 | `PENDING` |
| Reset Persistence | 第二次启动先读取并比较 `0x7FF0F0` 的 300 Byte | `PENDING` |
| RTT/EasyLogger | 每个 Case 的地址、长度、返回码和数据校验结果 | `PENDING` |
| Logic Analyzer | 仅在 RTT 异常时抽查 CS/SCK/MOSI/MISO | `NOT_USED` |

板测入口包含持久化标记，且在重新擦除前读取上一次标记和 300 Byte 数据；但没有实际 Reset 观察结果前，不得填写 `PASS`。

## Verification Role Next Action

1. 使用 Keil MDK 对 `OTA_APP.uvprojx` 执行 Clean/Rebuild，记录 warning、错误和固件产物路径。
2. 确认 `PROJECT_S02_FLASH_BOARD_TEST_ENABLE` 仅在专用板测启动中临时设为 `1U`，完成两次启动持久化测试后恢复 `0U`。
3. 逐项保存 RTT 输出和真实 Read Back/Compare 结果；若 JEDEC 或事务异常，再补充逻辑分析仪证据。
4. Verification Role 完成后，再由 Review Role 决定 `PASS`、返工或阻塞；当前不关闭 S02。
