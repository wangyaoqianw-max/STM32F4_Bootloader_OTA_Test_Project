# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S09_Firmware_Installation`
- Active Stage Status: `CLOSED / PASS`
- Branch: `main`
- S07A Baseline Commit: `254f510498748294f44b0c059796dbaeaccdea3e`
- S07A Design Commit: `e4ae9dea8f6ab0088829fb4acda29ab89c734132`
- S07A Implementation Plan Commit: `fa8409c335727720e387887d254caada5939f346`
- S07A Handoff Commit: `b4e4102`
- S07A Implementation Commits: `306c76b`, `c30c60d`, `d631fb8`
- S07A Verification Commit: `8703415`
- S07A Verification Report: `04_Test/Reports/Stages/S07A_RTOS_Startup_Refactor/verification.md`
- S07A Review Commit: `568bbccc`
- S07A Review Report: `00_Project/03_Stages/S07A_RTOS_Startup_Refactor/review.md`
- S08 Verification Report: `04_Test/Reports/Stages/S08_Bootloader_Foundation/verification.md`
- S08 Review Report: `00_Project/03_Stages/S08_Bootloader_Foundation/review.md`
- S08 Review Commit: `7dc5f7c`
- S09 Baseline Commit: `e4eb1ac5704971ed72ec4bdb8986e070752c5390`
- S09 Design / Plan Commit: `4882077d6c8798900c0e12f1fc902c28682263c3`
- S09 Implementation Commits: `6e3fef5`, `c8e0c07`, `1f5b4d9`, `fbe8e37`, `e4d6816`, `69c426a`, `28feb5a`, `53acd8e`
- S09 Verification Evidence: `04_Test/Reports/Stages/S09_Firmware_Installation/verification.md`
- S09 Verification Commit: `4fdc9ac`
- S09 Review Report: `00_Project/03_Stages/S09_Firmware_Installation/review.md`
- S09 Review Commit: `8099cb5`
- S09 Closure Commit: `8192573`
- S06 Design Commit: `eb57291f9d506965bfc20acc4261ce7e01888094`
- S06 Implementation Plan Commit: `9f304c731c06eafb842b50c3098702d4a842db2e`
- S06 Implementation Commits: `f6f50fd`, `b90d462`, `6d0d323`, `38f7c60`, `20ec343`, `66e2934`, `014b617`
- S06 Verification Report: `04_Test/Reports/Stages/S06_RTOS_Runtime/verification.md`
- S06 Handoff: `00_Project/03_Stages/S06_RTOS_Runtime/handoff.md`
- S06 Verification Commit: `8c67ad2`
- S06 Review Commit: `b2c8ba0`
- S05B Initial Design Commit: `9d6b037`
- S05B Design Approval Commit: `5622b63a7cb3d532aab55de73eaf88d823b9acb9`
- S05B Implementation Plan Commit: `3a055a84397ab5eab6dcddf2dea0590c97daff4c`
- S05B Rework Implementation Commit: `2140117`
- S05B Verification Commit: `33a1dfe`
- S05B Review Commit: `b19c80d`
- S05A Implementation / Verification Commit: `bd8883d`
- S05A CmBacktrace Integration Commit: `1c27c8e`
- S05A Review Commit: `32f3368`
- S04 Persistence Supplementary Regression Commit: `6f2fad5`
- S05 Merge Commit: `5b2b42136e0d8f1eb2d54463fb5319996d6f6b5f`
- S05C Implementation Commits: `bfdffef`, `ce73ebf`, `505f058`, `0efb687`, `7969958`, `57e9cf5`, `5971bf6`
- S05C Verification Commit: `8781326`
- S05C Review Commit: `f3f5ce0b34b9d92bdd426b69a0af645bdfe115bb`
- S05C Verification Report: `04_Test/Reports/Stages/S05C_Logic_Analyzer/verification.md`
- S05C Review Report: `00_Project/03_Stages/S05C_Logic_Analyzer/review.md`
- Last Closed Stage: `S09_Firmware_Installation`
- Last Closed Stage Status: `CLOSED / PASS`
- Next Planned Stage: `S10_Trial_Confirm_Rollback`
- Current Role: `Project Owner`
- Updated At: `2026-09-19`

## Current Goal

S05、S05A、S05B、S05C、S06、S07、S07A、S08 与 S09 均已关闭。S09 已完成 durable `PENDING` 消费、External Candidate 安装、Internal CRC/vector 验证和 `PENDING → TRIAL` 原子提交；代码验证 PASS，正常安装链和关键 reset 边界已通过真实板验证。剩余 erase/program/CRC/Metadata marker/Power Loss Fault Injection 由 Project Owner 接受为跨阶段 Deferred Follow-up，不视为已通过。下一计划阶段为 `S10_Trial_Confirm_Rollback`。

S06 已建立三线程 Application Runtime / Concurrency Model，并完成 ST7789/LCD 板级适配，为 S07 OTA Service V1 提供稳定的任务、资源所有权和并发基础。

S06 已验证 Runtime（历史合同；S07A 已替代其启动职责）：

```text
appSystem      → 前台 Application / v1.0 Blink / v1.1 Breath
otaWorker      → 后台 Ymodem / Firmware Storage / Validation
displayTask    → ST7789 / Graphics / Display Model
```

冻结 IPC：

```text
UART ISR/RX → otaWorker     : Task Notification
otaWorker   → displayTask   : Queue
```

S06 已验证 LCD 状态显示、前台 LED 与后台 Ymodem 并发、成功/失败 OTA 路径、Slot B Validation、任务阻塞状态和 Toolkit 回归。S07 Task 0 已恢复 CubeMX 生成造成的 heap、CmBacktrace 和 FreeRTOS task introspection 回归，代码提交为 `9adba52`；随后在该 Runtime Contract 上实施正式 OTA Service/session control。

## S07A RTOS Startup Refactor

当前状态：`CLOSED / PASS`。

冻结启动模型：

```text
defaultTask (temporary bootstrap, 4096 B)
↓
app_system_bootstrap()
↓
Startup Context / Event Flags / shared IPC
↓
appMainTask + otaWorker + displayTask
↓
Task-local Init
↓
MAIN_DONE + OTA_DONE + DISPLAY_DONE
↓
RUNNING / DEGRADED / FAILED
├─ RUNNING/DEGRADED → SYSTEM_RUN → defaultTask delete
└─ FAILED           → SYSTEM_ABORT
```

S07A 对 S06 Runtime 的关键修正：

```text
old:
appSystem = persistent foreground RTOS task

new:
defaultTask = only temporary bootstrap task
appSystem   = non-task composition/bootstrap module
appMainTask = persistent foreground task
```

冻结 Platform OS 扩展：

```text
platform_event_flags
→ create / set / wait / delete

platform_thread_get_stack_space()
→ remaining stack bytes
```

冻结 startup policy：

```text
timeout = 5000 ms
component init error → DEGRADED
runtime infrastructure failure / timeout → FAILED
```

第一版 Stack：

```text
defaultTask   4096 B temporary
appMainTask   2048 B persistent
otaWorker     4096 B persistent
displayTask   4096 B persistent
```

已验证 startup peak heap、minimum-ever-free heap、defaultTask delete 后的 Idle cleanup 回收，以及各长期 Task stack space；当前无继续压缩 otaWorker/displayTask stack 的必要。

App 目录冻结：

```text
01_APP/
├─ system/
├─ task/
├─ runtime/
└─ contract/
```

`app_main.*` 在实施时改名为 `app_main_task.*`。S07 OTA Service、Metadata V2、Ymodem、KEY 双确认和 PENDING 合同保持不变。

正式入口：

```text
00_Project/03_Stages/S07A_RTOS_Startup_Refactor/design.md
00_Project/03_Stages/S07A_RTOS_Startup_Refactor/implementation_plan.md
00_Project/03_Stages/S07A_RTOS_Startup_Refactor/handoff.md
```

## S07 OTA Service V1 Current Status

S07 已完成 Task 0–10 的代码实现、Host/Toolkit 回归和生产依赖隔离；Task 10 已在真实板上完成 PA0 启动/确认、COM9 YMODEM、READY 复位、中断、坏 CRC、重复 KEY、LCD 正常/失败画面和 EEPROM durable `PENDING` 回读。100% 终止进度事件洪泛问题已在 `2ab34f1` 修复。Task 10 验证报告为 `04_Test/Reports/Stages/S07_OTA_Service_V1/verification.md`，最终 Review 为 `00_Project/03_Stages/S07_OTA_Service_V1/review.md`。

当前结论：代码验证 `PASS`，硬件验证 `PASS`，S07 阶段状态为 `CLOSED / PASS`。S07 未执行 S09/S10 的安装、Trial、Confirm 或 Rollback；S08 已完成 Bootloader Foundation。当前由 S09 接手 durable `PENDING` 的 Internal Flash installation。

## Stable Toolkit Architecture

S05B 已将 PC 工具重构为：

```text
Human / Agent
      ↓
Unified Entry / Legacy Entry
      ↓
Workflows
      ↓
Core + Adapters
      ↓
External Tools
```

稳定目录职责：

```text
Config
+ Core
+ Adapters
+ Workflows
+ Contracts
+ Project Tests
+ Legacy Wrappers
```

Adapter 当前变化轴：

```text
Adapters/Build/Keil
Adapters/Probe/JLink
Adapters/Debug/GDB
Adapters/LogicAnalyzer/Sigrok
```

配置模型：

```text
toolchain.local.bat   machine tool paths, ignored
project.defaults.bat  repository project facts, committed
project.local.bat     machine-specific project overrides, ignored
logic_analyzer.profiles.json project logic-analyzer wiring profiles, committed
```

统一入口当前覆盖：

```text
toolkit.bat build
toolkit.bat flash run|prepare
toolkit.bat run
toolkit.bat rtt
toolkit.bat snapshot halt|resume
toolkit.bat fault capture|trigger
toolkit.bat firmware pack ...
toolkit.bat ymodem ...
toolkit.bat logic doctor|scan|spi|i2c|decode ...
```

稳定 Exit Classes：

```text
0   SUCCESS
1   BUILD_WARNING (only Build/Run warnings)
10  CONFIG_ERROR
20  BUILD_ERROR
30  PROBE_ERROR
40  DEBUG_ERROR
50  TRANSFER_ERROR
60  TEST_ERROR
```

## S05C Logic Analyzer Checkpoint

S05C 已完成并以 `CLOSED / PASS` 关闭。第一版范围为 SPI / I2C，UART 与 GPIO Timing 延期。

架构：

```text
Human / Agent
      ↓
toolkit.bat logic ...
      ↓
Workflows/LogicAnalyzer
      ↓
Adapters/LogicAnalyzer/Sigrok
      ├─ Executor
      └─ Parser
      ↓
sigrok-cli
      ↓
USB Logic Analyzer
```

已验证：

```text
sigrok doctor / scan                 PASS
SPI capture / decode                 PASS
W25Q64 JEDEC 0x9F → EF 40 17         PASS
I2C capture / decode                 PASS
AT24C02 address 0x50 transaction     PASS
Host / Toolkit regression            PASS
Application build / flash / RTT      PASS
GDB snapshot resume                   PASS
Review                               PASS
```

板测采集使用较宽时间窗口：SPI `24MHz / 20s`，I2C `1MHz / 20s`。两次均先启动 sigrok capture，再由独立 J-Link 客户端复位运行；同一 J-Link Probe 仍只允许一个客户端。

工具共享资源规则：只有共享客户端、设备、端口、输出文件/目录或构建输出时互斥；独立资源允许并行。

正式入口：

```text
00_Project/03_Stages/S05C_Logic_Analyzer/design.md
00_Project/03_Stages/S05C_Logic_Analyzer/implementation_plan.md
00_Project/03_Stages/S05C_Logic_Analyzer/handoff.md
00_Project/03_Stages/S05C_Logic_Analyzer/review.md
04_Test/Reports/Stages/S05C_Logic_Analyzer/verification.md
```

非阻塞 Follow-up：当前 I2C Parser 将一次 capture 内 annotations 聚合为一个逻辑 transaction。未来支持多设备或长窗口分析时，建议按 START/STOP 边界拆分 transactions。

## Stable S04 Storage / Firmware Contract

External Flash：

```text
Slot A: 0x000000 ~ 0x07FFFF
Slot B: 0x080000 ~ 0x0FFFFF

Per Slot:
+0x0000 ~ +0x0FFF : Header Sector
+0x1000 ~          : Firmware Payload
Payload capacity  : 508 KiB
```

Firmware Image V1：

- Header fixed 64 Byte；
- Header / Payload CRC32；
- Image Header 是 Firmware Version / Size / CRC 权威来源；
- `firmware_storage_validate_image()` 为只读验证；
- `pack_firmware.py` 输出 compact `.img = [64 Byte Header][Payload]`；
- Slot 中映射为 Header Sector + Payload Offset，不能线性写入 compact `.img`。

AT24C02 Metadata 保持双副本 + sequence + CRC + commit marker。S05 没有建立 `PENDING`。

## Stable S05 Ymodem Capability

```text
PC Sender
   ↓
USART1 / DMA / RingBuffer
   ↓
service_uart
   ↓
ymodem_parser
   ↓
ymodem_receiver
   ↓
ymodem_sink
   ↓
Firmware Storage
```

稳定边界：

- `service_uart` 拥有 UART DMA / RingBuffer / TX / error / data-loss；
- `ymodem_parser` 负责 framing / block complement / CRC-16；
- `ymodem_receiver` 负责 Block 0、sequence、ACK/NAK、retry、timeout、cancel、EOT；
- `ymodem_sink` 只定义 begin/write/end/abort；
- Ymodem 不感知 Slot、EEPROM Metadata、PENDING、Reset；
- Header-last commit 保证失败传输不会提交新有效 Header。

真实 Tera Term 板测已验证 Slot B 最终 `VALID`；中止时 `header_commit=0`，重新建立 Session 后可恢复。

当前 Sender 定位：

- Tera Term：S05 真实板默认验收入口；
- Python Sender：Host Test、Agent 自动化、协议诊断，支持 `--json`。

## Stable S05A GDB Contract

真实板已验证：Breakpoint / Continue / Next / Step / Backtrace / Memory Read / Variable Read、Runtime Snapshot、CmBacktrace、三类受控 Fault 和 GDB/RTT 交叉核对。

冻结退出合同：

```text
halt state
→ detach
→ MCU remains halted
```

```text
running state
→ continue&
→ disconnect
→ quit
→ MCU continues running
```

约束：

- GDB 自动化不得执行 `load`；
- Resume 不使用 `-batch`；
- `continue&` 后不得再执行 `detach`；
- 触发型 Fault GDB 会话使用阻塞 `continue` 等待 `diagnostics_fault_capture_stop`；运行态 Snapshot 仍使用 `continue&`；
- GDB/RTT/J-Link Commander 同一时刻不能并发占用同一 Probe；
- 只清理当前工具自己创建的进程。

## Stable Tooling

兼容入口继续存在：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Scripts/flash_app.bat
05_Tools/Scripts/rtt_capture.bat
05_Tools/Scripts/run_app_cycle.bat
05_Tools/Scripts/gdb_runtime_snapshot.bat
05_Tools/Scripts/gdb_fault_capture.bat
05_Tools/Scripts/send_ymodem.bat
05_Tools/Scripts/send_ymodem_python.bat
05_Tools/Firmware/pack_firmware.py
```

Legacy Scripts 只允许做兼容薄包装，不建立第二套核心实现。

## S06 Closed Runtime Contract

正式入口：

```text
00_Project/03_Stages/S06_RTOS_Runtime/design.md
00_Project/03_Stages/S06_RTOS_Runtime/implementation_plan.md
00_Project/03_Stages/S06_RTOS_Runtime/handoff.md
00_Project/03_Stages/S06_RTOS_Runtime/review.md
04_Test/Reports/Stages/S06_RTOS_Runtime/verification.md
```

冻结三线程拓扑：

```text
appSystem
├─ Application lifecycle
├─ Foreground work
└─ v1.0 Blink / v1.1 Breath

otaWorker
├─ UART/Ymodem session
├─ Firmware receive
├─ Slot B write
└─ Image validation

displayTask
├─ Display Queue
├─ Display Model
├─ platform_graphics
└─ ST7789 / SPI1
```

冻结优先级：

```text
otaWorker   ABOVE_NORMAL
appSystem   NORMAL
displayTask BELOW_NORMAL
```

资源 Ownership：

```text
Application lifecycle / LED Demo → appSystem
Ymodem/Firmware Download          → otaWorker
ST7789/Graphics/SPI1 usage        → displayTask
```

LCD/ST7789 Board Adaptation 已完成。硬件 binding：

```text
PB10 → LCD_RST
PA1  → LCD_BL
PA4  → LCD_CS
PA5  → SPI1_SCK
PA6  → LCD_DC
PA7  → SPI1_MOSI
```

LCD 第一版继续使用现有 Graphics 字符绘制，不引入 LVGL。Visual Inspection + RTT 已完成；Logic Analyzer 保持接在 SPI2/W25Q64 与 Software I2C/AT24C02，S06 未要求 SPI1 波形。

并发板测已验证：

```text
v1.0 LED Blink continues
+
Background Ymodem/Firmware Storage
+
LCD RECEIVING / VERIFYING / SUCCESS|FAILED
```

成功传输、重复重启传输、中途终止、Timeout、Slot B Validation、任务阻塞与 Stack/Heap 证据均记录于 S06 Verification。S06 未交付公开 OTA START/session control、`PENDING`、Reset Request、Trial、Confirmed 或 Rollback，这些继续属于 S07 及后续阶段。

## S08 Bootloader Foundation Current State

当前状态：`CLOSED / PASS`。

S08 已冻结并完成三个核心目标：

```text
1. Internal Flash Layout
2. SEGGER RTT + boot_log + Bare-metal CmBacktrace
3. APP Vector Validation + reliable jump
```

S08 Implementation Commit：`8ec0045`。
S08 Board Test：Build / Flash / valid Jump / RTT / GDB / SysTick / repeated Reset / Power-cycle / LED / LCD / Application peripheral interrupt 已通过。

冻结 Flash Layout：

```text
Bootloader  : 0x08000000 / 0x00010000 (64 KiB)
Application : 0x08010000 / 0x00070000 (448 KiB)
```

Bootloader 技术栈：

```text
Bare-metal
HAL + CMSIS
No FreeRTOS
No EasyLogger
RTT + lightweight boot_log + CmBacktrace
```

当前独立工程已创建于：

```text
03_Firmware/Bootloader/OTA_Bootloader/
```

SPI2/W25Q64 与 PB6/PB7 软件 I2C GPIO 已预配置，但 S08 不实现 W25Q64、AT24C02 或 Firmware Installation。

正式入口：

```text
00_Project/03_Stages/S08_Bootloader_Foundation/design.md
00_Project/03_Stages/S08_Bootloader_Foundation/implementation_plan.md
00_Project/03_Stages/S08_Bootloader_Foundation/handoff.md
```

## Next Action

S09 已由 Project Owner 正式关闭为 `CLOSED / PASS`。下一步进入 `S10_Trial_Confirm_Rollback` 的设计阶段；S09 延期的 erase/program/CRC/Metadata marker/Power Loss Fault Injection 继续保留为跨阶段补充验证，不得在后续文档中误标为已通过。

## S09 Firmware Installation Current Design

当前状态：`CLOSED / PASS`。

冻结主链：

```text
PENDING
→ External Header / CRC / Vector pre-validation
→ destructive gate
→ erase Internal APP Sector 4~7
→ W25Q64 → bounded RAM buffer → Internal Flash
→ local read-back
→ Internal whole-image CRC
→ Internal vector validation
→ atomic Metadata PENDING → TRIAL
→ jump APP
```

冻结边界：

- Bootloader 不迁移 Application Firmware Service，轻量独立实现 Header/Metadata/CRC consumer；
- W25Q64 在 Bootloader 中只读；
- AT24C02 保留 Metadata 所需最小写能力；
- Internal Flash API 限制为 APP Region；
- Bus 先初始化，Device 后初始化；
- Invalid Candidate 在任何 Internal erase 前拒绝；
- TRIAL commit 前 reset 保持 PENDING 并从头重装；
- TRIAL commit 后 reset 不重复安装；
- confirmedSlot/confirmedVersion 在 S10 Confirm 前不改变；
- S10 继续负责 Trial runtime confirmation、Watchdog、Failure Counter 与 Rollback。

正式入口：

```text
00_Project/03_Stages/S09_Firmware_Installation/design.md
00_Project/03_Stages/S09_Firmware_Installation/implementation_plan.md
00_Project/03_Stages/S09_Firmware_Installation/handoff.md
```

当前施工结论：

```text
代码验证：PASS
硬件验证：PARTIAL PASS（剩余 Fault Injection DEFERRED）
Bootloader ROM：21104 bytes / 20.61 KiB < 64 KiB
Bootloader BIN：11688 bytes / 11.41 KiB < 64 KiB
```

S09 已完成 Bootloader contract、最小 Driver、APP-only Internal Flash、Candidate pre-validation、Installer、`PENDING → TRIAL` 原子提交和 Boot Main 编排。新增 C 文件已按 `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md` 补齐文件头、公开 API、类型、边界和硬件约束注释。Factory Restore 已有真实 baseline PASS；最新重试因 PC 未收到初始 `C` 最终返回 `worker result=2`，相关证据和不覆盖原则见 `04_Test/Reports/Stages/S09_Firmware_Installation/verification.md`。失败路径已补充恢复正式 Application 的工具逻辑，避免临时测试固件残留。剩余真实板级 Fault Injection 已记录为下一阶段补充验证，不改变 S09/S10 架构边界。