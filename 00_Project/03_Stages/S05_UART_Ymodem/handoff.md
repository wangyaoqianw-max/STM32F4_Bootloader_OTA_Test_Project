# S05 UART Ymodem Handoff

## Metadata

- Stage: `S05_UART_Ymodem`
- Status: `CLOSED`
- Final Branch: `main`
- Baseline Commit: `8173a3da2c174294350e47d8e889cf22b9066e23`
- Design Commit: `400b8b4f4cb50672faea2c332379466bb637b3a6`
- Implementation Plan Commit: `a5417e47dc86546176ec87dff5f6c58ddfb14260`
- Project Owner Approval: `PASS / 2026-09-14`
- Implementation Commit: `00cbd3a`
- Verification Commit: `1d092de`
- Review Commit: `1d092de`
- Merge Commit: `5b2b42136e0d8f1eb2d54463fb5319996d6f6b5f`
- Closure Decision: `PASS`
- Updated At: `2026-09-15`

## Current Role

Stage Closed / Downstream Handoff。

S05 已完成设计、实现、验证、Review，并已通过 PR #7 合并到 `main`。后续不要继续把 S05 当作活动施工阶段；除非出现回归，否则只作为 S06/S07 的稳定输入。

## Delivered Capability

S05 建立了 Application 侧可复用的 Ymodem Receiver 文件传输能力：

```text
PC / Tera Term Ymodem Sender
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
```

S05 只解决“可靠接收 Firmware 文件”，不承载正式 OTA 业务状态。

## Frozen Architecture Boundary

### service_uart

负责：

- UART DMA RX；
- RingBuffer；
- UART TX；
- event / timeout；
- error / data loss；
- task-context ownerThread / 单 Consumer 约束。

Ymodem 不直接调用 HAL UART。

### ymodem_parser

负责：

- SOH 128 Byte / STX 1024 Byte Packet framing；
- Block Number complement；
- CRC-16/XMODEM；
- EOT / CAN parser event；
- 跨多次 UART read 的增量组包。

### ymodem_receiver

负责：

- Block 0；
- filename / filesize / Tera Term 可选 metadata；
- expected block / duplicate / sequence；
- ACK / NAK / `'C'`；
- timeout / retry；
- local / remote cancel；
- EOT / Empty Block 0 Session End；
- Receiver state machine；
- statistics / status。

### ymodem_sink

只定义文件生命周期：

```text
begin
write
end
abort
```

Ymodem 不知道 Slot、Metadata、PENDING、Reset。

### firmware_storage

S05 新增稳定写能力：

```c
firmware_storage_write_payload(...)
firmware_storage_write_header(...)
```

上层不直接计算 W25Q64 Firmware 物理地址。

## Critical Firmware Mapping Contract

S04 Packer 输出：

```text
.img = [64 Byte Header][Payload]
```

但 External Flash Slot 布局为：

```text
slot + 0x0000 : Header Sector
slot + 0x1000 : Payload
```

因此 compact `.img` 不能从 Slot Base 连续写入。

S05 Flash Sink 的稳定行为：

```text
receive first 64 bytes
→ cache + validate Firmware Header

receive Payload
→ firmware_storage_write_payload()
→ slot + 0x1000

Ymodem Session success
→ firmware_storage_write_header()
→ Header-last commit
```

任何 Timeout / Cancel / write failure / incomplete session 都不得提交新的 Header。

该规则是 S07 OTA Service 需要继续继承的存储提交语义。

## Protocol Profile

S05 V1 支持：

- Receiver only；
- Single file；
- SOH / STX；
- CRC-16/XMODEM；
- Block 0；
- ACK / NAK；
- Duplicate / Sequence Error；
- Timeout / Retry；
- CAN；
- Classic EOT handshake；
- Empty Block 0 end-of-session。

当前 Block 0 与 Tera Term 实际格式兼容：

```text
filename
file size       decimal
mtime           octal
mode            octal
serial          optional octal
```

字段解析使用有界输入检查，非法字段和未知尾部数据拒绝。

## Implementation Output

主要施工提交：

```text
090e2d2 tools: add Tera Term ymodem sender entry
417aa00 feat: add firmware storage write paths
12f063f feat: add ymodem packet parser
e6edc6f style: align ymodem public comments
d9afae4 feat: add ymodem receiver state machine
3d98d19 feat: add s05 ymodem flash sink
00cbd3a test: integrate s05 ymodem board endpoint
```

正式实现已合并到：

```text
main @ 5b2b42136e0d8f1eb2d54463fb5319996d6f6b5f
```

## Verification Result

代码验证：`PASS`

覆盖：

- Parser Host Test；
- Receiver Host Test；
- Firmware Storage Write Host Test；
- Flash Sink Host Test；
- S04 相关回归；
- Python Ymodem Sender Host Test；
- Firmware Packer Test；
- Keil normal / clean build；
- `git diff --check`。

硬件验证：`PASS`

最终 Tera Term + CH340 板测：

```text
filename                        OTA_APP_s04_v1.1.0.img
file_size / received            55884 / 55884
packets received / accepted    57 / 57
bytes received / written        55884 / 55884
CRC / sequence / duplicate      0 / 0 / 0
timeout / retry / cancel        0 / 0 / 0
UART dropped / errors           0 / 0
Flash Payload                   55820
Header commit                   1
Slot B validation               VALID
final result                    PASS
```

中止测试：

```text
received                        12288 / 55884
state                           ERROR / timeout
header_commit                   0
```

随后重新建立 Session，Tera Term 再次传输成功，Slot B 再次 VALID。

完整证据：

- `04_Test/Reports/Stages/S05_UART_Ymodem/verification.md`
- `00_Project/03_Stages/S05_UART_Ymodem/review.md`

## PC / Agent Tooling

默认板级 Sender：

```text
05_Tools/Scripts/send_ymodem.bat
→ Tera Term 5
```

Python Sender：

```text
05_Tools/Scripts/send_ymodem_python.bat
05_Tools/Ymodem/
```

Python Sender 用于 Host Test、自动串口枚举、Agent 调用和协议诊断；S05 正式板级 Reference Sender 仍为 Tera Term。

稳定工具链：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Scripts/flash_app.bat
05_Tools/Scripts/rtt_capture.bat
05_Tools/Scripts/run_app_cycle.bat
05_Tools/Firmware/pack_firmware.py
```

## Important Board-Test Operating Contract

发送输入必须是合法 S04 Firmware Image：

```text
.img = 64 Byte Header + Payload
```

不能直接发送原始 Application `.bin` 作为完整 S05 Firmware Image。

实际验证过的调用顺序：

```text
关闭占用 COM 的其他工具
→ 确认 CH340 COM
→ 启动 Tera Term / Sender，先打开 COM 并等待 'C'
→ 再 Flash / Reset MCU
→ 完成 Ymodem Session
→ RTT capture
→ Firmware validation
```

如果先 Reset MCU 再打开 Sender，可能错过 Receiver 初始 `'C'`，表现为 Host 等待 Receiver Timeout。该现象已经确认是自动化时序问题，不是 Ymodem/UART 故障。

## RTOS Information Exposed By S05

S05 板测阶段曾使用独立 `s05Ymodem` Thread：

```text
appSystem
→ start s05Ymodem test thread

s05Ymodem
→ bind service_uart ownerThread
→ become RingBuffer single consumer
→ run Ymodem session
→ run Storage validation
```

该结构证明独立任务模式可运行，但它是 **S05 Board Test 实现**，不是正式 Application Runtime 架构。

下游 S06 不应直接照搬该线程，而应根据完整 Application 并发需求重新设计 Task Topology 和资源所有权。

## Downstream Inputs For S06

S06 可以直接认为以下能力稳定：

- FreeRTOS 已经集成并运行；
- Platform RTOS abstraction 已存在；
- `service_uart` 已具备 task owner / event wait / DMA RingBuffer；
- UART RingBuffer 当前模型为 task-context single consumer；
- Ymodem Receiver 可以作为长生命周期协议状态机运行；
- Firmware Storage / W25Q64 存在较长 erase/program 操作；
- RTT + EasyLogger 已可作为并发调试证据；
- Build / Flash / RTT / Ymodem Sender 均已有 Agent 可调用入口。

S06 需要解决的是正式 Runtime / Concurrency Model，而不是再次移植 FreeRTOS。

优先讨论：

1. 正式 Task 划分；
2. OTA/Ymodem Task ownership；
3. UART single consumer 约束；
4. Task Notification / Queue / Event / Mutex 选择；
5. W25Q64 / Firmware Storage 并发保护；
6. Flash erase/program 对调度的影响；
7. 正常业务与 OTA 下载并发；
8. Blocking API policy；
9. Cancel / Error Recovery；
10. S07 OTA Service 运行合同。

## Explicitly Out Of S05

以下仍不属于 S05：

- Inactive Slot 动态选择；
- EEPROM `PENDING`；
- OTA Service；
- Reset request；
- Bootloader install；
- Trial / Confirm / Rollback；
- Watchdog reliability workflow；
- generic Protocol / Storage Manager。

## Deferred Cross-stage Regression

S04：

```text
Reset Persistence       PENDING / DEFERRED
Power-cycle Persistence PENDING / DEFERRED
```

它们不阻塞 S06，但必须在 `S07_OTA_Service_V1` 关闭前完成。

## Next Action

进入 `S06_RTOS_Runtime` Design Discussion。

先读取仓库当前任务、RTOS abstraction、UART/Ymodem 和 Storage 使用方式，再冻结 S06 设计；不要直接把 S05 Board Test Thread 提升为正式生产任务。
