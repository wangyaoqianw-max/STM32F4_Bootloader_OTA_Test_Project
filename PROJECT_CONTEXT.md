# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S06_RTOS_Runtime`
- Active Stage Status: `CLOSED / PASS`
- Branch: `main`
- S06 Design Commit: `eb57291f9d506965bfc20acc4261ce7e01888094`
- S06 Implementation Plan Commit: `9f304c731c06eafb842b50c3098702d4a842db2e`
- S06 Implementation Commits: `f6f50fd`, `b90d462`, `6d0d323`, `38f7c60`, `20ec343`, `66e2934`, `014b617`
- S06 Verification Report: `04_Test/Reports/Stages/S06_RTOS_Runtime/verification.md`
- S06 Handoff: `00_Project/03_Stages/S06_RTOS_Runtime/handoff.md`
- S06 Verification Commit: `8c67ad2`
- S06 Review Commit: `Pending final review commit`
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
- Last Closed Stage: `S06_RTOS_Runtime`
- Last Closed Stage Status: `CLOSED / PASS`
- Next Planned Stage: `S07_OTA_Service_V1`
- Current Role: `Project Owner`
- Updated At: `2026-09-17`

## Current Goal

S05、S05A、S05B、S05C 和 S06 已关闭；S04 Reset / Power-cycle Persistence 补充回归也已完成。`S06_RTOS_Runtime` 已完成实现、板级验证、Toolkit 回归、Verification、Handoff 和 Review，状态为 `CLOSED / PASS`。

S06 不再是“移植 FreeRTOS”。Application 已经运行 FreeRTOS，本阶段正式目标是建立三线程 Application Runtime / Concurrency Model，并完成 ST7789/LCD 板级适配，为 S07 OTA Service V1 提供清晰的任务、资源所有权和并发基础。

冻结 Runtime：

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

实施第一项：完成 LCD/ST7789 Board Adaptation，再进入三线程 Runtime 重构。

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

## S06 Approved Runtime Contract

正式入口：

```text
00_Project/03_Stages/S06_RTOS_Runtime/design.md
00_Project/03_Stages/S06_RTOS_Runtime/implementation_plan.md
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

初始优先级：

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

S06 第一项实施任务为 LCD/ST7789 Board Adaptation。当前硬件 binding：

```text
PB10 → LCD_RST
PA1  → LCD_BL
PA4  → LCD_CS
PA5  → SPI1_SCK
PA6  → LCD_DC
PA7  → SPI1_MOSI
```

LCD 第一版使用现有 Graphics 字符绘制，不引入 LVGL。LCD 验收采用 Visual Inspection + RTT；Logic Analyzer 保持接在 SPI2/W25Q64 与 Software I2C/AT24C02，不要求 S06 采集 SPI1 波形。

核心并发验收：

```text
v1.0 LED Blink continues
+
Background Ymodem/Firmware Storage
+
LCD RECEIVING / VERIFYING / SUCCESS|FAILED
```

S06 结束后必须给 S07 提供稳定 Runtime / Concurrency Contract，而不是提前实现 OTA Service 的 `PENDING / Reset` 业务逻辑。

## Next Action

执行 `00_Project/03_Stages/S06_RTOS_Runtime/implementation_plan.md`。

从 Task 1 开始：完成 LCD/ST7789 Board Adaptation，使用现有 Toolkit 完成 Build / Flash / RTT 和真实屏幕 Visual Acceptance；通过后再实施三线程 Runtime 重构。
