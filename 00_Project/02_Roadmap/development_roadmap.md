# Development Roadmap

路线图描述项目长期阶段顺序、能力演进、前置依赖和阶段完成标准，不替代单个 Stage 的 `design.md`、`implementation_plan.md`、`handoff.md` 和 `review.md`。

正式 Stage 的工作流状态以 `00_Project/WORKFLOW.md` 和 `00_Project/05_Status/current_status.md` 为准。

Roadmap State：

- `PLANNED`：已纳入长期路线，但尚未创建或启动正式 Stage；
- `ACTIVE`：正式 Stage 已创建且尚未关闭；
- `CLOSED`：Stage 已完成 Verification / Review 并正式关闭。

## 1. Roadmap Principles

本项目按“先建立基础能力，再形成 OTA 下载链，再形成 Bootloader 安装与可靠性闭环，最后增加诊断和安全扩展”的顺序推进。

阶段拆分遵循：

1. 每个 Stage 增加一项可独立验证的系统能力；
2. 每个 Stage 有明确前置依赖、工程交付物和验证证据；
3. Application 与 Bootloader 分开设计，Bootloader 不机械复制 Application 的完整分层与 RTOS 架构；
4. UART/Ymodem、Flash/EEPROM Raw Driver 等基础模块只解决各自职责，不提前承载 OTA 业务语义；
5. Firmware Image、Slot、Metadata 等跨模块契约在传输和 Bootloader 安装前明确；
6. LCD、CK02AT 和其他非核心扩展不阻塞 V1 OTA 主链；
7. 真实资料不足时按阶段补充，不把非阻塞开放项提前冻结；
8. Device Manager、Storage Device 等大型统一架构不为单一阶段强行引入。

## 2. Phase Overview

```text
Phase A - Application Infrastructure
S01 → S04

Phase B - OTA Download Path
S05 → S07

Phase C - Bootloader & Reliability
S08 → S10

Phase D - Diagnostics & Security Extension
S11 → S12
```

核心能力演进：

```text
S01  Application 能稳定运行
 ↓
S02  能可靠使用 External SPI Flash
 ↓
S03  能保存掉电状态
 ↓
S04  系统能够识别 Firmware Image / Slot / Metadata
 ↓
S05  能从 PC 可靠接收 Firmware 文件
 ↓
S05A 建立 Agent 可调用的 GDB / Fault 诊断能力
 ↓
S05B PC 工具形成可扩展、可升级、可复用框架
 ↓
S06  Application 形成正式 RTOS Runtime / Concurrency Model
 ↓
S07  Application OTA 下载链完成
 ↓
S08  Bootloader 能验证并启动 Application
 ↓
S09  Bootloader 能安装 Pending Firmware
 ↓
S10  OTA 具备 Trial / Confirm / Rollback 可靠性闭环
 ↓
S11  增加可视化诊断与展示
 ↓
S12  增加 OTA 安全机制实验
```

## 3. Current Roadmap

| Stage | Goal | Main Implementation | Prerequisites | Roadmap State | Completion Criteria |
| --- | --- | --- | --- | --- | --- |
| `S00_Template_Restructure` | 建立通用工程模板、跨工具上下文合同和工程准备机制 | 项目目录、阶段工作流、上下文合同、工程准备模板、构建规范 | 目录与工作流设计获批 | `CLOSED` | 结构与构建规范验证通过、Project Owner 审核通过，并被当前项目采用 |
| `S01_Application_Foundation` | 建立可继续扩展的 Application 基础工程 | App / Service / Platform / Impl / Vendor / Config；RTT + EasyLogger；基础初始化与错误处理；LED Blink | S00；STM32F411 基础工程与板级资料 | `CLOSED` | Clean Rebuild；LED Blink；RTT/EasyLogger；架构依赖与初始化流程检查通过 |
| `S02_External_Flash_Driver` | 建立 W25Q64 原始非易失存储能力 | SPI2 Platform/Impl；W25Q64 Read / Program / Erase / JEDEC / BUSY/WEL / 边界；RTT 板测 | S01；W25Q64 / SPI2 资料 | `CLOSED` | JEDEC、Erase、Program、Read Back、跨页、边界、Reset 保持和硬件回归通过 |
| `S03_EEPROM_Storage` | 建立掉电后可保存的小容量状态存储能力 | Software I2C；AT24C02 Read / Write；8 Byte Page Split；ACK Polling；边界；RTT 板测 | S01；AT24C02 / I2C 资料 | `CLOSED` | 单字节/页内/跨页、越界、Reset/掉电保持和错误诊断通过 |
| `S04_Firmware_Image_Storage` | 建立 Firmware Image、A/B Slot 和 Metadata 基础模型 | Slot A/B；Firmware Header；Version / Size / CRC；Metadata 双副本；Firmware Storage；PC pack tool | S02；S03 | `CLOSED` | Firmware Contract、Host Test、Keil Build、真实板测、Persistence 补充回归和 Review 通过 |
| `S05_UART_Ymodem` | 建立 Firmware 文件传输通道 | Tera Term Reference Sender；UART DMA/RingBuffer；Ymodem Parser / Receiver / Sink；Block 0；CRC-16；Timeout / Cancel / Retry；Firmware Storage Header/Payload 写入；RTT 板测 | S02；S04；现有通信 UART；Ymodem 高可信参考 | `CLOSED` | Tera Term 发送 S04 `.img`；MCU 完整接收并按 Slot 合同写入；`firmware_storage_validate_image()` VALID；中止后不提交 Header，重新传输可恢复 |
| `S05A_Debug_Crash_Diagnostics` | 建立 Agent 可调用的 GDB 在线调试、运行态快照和 Fault 诊断能力 | GDB/J-Link 配置；Runtime Snapshot resume/halt 脚本；Cortex-M Fault 上下文；CmBacktrace/RTT；PowerShell/BAT 入口；失败清理；J-Link 释放；真实板测 | S05；Keil AXF；J-Link GDB Server；GNU Arm GDB；STM32F411CE SWD | `CLOSED` | Resume/Halt 快照、三类受控 Fault、GDB/CmBacktrace 交叉核对、`continue& → disconnect → quit` 恢复、halt 保持暂停、无隐式 Flash 编程、失败路径、PID 清理和 J-Link 释放通过 |
| `S05B_Toolkit_Reuse` | 将 PC 工具重构为便于扩展、升级和跨工程复用的嵌入式开发工具框架 | 三层 Config；Core 公共能力；Build/Probe/Debug Adapters；Application/Debug Workflows；`toolkit.bat` 统一入口；Legacy Scripts 兼容；Project Test 隔离；结果合同 | S05；S05A；现有 `05_Tools` | `CLOSED` | 当前稳定能力回归通过；通用实现无项目/机器硬编码；Legacy 入口不双轨；第二同类工程只改配置即可复用；Review 通过 |
| `S06_RTOS_Runtime` | 正式化 Application 后台 OTA 所需的 RTOS Runtime 与并发模型 | 基于现有 FreeRTOS 重新冻结 Task Topology / Lifecycle；UART Consumer Ownership；OTA/Ymodem Task Ownership；Task Notification / Queue / Event / Mutex；Flash/Storage 并发保护；Blocking API Policy；日志与业务并发边界 | S01；S05；S05A；S05B；现有 FreeRTOS/Platform RTOS abstraction | `PLANNED` | 正常业务与 Firmware 接收可并发；UART/Flash/日志资源所有权明确；阻塞点有界且可解释；ISR/DMA/Task 边界明确；无明显 Busy Loop、死锁、重复 Consumer 或未受控资源竞争 |
| `S07_OTA_Service_V1` | 完成 Application 侧 OTA 下载链 | OTA Service；Inactive Slot；启动/控制 Ymodem；Firmware Validation；更新 EEPROM Metadata；设置 `PENDING`；请求 Reset | S04；S05；S06 | `PLANNED` | `PC → UART/Ymodem → External Flash → Validation → PENDING → Reset` 完整闭环；失败下载不破坏当前 APP/Confirmed Image |
| `S08_Bootloader_Foundation` | 建立独立精简 Bootloader，并可靠启动 Application | 独立工程；Internal Flash Layout；Vector Table；MSP / Reset_Handler / VTOR；中断/外设清理；APP Jump；Boot Reason 日志 | S01；S04；Internal Flash Layout | `PLANNED` | 无升级请求时稳定跳转到 APP；非法 APP 被拒绝；跳转后中断正常 |
| `S09_Firmware_Installation` | Bootloader 从 External Flash 安装 Pending Firmware | 读取 Metadata；识别 PENDING；再次校验；擦写 Internal Flash；写后 CRC；启动新 APP | S07；S08 | `PLANNED` | 完成 V1.0 → V1.1 OTA 安装；写入/校验失败不误标成功 |
| `S10_Trial_Confirm_Rollback` | 建立 Trial / Confirm / Watchdog / Rollback 可靠性闭环 | `TRIAL / CONFIRMED / ROLLBACK`；`firmware_confirm()`；IWDG；Reset Cause；Failure Counter；Previous Confirmed Image | S09 | `PLANNED` | 正常 Trial 可 Confirm；故障/未 Confirm 可检测；达到阈值可自动回滚 |
| `S11_Diagnostics_UI` | 增加 OTA 状态可视化诊断与演示能力 | LCD/Display；Version、Slot、Progress、CRC、Boot State、Trial/Confirmed/Rollback/Error | S10；LCD/CTP 资料 | `PLANNED` | 不依赖 RTT 即可观察主要 OTA 状态；UI 故障不影响 OTA 核心逻辑 |
| `S12_Security_Extension` | 在可靠 OTA 基础上学习和验证安全升级机制 | SHA-256、AES、HMAC / Digital Signature、CK02AT API、STM32 RDP | S10；安全资料 | `PLANNED` | 每项安全机制有独立设计、边界和验证证据；不破坏可靠 OTA 主链 |

## 4. Stage Boundaries

### S01 - S04: Application Infrastructure

建立 Application 工程基础、原始存储能力和 Firmware 数据模型。

```text
Application
├─ 正常运行和输出日志
├─ 使用 W25Q64 保存大块数据
├─ 使用 AT24C02 保存掉电状态
└─ 识别 Firmware Image / Slot / Metadata
```

### S05 - S05A - S05B - S07: OTA Download Path and Development Tooling Gate

建立 Firmware 从 PC 进入设备到 Application 设置升级请求的完整下载链，同时在正式 RTOS/OTA Service 组合前补齐可自动化调试和可复用 PC 工具基础。

```text
PC
 ↓
UART / Ymodem
 ↓
Application Runtime / OTA Service
 ↓
Inactive Slot
 ↓
Firmware Validation
 ↓
PENDING
 ↓
Reset
```

S05 已解决可靠文件运输；S05A 补齐 Agent 可调用的 GDB 调试与运行态证据；S05B 将已经积累的 PC 工具收敛成可扩展、可升级、可复用的工具框架；S06 解决正式并发运行模型；S07 才组合业务状态和升级请求。

### S08 - S10: Bootloader & Reliability

```text
PENDING
 ↓
Bootloader Validate
 ↓
Install
 ↓
TRIAL
 ├─ Confirm → CONFIRMED
 └─ Fail / IWDG Reset
          ↓
      Failure Count
          ↓
       ROLLBACK
```

完成 S10 后，V1 的核心可靠 OTA 目标基本实现。

### S11 - S12: Diagnostics & Security Extension

不再改变 V1 OTA 的核心职责，在可靠闭环之上增加可视化诊断和安全实验。

## 5. S06 Planning Correction

早期 Roadmap 中“集成 FreeRTOS”的描述已经过时。

当前 Application 已经运行 FreeRTOS；S05 板测还验证过独立 `s05Ymodem` Thread、`service_uart` ownerThread 和 RingBuffer 单 Consumer 模型。

因此 S06 不再把“移植/启动 FreeRTOS”作为主要任务，而聚焦：

```text
Task Topology
Task Lifecycle
UART Consumer Ownership
OTA/Ymodem Task Ownership
IPC Selection
Shared Resource Protection
Flash/Storage Serialization
Blocking / Timeout Policy
Error Recovery
Business vs OTA Concurrency
```

S05 测试线程只作为实证输入，不自动成为正式 Application Task 架构。

## 6. Deferred / On-demand Inputs

按阶段补充：

- CK02AT Datasheet / API：S12；
- LCD / CTP 详细资料：最晚 S11；
- HC-05 参数：只有决定增加 Bluetooth OTA Transport 时进入正式 Stage；
- SHA / AES / HMAC / Digital Signature 具体方案：S12。

继续延期的架构工作：

- Device Manager；
- `platform_storage_device_t`；
- 通用 NVM Manager；
- W25Q64 EEPROM Emulation。

S04 跨阶段 Persistence 补充回归已完成：

```text
Reset Persistence       PASS
Power-cycle Persistence PASS
```

当前不再形成 S07 关闭前延期项。

## 7. Current Execution Checkpoint

最近关闭阶段：

```text
S05A_Debug_Crash_Diagnostics
Roadmap State: CLOSED
Workflow Status: CLOSED
Review: PASS
```

S05 已交付：

1. Ymodem Receiver-only / Single-file 组件；
2. Parser / Receiver / Sink 分层；
3. CRC-16/XMODEM、Block 0、SOH/STX、ACK/NAK、CAN/EOT；
4. Timeout / Retry / Duplicate / Sequence 处理；
5. Firmware Storage `write_payload()` / `write_header()`；
6. compact `.img` → Header Sector + Payload Offset 映射；
7. Header-last commit；
8. Tera Term 5 自动化 Sender；
9. Python Ymodem Sender 辅助工具；
10. Host Test / Keil Build / J-Link / RTT 自动化；
11. Tera Term 真实板测；
12. 中止后不提交 Header、失败后重新传输恢复；
13. Slot B 最终 Firmware Validation = VALID；
14. Verification / Review PASS。

S05 正式入口：

- `00_Project/03_Stages/S05_UART_Ymodem/design.md`
- `00_Project/03_Stages/S05_UART_Ymodem/implementation_plan.md`
- `00_Project/03_Stages/S05_UART_Ymodem/handoff.md`
- `00_Project/03_Stages/S05_UART_Ymodem/review.md`
- `04_Test/Reports/Stages/S05_UART_Ymodem/verification.md`

当前活动阶段：

```text
S05B_Toolkit_Reuse
Roadmap State: ACTIVE
Workflow Status: DESIGN_APPROVED
Branch: main
Baseline Commit: 9208cfd
Scope: extensible, upgradable and reusable embedded PC toolkit
Next action: create implementation_plan.md
```

S05A 已完成 GDB 手工兼容性与控制能力板测，包括 Breakpoint、Continue、Next、Step、Backtrace、Memory Read、Variable Read；GDB Runtime Snapshot 的 resume/halt 自动化、CmBacktrace 接入、三类受控 Fault 和 GDB/CmBacktrace 现场交叉核对也已通过真实板测。S04 Reset / Power-cycle Persistence 补充回归已完成。

S05A 交接入口：

- `00_Project/03_Stages/S05A_Debug_Crash_Diagnostics/design.md`
- `00_Project/03_Stages/S05A_Debug_Crash_Diagnostics/implementation_plan.md`
- `00_Project/03_Stages/S05A_Debug_Crash_Diagnostics/handoff.md`
- `00_Project/03_Stages/S05A_Debug_Crash_Diagnostics/review.md`
- `04_Test/Reports/Stages/S05A_Debug_Crash_Diagnostics/verification.md`

S05B 交接入口：

- `00_Project/03_Stages/S05B_Toolkit_Reuse/design.md`
- `00_Project/03_Stages/S05B_Toolkit_Reuse/implementation_plan.md`
- `00_Project/03_Stages/S05B_Toolkit_Reuse/handoff.md`
- `00_Project/03_Stages/S05B_Toolkit_Reuse/review.md`

后续阶段：

```text
S06_RTOS_Runtime
Roadmap State: PLANNED
Next action: Design Discussion after S05B closes
```

S05B 尚未进入实施；正式实施计划完成并审阅后才进入 `READY_FOR_IMPLEMENTATION`。S06 在 S05B 关闭后继续。
