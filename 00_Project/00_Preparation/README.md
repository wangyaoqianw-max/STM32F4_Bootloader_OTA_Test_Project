# Engineering Preparation Stage

工程准备阶段位于正式功能开发阶段之前，用于收集项目原始资料、确认硬件事实、建立开发环境基线，并把暂时无法确认的问题显式记录下来。

本阶段不负责完成软件架构、Flash 软件分区、Boot 决策、OTA 状态机、Metadata 格式等设计工作。这些内容应在后续 Stage Design 中基于已确认输入产生。

## 1. 准备阶段数据源

工程准备阶段的结构化数据统一填写在：

```text
00_Project/00_Preparation/Engineering_Preparation_Bilingual.xlsx
```

该工作簿是准备阶段人工填写数据的唯一正式数据源。不要再建立平行的 Markdown 表格保存相同数据，避免事实不一致。

工作簿采用中英双语字段，包含以下 Sheet：

| Sheet | 内容 |
| --- | --- |
| `00_说明_Instructions` | 填写规则、状态定义和阶段边界 |
| `01_资料_References` | Datasheet、Reference Manual、Errata、Application Note、协议、原理图等资料索引 |
| `02_资源_Hardware` | MCU、存储、通信、传感器、执行器、调试接口等硬件资源 |
| `03_引脚_Pinout` | MCU Pin、板级 Signal、复用功能、方向和有效电平 |
| `04_接口_HSI` | UART / SPI / I2C / IRQ / DMA / Reset / WP / 存储器物理能力等硬件软件接口事实 |
| `05_问题_Issues` | 资料缺失、型号不确定、原理图疑点和待实测事项 |
| `06_环境_Environment` | IDE、Compiler、SDK、Debugger、Python 和辅助工具版本 |

## 2. 原始资料存放位置

Excel 只保存索引和提炼后的结构化事实，原始资料仍按类型保存在仓库目录中：

```text
01_Reference/
├── Datasheets/
├── Reference_Manuals/
├── Application_Notes/
├── Protocols/
└── Other/

02_Hardware/
├── Schematic/
├── Pinout/
└── Hardware_Software_Interface/
```

其中：

- `01_Reference` 保存外部原始参考资料；
- `02_Hardware/Schematic` 保存板级原理图等硬件源文件；
- `Pinout` 和 `Hardware_Software_Interface` 目录可保存辅助图、导出文件或专项说明，但准备阶段的结构化事实以 Excel 对应 Sheet 为准。

## 3. 推荐执行顺序

1. 确认项目需求和目标硬件平台。
2. 收集官方 Datasheet、Reference Manual、Errata、Application Note、协议规范、开发板原理图和 Pinout。
3. 将资料放入 `01_Reference` 或 `02_Hardware/Schematic` 对应目录，并填写 `01_资料_References`。
4. 填写 `02_资源_Hardware`，确认板上实际存在或明确计划接入的器件和用途。
5. 根据原理图、Pinout 和 Datasheet 填写 `03_引脚_Pinout`。
6. 从已确认资料中提取固件实现必须遵守的约束，填写 `04_接口_HSI`。
7. 无法确认的内容填写到 `05_问题_Issues`，不得用猜测补全。
8. 填写 `06_环境_Environment`，记录可复现的开发工具链。
9. 检查准备阶段退出条件，再进入第一个正式 Design Stage。

## 4. 事实状态

工作簿统一使用以下双语状态：

- `已确认 / CONFIRMED`：已由可靠资料或板级事实确认；
- `已实测 / MEASURED`：已通过实测确认，并应记录验证方法或证据；
- `临时假设 / ASSUMED`：当前仅为临时假设，不得作为冻结设计依据；
- `未确认 / UNKNOWN`：尚未确认；
- `不适用 / N/A`：当前项目不适用。

关键参数不允许仅凭经验直接填写为 `已确认 / CONFIRMED`。

问题状态使用：

- `待处理 / OPEN`；
- `阻塞 / BLOCKED`；
- `已解决 / RESOLVED`；
- `不适用 / N/A`。

## 5. 准备阶段退出条件

满足以下条件后，才建议开始第一个正式设计阶段：

- 关键硬件器件型号已确认，或已明确标记为 `UNKNOWN` 并评估影响；
- MCU、外部存储、EEPROM、通信接口、调试接口等关键资料已有可追溯来源；
- 关键 Pinout 与有效电平已核对；
- 固件依赖的总线、地址、IRQ、DMA、Reset、写保护等约束已记录；
- 开发工具链版本和基础构建、烧录、调试方式已记录；
- 所有未确认事项均进入 `05_问题_Issues`；
- 不存在状态为 `阻塞 / BLOCKED` 且会影响第一个设计阶段的问题。

## 6. 边界约束

以下内容不属于准备阶段硬件事实，不应提前写入工作簿作为冻结结论：

- Internal Flash / External Flash 软件分区地址；
- Bootloader 与 Application 的最终 Memory Layout；
- OTA A/B Slot 角色和状态机；
- Firmware Image Header / Metadata 数据结构；
- Trial Boot、Confirm、Rollback 策略；
- Watchdog 软件策略；
- CRC / SHA / AES / HMAC / Signature 的最终方案。

这些内容应在需求与准备阶段输入确认后，通过正式 Design Stage 决策并留下设计记录。
