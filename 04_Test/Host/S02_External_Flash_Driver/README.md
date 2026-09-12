# S02 SPI Impl 大长度传输 Host Test

## 目的

验证 S02 返工项：STM32 SPI Impl 的单次 HAL 传输上限（`HAL_SPI_Transmit()` /
`HAL_SPI_Receive()` 的 `uint16_t Size`）由 Impl 内部拆分处理，不再泄漏为
`platform_spi_read()` / `platform_spi_write()` 的 32-bit 长度上限。

同时验证拆分没有破坏已有语义：

- 一个 Platform transaction 内可以执行多个 HAL chunk；
- 任意 chunk 失败立即停止后续传输，并返回映射后的 `platform_error_t`；
- `HAL_BUSY` / `HAL_TIMEOUT` / `HAL_ERROR` 不被吞掉；
- 参数与 Context 校验路径保持不变。

## 文件

| 文件 | 作用 |
| --- | --- |
| `s02_spi_chunking_host_test.c` | 测试主体；包含 HAL 替身、Impl 依赖替身和全部用例 |
| `stubs/spi.h` | Host Test 专用 HAL SPI 替身头文件，通过 `-I` 顺序覆盖 `Core/Inc/spi.h` |

被测实现不是副本：测试直接 `#include` 仓库中的
`04_Impl/impl_mcu/impl_platform_spi.c`，验证对象就是生产源码本身。

`stm32_spi_write()` / `stm32_spi_read()` 为文件内 `static`，Impl 私有 Context
也不对外暴露，仓库当前没有其他 Host Test 接缝，因此采用包含 `.c` 的方式调用。

测试只为链接补了 4 个本次未被触达的 Platform 依赖替身（`platform_object_is_valid`、
`platform_object_set_state`、`platform_device_set_power_state`、
`platform_spi_bus_init`）；被测读写路径不会调用它们。

## 运行

在仓库根目录执行（Windows PowerShell，需要 PATH 中有 gcc）：

```powershell
gcc -std=c99 -Wall -Wextra -Werror -Wno-unused-function `
  -ffunction-sections '-Wl,--gc-sections' `
  -I04_Test/Host/S02_External_Flash_Driver/stubs `
  -I03_Firmware/Application/OTA_APP/00_Config `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_common `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/spi `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/gpio `
  -I03_Firmware/Application/OTA_APP/04_Impl/impl_board `
  -I03_Firmware/Application/OTA_APP/04_Impl/impl_mcu `
  -o "$env:TEMP\s02_spi_chunking_host_test.exe" `
  04_Test/Host/S02_External_Flash_Driver/s02_spi_chunking_host_test.c

& "$env:TEMP\s02_spi_chunking_host_test.exe"
```

退出码 `0` 表示全部检查通过，退出码 `1` 表示存在失败用例；
输出会逐条打印 `[PASS]` / `[FAIL]` 和每次 HAL 调用的实际 chunk 长度。

`-Wno-unused-function` 只抑制被测文件中本次未被触达的 lifecycle/helper 函数告警；
`-Werror` 保证测试代码与被测文件没有其他告警。

## 覆盖的用例

| 用例 | 期望证据 |
| --- | --- |
| `write length=1` | 1 次 HAL 调用，size=1 |
| `write length=0xFFFF` | 1 次 HAL 调用，size=0xFFFF |
| `write length=0x10000` | 2 次 HAL 调用：0xFFFF + 1，且不再返回 `PLATFORM_ERR_OVERFLOW` |
| `write length=0x30000` | 4 次 HAL 调用：0xFFFF + 0xFFFF + 0xFFFF + 3 |
| `read` 同上四个长度 | 与 `write` 对称的 chunk 序列 |
| chunk 数据覆盖 | 按累计偏移校验源/目标指针推进，无重复或漏传字节 |
| chunk 2 注入 `HAL_TIMEOUT` | 返回 `PLATFORM_ERR_TIMEOUT`，调用次数停在 2 |
| chunk 1 注入 `HAL_BUSY` | 返回 `PLATFORM_ERR_BUSY`，调用次数停在 1 |
| chunk 3 注入 `HAL_ERROR` | 返回 `PLATFORM_ERR_IO`，调用次数停在 3 |
| NULL / length=0 / Context 缺失 / HAL Handle 未绑定 | 返回对应错误码，且不发生任何 HAL 调用 |

## 局限

- 本测试运行在 PC 上，只证明 Impl 的 chunk 拆分逻辑与错误传播；
- 不证明真实 SPI 波形、真实 W25Q64 时序或板级电气行为；
- 板级回归（Init / JEDEC ID、普通 Read、Read Back Compare）必须在真实硬件上单独执行；
- 本目录文件不进入 Keil 工程，不参与固件构建。

