# Hardware Resource Inventory

本文件登记项目实际使用的板级硬件资源。这里只记录硬件事实和项目用途，不记录软件架构或最终实现方案。

## 1. 状态定义

- `CONFIRMED`：型号、连接或能力已由可靠资料确认。
- `MEASURED`：已通过实测确认。
- `ASSUMED`：临时假设，后续必须确认。
- `UNKNOWN`：尚未确认。
- `N/A`：当前项目不适用。

## 2. Hardware Inventory

| ID | Resource Type | Device / Model | Qty | Interface | Project Purpose | Reference ID | Status | Notes |
| --- | --- | --- | ---: | --- | --- | --- | --- | --- |
| HW-001 | MCU |  | 1 | - | Main Controller |  |  |  |
| HW-002 | External Flash |  | 1 | SPI | Firmware Storage |  |  |  |
| HW-003 | EEPROM |  | 1 | I2C | Persistent State / Metadata |  |  |  |
| HW-004 | Security Device |  | 1 | I2C | Security Experiment |  |  |  |
| HW-005 | LED |  |  | GPIO / PWM | Functional Verification |  |  |  |
| HW-006 | UART Interface |  |  | UART | Firmware Transfer / Debug |  |  |  |
| HW-007 | Debug Interface |  |  | SWD | Flash / Debug / RTT |  |  |  |

## 3. MCU Resources

| Item | Value | Reference ID | Status | Notes |
| --- | --- | --- | --- | --- |
| MCU Part Number |  |  |  |  |
| Core |  |  |  |  |
| Internal Flash Size |  |  |  |  |
| SRAM Size |  |  |  |  |
| Package |  |  |  |  |
| Max Core Clock |  |  |  |  |
| Supply Voltage |  |  |  |  |
| Internal Watchdog |  |  |  |  |

## 4. Non-volatile Storage Resources

| Device | Capacity | Erase Unit | Program / Page Unit | Interface | Special Pins / Constraints | Reference ID | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
|  |  |  |  |  |  |  |  |

> 注意：这里只记录器件物理能力。Slot A / Slot B、Bootloader / APP 地址、Metadata 软件布局等属于后续 Firmware Design。

## 5. Communication / Peripheral Resources

| Resource | Hardware Device | MCU Peripheral Candidate | Electrical Level | External Connector / Module | Status | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| UART |  |  |  |  |  |  |
| SPI |  |  |  |  |  |  |
| I2C |  |  |  |  |  |  |
| SWD |  |  |  |  |  |  |

## 6. Review Checklist

- [ ] 所有关键器件型号均已核对板卡丝印、原理图或 BOM。
- [ ] 所有关键器件均能追溯到 `01_Reference/reference_index.md`。
- [ ] 未确认的型号或能力没有被写成 `CONFIRMED`。
- [ ] 软件分区、状态机和协议设计没有混入本文件。
