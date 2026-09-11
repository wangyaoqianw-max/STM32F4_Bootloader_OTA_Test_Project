# Reference Index

本文件用于登记项目准备阶段收集到的外部资料，并记录其来源、版本与可信度。

> 原则：优先保存官方原始资料；无法确认来源或版本的信息必须明确标记，不得作为冻结设计依据。

## 1. 状态定义

- `CONFIRMED`：资料来源与适用对象已确认。
- `UNVERIFIED`：已收集，但来源、版本或适用性尚未确认。
- `MISSING`：项目需要，但尚未获得可靠资料。
- `OBSOLETE`：已被更新版本替代，仅保留历史参考。

## 2. 资料索引

| ID | Document | Device / Topic | Revision | Source / Vendor | Local File | Authority | Status | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| REF-001 |  |  |  |  |  | Official / Reference / Unknown |  |  |

## 3. 建议收集资料

项目实例化时至少检查以下类别：

- MCU Datasheet
- MCU Reference Manual
- Cortex-M Programming Manual
- Errata Sheet
- 外部 Flash Datasheet
- EEPROM Datasheet
- 其他板载器件 Datasheet
- 协议规范（如 Ymodem）
- 与启动、Flash、IWDG、DMA、低功耗或安全相关的 Application Note
- 开发板原理图、Pinout、BOM 或官方说明

## 4. 使用约束

1. `01_Reference` 保存原始资料，不在此目录写软件设计结论。
2. 从资料中提取并确认的板级事实写入 `02_Hardware`。
3. 当不同资料之间存在冲突时，先登记到硬件待确认事项，不得自行选择一个结果并写成已确认事实。
4. 任何影响地址、容量、擦写粒度、电平、时序、复用功能或安全行为的结论，应能追溯到本索引中的资料或实测记录。
