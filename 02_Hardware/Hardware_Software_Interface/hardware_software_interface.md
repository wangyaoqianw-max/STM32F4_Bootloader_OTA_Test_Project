# Hardware Software Interface

本文件记录固件实现必须遵守的硬件接口事实和约束。内容来自原理图、Datasheet、Reference Manual、开发板资料或实测结果。

> 本文件不是驱动设计文档，不定义软件分层、API、状态机或 Flash 软件分区。

## 1. MCU / System

| Item | Value | Reference | Status | Notes |
| --- | --- | --- | --- | --- |
| MCU |  |  |  |  |
| Supply Voltage |  |  |  |  |
| External Clock Source |  |  |  |  |
| Clock Frequency |  |  |  |  |
| Reset Source / NRST Circuit |  |  |  |  |
| BOOT Pin State |  |  |  |  |
| Debug Interface |  |  |  |  |

## 2. UART Interfaces

| Item | Value | Reference | Status | Notes |
| --- | --- | --- | --- | --- |
| Instance |  |  |  |  |
| TX Pin / AF |  |  |  |  |
| RX Pin / AF |  |  |  |  |
| Electrical Level |  |  |  |  |
| External Module / Connector |  |  |  |  |
| IRQ |  |  |  |  |
| DMA TX |  |  |  |  |
| DMA RX |  |  |  |  |
| Hardware Flow Control |  |  |  |  |

## 3. SPI Interfaces

| Item | Value | Reference | Status | Notes |
| --- | --- | --- | --- | --- |
| Instance |  |  |  |  |
| SCK Pin / AF |  |  |  |  |
| MISO Pin / AF |  |  |  |  |
| MOSI Pin / AF |  |  |  |  |
| CS Pin / Active Level |  |  |  |  |
| SPI Mode Constraint |  |  |  |  |
| Maximum Device Clock |  |  |  |  |
| IRQ / DMA Constraints |  |  |  |  |

## 4. I2C Interfaces

| Item | Value | Reference | Status | Notes |
| --- | --- | --- | --- | --- |
| Instance |  |  |  |  |
| SCL Pin / AF |  |  |  |  |
| SDA Pin / AF |  |  |  |  |
| Bus Voltage |  |  |  |  |
| Pull-up |  |  |  |  |
| Device Address |  |  |  |  |
| Addressing Notes |  |  |  |  |

## 5. External Non-volatile Memory

### Device Information

| Item | Value | Reference | Status | Notes |
| --- | --- | --- | --- | --- |
| Device / Model |  |  |  |  |
| Capacity |  |  |  |  |
| Address Width |  |  |  |  |
| Erase Unit |  |  |  |  |
| Program / Page Unit |  |  |  |  |
| Erased Value |  |  |  |  |
| JEDEC / Device ID |  |  |  |  |
| Write / Erase Busy Mechanism |  |  |  |  |
| WP / HOLD / Reset Wiring |  |  |  |  |

### EEPROM / Byte-addressable NVM

| Item | Value | Reference | Status | Notes |
| --- | --- | --- | --- | --- |
| Device / Model |  |  |  |  |
| Capacity |  |  |  |  |
| Page Size |  |  |  |  |
| Device Address |  |  |  |  |
| Write Cycle Time |  |  |  |  |
| Write Protect Wiring |  |  |  |  |

## 6. GPIO / Control Signals

| Signal | MCU Pin | Direction | Active Level | Reset Default / External Pull | Controlled Device | Status | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
|  |  |  |  |  |  |  |  |

## 7. Interrupt / DMA Resources

| Function | Peripheral | IRQ / DMA Resource | Priority Constraint / Conflict | Reference | Status | Notes |
| --- | --- | --- | --- | --- | --- | --- |
|  |  |  |  |  |  |  |

## 8. Reset / Watchdog / Boot Constraints

| Item | Hardware Fact / Constraint | Reference | Status | Notes |
| --- | --- | --- | --- | --- |
| NRST |  |  |  |  |
| BOOT0 / Boot Strap |  |  |  |  |
| IWDG Availability |  |  |  |  |
| WWDG Availability |  |  |  |  |
| External Reset / Power Control |  |  |  |  |

## 9. Hardware Constraints Requiring Software Attention

| ID | Constraint | Affected Area | Source | Status | Notes |
| --- | --- | --- | --- | --- | --- |
| HSI-001 |  |  |  |  |  |

## 10. Boundary Check

以下内容不应记录在本文件：

- Bootloader / APP 最终地址；
- External Flash Slot A / Slot B 地址；
- Metadata 软件格式；
- OTA 状态机；
- 驱动 API；
- HAL / LL / Register 实现选择；
- RTOS 任务与同步方案。

这些属于 Firmware Design，应在后续设计阶段确定。
