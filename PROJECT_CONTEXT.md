# Project Context

本文件是人工与任意 AI 工具恢复项目上下文的统一入口。详细内容以链接指向的正式文档为准。

## Context Metadata

- Active Stage: `S05A_Debug_Crash_Diagnostics`
- Active Stage Status: `CLOSED`
- S05A Implementation / Verification Commit: `bd8883d`
- S05A CmBacktrace Integration Commit: `1c27c8e`
- S05A Review Commit: `32f3368`
- Branch: `main`
- S05A Baseline Commit: `ec6119f64306d027c86208329823cf42b77ceebf`
- S05 Baseline Commit: `8173a3da2c174294350e47d8e889cf22b9066e23`
- S05 Design Commit: `400b8b4f4cb50672faea2c332379466bb637b3a6`
- S05 Implementation Plan Commit: `a5417e47dc86546176ec87dff5f6c58ddfb14260`
- S05 Design Approval Commit: `b62bdad9d1279158d4925a417ab5a0e1b4668db3`
- S05 Implementation Commit: `00cbd3a`
- S05 Verification Commit: `1d092de`
- S05 Review Commit: `1d092de`
- S05 Merge Commit: `5b2b42136e0d8f1eb2d54463fb5319996d6f6b5f`
- Last Closed Stage: `S05_UART_Ymodem`
- S05 Final Result: `CLOSED / PASS`
- Next Planned Stage: `S06_RTOS_Runtime` (after S05A)
- Current Role: `S06 Design Role / context handoff`
- Updated At: `2026-09-16`

## Current Goal

S05 已正式关闭并合并到 `main`。在进入 `S06_RTOS_Runtime` 之前新增 `S05A_Debug_Crash_Diagnostics` 小阶段。

S05A 已完成 GDB 自动化、失败清理、Runtime Snapshot 真实板测、CmBacktrace Keil/FreeRTOS/RTT 接入，以及三类受控 Fault 的 GDB/RTT 现场采集和交叉核对，并通过 Review 正式关闭。S04 Reset / Power-cycle Persistence 已作为补充回归完成真实板测。

## Required Reading For S06 Design

建议按顺序读取：

1. `AGENTS.md`
2. `README.md`
3. `PROJECT_CONTEXT.md`
4. `00_Project/WORKFLOW.md`
5. `00_Project/01_Requirements/项目需求V1.md`
6. `00_Project/02_Roadmap/development_roadmap.md`
7. `00_Project/03_Stages/S05_UART_Ymodem/handoff.md`
8. `00_Project/03_Stages/S05_UART_Ymodem/review.md`
9. `04_Test/Reports/Stages/S05_UART_Ymodem/verification.md`
10. `00_Project/03_Stages/S05A_Debug_Crash_Diagnostics/design.md`
11. `00_Project/03_Stages/S05A_Debug_Crash_Diagnostics/implementation_plan.md`
12. `00_Project/03_Stages/S05A_Debug_Crash_Diagnostics/handoff.md`
13. `00_Project/03_Stages/S05A_Debug_Crash_Diagnostics/review.md`
14. `04_Test/Reports/Stages/S05A_Debug_Crash_Diagnostics/verification.md`
15. `03_Firmware/AGENTS.md`
16. `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`
17. current App / FreeRTOS task initialization
18. current `service_uart`
19. current `service_ymodem`
20. current `service_firmware`
21. current Platform RTOS abstraction

## Stable S04 Storage / Firmware Contract

### External Flash

```text
Slot A: 0x000000 ~ 0x07FFFF
Slot B: 0x080000 ~ 0x0FFFFF

Per Slot:
+0x0000 ~ +0x0FFF : Header Sector
+0x1000 ~          : Firmware Payload
Payload capacity  : 508 KiB
```

### Firmware Image V1

- Header fixed 64 Byte；
- Header / Payload CRC32；
- Image Header 是实际 Firmware Version / Size / CRC 权威来源；
- `firmware_storage_validate_image()` 为只读验证；
- `pack_firmware.py` 输出 compact `.img = [64 Byte Header][Payload]`；
- Slot 中必须映射为 Header Sector + Payload Offset，不能线性写入 compact `.img`。

### Metadata

AT24C02 Metadata 继续使用双副本 + sequence + CRC + commit marker。S05 没有修改 OTA Metadata，也没有建立 `PENDING` 状态。

## Stable S05 Ymodem Capability

### Architecture

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
- `ymodem_parser` 负责 Packet framing / block complement / CRC-16；
- `ymodem_receiver` 负责 Block 0、Block sequence、ACK/NAK、retry、timeout、cancel、EOT；
- `ymodem_sink` 只定义 begin/write/end/abort 生命周期；
- Ymodem 不感知 Slot、EEPROM Metadata、PENDING、Reset；
- Packet CRC 复用 CRC-16/XMODEM；
- Firmware Storage 提供 `write_payload()` / `write_header()`；
- Header-last commit 保证失败传输不会提交新的有效 Header。

### Board Verification Result

最终 Tera Term 真实板测：

```text
file_size / received          55884 / 55884
packets received / accepted  57 / 57
bytes received / written      55884 / 55884
retry                         0
UART dropped / errors         0 / 0
Payload written               55820
Header commit                 1
Slot B validation             VALID
final result                  PASS
```

中止传输后 `header_commit=0`，随后重新建立 Session 可以再次成功传输并得到 VALID。

正式证据：

- `00_Project/03_Stages/S05_UART_Ymodem/design.md`
- `00_Project/03_Stages/S05_UART_Ymodem/implementation_plan.md`
- `00_Project/03_Stages/S05_UART_Ymodem/handoff.md`
- `00_Project/03_Stages/S05_UART_Ymodem/review.md`
- `04_Test/Reports/Stages/S05_UART_Ymodem/verification.md`

## PC / Local Tooling

默认板级 Ymodem Sender：Tera Term 5。

仓库入口：

```text
05_Tools/Scripts/send_ymodem.bat
```

Python Sender：

```text
05_Tools/Scripts/send_ymodem_python.bat
05_Tools/Ymodem/
```

Python Sender 定位为 Host Test、Agent 自动化和协议诊断辅助，不替代 S05 默认 Tera Term 板级验收入口。

Application 稳定工具链：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Scripts/flash_app.bat
05_Tools/Scripts/rtt_capture.bat
05_Tools/Scripts/run_app_cycle.bat
05_Tools/Firmware/pack_firmware.py
```

真实 Ymodem 板测的已验证顺序：

```text
准备合法 .img
→ 先启动 Sender / Tera Term 并打开 CH340 串口等待 'C'
→ 再 Flash / Reset MCU
→ Ymodem transfer
→ RTT capture
→ Firmware Storage validation
```

如果先 Reset MCU、后打开 Sender，可能错过初始 `'C'` 并表现为 Receiver Timeout；这是已确认的工具调用顺序约束，不是协议故障。

## S05A GDB Debug Checkpoint

S05A 是 S05 关闭后、S06 设计前新增的独立小阶段，当前状态为 `CLOSED`。

已完成真实板卡验证：

```text
J-Link GDB Server V7.92             PASS
STM32F411CE + SWD @ 4000 kHz        PASS
Keil OTA_APP.axf symbol loading     PASS
Breakpoint / Continue               PASS
Next / Step                         PASS
Backtrace / Memory read             PASS
Variable read                       PASS
```

已冻结运行与退出行为：

```text
Halt state
→ detach
→ MCU remains halted
```

```text
Running state
→ continue&
→ disconnect
→ quit
→ MCU continues running
```

`continue& → disconnect` 已通过重新连接和 `uwTick` 增长验证。GDB 自动化不得执行 `load`；Resume 会话不得使用 `-batch`，也不得在 `continue&` 后执行 `detach`。J-Link GDB Server 使用 `monitor reset`，不使用 `monitor reset halt`。

自动化板测已完成：

```text
halt 入口                         PASS
halt 后 uwTick 约 2 秒             0x38A62C -> 0x38A62C
resume 入口                       PASS
resume 后 uwTick 约 2 秒           0x396313 -> 0x399082
continue& -> disconnect -> quit    PASS
失败路径与非零退出码               PASS
J-Link Server / GDB 进程清理        PASS
```

正式证据：`04_Test/Reports/Stages/S05A_Debug_Crash_Diagnostics/verification.md`。

当前 S05A 范围包括：

```text
GDB toolchain configuration
GDB runtime snapshot resume/halt scripts
Agent-callable PowerShell / BAT entrypoints
Failure cleanup and J-Link release
Real-board verification
CmBacktrace Keil/FreeRTOS/RTT integration
Controlled Fault injection and Cortex-M context capture
GDB Fault Capture and GDB/CmBacktrace cross validation
```

暂不包括：

```text
S04 Reset Persistence       PASS
S04 Power-cycle Persistence PASS
```

正式交接：`00_Project/03_Stages/S05A_Debug_Crash_Diagnostics/handoff.md`

## Current RTOS Reality Before S06

早期 Roadmap 将 S06 描述为“集成 FreeRTOS”，该前提已经过时。

当前 Application 已经执行 FreeRTOS Kernel 初始化和调度，并存在正式任务基础。S05 板测还实际使用过独立 `s05Ymodem` Thread：该线程绑定为 `service_uart` owner，并作为 RingBuffer 的单 task-context Consumer 执行完整 Ymodem / Storage 流程。

这只是一项 S05 板测实现，不代表 S06 的正式 Runtime 设计已经冻结。

S06 应重新讨论并冻结：

```text
Task Topology
+ Task Lifecycle
+ UART Consumer Ownership
+ OTA/Ymodem Task Ownership
+ Task Notification / Queue / Event
+ Mutex / Shared Resource Policy
+ W25Q64 / Firmware Storage Serialization
+ Blocking API Policy
+ Timeout / Cancel / Error Recovery
+ Normal Business vs Background OTA Concurrency
+ Logging Resource Contention
```

避免再次“移植一遍 FreeRTOS”，也不要直接把 S05 测试线程原样升级成生产任务。

## S06 Design Questions

第一轮 Design Discussion 至少需要回答：

1. `appSystem` 的长期职责是什么；
2. 是否建立独立 OTA Task，还是由已有任务驱动 OTA Service；
3. Ymodem Receiver 的生命周期由谁创建、启动、取消和销毁；
4. `service_uart` ownerThread 是否需要支持重新绑定，还是构造后固定；
5. UART RingBuffer 单 Consumer 如何成为正式约束；
6. Firmware Storage / W25Q64 是否需要 Mutex，锁放在哪一层；
7. OTA 下载期间普通业务允许继续执行到什么程度；
8. Flash erase/program 的长延迟如何影响调度；
9. Task Notification、Queue、Event Group 分别解决什么实际问题；
10. S06 最终应向 S07 提供什么稳定 Runtime API / 运行合同。

## S04 Persistence Regression Completion

2026-09-16 已完成 S04 两项跨阶段持久性回归：

```text
Reset Persistence       PASS
Power-cycle Persistence PASS
```

Power-cycle 自动化使用指定测试 AXF 解析的 `_SEGGER_RTT` 固定地址；Logger 无数据卡死时由
watchdog 重启，目标不可连接时按 1/2/4 秒退避重连，并要求启动标志之后捕获新快照。真实日志
捕获 2 次启动标志、62 条快照，事件前后快照全部一致。正式 Keil target 已移除临时入口，
复测需通过本机配置提供独立测试 AXF。详细证据见：

```text
04_Test/Reports/Stages/S04_Firmware_Image_Storage/verification.md
00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md
```

## Deferred Regression

当前没有未完成的 S04 Persistence 跨阶段延期项：

```text
Reset Persistence       PASS
Power-cycle Persistence PASS
```

两项回归已完成，不再形成 S07 关闭前待办。

## Explicitly Deferred Beyond S06

- Application OTA Service 完整业务状态机；
- Inactive Slot / PENDING / Reset；
- Bootloader Internal Flash installation；
- Trial / Confirm / Rollback；
- IWDG failure counter；
- SHA / AES / HMAC / Digital Signature；
- Device Manager / generic Storage Manager。

## Next Action

S05A Review 已通过，S04 Persistence 补充回归也已完成；下一步开启 `S06_RTOS_Runtime` 设计讨论。

S06 仍需先读取仓库当前 RTOS 和任务现状，讨论设计；不要直接进入实现。
