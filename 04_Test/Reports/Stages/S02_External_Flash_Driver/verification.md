# S02 External Flash Driver Verification Input

## Verification Boundary

- Stage: `S02_External_Flash_Driver`
- Verification input status: `READY_FOR_VERIFICATION`
- Code verification: `PASS` for the host-side static checks listed below
- Hardware verification: `PASS`，基于用户提供的 RTT 实机日志
- Keil/MDK Build: `PASS`，0 Error、8 Warning；Clean/Rebuild: `NOT_RUN`
- Board and RTT evidence: `PASS`; logic-analyzer evidence: `NOT_USED`

本文件记录 Implementation Role 已完成的可回读证据和下一步 Verification Role 输入，不替代真实开发板验收，也不把静态编译结果描述为硬件通过。

## Implementation Commits

| Commit | 内容 |
| --- | --- |
| `f89a394678b42199d73972100fe3636c81b6fe80` | Platform SPI read、SPI2 Storage Bus、SPI1/SPI2 PCLK 实现 |
| `70f179ebf0e571430cc93101b998371c552837ef` | W25Q64 BSP、初始化、JEDEC/SR1/Read 和板测骨架 |
| `42217e2baf193a3e52a77be983bda44d81afd1c8` | WEL/BUSY、Page Program、Sector Erase |
| `86ecbb322819d726f18026b6cc248aed50832b60` | 跨页 Write 和边界负向测试 |
| `82573b2532cc2ca7645d9a6698eefcca3234f867` | App 编排、默认门禁和双启动持久化板测 |
| `e2d1ad53e9b24f2cb55fb2a663f34d0095bd276b` | 日志初始化时序、App 系统任务和 Platform Thread 工程接线 |

## Application Startup Verification

- `service_log_init()` 位于 `freertos.c` 的 `USER CODE BEGIN Init` 区域，在 `osKernelInitialize()` 后、创建默认任务前执行，CubeMX 重新生成时会保留。
- `defaultTask` 只调用 `app_system_start()`，成功后立即删除自身；`app_system` 使用 4096 Byte 独立栈运行 `app_main()`。
- App 层通过 `platform_thread_create()` 创建任务，不直接包含或调用 CMSIS-RTOS；FreeRTOS 适配文件已加入 Keil 工程。
- 板测完成后，`s02_flash_board_test.c` 仍保留在 `04_Test/Board`，但已从 Application 源码、配置和 Keil 工程移除。
- 该调整只解决任务启动和日志观测基础设施，不替代真实 Flash Read Back/Compare 硬件证据。

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
- S02 板测配置和 Keil 工程接线已移除，生产 Application 不再自动执行破坏性 Flash 测试；测试源码仅保留在 `04_Test/Board`。

## MDK Build Verification

本次使用仓库统一入口执行普通 Build，未执行 Clean/Rebuild：

```text
05_Tools\Scripts\build_app.bat
```

- Source Commit：`8cc3e0f`
- Target：`OTA_APP`
- Tool：Keil UV4
- Compiler：`V5.06 update 7 (build 960)`
- Build：`PASS`（目标已生成，脚本仅因 warning 返回非零）
- Error：`0`
- Warning：`8`（既有 ARMCC 兼容性和文件末尾换行提示，当前按用户要求暂不处理）
- Script exit code：`1`（warning gate）
- Build log：`06_Output/Logs/OTA_APP_build.log`
- 主要输出：`03_Firmware/Application/OTA_APP/MDK-ARM/Objects/OTA_APP.axf`、`.hex`，以及 `Listings/OTA_APP.map`
- Clean/Rebuild：`NOT_RUN`；当前脚本调用 UV4 `-b`，不包含 Clean 操作
- 本机配置：`05_Tools/Config/toolchain.local.bat`，已被 `.gitignore` 排除，不进入 Git

## Hardware Case Matrix

以下结果来自用户提供的三次启动 RTT 日志；后两次启动包含完整测试项。

| Case | 目标证据 | 当前状态 |
| --- | --- | --- |
| JEDEC ID | 实际读取 `EF 40 17` | `PASS` |
| SR1 | 实际读取 `00`，初始化后 BUSY/WEL 清除 | `PASS` |
| Test Sector | 仅使用 `0x7FF000 ~ 0x7FFFFF` | `PASS` |
| Sector Erase | 擦除后完整 Read Back 为 `0xFF` | `PASS` |
| Single-page Program | `0x7FF000`、64 Byte、真实 Read Back Compare | `PASS` |
| Cross-page Write | `0x7FF0F0`、300 Byte、真实 Compare | `PASS` |
| Boundary Read/Write | 越界读写被拒绝，合法尾地址读成功 | `PASS` |
| Atomic Page Program | `0x...F0` 跨页被拒绝 | `PASS` |
| Unaligned Erase | `0x7FF001` 被拒绝 | `PASS` |
| Reset Persistence | 重启后读取并比较 `0x7FF0F0` 的 300 Byte | `PASS` |
| RTT/EasyLogger | 日志初始化、任务名、地址、长度、返回码和 PASS/FAIL 均可观测 | `PASS` |
| Logic Analyzer | 仅在 RTT 异常时抽查 CS/SCK/MOSI/MISO | `NOT_USED` |

日志中 `appSystem` 任务名确认 Application 已在独立任务中运行；`persistence addr=0x7FF0F0 len=300 PASS` 确认重启后的持久化数据校验通过。

## Verification Role Next Action

1. 使用 Keil MDK 对 `OTA_APP.uvprojx` 执行 Clean/Rebuild，记录 warning、错误和固件产物路径。
2. 确认普通启动固件不再包含 S02 板测入口，保留测试源码供后续按需重新接入。
3. Verification Role 完成后，再由 Review Role 决定 `PASS`、返工或阻塞；当前不关闭 S02。
