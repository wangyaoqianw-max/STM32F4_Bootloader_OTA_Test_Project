# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S10_Trial_Confirm_Rollback`
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
- S10 Baseline Commit: `de17162c179f3a6551c9edca0d5df35c03ffdc48`
- S10 Design Commit: `223fae71d3c641cf9e36d6048d22a73815152b94`
- S10 Design Metadata Commit: `d054f165553ed320561d726fe736f8bea7ec525e`
- S10 Handoff Commit: `6a5d2fadabf5ee7face632e90cc18e1daab02f47`
- S10 Design Review Amendment Commit: `ffcf6b835d2c9dd43ee907b4770c6df43c964dc2`
- S10 Design Review Acceptance Sync Commit: `c4f411f8a4dec926ebca0d9463ba9184ffc8af35`
- S10 Design Approval Commit: `621df210a0fba930d05e32fe450a494b9b0c2cca`
- S10 Implementation Plan Commit: `d63b1f8a9d1981cf1fb1e2a07a6a9af17829c1ac`
- S10 Verification Follow-up Commits: `5ca2d14`, `f0e5d88`, `25d2acc`, `0ee7f5d`, `ba6ce6f`, `7eab4cc`, `9fb7f09`
- S10 Verification Commit: `745335a`
- S10 Review Commit: `745335a`
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
- Last Closed Stage: `S10_Trial_Confirm_Rollback`
- Last Closed Stage Status: `CLOSED / PASS`
- Next Planned Stage: `Not selected`
- Current Role: `Project Owner`
- Updated At: `2026-09-21`

## Current Goal

S10 已完成 Implementation Plan Task 0→10 的生产实现、自动化回归、板级功能观察和交接文档。实际施工基线为当前已同步的 `79b95d1f4681c2f7b5f961785079a112a3c62492`；计划记录的 `651b3001` 是其祖先，之间仅有阶段文档提交。Application/Bootloader 代码验证为 `PASS`，Review 未发现 Critical / Important 软件逻辑问题。依据 2026-09-21 Project Owner 批准的验收修订，S10 以软件逻辑正确性作为主要通过条件，阶段记录为 `CLOSED / PASS`；真实硬件验证仍为 `PARTIAL / DEFERRED FOLLOW-UP`，不把缺失的连续 Bootloader RTT/GDB 和 S10-07 板级门禁改写为硬件 PASS。当前剩余操作项只有在 J-Link 通道恢复后确认正式 v1.0 RTT 的板卡恢复状态；该操作不改变 S10 软件结论。S09 剩余 Fault Injection 继续作为独立 Deferred Follow-up，不视为 S10 已通过。

最新 Review 决策：验收口径、验证报告、矩阵、交接和状态入口已同步；正式生产构建无临时 S10 测试钩子。板卡恢复尝试在 2026-09-21 期间受 J-Link USB 通道无法打开影响，未据此虚报烧录或运行成功，待探针连接恢复后只执行正式 v1.0 恢复性验证。

2026-09-20 根因隔离进一步确认：W25Q64 直接写入和复位保持正常；Factory Restore 的 Sender 在所有数据块 ACK 后停在 EOT/结束 Header 阶段，外层 60 s 进程超时先于 Sender 的 120 s 等待窗口终止进程。Header-last 使 Slot A 留下 Payload 已写、Header 全 `0xFF` 的无效半成品，后续 Rollback 的 confirmed prevalidate 失败是下游现象。S10-01 继续 `BLOCKED`；必须先修正/验证 Factory Restore 超时，并按 C0～C5 独立读取 W25Q64 后才能继续。

2026-09-21 已使用新的 External Loader 建立 F0，完成 v1.1 YMODEM、第二次 PA0、确认前断点和 Trial 窗口真实断电/上电。用户观察到 LED 频率恢复为 v1.0、LCD 显示正常；内部 Application `0x08010000` 的 `81348` bytes 与 v1.0 payload 逐字节匹配，LED/LCD 恢复观察记为 `PASS`。本轮硬件行为整体仍为 `PARTIAL`，因为未预置 Bootloader `boot_main_validate_and_jump` 断点，固定地址 RTT Logger 未捕获 `TRIAL → ROLLBACK → restore → NONE` 中间链。下一轮必须同时设置 Application 确认前断点和 Bootloader 断点；Bootloader 断点命中后先读取 `0x200000E0` RTT，再由同一 GDB 会话 `continue`。

同日重试 `20260921-143727` 中，External Loader Slot A Header/Payload 读回通过，但 Metadata baseline 被 Bootloader `Soft-I2C init FAIL: BUSY` / `BOOT halt: external device init` 阻塞，未进入 OTA。硬复位保持 CPU 停止时读到 `GPIOB_IDR=0x0000F757`，PB7 为低，确认当前阻塞在 AT24C02 总线 Idle 检查；该重试不改变 Slot A 预烧录读回 PASS，阶段仍为 `READY_FOR_VERIFICATION`。

2026-09-21 确认前断电功能行为已接受：设备回滚到 v1.0，LED 恢复 v1.0 闪烁频率，LCD 正常显示。后续 `20260921-150308` 已完成 Metadata baseline PASS；`20260921-150545` 和 `20260921-151853` 只暴露出掉电清除 GDB 硬件断点、GDB Python 不可用和主机重挂过晚等观测链限制。功能接受记为 `PASS`，代码验证仍为 `PASS`，正式硬件验证保持 `PARTIAL`；未加入临时 Bootloader 延时钩子。

2026-09-21 S10-06 双断电功能验收：当前 v1.0 基线发送 `app_v1.1.img`（COM9/115200，84680 bytes，83 blocks，retries=2，exit 0）后，第一次断电上电，再在下一次 Bootloader 恢复窗口内立即第二次断电，最终上电后用户观察到 LED 恢复 v1.0 频率、LCD 正常。S10-06 功能行为接受为 `PASS`；连续 Bootloader RTT/GDB 中间证据未采集，正式硬件验证仍为 `PARTIAL`。S10 不标记 `CLOSED`。

S10-02、S10-03、S10-04、S10-05、S10-06 的功能行为已接受通过，部分中间 RTT/Reset Cause/Metadata 证据未连续采集。S10-07 当前因没有已批准、可回读的板级坏镜像注入入口而 `BLOCKED`；S10-09 最终回归、报告和 Review 尚未完成。S05C 真实 I2C/SPI 采集和 S09 Deferred Fault Injection 仍按原阶段归属。

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

S10 Design 与 Implementation Plan 均已冻结，Task 0→10 已完成。2026-09-20 已重新通过自动化 Host/Contract、Python 回归和 Application/Bootloader Clean Build；正式 NONE 启动、重复 v1.1 YMODEM、Runtime Ready 和 strict Confirm 的 RTT/GDB 证据仍有效，IWDG Debug Freeze/Resume 与 direct no-feed IWDG reset 证据也已取得，代码验证为 `PASS`，硬件验证为 `PARTIAL`。独立测试方案已按根因隔离结果修订；新的 External Loader F0 和 Metadata baseline 已通过。S10-02/S10-03/S10-04/S10-05/S10-06 功能行为已接受通过，部分中间 RTT/Reset Cause/Metadata 证据未连续采集。S10-07 因缺少已批准的板级坏镜像注入入口而 `BLOCKED`，剩余 S10-09 最终回归/Review。S09 Deferred Fault Injection 继续保持原阶段归属。

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
