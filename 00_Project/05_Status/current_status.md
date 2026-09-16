# Current Project Status

## Context Metadata

- Active Stage: `S05A_Debug_Crash_Diagnostics`
- Status: `CLOSED`
- S05A Implementation / Verification Commit: `bd8883d`
- S05A CmBacktrace Integration Commit: `1c27c8e`
- S05A Review Commit: `32f3368`
- S04 Persistence Supplementary Regression Commit: `6f2fad5`
- Branch: `main`
- S05A Baseline Commit: `ec6119f64306d027c86208329823cf42b77ceebf`
- Baseline Commit: `8173a3da2c174294350e47d8e889cf22b9066e23`
- Design Commit: `400b8b4f4cb50672faea2c332379466bb637b3a6`
- Implementation Plan Commit: `a5417e47dc86546176ec87dff5f6c58ddfb14260`
- Design Approval Commit: `b62bdad9d1279158d4925a417ab5a0e1b4668db3`
- Implementation Commit: `00cbd3a`
- Verification Commit: `1d092de`
- Review Commit: `1d092de`
- S05 Merge Commit: `5b2b42136e0d8f1eb2d54463fb5319996d6f6b5f`
- Last Closed Stage: `S05_UART_Ymodem`
- Last Closed Stage Status: `CLOSED / PASS`
- Next Planned Stage: `S06_RTOS_Runtime` (after S05A)
- Current Role: `S06 Design Role / context handoff`
- Updated At: `2026-09-16`

## Current Goal

`S05_UART_Ymodem` 已完成 Design、Implementation、Verification、Review，并已通过 PR #7 合并到 `main`。在进入 `S06_RTOS_Runtime` 之前新增 `S05A_Debug_Crash_Diagnostics` 小阶段。

当前 S05A 已完成 GDB 自动化与真实板测。手工 GDB 控制能力、Runtime Snapshot resume/halt、失败路径、进程清理和 J-Link 释放均已验证。CmBacktrace 源码已完成 Keil/FreeRTOS/RTT 工程接入；Invalid Address、Undefined Instruction、Divide by Zero 三类受控 Fault 均已完成真实板端 GDB/RTT 采集和现场交叉核对。S04 Reset / Power-cycle Persistence 补充回归也已完成。

## S05 Delivered Capabilities

```text
Tera Term / Python Ymodem Sender
        ↓
USART1 + DMA + RingBuffer
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
        ↓
W25Q64 Slot B
        ↓
firmware_storage_validate_image() == VALID
```

已交付：

- Ymodem Receiver-only / Single-file 组件；
- SOH 128 Byte / STX 1 KiB Packet；
- Block 0 文件信息解析；
- CRC-16/XMODEM、ACK / NAK、Duplicate、Sequence、Timeout、Retry、CAN、EOT；
- Parser / Receiver / Sink 分层；
- Firmware Storage `write_payload()` / `write_header()`；
- compact `.img = [64B Header][Payload]` 到 Slot `Header @ +0x0000 / Payload @ +0x1000` 的映射；
- Header-last commit；
- Tera Term 5 自动化 Sender；
- Python Ymodem Sender 作为 Host/Agent/诊断辅助工具；
- Host Test、Keil Build、J-Link/RTT、真实 CH340 板测闭环。

## S05 Verification Summary

真实 Tera Term 板测已验证：

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

中途停止 Sender 后，Receiver 进入 Timeout/Error，`header_commit=0`；随后重新建立会话可再次成功传输并将 Slot B 校验为 VALID。

正式证据：

- `00_Project/03_Stages/S05_UART_Ymodem/review.md`
- `04_Test/Reports/Stages/S05_UART_Ymodem/verification.md`

## S05A GDB Debug Checkpoint

S05A 位于已关闭的 S05 与计划中的 S06 之间，当前状态为 `CLOSED`。

已完成的真实板测基线：

```text
J-Link GDB Server V7.92             PASS
STM32F411CE + SWD @ 4000 kHz        PASS
Keil OTA_APP.axf symbol loading     PASS
Breakpoint / Continue               PASS
Next / Step                         PASS
Backtrace / Memory read             PASS
Variable read                       PASS
```

已冻结的退出合同：

```text
halt state   → detach
             → MCU remains halted

running state → continue&
              → disconnect
              → quit
              → MCU continues running
```

`continue& → disconnect` 已通过重新连接和 `uwTick` 增长验证。S05A 自动化不得执行 GDB `load`，Resume 会话不得使用 `-batch`，也不得在 `continue&` 后使用 `detach`。

自动化板测结果：

```text
halt 后 uwTick 约 2 秒：0x38A62C -> 0x38A62C       PASS
resume 后 uwTick 约 2 秒：0x396313 -> 0x399082     PASS
continue& -> disconnect -> quit                      PASS
失败路径、非零退出码、PID 清理、J-Link 释放            PASS
```

正式证据：`04_Test/Reports/Stages/S05A_Debug_Crash_Diagnostics/verification.md`。

当前 S05A 已覆盖 GDB Runtime Snapshot、resume/halt 生命周期、失败清理、板测证据、CmBacktrace 的 Keil/FreeRTOS/RTT 工程接入，以及三类受控 Fault 注入、现场采集和 GDB/CmBacktrace 交叉验证。Review 已通过并关闭 S05A；S04 Reset/Power-cycle Persistence 已在补充回归中完成。

正式交接：

- `00_Project/03_Stages/S05A_Debug_Crash_Diagnostics/handoff.md`
- `00_Project/03_Stages/S05_UART_Ymodem/handoff.md`

## Stable Tooling After S05

Application 工具链：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Scripts/flash_app.bat
05_Tools/Scripts/rtt_capture.bat
05_Tools/Scripts/run_app_cycle.bat
05_Tools/Firmware/pack_firmware.py
05_Tools/Scripts/send_ymodem.bat
05_Tools/Scripts/send_ymodem_python.bat
```

Ymodem 板测必须发送 S04 `.img`，不能把原始 Application `.bin` 当作最终 Firmware Image。

当前已验证的自动化顺序为：

```text
先启动 Sender / Tera Term 并打开 COM 等待 'C'
→ 再 Flash / Reset MCU
→ Ymodem transfer
→ RTT evidence
→ Firmware validation
```

## S06 Design Entry After S05A

S06 名称保持 `S06_RTOS_Runtime`，但需要修正早期路线中的一个前提：**FreeRTOS 已经存在并正常运行，不需要再次“集成 FreeRTOS”。**

当前 Application 已具备 RTOS Kernel、`appSystem` 等任务基础；S05 板测也曾使用独立 `s05Ymodem` Thread，并验证 `service_uart` 的单 Consumer / ownerThread 约束。

因此 S06 应在 S05A GDB 自动化关闭后优先讨论：

1. Application 正式 Task 划分与生命周期；
2. OTA / Ymodem 应由哪个 Task 拥有；
3. `service_uart` ownerThread 和 RingBuffer 单消费者边界；
4. Task Notification / Queue / Event / Mutex 的实际需求；
5. W25Q64 / Firmware Storage 的并发访问和串行化策略；
6. 正常业务与后台 Firmware 下载如何并发；
7. Blocking API 的允许位置、Timeout 和调度影响；
8. 任务退出、Cancel、Error Recovery；
9. 日志与 OTA 数据路径的资源竞争；
10. S07 OTA Service 需要从 S06 获得哪些稳定 Runtime 能力。

S06 尚未创建正式 `design.md / implementation_plan.md`，当前不得直接施工 S06 生产代码。

## Deferred Regression

S04 跨阶段回归已完成，当前无延期项：

```text
Reset Persistence       PASS
Power-cycle Persistence PASS
```

Power-cycle 通过自动 RTT 固定地址、无数据 watchdog 和退避重连捕获上电启动标志，并要求启动
标志之后出现新快照；事件前后快照一致。正式 Keil target 已移除临时入口，复测需通过
`S04_PERSISTENCE_AXF` 提供独立测试 AXF。详细证据见
`04_Test/Reports/Stages/S04_Firmware_Image_Storage/verification.md`。

## Blockers

S05A GDB / CmBacktrace / controlled Fault checkpoint 无阶段内阻塞项，Review 已通过。S04 Persistence 补充回归已完成，无相关阻塞项。

下一步进入 S06 Design Discussion；S04 Reset / Power-cycle Persistence 补充回归已完成。
