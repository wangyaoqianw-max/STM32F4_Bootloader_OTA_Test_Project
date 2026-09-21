# W25Q64 STM32F411 External Loader 设计

## 状态

- 状态：已获用户批准，进入实现
- 日期：2026-09-21
- 目标目录：`05_Tools/ExternalLoader/W25Q64_STM32F411/`
- 官方范本：`stm32-memory-loaders` 的 `N25Q128A_STM32412G-DISCO`

## 1. 背景与目标

S10 的 Factory Baseline 需要独立证明 W25Q64 的物理写入结果。现有 Application 只适合运行时业务，不适合作为 STM32CubeProgrammer 的 External Loader。本工具提供一个独立、可重复构建的 Loader，用于直接擦除、写入、读取和校验当前板上的 W25Q64JV。

目标：

- 面向 STM32F411CEUx 和当前硬件连接；
- 只依赖裸机寄存器和 CMSIS 设备头，不引入 RTOS、Application、YMODEM 或 AT24C02；
- 生成可被 STM32CubeProgrammer 加载的 `.stldr`；
- 支持 S10 C0～C5 检查所需的独立 W25Q64 读取和验证。

不在范围内：

- 修改 Application、Bootloader 或 S10 生产逻辑；
- 写入 AT24C02 Metadata；
- 在本次构建中执行破坏性的整片擦除或预烧录。

## 2. 硬件绑定

| 资源 | 配置 |
|---|---|
| MCU | STM32F411CEUx |
| Flash | W25Q64JV，JEDEC `EF 40 17` |
| SPI | SPI2，Mode 0，8-bit，MSB first |
| SCK | PB13，AF5 |
| MISO | PB14，AF5 |
| MOSI | PB15，AF5 |
| CS | PB12，GPIO 输出，低有效 |
| 容量 | `0x00800000` bytes |
| Page | `0x100` bytes |
| Sector | `0x1000` bytes |
| CubeProgrammer 映射基址 | `0x90000000` |

## 3. 结构与接口

工具采用官方范本的 Loader 组织：

```text
Dev_Inf.c       → StorageInfo，描述容量、页和 Sector 几何
Loader_Src.c    → CubeProgrammer 入口和 W25Q64 操作
Keil 工程       → STM32F411CE，ARMCC，RAM 运行布局
```

实现的入口：

```text
Init
Write
Read
SectorErase
MassErase
CheckSum
Verify
```

`Address` 按 CubeProgrammer 的 `0x90000000 + offset` 约定归一化为 W25Q64 24-bit 物理地址。擦除只使用 4 KiB `0x20` Sector Erase，写入按 256-byte Page 边界拆分。

## 4. 链接和制品

External Loader 不从 MCU Internal Flash 启动。工程将生成与官方 IAR 范本等价的两段式 ELF/AXF：

- `StorageInfo` 放在文件地址 `0x00000000`，供 CubeProgrammer 读取；
- Loader 代码和数据放在 STM32 SRAM `0x20000004` 起始区域，供 CubeProgrammer 下载后执行。

Keil 输出为 `.axf`，构建后复制为：

```text
06_Output/Packages/ExternalLoader/W25Q64_STM32F411.stldr
```

普通 `Objects/`、`Listings/` 和 `06_Output` 生成物不作为工程输入提交。

## 5. 验收条件

### 代码验证

- Keil Clean Rebuild 成功；
- 0 error；
- 入口符号和 `StorageInfo` 均保留在 AXF/STLDR；
- `StorageInfo` 的容量、页大小、Sector 大小和擦除值正确；
- 生成制品可由 STM32CubeProgrammer 通过 `-el` 识别。

### 硬件验证

构建完成不等于硬件通过。后续板测至少验证：

1. JEDEC 读取为 `EF 40 17`；
2. A/B Header 的独立读取；
3. 4 KiB 擦除后数据为 `0xFF`；
4. 256-byte Page 写入和回读一致；
5. `app_v1.0` 物理映射镜像写入 Slot A 后，A Header 和 `+0x1000` Payload 均正确。

## 6. 参考

- [STM32CubeProgrammer User Manual](https://www.st.com/resource/en/user_manual/um2237-stm32cubeprogrammer-software-description-stmicroelectronics.pdf)
- 本地官方范本：`E:\嵌入式资料\开源项目\stm32-memory-loaders-main\external_memory_loaders\STM32F4x_boards\N25Q128A_STM32412G-DISCO`
