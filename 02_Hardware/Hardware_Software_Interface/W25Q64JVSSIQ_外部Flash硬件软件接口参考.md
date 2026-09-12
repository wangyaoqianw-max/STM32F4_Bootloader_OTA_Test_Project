# W25Q64JVSSIQ 外部 Flash 硬件软件接口参考

## 1. 文档目的与边界

本文将 W25Q64JVSSIQ 数据手册、当前板卡 SPI Flash 原理图片段，以及现有 Application 工程中的 SPI 配置进行汇总，作为 `S02_External_Flash_Driver` 设计与后续实现的硬件事实输入。

本文**不是** Flash 驱动接口设计、Flash 分区设计或 OTA 方案；不冻结 A/B Slot、Firmware Image、Metadata、SFUD 接入方式或任务并发方案。

## 2. 器件与存储边界

| 项目 | 本项目事实 / 约束 |
| --- | --- |
| 板载器件 | U4：Winbond `W25Q64JVSSIQ`，SOIC-8 封装 |
| 容量 | 64 Mbit = 8 MiB |
| 可访问地址 | `0x000000` ~ `0x7FFFFF`，使用 24 位地址 |
| Page Program 单元 | 最多 256 Byte；一次写入不得跨越 256 Byte Page 边界 |
| 最小擦除单元 | 4 KiB Sector；共 2,048 个 Sector |
| 较大擦除单元 | 32 KiB / 64 KiB Block；共 128 个 64 KiB Block |
| 擦除后的数据 | `0xFF` |
| 工作电压范围 | 2.7 V ~ 3.6 V；原理图片段仅标为 `FLSAH_VCC`，其实际标称电压仍应在板级电源资料中确认 |
| 目标工作方式 | 四线标准 SPI；IO2 / IO3 未连接 MCU，Quad 数据通路不可用 |

## 3. 板级连接事实

### 3.1 U4 引脚与 MCU 映射

| U4 引脚 | W25Q64JVSSIQ 信号 | 原理图网络 | STM32F411 引脚 | 固件使用要求 |
| --- | --- | --- | --- | --- |
| 1 | `/CS` | `FLASH_SPI_NSS` | PB12 | 低有效片选；上电和复位期间必须保持高 |
| 2 | DO / IO1 | `FLASH_SPI_MISO` | PB14 / SPI2_MISO | 标准 SPI 数据输入 |
| 3 | `/WP` / IO2 | `FLSAH_VCC` | 未连接 MCU | 板上拉高；不得用于 Quad SPI |
| 4 | GND | GND | — | 地 |
| 5 | DI / IO0 | `FLASH_SPI_MOSI` | PB15 / SPI2_MOSI | 标准 SPI 数据输出 |
| 6 | CLK | `FLASH_SPI_CLK` | PB13 / SPI2_SCK | SPI 时钟 |
| 7 | `/HOLD` or `/RESET` / IO3 | `FLSAH_VCC` | 未连接 MCU | 板上拉高；不可作为 MCU 可控暂停/复位信号 |
| 8 | VCC | `FLSAH_VCC` | — | 供电；C10 100 nF 就近去耦 |

原理图中 `/CS` 由 R20（10 kOhm）上拉至 `FLSAH_VCC`。这保证 MCU 未完成 GPIO 初始化时 Flash 未被选中，是必须保留的启动安全条件。

`FLASH_SPI_*_M` 与 MCU 的映射为：PB12 = NSS、PB13 = CLK、PB14 = MISO、PB15 = MOSI。SPI 外设实例为 **SPI2**，不是 SPI1。

### 3.2 接口模式结论

W25Q64JV 支持 SPI Mode 0 `(CPOL=0, CPHA=0)` 与 Mode 3 `(CPOL=1, CPHA=1)`，均为 MSB first。数据手册规定标准 SPI 在 CLK 上升沿采样输入、下降沿输出数据。

本器件型号末尾为 `IQ`：QE（Quad Enable）出厂固定为 1。此时引脚 3、7 分别作为 IO2、IO3，`/WP` 与 `/HOLD` 功能不可用。由于本板把两脚固定上拉到 `FLSAH_VCC`、未连接 MCU，Quad Read、Quad Page Program、Quad I/O 与 Burst-with-Wrap 等依赖 IO2/IO3 的指令在本板上不可用。Raw Driver 第一阶段仅采用标准单线数据指令；如以后评估 Dual 指令，必须单独验证 STM32F411 对 IO0 双向切换的实现与时序，不能直接按标准 SPI 事务复用。

标准 SPI 指令仍使用 MOSI（IO0）和 MISO（IO1），不受该硬件连接限制。

### 3.3 SPI2 / GPIO 配置约束

后续 S02 配置应满足以下硬件约束：

- PB13、PB14、PB15 配置为 SPI2 的复用功能，形成全双工主机模式。
- PB12 应作为由软件显式控制的 GPIO 输出片选，复位默认输出高电平；SPI 的 NSS 管理应使用 Software NSS。
- 设备时钟应不高于数据手册允许值，并以实际板级波形验证为准：`03h Read Data` 上限 50 MHz；其它高速读类指令在 3.0 V ~ 3.6 V 时上限 133 MHz、在 2.7 V ~ 3.0 V 时上限 104 MHz。本文不冻结 S02 的实际工作频率。
- 片选必须覆盖一整条指令（命令、地址、数据 / dummy）；写、编程和擦除指令的最后一个字节后必须拉高 `/CS`，才会启动内部操作。

## 4. 当前工程一致性检查

当前 `OTA_APP.ioc` 已保留 PB12~PB15 的 `SPI2_NSS/SCK/MISO/MOSI` 引脚标记，但实际生成代码只初始化了 `SPI1`：PA5 = SCK、PA7 = MOSI，且没有 MISO。因此该 SPI1 配置不能访问本板外部 Flash。

现有 Platform / Impl SPI 模块同样只构造 `SPI1` 的 `display_spi_bus`，并且仅提供阻塞 `write` 操作，不具备读取 Status、JEDEC ID 或 Flash 数据所需的全双工 transfer / receive 能力。这些是 S02 设计与实施前必须解决的工程差异；本文仅记录事实，不修改 CubeMX、Platform 或 Impl 代码。

## 5. 原始 Flash 驱动的最小指令集

下表只列出本项目 Raw External Flash Driver 第一阶段所需或建议诊断使用的标准 SPI 指令。

| 用途 | 指令 | Opcode | 标准事务（均为 MSB first） | 项目约束 |
| --- | --- | --- | --- | --- |
| 识别器件 | Read JEDEC ID | `0x9F` | 命令后读取 3 Byte | 应返回 `EF 40 17`（Winbond / 64-Mbit）；板级最先验证项 |
| 写使能 | Write Enable | `0x06` | 单字节命令 | 每次 Program / Erase 前都必须执行；随后读 SR1 确认 WEL = 1 |
| 写禁止 | Write Disable | `0x04` | 单字节命令 | 可用于错误路径或主动收口写窗口 |
| 读状态 | Read Status Register-1 | `0x05` | 命令后读取 1 Byte | SR1 bit0 = BUSY，bit1 = WEL；内部操作期间仍可读状态 |
| 读数据 | Read Data | `0x03` | 命令 + 24-bit 地址 + 连续读数据 | 允许连续读，但访问范围不得超过 `0x7FFFFF` |
| 写页 | Page Program | `0x02` | `0x06` 后，命令 + 24-bit 地址 + 1~256 Byte 数据 | 目标区域必须已擦除；每次不得跨 Page |
| 擦扇区 | Sector Erase | `0x20` | `0x06` 后，命令 + 24-bit 扇区地址 | 地址必须 4 KiB 对齐；优先作为 V1 最小擦除 API |
| 擦 Block | 32 KiB / 64 KiB Block Erase | `0x52` / `0xD8` | `0x06` 后，命令 + 24-bit 地址 | 仅在上层明确需要较大批量擦除时使用 |
| 软件复位 | Enable Reset + Reset Device | `0x66` + `0x99` | 两笔独立命令，按顺序发送 | 仅在确认 BUSY=0 且 SUS=0 后使用；运行中复位可能破坏数据 |

> [!warning]
> `Chip Erase`（`0xC7` / `0x60`）会擦除整颗 8 MiB Flash，最长可达 100 s。它不属于 Raw Driver 的常规业务路径，也不能作为 OTA、格式化或普通测试的默认操作。

## 6. 状态、时序与超时边界

Flash 在 Page Program、Erase 或写状态寄存器期间，除状态寄存器读取外会忽略其它指令。因此驱动必须采用“发起操作后轮询 SR1.BUSY，直至为 0 或软件超时”的模式，不能用固定短延时替代完成判断。

| 操作 | 典型值 | 数据手册最大值 | 驱动超时下限 |
| --- | ---: | ---: | ---: |
| Page Program（最多 256 Byte） | 0.4 ms | 3 ms | 不小于 3 ms，并预留调度余量 |
| 4 KiB Sector Erase | 45 ms | 400 ms | 不小于 400 ms，并预留调度余量 |
| 32 KiB Block Erase | 120 ms | 1,600 ms | 不小于 1,600 ms，并预留调度余量 |
| 64 KiB Block Erase | 150 ms | 2,000 ms | 不小于 2,000 ms，并预留调度余量 |
| Chip Erase | 20 s | 100 s | 不应作为常规 API |
| 软件复位完成 | — | 30 us | 复位后至少等待该时间再发命令 |
| 上电后允许写指令 | — | 5 ms | 初始化或首个写操作前满足该条件 |

### 6.1 原始读 / 写 / 擦除规则

- 所有 `address + length` 校验必须防止整数溢出，并保证最终地址不超过 `0x7FFFFF`。
- Page Program 的长度为 1~256 Byte，且 `address % 256 + length <= 256`；跨页写由上层循环切分为多笔 Page Program。
- Sector Erase 地址必须满足 `address % 4096 == 0`。若未来开放 Block Erase，则分别要求 32 KiB、64 KiB 对齐。
- NOR Flash 写入只能将 bit 从 1 改为 0；重新写入前必须由调用方或上层流程先完成适当粒度的擦除。Raw Driver 不应静默替调用方擦除。
- 每笔修改性命令后，WEL 会被器件清零；下一笔修改操作必须重新执行 `Write Enable`。
- 从复位、上电或异常恢复路径读取 SR1 / SR2 / SR3 以记录保护与器件状态；不要无依据写状态寄存器或保护位。

## 7. S02 设计与板级验证输入

进入实现前，S02 设计至少应将下列项目形成可验证的设计结论：

1. 将 CubeMX / 生成代码调整为 SPI2 全双工，同时保持 PB12 软件片选上电为高；确认不会影响其它已用引脚。
2. 在 Platform / Impl 边界提供单次事务所需的发送、接收或全双工交换能力，且不泄漏 HAL Handle。
3. Raw Flash Driver 首先完成 JEDEC ID、SR1 轮询、单扇区擦除、跨页写与连续读；地址、长度、对齐和超时错误必须可区分。
4. 板级最小验证顺序：`0x9F` 识别 → 读状态 → 擦除一个专用 4 KiB 扇区 → 跨页写入测试模式 → 连续读回逐字节比对 → 复位后再次读回。
5. 在没有冻结 Flash 分区前，板测只能使用明确划出的专用测试扇区；不得用整片擦除替代测试。

## 8. 原始资料与追溯

- [W25Q64JVSSIQ 英文规格书（Markdown）](../../01_Reference/Datasheets/W25Q64JVSSIQ_英文规格书/W25Q64JVSSIQ_英文规格书.md)：器件容量、指令、状态寄存器、时序和 `IQ` 封装选项。
- [W25Q64JVSSIQ 英文规格书（PDF）](../../01_Reference/Datasheets/W25Q64JVSSIQ_英文规格书/W25Q64JVSSIQ_英文规格书.PDF)：原始厂商资料，Revision J（2018-03-27）。
- [MiniF4x1Cx_V31 原理图](../Schematic/MiniF4x1Cx_V31%20SchDoc.pdf)：U4、R20、C10 与 SPI2 网络连接的原始依据。
- `03_Firmware/Application/OTA_APP/OTA_APP.ioc`、`Core/Src/spi.c`、`03_Platform/platform_mcu/spi/`、`04_Impl/impl_mcu/impl_platform_spi.c`：当前工程 SPI 配置与能力的核对依据。

## 9. 变更记录

| 日期 | 说明 |
| --- | --- |
| 2026-09-12 | 初版：根据 W25Q64JVSSIQ 数据手册、外部 Flash 原理图片段与当前工程 SPI 配置建立。 |
