# S02 External Flash Driver Verification

## Verification Boundary

- Stage: `S02_External_Flash_Driver`
- Verification status: `READY_FOR_REVIEW`
- Code verification: `PASS`
- Original S02 Hardware Verification: `PASS`，基于 Project Owner 提供的 RTT 实机日志
- Keil/MDK normal Build: `PASS`，0 Error、8 Warning（原 S02 验证）
- Keil Clean/Rebuild: `PASS`，由 Project Owner 于 `2026-09-12` 实际执行并确认成功；本次未单独记录 warning 数量
- Rework Commit: `42c02b891d7f32b728857c44024c3c91a15ea604`
- Original S02 Board and RTT evidence: `PASS`
- Logic-analyzer evidence: `NOT_USED`
- Rework Code Verification (Finding 1): `PASS`，见文末 “Rework Verification”
- Rework Hardware Regression (Finding 1): `PASS`，Project Owner 于 `2026-09-12` 提供 RTT 实机日志

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

## Original S02 Hardware Case Matrix — PASS

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

该入口已写入 `AGENTS.md` 和 `05_Tools/Scripts/README.md`，用于人工与 Agent 稳定调用本机 Keil。
`toolchain.local.bat` 属于 machine-local 配置，当前版本不再由 Git 跟踪，本机文件可以保留；
仓库只保留 `toolchain.local.example.bat` 作为模板。该文件此前曾出现在 Git 历史，本次不重写
历史。该能力作为跨阶段可复用工程资产保留。

## Verification Conclusion

原 S02 Verification 所需证据以及 Finding 1 的返工代码、构建和硬件回归证据均已完整：

- Code Verification：`PASS`
- Normal Keil Build：`PASS`
- Keil Clean/Rebuild：`PASS`
- Original S02 Hardware Verification：`PASS`
- Rework Hardware Regression：`PASS`
- Destructive Test Cleanup：`PASS`
- SFUD Boundary Evaluation：已完成，实际集成延后

阶段进入 `READY_FOR_REVIEW`，由 Review Role 对照冻结 Design、Implementation Plan、代码差异、
Handoff 和本验证报告决定 `PASS / CHANGES_REQUESTED / BLOCKED`。

---

## Rework Verification — Finding 1（2026-09-12）

### 返工范围

只修正 Review Finding 1：STM32 SPI Impl 不能把 HAL 单次 `0xFFFF` Byte 限制暴露为
Platform SPI / W25Q64 公共接口的长度上限。未重做 S02 其他已通过能力，未引入
SFUD、DMA/Interrupt SPI、OTA 或 Bootloader。

### 修改文件

```text
03_Firmware/Application/OTA_APP/04_Impl/impl_mcu/impl_platform_spi.c   （修正）
04_Test/Host/S02_External_Flash_Driver/s02_spi_chunking_host_test.c    （新增）
04_Test/Host/S02_External_Flash_Driver/stubs/spi.h                     （新增）
04_Test/Host/S02_External_Flash_Driver/README.md                       （新增）
```

### 实现结论

`stm32_spi_write()` / `stm32_spi_read()` 现在在 Impl 内按 `min(remaining, 0xFFFF)` 拆分
HAL blocking transfer，`dataLength > 0xFFFF → PLATFORM_ERR_OVERFLOW` 分支已删除。

拆分发生在 `04_Impl/impl_mcu/impl_platform_spi.c`，不在 W25Q64 Driver 内；
`platform_size_t`、Platform SPI 公共 API、Bus / Device / Transaction 模型和 CS 控制逻辑均未修改。

对既有行为的影响可判定为“无”：

```text
length = 1        → 1 次 HAL 调用（与返工前一致）
length = 0xFFFF   → 1 次 HAL 调用（与返工前一致）
length = 0x10000  → 2 次 HAL 调用（返工前直接返回 OVERFLOW）
length = 0x30000  → 4 次 HAL 调用（返工前直接返回 OVERFLOW）
```

### Code Verification

1）静态语法检查：`PASS`，无告警。

```text
gcc -std=c99 -Wall -Wextra -fsyntax-only -DSTM32F411xE -DUSE_HAL_DRIVER <S02 include paths> \
    impl_platform_spi.c platform_spi.c
退出码 0，无 warning / error
```

2）静态代码路径检查：`impl_platform_spi.c` 中已不存在 `PLATFORM_ERR_OVERFLOW`。

```text
Get-ChildItem -Recurse -File -Include *.c,*.h 03_Firmware,04_Test |
    Select-String -Pattern 'STM32_SPI_HAL_MAX_TRANSFER_SIZE|PLATFORM_ERR_OVERFLOW'
命中：ring_buffer.c / platform_uart.c / service_uart.c / impl_platform_uart.c / platform_error.h
未命中：impl_platform_spi.c
```

3）Host Test（HAL 替身，PC 运行）：`PASS`，18 项检查。

```text
S02 SPI Impl chunking host test (HAL stub, PC only)
[PASS] write length=1 -> 1 HAL chunk
       HAL calls=1 sizes=[1]
[PASS] write length=0xFFFF -> 1 HAL chunk of 0xFFFF
       HAL calls=1 sizes=[65535]
[PASS] write length=0x10000 -> 0xFFFF + 1, no PLATFORM_ERR_OVERFLOW
       HAL calls=2 sizes=[65535,1]
[PASS] write keeps per-chunk HAL timeout and bound HAL handle
[PASS] write length=0x30000 -> 0xFFFF + 0xFFFF + 0xFFFF + 3
       HAL calls=4 sizes=[65535,65535,65535,3]
[PASS] read length=1 -> 1 HAL chunk
       HAL calls=1 sizes=[1]
[PASS] read length=0xFFFF -> 1 HAL chunk of 0xFFFF
       HAL calls=1 sizes=[65535]
[PASS] read length=0x10000 -> 0xFFFF + 1, no PLATFORM_ERR_OVERFLOW
       HAL calls=2 sizes=[65535,1]
[PASS] read length=0x30000 -> 0xFFFF + 0xFFFF + 0xFFFF + 3
       HAL calls=4 sizes=[65535,65535,65535,3]
[PASS] write 0x10000 timeout on chunk 2 -> PLATFORM_ERR_TIMEOUT and stop
       HAL calls=2 sizes=[65535,1]
[PASS] read 0x10000 busy on chunk 1 -> PLATFORM_ERR_BUSY and stop
       HAL calls=1 sizes=[65535]
[PASS] write 0x30000 HAL_ERROR on chunk 3 -> PLATFORM_ERR_IO and stop
       HAL calls=3 sizes=[65535,65535,65535]
[PASS] write NULL -> PLATFORM_ERR_INVALID_PARAM without HAL call
[PASS] read NULL -> PLATFORM_ERR_INVALID_PARAM without HAL call
[PASS] write length=0 -> PLATFORM_ERR_INVALID_PARAM without HAL call
[PASS] read length=0 -> PLATFORM_ERR_INVALID_PARAM without HAL call
[PASS] missing Impl context -> PLATFORM_ERR_INVALID_PARAM without HAL call
[PASS] unbound HAL handle -> PLATFORM_ERR_NOT_INITIALIZED without HAL call
RESULT: PASS (18 checks)
退出码 0
```

测试直接 `#include` 仓库中的 `impl_platform_spi.c`，验证对象是生产源码而不是副本；
`04_Test/Host/S02_External_Flash_Driver/README.md` 记录了运行命令、用例表和局限。

4）返工前文件对照（负向对照）：对 `HEAD` 版本的 `impl_platform_spi.c` 运行同一测试为
`FAIL`（10/18 失败），`length=0x10000` 时为 `HAL calls=0`，直接复现 Finding 1。

```text
[FAIL] write length=0x10000 -> 0xFFFF + 1, no PLATFORM_ERR_OVERFLOW
       HAL calls=0 sizes=[]
[FAIL] write length=0x30000 -> 0xFFFF + 0xFFFF + 0xFFFF + 3
       HAL calls=0 sizes=[]
[FAIL] read length=0x10000 -> 0xFFFF + 1, no PLATFORM_ERR_OVERFLOW
       HAL calls=0 sizes=[]
RESULT: FAIL (18 checks, 10 failed)
退出码 1
```

5）`git diff --check`：`PASS`。

### Keil Build Verification

构建记录：Target `OTA_APP`，Compiler `V5.06 update 7 (build 960)`，
Build 类型 = Normal Build + Clean Rebuild，源码为本次返工工作区内容，对应 Rework Commit
`42c02b891d7f32b728857c44024c3c91a15ea604`。构建期间未修改源码或工程配置，
`git status --short` 未出现构建生成物。

Normal Build（`05_Tools\Scripts\build_app.bat`）：

```text
Build target 'OTA_APP'
compiling impl_platform_spi.c...
linking...
Program Size: Code=35200 RO-data=1020 RW-data=264 ZI-data=44256
".\Objects\OTA_APP.axf" - 0 Error(s), 1 Warning(s).
```

`0 Error`，无新增 Error。增量构建只重编部分文件，因此 Warning 计数为 1（既有
`platform_w25q64.c(277) #188-D`），不代表 warning 总数下降。

Clean/Rebuild（`UV4 -r`，全量重编）：

```text
Program Size: Code=35200 RO-data=1020 RW-data=264 ZI-data=44256
".\Objects\OTA_APP.axf" - 0 Error(s), 8 Warning(s).
Build Time Elapsed:  00:00:07
```

`0 Error`、`8 Warning`，与返工前已记录的 8 Warning 基线一致，无新增 Error。
构建日志：`06_Output/Logs/OTA_APP_build.log`、`06_Output/Logs/OTA_APP_rebuild.log`。

### Hardware Verification — `PASS`

Project Owner 于 `2026-09-12` 在真实开发板上执行返工后的最小回归，并提供 RTT 日志。日志
确认 W25Q64 初始化、识别、普通读取和既有持久化数据回读均成功：

```text
[S02-R] w25q64 init result=0
[S02-R] jedec=EF 40 17 result=0 PASS
[S02-R] sr1=00 result=0 PASS
[S02-R] ordinary read addr=0x000000 len=1 data=FF result=0 PASS
[S02-R] persistence marker addr=0x7FFFF8 len=8 result=0 PASS
[S02-R] readback compare addr=0x7FF0F0 len=300 result=0 PASS
[S02-R] regression result=0 PASS
```

`result=0` 即 `PLATFORM_ERR_OK`。普通读取返回 `FF` 仅表示该地址当前数据为 `FF`，读取接口
本身成功。持久化标记和 300 Byte Read Back / Compare 均为实际 Flash 读取结果，不是预设日志。

返工前已 PASS 的 Sector Erase 全 4 KiB、300 Byte Cross-page Write、Reset Persistence
和 destructive 边界用例无需重跑：本次改动只影响 `> 0xFFFF` 的长度路径，`<= 0xFFFF`
仍保持“每笔请求一次 HAL 调用”的原行为（Host Test 已证明 `1` 与 `0xFFFF` 均为 1 次 HAL 调用）。

除非最小回归出现异常，不要求重新执行完整 Sector Erase、300 Byte Cross-page Write、全部
边界负向测试或 Reset Persistence。

真实 `> 0xFFFF` Byte 读取的板级证据本次未提供：测试代码需要 ≥64 KiB Buffer，
会明显占用 STM32F411 的 128 KiB SRAM；是否为此占用 RAM 属于 Project Owner 决策，
本轮返工未自动实施。

本次临时最小回归测试已在取得上述硬件证据后从 Application 启动路径、Keil 工程和测试目录
接线中移除；原有生产启动路径恢复为仅初始化 Application 并运行状态灯循环。

### Verification Result

```text
Code Verification（返工）      : PASS
Keil Normal Build              : PASS（0 Error）
Keil Clean/Rebuild             : PASS（0 Error）
Hardware Regression（返工）    : PASS（Project Owner RTT 实机日志）
```

Finding 1 的代码、构建和硬件回归证据已完整；阶段推进到 `READY_FOR_REVIEW`。
`review.md` 仍记录 `CHANGES_REQUESTED`，需要 Review Role 再次独立执行，S02 尚未关闭。
