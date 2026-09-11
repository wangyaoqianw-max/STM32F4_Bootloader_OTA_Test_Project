# Pinout

本文件记录 MCU 引脚、板级信号、复用功能和有效电平等连接事实。

## 1. Pin Mapping

| Signal | MCU Pin | Peripheral / Function | AF | Direction | Active Level | Pull | Board Device / Connector | Reference | Status | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
|  |  |  |  |  |  |  |  |  |  |  |

## 2. Dedicated / Debug Pins

| Signal | MCU Pin | Purpose | Shared With | Status | Notes |
| --- | --- | --- | --- | --- | --- |
| SWDIO |  | Debug |  |  |  |
| SWCLK |  | Debug |  |  |  |
| NRST |  | Reset |  |  |  |
| BOOT0 |  | Boot Mode |  |  |  |

## 3. Bus Grouping

### SPI

| Bus | Signal | MCU Pin | Device | Status | Notes |
| --- | --- | --- | --- | --- | --- |
|  | SCK |  |  |  |  |
|  | MISO |  |  |  |  |
|  | MOSI |  |  |  |  |
|  | CS |  |  |  |  |

### I2C

| Bus | Signal | MCU Pin | Device | Pull-up / Voltage | Status | Notes |
| --- | --- | --- | --- | --- | --- | --- |
|  | SCL |  |  |  |  |  |
|  | SDA |  |  |  |  |  |

### UART

| UART | Signal | MCU Pin | External Device / Connector | Voltage Level | Status | Notes |
| --- | --- | --- | --- | --- | --- | --- |
|  | TX |  |  |  |  |  |
|  | RX |  |  |  |  |  |

## 4. Verification Rules

1. 优先依据原理图、PCB/BOM、开发板官方 Pinout 和 MCU Datasheet 交叉核对。
2. `AF` 必须来自 MCU Datasheet / Alternate Function 表，不允许按经验猜测。
3. `Active Level` 表示板级逻辑，例如 LED 是否低电平点亮、CS 是否低有效。
4. GPIO Speed、HAL 初始化代码、RTOS 使用方式等软件实现参数不属于本文件。
5. 同一 Pin 存在复用冲突时，应记录到 `../hardware_open_issues.md`。
