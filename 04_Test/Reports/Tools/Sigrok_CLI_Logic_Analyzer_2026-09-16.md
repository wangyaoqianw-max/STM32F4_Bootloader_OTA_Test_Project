# Sigrok CLI 逻辑分析仪实验报告

## 实验元数据

- 实验日期：2026-09-16
- 仓库基线：`406d563f934297c0018e7feb520f6631b813fe11`
- 分支：`main`
- 目标 MCU：`STM32F411CEU6`
- MCU 工具链：`J-Link V9.8 PLUS`、SEGGER J-Link、Keil
- 逻辑分析仪：Saleae Logic，驱动接口 `fx2lafw:conn=4.7`
- 工具版本：`sigrok-cli 0.8.0-git-f44dd91`
- 实验范围：临时只读总线活动采集与协议解码；临时固件和工程配置已移除

> [!summary]
> `sigrok-cli` 可以控制逻辑分析仪并解码本工程真实的 SPI2 与软件 I2C 活动。实际通道映射为候选 B，即交换 `0↔1`、`2↔3`、`4↔5`。实验结束后已恢复正常 SEGGER 工具链，S05B 阶段状态未修改。

## 1. 软件与设备检查

| 检查项 | 结果 | 证据 |
|---|---|---|
| `sigrok-cli` 启动与版本 | PASS | `sigrok-cli 0.8.0-git-f44dd91`；`libsigrok 0.6.0-git`；`libsigrokdecode 0.6.0-git` |
| 设备扫描 | PASS | `--scan` 返回 Saleae Logic，8 个数字通道；Exit Code 为 0 |
| SPI 解码器 | PASS | `spi --show` 可用，要求 CLK，可选 MISO/MOSI/CS |
| I2C 解码器 | PASS | `i2c --show` 可用，要求 SCL/SDA |

扫描期间出现 `asix-omega-rtm-cli` helper 的非致命警告，但不影响 `fx2lafw` Saleae Logic 设备枚举。

## 2. 临时固件与安全边界

为产生可重复的总线活动，曾临时加入逻辑分析仪测试代码和 Keil 工程编译项，测试结束后已全部移除，未保留任何生产源码或工程配置修改。

临时固件只执行以下只读操作：

- SPI2：构造并启动 Storage 总线；读取 W25Q64 JEDEC ID、状态寄存器和少量数据。
- 软件 I2C：构造 PB6/PB7 总线；探测 AT24C02 地址并读取少量数据。
- 未执行 Flash 写入、扇区擦除、EEPROM 写入或其他修改性存储操作。

临时 Keil 构建通过。RTT 关键结果：

```text
W25Q64 JEDEC result=0 id=EF:40:17
SPI status result=0 value=00
SPI read result=0 data=FF FF FF FF
I2C read result=0 data=46 57 4D 44
```

## 3. 最终通道映射

候选 A 未产生有效协议帧；交换三组通道后的候选 B 解码出有效 SPI 和 I2C 帧，因此采用以下映射：

| 逻辑分析仪通道 | 实际信号 | MCU 引脚 | 用途 |
|---|---|---|---|
| CH0 / D0 | SPI_NSS / FLASH_CS | PB12 | SPI2 片选 |
| CH1 / D1 | SPI_CLK | PB13 | SPI2 时钟 |
| CH2 / D2 | I2C_SCL | PB6 | 软件 I2C 时钟 |
| CH3 / D3 | SPI_MISO | PB14 | SPI2 主入 |
| CH4 / D4 | I2C_SDA | PB7 | 软件 I2C 数据 |
| CH5 / D5 | SPI_MOSI | PB15 | SPI2 主出 |
| CH6 / D6 | 未使用 | - | - |
| CH7 / D7 | 未使用 | - | - |

## 4. 协议采集证据

### 4.1 SPI2

- 使用 48 MHz 采样率；请求 500,000 个采样点，设备实际返回约 9,216 个采样点，受设备缓冲窗口限制。
- 候选 B 解码参数：`spi:clk=D1:cs=D0:mosi=D5:miso=D3`。
- 观察到有效命令和数据，包括 `0x05`、`0x03`、地址 `0x00` 以及读回数据 `0xFF`。
- 候选 A（`clk=D0:cs=D1:mosi=D4:miso=D2`）未产生有意义的协议帧。

### 4.2 软件 I2C

- 使用 1 MHz 采样率、500,000 个采样点进行采集。
- 候选 B 解码参数：`i2c:scl=D2:sda=D4`。
- 解码到完整事务：设备地址 `0x50`、写地址 `0x00`、重复起始、读数据 `46 57 4D 44`、ACK、NACK 和 STOP。
- 候选 A（`scl=D3:sda=D5`）仅观察到 Start，未产生有效地址和数据。

## 5. 恢复验证

| 项目 | 结果 | 证据 |
|---|---|---|
| 临时测试代码和工程入口移除 | PASS | 测试结束后恢复原工程；提交前工作区仅包含本报告 |
| 正常 Application 构建 | PASS | `05_Tools\toolkit.bat build` 无错误、无警告 |
| 正常 Application 烧录并运行 | PASS | `05_Tools\toolkit.bat flash run` 返回 `[FLASH][PASS]` |
| 正常 RTT | PASS | `05_Tools\toolkit.bat rtt 5` 返回 `[RTT][PASS]`，看到 Application Foundation 和 Storage SPI 初始化日志 |
| 工具链进程释放 | PASS | 未发现 `JLink`、`JLinkGDBServer`、`JLinkRTTLogger`、`arm-none-eabi-gdb`、`probe-rs`、`sigrok-cli` 残留进程 |
| SEGGER Driver 与系统 PATH | NOT MODIFIED | 本实验未执行 Driver 修改或 PATH 修改命令 |
| S05B 阶段状态 | UNCHANGED | 仍为 `S05B_Toolkit_Reuse / CLOSED`；未修改 S05B 正式验证报告 |

## 6. 结论

```text
SIGROK_CLI_CONTROL:          PASS
SPI2_ANALYSIS:               PASS
SOFTWARE_I2C_ANALYSIS:       PASS
JLINK_KEIL_RTT_RECOVERY:     PASS
OVERALL:                     LOGIC_ANALYZER_DEBUG_ASSISTANT_FEASIBLE
```

本实验验证了 Agent 可以通过 `sigrok-cli` 编排逻辑分析仪采集、选择通道并解析 SPI/I2C 波形。`sigrok-cli` 不替代 J-Link 的 MCU 控制能力：Flash、Halt/Run、寄存器和 GDB 仍由 J-Link/Keil/GDB 负责，逻辑分析仪用于外部总线波形与协议证据。

## 7. 限制与未验证项

- 本实验是数字逻辑采集，未进行模拟电气质量、边沿、幅度或信号完整性测量。
- 临时固件未执行 Flash/EEPROM 写入或擦除，因此未验证写事务波形。
- 48 MHz 长窗口采集受到设备实际返回采样点数限制；后续应采用短窗口、重复事务或可靠触发策略。
- 触发等待路径本轮未作为结论依据。
- 未将逻辑分析仪入口集成到正式 Toolkit 或 S05B 架构；未提交本机绝对路径、驱动信息或采集日志。
