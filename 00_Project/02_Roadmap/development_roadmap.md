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
6. LCD 基础 Runtime/OTA 状态显示前移到 S06，用于并发验收和工程展示，但 UI 故障不得阻塞 V1 OTA 主链；完整诊断 UI 仍在 S11 扩展；
7. 真实资料不足时按阶段补充，不把非阻塞开放项提前冻结；
8. Device Manager、Storage Device 等大型统一架构不为单一阶段强行引入。

## 2. Phase Overview

```text
Phase A - Application Infrastructure
S01 → S04

Phase B - OTA Download Path & Runtime Hardening
S05 → S07A

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
S05C 增加 Agent 可调用的 SPI / I2C 外部总线证据能力
 ↓
S06  Application 形成正式 RTOS Runtime / Concurrency Model，并接入基础 LCD 状态显示
 ↓
S07  Application OTA 下载链完成
 ↓
S07A 整理 RTOS Startup / Bootstrap / Task Runtime 边界，并重新验证 Stack/Heap
 ↓
S08  Bootloader 能验证并启动 Application
 ↓
S09  Bootloader 能安装 Pending Firmware
 ↓
S10  OTA 具备 Trial / Confirm / Rollback 可靠性闭环
 ↓
S11  扩展完整可视化诊断与展示
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
| `S05C_Logic_Analyzer` | 在 Toolkit 中增加可复现的 SPI / I2C 外部总线证据能力 | sigrok Executor / Parser；Capture / Decode Workflow；Effective Config；Logic Analyzer profiles；W25Q64 / AT24C02 项目只读断言；Agent 临时映射覆盖 | S05B；sigrok-cli；USB Logic Analyzer；现有 SPI2 / Software I2C 接线 | `CLOSED` | sigrok discovery、SPI/W25Q64、I2C/AT24C02、Structured Result、Golden Fixtures、Host/Toolkit 回归、Verification 和 Review 通过；UART/GPIO 延期 |
| `S06_RTOS_Runtime` | 正式化 Application 后台 OTA 所需的 RTOS Runtime 与并发模型，并接入基础 LCD 状态显示 | ST7789/SPI1 Board Adaptation；`appSystem + otaWorker + displayTask`；UART Notification；OTA→Display Queue；Display Model；Blocking/Timeout/Ownership；并发与恢复板测 | S01；S05；S05A；S05B；S05C；现有 FreeRTOS/Platform RTOS abstraction | `CLOSED` | LCD 可用；三线程按设计阻塞/唤醒；v1.0 前台 LED 与后台 Firmware 接收并发；LCD 显示 OTA 状态/进度；UART/Flash/日志资源边界明确；成功/失败路径与 Toolkit 回归通过 |
| `S07_OTA_Service_V1` | 完成 Application 侧 OTA 下载链 | OTA Service；Inactive Slot；启动/控制 Ymodem；Firmware Validation；更新 EEPROM Metadata；设置 `PENDING`；请求 Reset | S04；S05；S06 | `CLOSED` | `PC → UART/Ymodem → External Flash → Validation → PENDING → Reset` 完整闭环；失败下载不破坏当前 APP/Confirmed Image |
| `S07A_RTOS_Startup_Refactor` | 整理 Application RTOS 启动生命周期并验证 RAM 安全 | `defaultTask → app_system_bootstrap() → appMainTask/otaWorker/displayTask`；Event Flags Startup Barrier；RUNNING/DEGRADED/FAILED；App 目录整理；Stack/Heap High Water | S06；S07 | `CLOSED` | 不再创建 appSystem Task；defaultTask Bootstrap 后退出；长期 Task 在显式 `SYSTEM_RUN` 后运行；无启动竞态；Stack/Heap 有真实证据；S07 全链路无回归 |
| `S08_Bootloader_Foundation` | 建立独立精简 Bootloader，并可靠启动 Application | 64 KiB/448 KiB Internal Flash Layout；Bare-metal HAL/CMSIS；RTT + lightweight boot_log + CmBacktrace；MSP / Reset_Handler / VTOR；SysTick/NVIC cleanup；APP Jump | S01；S04；S07A | `CLOSED` | 双工程地址无重叠；诊断可用；合法 APP 稳定启动；非法 APP 被拒绝；跳转后 FreeRTOS/中断正常 |
| `S09_Firmware_Installation` | Bootloader 从 External Flash 安装 Pending Firmware | 轻量 Header/Metadata/CRC consumer；Bus→Device 初始化；W25Q64 read-only；AT24C02 Metadata commit；APP-only Internal Flash；Candidate pre-validation；PENDING→TRIAL 原子提交；Fault Injection | S07；S08 | `CLOSED` | v1.0→v1.1 安装通过；Invalid Candidate 不擦 APP；TRIAL commit 前 reset 可重装、commit 后不重复安装；写入/校验失败不误标成功 |
| `S10_Trial_Confirm_Rollback` | 建立 Trial / Confirm / Watchdog / Rollback 可靠性闭环 | `TRIAL / CONFIRMED / ROLLBACK`；`firmware_confirm()`；IWDG；Reset Cause；Failure Counter；Previous Confirmed Image | S09 | `ACTIVE` | 正常 Trial 可 Confirm；故障/未 Confirm 可检测；达到阈值可自动回滚 |
| `S11_Diagnostics_UI` | 在完整 OTA/Bootloader 可靠性闭环上扩展高级诊断与演示 UI | 复用 S06 Display Runtime；增加 Version、Slot、CRC、Boot State、Trial/Confirmed/Rollback、Reset Cause、Error History；可选 CTP/LVGL | S10；S06 Display Runtime；必要的 LCD/CTP 资料 | `PLANNED` | 不依赖 RTT 即可观察完整 OTA/Bootloader 关键状态；UI 故障不影响 OTA 核心逻辑；不重复实现底层 LCD Driver |
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

### S05 - S05C - S07A: OTA Download Path and Runtime Hardening

建立 Firmware 从 PC 进入设备到 Application 设置升级请求的完整下载链，同时在正式 RTOS/OTA Service 组合前补齐自动化调试、可复用 PC 工具和外部总线证据能力。

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

S05 已解决可靠文件运输；S05A 补齐 Agent 可调用的 GDB 调试与运行态证据；S05B 将已经积累的 PC 工具收敛成可扩展、可升级、可复用的工具框架；S05C 增加 SPI / I2C 外部总线证据并冻结工具共享资源运行规则；S06 建立正式并发运行模型并接入基础 LCD Runtime；S07 完成 OTA Service 与 PENDING 下载链；S07A 在进入 Bootloader 前专门整理 Bootstrap、Task-local Init、Startup Barrier 与 Stack/Heap 安全，避免把已验证但职责混合的 Runtime 直接带入后续阶段。

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

S11 不重新建立 LCD 基础驱动，而是在 S06 Display Runtime 上补全 Bootloader/可靠性闭环的诊断信息；S12 再增加安全实验。二者都不改变 V1 OTA 核心职责。

## 5. S06 Closure / Runtime Contract

早期 Roadmap 中“集成 FreeRTOS”的描述已经过时。S06 已在现有 FreeRTOS Application 上正式建立并验证 Runtime / Concurrency Contract，阶段状态为 `CLOSED / PASS`。

冻结 Runtime：

```text
appSystem      NORMAL       → foreground Application / LED Demo
otaWorker      ABOVE_NORMAL → UART/Ymodem/Firmware Storage/Validation
displayTask    BELOW_NORMAL → Display Queue/Model/ST7789
```

IPC：

```text
UART ISR/RX → otaWorker   : Task Notification
otaWorker   → displayTask : Queue
```

S06 已完成 ST7789/LCD Board Adaptation、三线程 Runtime、OTA→Display Queue、阻塞/资源所有权审查、Stack/Heap 诊断以及成功/失败 OTA 并发板测。S05 测试线程仅作为历史实证输入，正式 Runtime 以 S06 Handoff 为准。

S07A 已在不改变上述业务 ownership 和 IPC 语义的前提下完成启动生命周期重构：不再创建独立 `appSystem` Task；CubeMX `defaultTask` 作为唯一临时 Bootstrap execution context，直接执行 `app_system_bootstrap()`，长期前台业务迁移到独立 `appMainTask`。当前 Application Startup Contract 以 S07A Handoff / Review 为准。

## 6. Deferred / On-demand Inputs

按阶段补充：

- CK02AT Datasheet / API：S12；
- CTP 详细资料及 LVGL 是否引入：S11 按需；
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
S09_Firmware_Installation
Roadmap State: CLOSED
Workflow Status: CLOSED / PASS
Review: PASS
```

S09 已交付 External Candidate pre-validation、轻量 Bootloader 存储访问、APP-only Internal Flash、安装事务、Metadata `PENDING → TRIAL` 原子边界、Factory Restore 和正常安装链验证。

当前 Active Stage 尚未完成 Verification / Review。当前阶段：

```text
S10_Trial_Confirm_Rollback
Roadmap State: ACTIVE
Workflow Status: READY_FOR_VERIFICATION
Code Verification: PASS
Hardware Verification: PENDING
```

S09 剩余 erase/program/CRC/Metadata marker/Power Loss 真实板级 Fault Injection 由 Project Owner 接受为跨阶段 Deferred Follow-up，不视为已通过；S10 的生产职责仍为 Trial Confirm、Watchdog、Failure Counter 和 Rollback。
