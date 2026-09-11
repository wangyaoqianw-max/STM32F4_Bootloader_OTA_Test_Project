# Engineering Preparation Stage

工程准备阶段位于正式功能开发阶段之前，用于收集项目原始资料、确认硬件事实、建立开发环境基线，并把暂时无法确认的问题显式记录下来。

本阶段不负责完成软件架构、Flash 软件分区、Boot 决策、OTA 状态机、Metadata 格式等设计工作。这些内容应在后续 Stage Design 中基于已确认输入产生。

## 1. 目标

准备阶段结束时，应能够回答：

- 项目使用什么 MCU、开发板及板载器件；
- 每个关键器件的官方资料在哪里，版本是什么；
- MCU 引脚与板级信号如何连接；
- UART / SPI / I2C / IRQ / DMA / Reset / Debug 等硬件接口事实是什么；
- 当前使用什么编译、烧录、调试和辅助工具；
- 哪些硬件信息仍未确认，以及这些未知项是否阻塞后续设计。

## 2. 人工输入文件

准备阶段需要人工收集或填写以下文件：

| 文件 | 作用 |
| --- | --- |
| `01_Reference/reference_index.md` | 登记 Datasheet、Reference Manual、协议、应用笔记、原理图等原始资料 |
| `02_Hardware/hardware_resource_inventory.md` | 登记 MCU、存储、通信、传感器、执行器、调试接口等板级资源 |
| `02_Hardware/Pinout/pinout.md` | 记录 MCU Pin、板级 Signal、复用功能和有效电平 |
| `02_Hardware/Hardware_Software_Interface/hardware_software_interface.md` | 记录固件必须遵守的硬件接口与约束事实 |
| `02_Hardware/hardware_open_issues.md` | 记录资料缺失、型号不确定、原理图疑点和待实测事项 |
| `00_Project/00_Preparation/development_environment.md` | 记录 IDE、编译器、SDK、调试器、脚本环境等开发基线 |

## 3. 推荐执行顺序

1. 确认项目需求和目标硬件平台。
2. 收集官方 Datasheet、Reference Manual、Errata、Application Note、协议规范、开发板原理图和 Pinout。
3. 将资料登记到 `01_Reference/reference_index.md`，并保存到对应资料目录。
4. 填写 `hardware_resource_inventory.md`，确认板上实际存在的器件和用途。
5. 根据原理图、Pinout 和 Datasheet 填写 `Pinout/pinout.md`。
6. 从已确认资料中提取固件实现必须知道的约束，填写 `Hardware_Software_Interface/hardware_software_interface.md`。
7. 将无法确认的内容写入 `hardware_open_issues.md`，不得用猜测补全。
8. 填写 `development_environment.md`，记录可复现的开发工具链。
9. 检查准备阶段退出条件，然后再进入第一个正式设计 Stage。

## 4. 事实状态

硬件与资料模板统一使用以下状态：

- `CONFIRMED`：已由可靠资料或板级事实确认。
- `MEASURED`：已通过实测确认，并应记录测试方法或证据。
- `ASSUMED`：当前仅为临时假设，不得作为冻结设计依据。
- `UNKNOWN`：尚未确认。
- `N/A`：当前项目不适用。

关键参数不允许通过经验直接填写成 `CONFIRMED`。

## 5. 准备阶段退出条件

满足以下条件后，才建议开始第一个正式设计阶段：

- 关键硬件器件型号已确认，或已明确标记为 `UNKNOWN` 并评估影响；
- MCU、外部存储、EEPROM、通信接口、调试接口等关键资料已有可追溯来源；
- 关键 Pinout 与有效电平已核对；
- 固件依赖的总线、地址、IRQ、DMA、Reset、写保护等约束已记录；
- 开发工具链版本和基础构建/烧录方式已记录；
- 所有未确认事项均进入 `hardware_open_issues.md`；
- 不存在会阻塞第一个设计阶段的未知硬件事实。

## 6. 边界约束

以下内容不属于准备阶段硬件事实，不应提前写入本阶段模板作为冻结结论：

- Internal Flash / External Flash 软件分区地址；
- Bootloader 与 Application 的最终 Memory Layout；
- OTA A/B Slot 角色和状态机；
- Firmware Image Header / Metadata 数据结构；
- Trial Boot、Confirm、Rollback 策略；
- Watchdog 软件策略；
- CRC / SHA / AES / HMAC / Signature 的最终方案。

这些内容应在需求与准备阶段输入确认后，通过正式 Design Stage 决策并留下设计记录。
