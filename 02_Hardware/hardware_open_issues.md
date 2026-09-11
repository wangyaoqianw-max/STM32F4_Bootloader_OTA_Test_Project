# Hardware Open Issues

本文件记录工程准备阶段尚未确认的硬件事实、资料缺失、原理图疑点和待实测事项。

> 原则：不知道就明确写 `OPEN / UNKNOWN`，不要为了填满模板而猜测。

## 1. Issue Status

- `OPEN`：尚未处理。
- `INVESTIGATING`：正在查资料、核对原理图或实测。
- `RESOLVED`：问题已确认，并已同步更新正式硬件文档。
- `DEFERRED`：当前阶段不处理，但不影响后续工作。

## 2. Open Issues

| ID | Topic | Unknown / Conflict | Impact | Evidence Needed | Owner | Status | Resolution / Reference |
| --- | --- | --- | --- | --- | --- | --- | --- |
| HW-ISSUE-001 |  |  |  |  |  | OPEN |  |

## 3. Typical Issue Sources

- 器件丝印与原理图/BOM 型号不一致；
- Datasheet 版本或厂商无法确认；
- 某引脚是否真正连接到 MCU 尚未核对；
- LED、CS、RESET、WP 等有效电平未知；
- I2C 地址受硬件 Strap 引脚影响但连接未知；
- SPI Flash / EEPROM 的实际容量或变体未知；
- IRQ / DMA 映射存在冲突；
- 原理图标注与实测行为不一致；
- 板载器件存在但缺少可靠公开资料。

## 4. Resolution Rule

问题关闭时至少完成一项：

1. 找到可靠资料并更新 `01_Reference/reference_index.md`；
2. 核对原理图、BOM、PCB 或开发板资料并更新对应硬件文档；
3. 完成实测，并记录方法、结果和证据；
4. 确认该问题对当前项目不适用并标记 `DEFERRED`。

关闭后必须把最终事实同步到 `hardware_resource_inventory.md`、`Pinout/pinout.md` 或 `Hardware_Software_Interface/hardware_software_interface.md`，本文件只保留问题处理记录。
