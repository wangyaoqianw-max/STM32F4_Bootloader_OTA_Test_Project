# Firmware AGENTS.md

本文件适用于 `03_Firmware` 下的 Application、Bootloader、Shared、固件文档和后续固件测试代码。仓库级流程仍由根目录 `AGENTS.md` 约束。

## 1. 编码前门禁

新增或修改项目自研 `.c/.h` 前，必须完整读取：

```text
03_Firmware/00_Doc/Standards/嵌入式C代码规范.md
```

开始编码前的汇报必须包含：

```text
Coding Standard:
03_Firmware/00_Doc/Standards/嵌入式C代码规范.md
Status: READ
```

同时确认：

- 当前需求、冻结设计和实施计划；
- 目标文件属于自研代码、自动生成代码还是 Vendor；
- 模块职责、调用关系和依赖方向；
- 数据来源、去向、所有权和生命周期；
- ISR、DMA、Task、Callback 和共享资源关系；
- 错误策略与验证方法。

仍无法回答关键边界时，不进入复杂实现。

## 2. 架构与依赖

默认依赖方向：

```text
APP
 ↓
Service
 ↓
Platform
 ↓
Impl / Board / BSP
 ↓
HAL / CMSIS / RTOS / Vendor
 ↓
Hardware
```

- APP 负责业务流程、模块组合和任务级状态协调。
- Service 提供有明确业务语义或独立复用价值的能力。
- Platform 描述系统能力，公共 API 不泄漏 HAL、RTOS Handle、MCU 寄存器或 Vendor 类型。
- Impl、Board 和 BSP 负责具体 MCU、外设、中断、DMA 与 RTOS 映射。
- 下层不得反向依赖具体业务逻辑。
- 不为形式完整增加无实际职责的层、包装接口、回调链或状态机。

## 3. 固件工程边界

- `Application` 是默认 RTOS 主工程，可按需求使用 APP、Service、Platform、Impl 和 Vendor 分层。
- `Bootloader` 按启动、升级、恢复和资源约束裁剪，不机械复制 Application 的完整 RTOS 架构。
- `Shared` 只保存 Application 与 Bootloader 已经共同使用、接口稳定的代码；只有一个使用方时放回所有者目录。
- Application 与 Bootloader 的链接脚本、Flash Layout、启动入口和构建配置分别维护。

## 4. Vendor 与自动生成代码

- HAL、LL、CMSIS、RTOS Kernel、Vendor SDK 和第三方库保持原目录、命名、类型、格式与 API。
- 不为了统一风格批量修改、重命名或格式化第三方代码。
- 必要适配优先放在 Platform、Impl、Board 或专用 Adapter。
- CubeMX 管理的文件优先只修改 `USER CODE BEGIN/END` 区域。
- 必须修改生成区或第三方源码时，记录原因、影响、升级风险和重新生成后的迁移方式。
- 业务逻辑不得长期堆积在 `main.c`、中断文件或外设初始化文件中。

## 5. ISR、DMA 与并发

ISR 保持最短路径：识别并清除事件、保存最少状态、通知任务、退出。ISR 中禁止阻塞、普通 Mutex、动态内存、长循环、完整协议解析和大量日志。

使用 FreeRTOS FromISR API 时正确处理 `xHigherPriorityTaskWoken` 和 `portYIELD_FROM_ISR()`。

DMA 只负责数据搬运。设计必须明确 Buffer、方向、长度、所有权、完成事件、错误、Overflow、Cache 和生命周期。TX 完成前不得修改 Buffer；RX 不得读取未完成区域。

Circular DMA、RingBuffer 或双缓冲必须明确：

```text
Producer / Consumer
读写位置
Buffer Size
Wrap-around
Full / Empty
Overflow Policy
并发同步
```

采用 Single Producer / Single Consumer 时保持该约束，不无必要加锁。UART DMA + IDLE 不等同于完整协议帧。

## 5.1 Keil 构建边界

使用或修改 Keil 工程前，必须读取：

```text
03_Firmware/00_Doc/Standards/Keil工程与构建输出规范.md
```

- 源码、`.ioc`、`.uvprojx`、人工维护的链接配置属于工程输入；构建生成物不得与其混放。
- 每个 Keil 工程的输出统一进入本工程 `MDK-ARM/Objects/` 和 `MDK-ARM/Listings/`。
- 两个输出目录只能保存可通过 Clean + Rebuild 恢复的文件，并由根 `.gitignore` 排除。
- 构建期间不得修改本次构建使用的源码和配置，也不得并发清理、移动或重写构建输出。
- 遇到 `couldn't write *.o`、`I/O error` 或 `Invalid argument` 时，先检查 Target 输出配置、目录、权限、文件占用、文件系统和实时扫描；没有源码诊断证据时，不得通过修改 `.c` 文件规避 I/O 故障。
- 调整输出目录后必须执行 Clean Rebuild，并用 `git status` 确认没有生成物落到未忽略位置。

## 6. RTOS 规则

- Task 必须具有独立执行节奏、阻塞等待或实时性职责，不采用“一个模块一个 Task”。
- 无工作时 Task 应进入 Blocked 状态，禁止无阻塞 Busy Loop。
- 固定周期任务优先使用绝对周期调度。
- IPC 选择使用最简单且语义匹配的机制：事件通知用 Task Notification，数据传递用 Queue，资源互斥用 Mutex，事件同步用 Event Group，事件计数用 Counting Semaphore。
- Queue Full、Timeout、资源创建失败和 Task Stack 风险必须有明确策略。
- Queue 传递指针时明确所有权和生命周期。

## 7. 数据、内存与生命周期

复杂模块按实际职责区分：

```text
Config   模块应该怎样工作
Context  模块现在怎样运行
Data     模块当前提供什么数据
```

简单模块不机械建立三套结构体。跨 Task、ISR、DMA 或 Callback 的数据必须明确谁写、谁读、如何同步以及何时失效。

默认优先静态内存、固定 Buffer 和编译期配置。使用动态内存时说明最大占用、生命周期、失败处理和碎片风险；ISR 默认禁止普通动态分配。

对象释放前先停止可能继续访问它的 DMA、IRQ、Timer、Callback 和异步事件。

## 8. 错误、日志与诊断

- 禁止无声失败，可能失败的 API 必须检查并传播返回值。
- Impl 将 HAL、RTOS 和 Vendor 错误映射为上层稳定错误语义。
- Retry 只用于可恢复错误，必须有次数或时间边界。
- Assert 只用于内部不变量，不替代运行时错误处理。
- 日志通过统一机制输出，避免同一错误逐层重复打印。
- ISR、DMA Callback、高频 Timer、控制循环和高优先级任务严格限制日志。
- Fault Handler 保持最小依赖，并尽可能保存关键现场。

## 9. 硬件事实

GPIO、Pin AF、电平、Clock、SPI Mode、I2C Address、IRQ、DMA、Flash、Reset 和电源时序必须依据以下资料确认：

```text
Datasheet
→ Reference Manual
→ Programming Manual
→ 官方 Application Note / Errata
→ Schematic
→ Vendor Example
→ 第三方资料
```

资料不足时明确标记为“待确认”，不得按经验生成具体硬件值。Flash 擦写必须检查地址、分区、对齐、Erase Unit、Program Unit 和保护区。

## 10. Coding Standard Review

每个实现任务提交前必须检查：

1. 命名、类型、文件组织和注释是否符合详细 C 规范；
2. NULL、长度、范围、状态、Timeout 和返回值是否检查；
3. Buffer、资源、对象和异步操作的生命周期是否正确；
4. ISR、DMA、Task 和共享状态是否存在竞态或阻塞风险；
5. Platform 是否泄漏具体实现类型；
6. 是否修改了不应修改的生成代码或 Vendor 代码；
7. 是否产生新的 Compiler Warning。

结果写入阶段交接：

```text
Coding Standard Review: PASS / NEEDS_FIX / EXCEPTION
```

`EXCEPTION` 必须记录文件、规则、工程理由和后续处理。

## 11. 验证

根据模块选择编译、静态检查、Host Test、RTT、UART、Debugger、Logic Analyzer、Oscilloscope 或寄存器检查。

完成声明必须分别记录：

```text
代码验证：PASS / FAIL / NOT_APPLICABLE
硬件验证：PASS / PENDING / FAIL / NOT_APPLICABLE
```

编译或 Host Test 通过不能证明真实硬件功能通过。无法执行板测时，将硬件状态标记为 `PENDING` 并给出具体板测步骤和预期现象。
