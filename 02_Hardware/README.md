# Hardware Inputs

本目录保存固件开发需要使用的原始或辅助硬件输入，例如板级原理图、Pinout 导出文件和专项硬件说明。

工程准备阶段的结构化硬件事实统一填写在：

```text
00_Project/00_Preparation/Engineering_Preparation.xlsx
```

对应 Sheet：

1. `02_资源_Hardware`：确认板上实际存在或明确计划接入的器件、型号、接口和用途；
2. `03_引脚_Pinout`：确认 MCU Pin、板级 Signal、复用功能、方向和有效电平；
3. `04_接口_HSI`：提取固件必须遵守的总线、地址、IRQ、DMA、Reset、WP、存储器物理能力等约束；
4. `05_问题_Issues`：记录所有仍未确认的硬件事实、资料缺口和待实测事项。

`Engineering_Preparation.xlsx` 是上述结构化准备数据的唯一正式数据源。本目录不再维护内容重复的 Markdown 表格。

原始资料仍按职责保存：

```text
02_Hardware/
├── Schematic/
├── Pinout/
└── Hardware_Software_Interface/
```

- `Schematic`：保存开发板或项目硬件原理图；
- `Pinout`：可保存厂商 Pinout、引脚图、EDA 导出文件等辅助资料；
- `Hardware_Software_Interface`：可保存需要长期保留的专项硬件接口说明或外部交付文档。

Datasheet、Reference Manual、协议规范等外部资料放在 `01_Reference`，并通过工作簿 `01_资料_References` Sheet 建立来源索引。

以下内容不属于准备阶段硬件事实：

- Bootloader / Application 最终 Flash 地址；
- External Flash A/B Slot 软件分区；
- OTA 状态机；
- Firmware Metadata / Image Header 数据格式；
- 驱动 API、HAL / LL / Register 实现方案；
- RTOS 任务和同步设计。

这些属于后续 Firmware Design，应通过正式 Design Stage 决策。
