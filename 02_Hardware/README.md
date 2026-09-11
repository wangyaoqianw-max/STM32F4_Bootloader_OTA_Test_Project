# Hardware Inputs

本目录只维护固件开发需要使用的硬件输入：原理图、资源清单、Pinout、硬件软件接口事实和未确认问题。

工程准备阶段按以下顺序填写：

1. `hardware_resource_inventory.md`：确认板上实际存在的器件、型号和用途；
2. `Pinout/pinout.md`：确认 MCU Pin、板级 Signal、复用功能和有效电平；
3. `Hardware_Software_Interface/hardware_software_interface.md`：提取固件必须遵守的总线、地址、IRQ、DMA、Reset、存储器物理能力等约束；
4. `hardware_open_issues.md`：记录所有仍未确认的硬件事实和资料缺口。

原始 Datasheet、Reference Manual、协议规范等放在 `01_Reference`，并通过 `01_Reference/reference_index.md` 建立来源索引。

以下内容不属于本目录：

- Bootloader / Application 最终 Flash 地址；
- External Flash A/B Slot 软件分区；
- OTA 状态机；
- Firmware Metadata / Image Header 数据格式；
- 驱动 API、HAL / LL / Register 实现方案；
- RTOS 任务和同步设计。

这些属于后续 Firmware Design。
