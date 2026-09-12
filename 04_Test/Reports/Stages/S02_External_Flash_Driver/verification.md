# S02 External Flash Driver Verification

## Verification Boundary

- Stage: `S02_External_Flash_Driver`
- Verification status: `PASS`
- Code verification: `PASS`
- Hardware verification: `PASS`，基于 Project Owner 提供的 RTT 实机日志
- Keil/MDK normal Build: `PASS`，0 Error、8 Warning
- Keil Clean/Rebuild: `PASS`，由 Project Owner 于 `2026-09-12` 实际执行并确认成功；本次未单独记录 warning 数量
- Board and RTT evidence: `PASS`
- Logic-analyzer evidence: `NOT_USED`

本文件记录 S02 的正式 Verification 证据。代码验证、Keil 构建验证和真实开发板验证分别记录，不以编译结果替代硬件结果，也不以 RTT 文本本身替代 Read Back / Compare。

## Implementation Commits

| Commit | 内容 |
| --- | --- |
| `f89a394678b42199d73972100fe3636c81b6fe80` | Platform SPI read、SPI2 Storage Bus、SPI1/SPI2 PCLK 实现 |
| `70f179ebf0e571430cc93101b998371c552837ef` | W25Q64 BSP、初始化、JEDEC/SR1/Read 和板测骨架 |
| `42217e2baf193a3e52a77be983bda44d81afd1c8` | WEL/BUSY、Page Program、Sector Erase |
| `86ecbb322819d726f18026b6cc248aed50832b60` | 跨页 Write 和边界负向测试 |
| `82573b2532cc2ca7645d9a6698eefcca3234f867` | App 编排、默认门禁和双启动持久化板测 |
| `e2d1ad53e9b24f2cb55fb2a663f34d0095bd276b` | 日志初始化时序、App 系统任务和 Platform Thread 工程接线 |
| `ec461729a24efe039c5331621b0f5a4df8992587` | 新增 Agent/开发者统一 Keil Build 入口 |
| `8cc3e0f1ca2d37d93856570b313719cc51eeb052` | 板测结束后移除生产工程中的临时 destructive test 接线 |
| `5bc4ccf7d0b367c37dfca45b5d3fbff83d2a1bed` | 实现分支最终工程编译状态 |
| `c6c77a240fce463afa4c86d797bb52d2781fb651` | PR #3 合并至 `main` |

## Application Startup Verification

- `service_log_init()` 位于 `freertos.c` 的 `USER CODE BEGIN Init` 区域，在 `osKernelInitialize()` 后、创建默认任务前执行，CubeMX 重新生成时会保留。
- `defaultTask` 只调用 `app_system_start()`，成功后立即删除自身；`app_system` 使用独立任务栈运行 `app_main()`。
- App 层通过 `platform_thread_create()` 创建任务，不直接包含或调用 CMSIS-RTOS；FreeRTOS 适配文件已加入 Keil 工程。
- 板测完成后，`s02_flash_board_test.c` 仍保留在 `04_Test/Board`，但已从 Application 源码、配置和 Keil 工程移除。
- 普通生产启动路径不会自动执行 S02 destructive Flash Test。

## Host-side Code Verification

执行过统一 GCC 静态语法检查：

```text
gcc -std=c99 -Wall -Wextra -Werror=implicit-function-declaration \
    -fsyntax-only -DSTM32F411xE -DUSE_HAL_DRIVER <S02 include paths> <Task1-5 changed C files>
```

结果：`PASS`。验证范围包括：

- `platform_spi.c`
- `impl_platform_spi.c`
- `impl_platform_bsp_spi.c`
- `impl_platform_bsp_gpio.c`
- `platform_bsp_w25q64.c`
- `platform_w25q64.c`
- `app_main.c`
- `s02_flash_board_test.c`

其他静态检查：

- `git diff --check`：`PASS`；
- `OTA_APP.uvprojx` XML 解析及工程内全部 `FilePath` 存在性：`PASS`；
- W25Q64 Raw Driver 直接引用 HAL、`hspi2` 或 `SPI2`：未发现，`PASS`；
- Chip Erase `0x60/0xC7` 或 Chip Erase 公共 API：未发现，`PASS`；
- S02 临时板测配置和 Keil 工程接线已移除，测试源码仅保留在 `04_Test/Board`。

## MDK Build Verification

### Normal Build

仓库统一入口：

```text
05_Tools\Scripts\build_app.bat
```

结果：

- Target：`OTA_APP`
- Tool：Keil UV4
- Compiler：`V5.06 update 7 (build 960)`
- Build：`PASS`
- Error：`0`
- Warning：`8`，为既有 ARMCC 兼容性和文件末尾换行提示，当前作为非阻塞项保留
- Build log：`06_Output/Logs/OTA_APP_build.log`
- 主要输出：`03_Firmware/Application/OTA_APP/MDK-ARM/Objects/OTA_APP.axf`、`.hex` 和 `Listings/OTA_APP.map`
- 本机配置：`05_Tools/Config/toolchain.local.bat`，由 `.gitignore` 排除

### Clean/Rebuild

Project Owner 已于 `2026-09-12` 在 Keil MDK 中对当前 `OTA_APP` 工程执行 Clean/Rebuild，并确认构建成功。

- Clean/Rebuild：`PASS`
- 执行者：Project Owner
- Error：未报告构建错误
- Warning：本次未单独记录数量，不据此覆盖普通 Build 已记录的 `8 Warning`
- 结论：满足 S02 Acceptance Criteria 中的 Clean Rebuild 门禁

## Hardware Case Matrix

以下结果来自 Project Owner 提供的三次启动 RTT 日志；后两次启动包含完整测试项。

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
| RTT/EasyLogger | 地址、长度、返回码和 PASS/FAIL 可观测 | `PASS` |
| Logic Analyzer | RTT 与 Read Back 证据正常，本阶段未要求额外抓波形 | `NOT_USED` |

`persistence addr=0x7FF0F0 len=300 PASS` 等日志对应的是测试代码实际 Read Back / Compare 结果，不是单纯打印预设 PASS 文本。

## Additional Engineering Asset

施工期间新增了统一 Keil Build 入口：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Config/toolchain.local.example.bat
```

该入口已写入 `AGENTS.md` 和 `05_Tools/Scripts/README.md`，用于人工与 Agent 稳定调用本机 Keil。机器相关路径保存在被忽略的 `toolchain.local.bat` 中，不进入生产代码或 Git 历史。该能力作为跨阶段可复用工程资产保留。

## Verification Conclusion

S02 Verification 所需证据已经完整：

- Code Verification：`PASS`
- Normal Keil Build：`PASS`
- Keil Clean/Rebuild：`PASS`
- Hardware Verification：`PASS`
- Destructive Test Cleanup：`PASS`
- SFUD Boundary Evaluation：已完成，实际集成延后

Verification Role 无剩余阻塞项。阶段可以进入 `READY_FOR_REVIEW`，由 Review Role 对照冻结 Design、Implementation Plan、代码差异、Handoff 和本验证报告决定 `PASS / CHANGES_REQUESTED / BLOCKED`。
