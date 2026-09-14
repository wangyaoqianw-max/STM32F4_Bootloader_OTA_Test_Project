# Development Roadmap

路线图描述项目长期阶段顺序、能力演进、前置依赖和阶段完成标准，不替代单个 Stage 的 `design.md`、`implementation_plan.md`、`handoff.md` 和 `review.md`。

正式 Stage 的工作流状态以 `00_Project/WORKFLOW.md` 和 `00_Project/05_Status/current_status.md` 为准。

Roadmap State 语义：

- `PLANNED`：已纳入长期路线，但尚未创建或启动正式 Stage；
- `ACTIVE`：正式 Stage 已创建且尚未关闭，具体工作流状态以 `current_status.md` 为准；
- `CLOSED`：Stage 已完成 Verification / Review 并正式关闭。

## 1. Roadmap Principles

本项目按“先建立基础能力，再形成 OTA 下载链，再形成 Bootloader 安装与可靠性闭环，最后增加诊断和安全扩展”的顺序推进。

阶段拆分遵循以下原则：

1. 每个 Stage 必须增加一项可以独立验证的系统能力；
2. 每个 Stage 应有明确前置依赖、工程交付物和硬件/软件验收证据；
3. Application 与 Bootloader 分开设计，Bootloader 不机械复制 Application 的完整分层和 RTOS 架构；
4. UART/Ymodem、Flash/EEPROM Raw Driver 等基础模块只解决各自职责，不提前承载 OTA 业务语义；
5. Firmware Image、Slot、Metadata 等跨模块契约在传输和 Bootloader 安装前明确；
6. LCD、CK02AT 和其他非核心扩展不阻塞 V1 OTA 主链；
7. 真实硬件资料不足时，在对应 Stage 再补充，不把当前非阻塞开放项提前冻结为强制前置条件；
8. Device Manager、Storage Device 等大型统一架构不为单一阶段强行引入，等主项目完成后基于真实使用场景专项重构。

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
S05  能从 PC 接收 Firmware 文件
 ↓
S06  Application 具备后台并发运行环境
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
| `S00_Template_Restructure` | 建立通用工程模板、跨工具上下文合同和工程准备机制 | 项目目录、阶段工作流、上下文合同、工程准备模板、构建规范 | 目录与工作流设计获批 | `CLOSED` | 结构与构建规范验证通过、Project Owner 审核通过，并已被当前项目实际采用 |
| `S01_Application_Foundation` | 建立可继续扩展的 Application 基础工程 | 建立 App / Service / Platform / Impl / Vendor / Config 基础结构；选择性复用已有项目架构；接入 RTT + EasyLogger；建立基础初始化与错误处理；实现 Firmware V1.0 的 LED Blink 验证功能 | S00；STM32F411 基础工程与已确认板级资料 | `CLOSED` | Clean Rebuild 通过；板端 LED Blink 正常；RTT/EasyLogger 输出正常；架构依赖方向与基础初始化流程检查通过；连续 Reset 4 次稳定复现 |
| `S02_External_Flash_Driver` | 建立 W25Q64 原始非易失存储能力 | SPI2 Platform/Impl；W25Q64 Raw Driver；Read / Program / Erase / JEDEC ID / BUSY/WEL / 跨页和边界检查；RTT 板测；Raw Driver 稳定后评估 SFUD 接入 | S01；W25Q64 与 SPI2 硬件资料 | `CLOSED` | JEDEC ID 正确；Sector Erase、Page Program、Read Back、跨页和边界测试通过；Reset 后数据保持；错误返回可诊断；SPI 大长度语义返工通过 Host/Build/Hardware Regression；形成 SFUD 适配基线 |
| `S03_EEPROM_Storage` | 建立掉电后可保存的小容量状态存储能力 | Software I2C `probe()`；AT24C02 Raw Driver；Random/Sequential Read；8 Byte Page 自动拆分写；ACK Polling；地址边界；RTT 板测 | S01；AT24C02 与 PB6/PB7 硬件资料 | `CLOSED` | 单字节/页内/跨页读写正确；`0xFF` 与越界保护正确；Reset/实际掉电后数据保持；错误返回与 RTT 日志可诊断 |
| `S04_Firmware_Image_Storage` | 建立 Firmware Image、A/B Slot 和 Metadata 的基础存储模型 | External Flash Slot A/B 分区；Firmware Image Header；Version / Size / CRC；Slot 状态；Metadata 双副本 Contract；镜像整体 CRC 校验；PC pack tool；UART test-only Slot B 注入；明确 Application/Bootloader 共用数据契约 | S02；S03 | `CLOSED` | Firmware Image / Slot / Header / CRC / Metadata 主链路、Host Test、Keil Build、真实板测和 Review 均通过；Reset / Power-cycle Persistence 保留为跨阶段延期回归项，必须在 S07 关闭前补测 |
| `S05_UART_Ymodem` | 建立 Firmware 文件传输通道 | OTA 专用 UART；接收缓冲；Ymodem 协议状态机；文件名/大小处理；Packet CRC；Timeout / Cancel / Retry；数据流式写入指定 External Flash 区域 | S02；S04；UART 硬件资料；补充 Ymodem 参考资料 | `PLANNED` | PC 通过 Ymodem 发送 Firmware 文件；MCU 完整接收并写入 Flash；接收 Size 与 CRC 和源文件一致；中断/取消场景可恢复 |
| `S06_RTOS_Runtime` | 建立 Application 后台 OTA 所需的并发运行环境 | 集成 FreeRTOS；确定 Task 划分；建立 Task Notification / Queue / Mutex 等必要 IPC；明确 UART 接收、Flash 写入、普通业务和日志之间的并发边界 | S01；S05 的通信模型已明确 | `PLANNED` | 正常业务与 Firmware 接收可以并发；无工作任务能够阻塞；ISR/DMA/Task 边界明确；无明显 Busy Loop、死锁或资源竞争 |
| `S07_OTA_Service_V1` | 完成 Application 侧 OTA 下载链 | 实现 OTA Service；选择 Inactive Slot；启动/控制 Ymodem；接收 Firmware Metadata；写入 Firmware；整包 CRC；更新 EEPROM Metadata；设置 `PENDING`；请求 Reset | S04；S05；S06；补齐 S04 deferred persistence regression | `PLANNED` | 完整执行 `PC → UART/Ymodem → External Flash → Firmware Validation → Metadata PENDING → Reset`；失败下载不破坏当前 Application 和 Confirmed Image；S04 Reset/Power-cycle Persistence 补测通过 |
| `S08_Bootloader_Foundation` | 建立独立精简 Bootloader，并可靠启动 Application | 独立 Bootloader 工程；Internal Flash Layout；Vector Table 合法性检查；MSP / Reset_Handler / VTOR；中断与外设清理；APP Jump；Boot Reason 日志 | S01；S04 的共用契约；Internal Flash Layout 设计 | `PLANNED` | Reset 后进入 Bootloader；无升级请求时能验证并稳定跳转到 Firmware V1.0；非法 APP 能被拒绝；跳转后 APP 中断工作正常 |
| `S09_Firmware_Installation` | 实现 Bootloader 从 External Flash 安装 Pending Firmware | Bootloader 读取 Metadata；识别 `PENDING`；再次校验 External Flash Firmware；擦除 Internal Flash APP 区；分块 Read/Program；Internal Flash 写后 CRC；启动新 Application | S07；S08 | `PLANNED` | 实际完成 Firmware V1.0 → V1.1 OTA 安装；V1.1 PWM Breathing LED 正常运行；写入或校验失败时不误标记成功 |
| `S10_Trial_Confirm_Rollback` | 建立可靠 OTA 的 Trial Boot、运行确认、Watchdog 与回滚闭环 | 引入 `TRIAL / CONFIRMED / ROLLBACK` 状态；Application `firmware_confirm()`；IWDG；Reset Cause；Boot Failure Counter；上一版 Confirmed Image 保留；失败自动回滚；Metadata 一致性恢复 | S09 | `PLANNED` | 正常 V1.1 Trial 可 Confirm；故意制造 HardFault/死循环/未 Confirm 场景后可被 IWDG 检测；达到失败阈值后自动恢复上一 Confirmed Firmware |
| `S11_Diagnostics_UI` | 增加 OTA 状态可视化诊断与演示能力 | LCD/Display Driver；显示 Firmware Version、Active/Pending Slot、OTA Progress、CRC Result、Boot State、Trial/Confirmed/Rollback/Error 等状态 | S10；LCD/CTP 资料补充 | `PLANNED` | 不依赖 RTT 即可观察主要 OTA 状态；UI 故障不影响 OTA 核心状态机和 Boot 决策 |
| `S12_Security_Extension` | 在可靠 OTA 基础上学习和验证安全升级机制 | 分阶段实验 SHA-256、AES、HMAC 或 Digital Signature、CK02AT API、STM32 RDP；区分完整性、机密性、来源认证和运行固件保护 | S10；对应安全资料/CK02AT API 在需要时补充 | `PLANNED` | 每项安全机制都有独立设计、实现边界和验证证据；安全扩展不破坏既有可靠 OTA 主链 |

## 4. Stage Boundaries

### S01 - S04: Application Infrastructure

这一阶段解决 Application 工程基础、原始存储能力和 Firmware 数据模型。

结束时系统已经能够：

```text
Application
├─ 正常运行和输出日志
├─ 使用 W25Q64 保存大块数据
├─ 使用 AT24C02 保存掉电状态
└─ 识别 Firmware Image / Slot / Metadata
```

S04 使用现有 UART 进行了 test-only raw image injection，仅用于验证 Firmware Storage Contract，不替代 S05 Ymodem。

### S05 - S07: OTA Download Path

这一阶段解决 Firmware 如何从 PC 进入设备，以及 Application 如何在正常运行期间完成后台下载。

结束时形成：

```text
PC
 ↓
UART / Ymodem
 ↓
Application OTA Service
 ↓
Inactive Slot
 ↓
Firmware Validation
 ↓
PENDING
 ↓
Reset
```

此时 Application 不直接覆盖当前 Internal Flash APP。

### S08 - S10: Bootloader & Reliability

这一阶段解决启动、安装、运行确认和异常恢复。

结束时形成完整闭环：

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

完成 S10 后，V1 的核心可靠 OTA 目标视为基本实现。

### S11 - S12: Diagnostics & Security Extension

这一阶段不再改变 V1 OTA 的核心职责，而是在稳定闭环之上增加可视化诊断、演示能力和安全机制实验。

## 5. Deferred / On-demand Inputs

以下资料按阶段按需补充：

- CK02AT Datasheet / API：延后至 S12；
- LCD / CTP 详细资料：最晚 S11 前补充；
- Ymodem 原始协议或高可信参考资料：最晚 S05 前补充；
- HC-05 参数：只有后续决定增加 Bluetooth OTA Transport 时再进入正式 Stage；
- SHA / AES / HMAC / Digital Signature 的具体算法和库选择：S12 再冻结。

继续延期的架构工作：

- Device Manager；
- `platform_storage_device_t`；
- 通用 NVM Manager；
- W25Q64 EEPROM Emulation。

S04 跨阶段延期回归：

```text
Reset Persistence       PENDING / DEFERRED
Power-cycle Persistence PENDING / DEFERRED
```

这两项不阻塞 S05 / S06，但必须在 `S07_OTA_Service_V1` 关闭前完成真实硬件验证。

## 6. Current Execution Checkpoint

最近关闭阶段：

```text
S04_Firmware_Image_Storage
Roadmap State: CLOSED
Workflow Status: CLOSED
Branch: main
Review: PASS
```

S04 已交付：

1. A/B Slot + Header Sector；
2. Firmware Header V1 fixed binary contract；
3. Version / Size / Header CRC / Payload CRC；
4. CRC-8/SMBUS、CRC-16/XMODEM、CRC-32/ISO-HDLC；
5. AT24C02 Metadata 双副本、sequence、CRC、commit marker；
6. Image Header / Metadata authority boundary；
7. `service_firmware` / `service_common/crc`；
8. PC `pack_firmware.py`；
9. UART test-only Slot B injection；
10. Host Test / Keil Build / RTT Board Test；
11. Build / Flash / RTT automation；
12. Final Review PASS。

延期回归：

```text
Reset Persistence       PENDING
Power-cycle Persistence PENDING
Required before S07 closure
```

下一阶段：

```text
S05_UART_Ymodem
Roadmap State: PLANNED
Next action: create Design Stage in a new discussion
```

S04 正式入口：

- `00_Project/03_Stages/S04_Firmware_Image_Storage/design.md`
- `00_Project/03_Stages/S04_Firmware_Image_Storage/implementation_plan.md`
- `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`
- `00_Project/03_Stages/S04_Firmware_Image_Storage/review.md`
- `04_Test/Reports/Stages/S04_Firmware_Image_Storage/verification.md`
- `00_Project/05_Status/current_status.md`
