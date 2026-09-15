# S05 UART Ymodem Handoff

## Metadata

- Stage: `S05_UART_Ymodem`
- Status: `CLOSED`
- Branch: `codex/s05-uart-ymodem`
- Baseline Commit: `8173a3da2c174294350e47d8e889cf22b9066e23`
- Design Commit: `400b8b4f4cb50672faea2c332379466bb637b3a6`
- Implementation Plan Commit: `a5417e47dc86546176ec87dff5f6c58ddfb14260`
- Project Owner Approval: `PASS / 2026-09-14`
- Implementation Commit: `00cbd3a`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Updated At: `2026-09-15`

## Current Role

Review Role，Verification / Review 已通过，S05 阶段已关闭。

Project Owner 已确认 `design.md` 与 `implementation_plan.md`，阶段已通过正常设计门禁：

```text
DRAFT
→ DESIGN_APPROVED
→ READY_FOR_IMPLEMENTATION
```

允许严格按照已批准计划开始施工；需要偏离冻结边界时，先记录冲突并重新确认，不得静默改变设计。

## Stage Goal

建立可复用 Ymodem Receiver 文件传输组件，并通过独立 PC Sender（Tera Term 5）将 S04 Firmware Image V1 `.img` 可靠接收到 STM32，按既有 Firmware Storage Contract 写入固定 Slot B，并完成真实板级验证。

S05 是可靠文件运输能力，不负责正式 OTA 业务状态机。

## Frozen Design Direction

```text
PC / Tera Term Ymodem Sender
          ↓
service_uart
          ↓
ymodem_parser
          ↓
ymodem_receiver
          ↓
ymodem_sink
          ↓
S05 Flash Sink
          ↓
firmware_storage
          ↓
W25Q64 Slot B
```

职责：

- `service_uart`：DMA / RingBuffer / UART TX / error / data loss；
- `ymodem_parser`：Packet framing / block complement / CRC-16；
- `ymodem_receiver`：Block 0 / Block sequence / ACK-NAK / retry / timeout / cancel / EOT；
- `ymodem_sink`：begin / write / end / abort 文件生命周期；
- `firmware_storage`：Slot 与物理 Flash 布局；
- S07 OTA Service：后续才负责 inactive slot / PENDING / Reset。

## Critical S04 Contract Found During S05 Design

S04 PC packer 输出紧凑传输文件：

```text
.img = [64 Byte Header][Payload]
```

但 W25Q64 Slot 物理布局固定为：

```text
slot + 0x0000 : Header Sector
slot + 0x1000 : Payload
```

因此 S05 禁止把 `.img` 从 Slot Base 连续写入。

S05 Flash Sink 必须：

```text
receive first 64 file bytes
→ cache + validate Header

receive Payload
→ firmware_storage_write_payload()
→ physical slot + 0x1000

successful Ymodem completion
→ firmware_storage_write_header()
→ Header-last commit
```

这保持 S04 已冻结的 failure-atomicity 方向。

## Planned Firmware Storage Extension

新增合同感知接口：

```c
platform_error_t firmware_storage_write_payload(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    uint32_t payloadOffset,
    const uint8_t *data,
    uint32_t length);

platform_error_t firmware_storage_write_header(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    const uint8_t rawHeader[FIRMWARE_IMAGE_HEADER_SIZE]);
```

不新增 Ymodem 语义，不修改 Metadata。

## Tera Term Automation Input

第一版 PC Reference Sender：Tera Term 5 YMODEM。

当前开发机已确认 executable：

```text
E:\APP\ProgramFile\tera_term\teraterm5\ttermpro.exe
```

该路径只写入被 Git 忽略的：

```text
05_Tools/Config/toolchain.local.bat
```

提交仓库的 `toolchain.local.example.bat` 只增加空 `TERA_TERM_EXE` 占位。

已新增：

```text
05_Tools/TeraTerm/send_ymodem.ttl
05_Tools/Scripts/send_ymodem.bat
```

第一个 Implementation Task 先证明 Tera Term macro 可被本地 Agent/Codex 稳定调用；在 MCU Receiver 尚未存在时，Ymodem timeout/failure 是预期结果，不得标成 transfer PASS。

## Existing Toolchain Reuse

S05 Board Test 不重建 Build/Flash/RTT 工具链，继续使用：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Scripts/flash_app.bat
05_Tools/Scripts/rtt_capture.bat
05_Tools/Scripts/run_app_cycle.bat
```

真实板测主链：

```text
Build
→ Flash
→ Reset / Run
→ RTT Capture
→ Tera Term Ymodem Send
→ RTT protocol/storage evidence
→ firmware_storage_validate_image(Slot B)
```

RTT 至少记录：

- YMODEM_READY；
- filename / file size；
- transfer progress；
- CRC / retry / sequence / duplicate summary；
- cancel/error reason；
- EOT/session completion；
- Payload/Header write result；
- final Firmware validation result。

## Host / Board Verification Direction

Host Test：

```text
Parser fragmentation / SOH / STX / CRC / complement
Block 0 bounded parsing
normal Receiver state flow
duplicate / sequence / retry / timeout / cancel
Sink failure before ACK
Firmware Storage address/boundary translation
```

Hardware：

```text
Tera Term -> STM32 Ymodem -> Slot B
interruption/cancel recovery
second transfer after failure
final firmware_storage_validate_image() == VALID
```

Exact packet corruption may remain Host Test evidence if Tera Term cannot inject it deterministically.

## Explicitly Out Of Scope

- OTA Service；
- inactive slot selection；
- EEPROM PENDING；
- Reset request；
- FreeRTOS background OTA concurrency；
- Bootloader install；
- Trial / Confirm / Rollback；
- Ymodem Sender on STM32；
- generic Transport/Protocol Manager。

## Implementation Output

已完成并提交的施工批次：

```text
090e2d2 tools: add Tera Term ymodem sender entry
417aa00 feat: add firmware storage write paths
12f063f feat: add ymodem packet parser
e6edc6f style: align ymodem public comments
d9afae4 feat: add ymodem receiver state machine
3d98d19 feat: add s05 ymodem flash sink
00cbd3a test: integrate s05 ymodem board endpoint
```

代码验证结果与完整命令证据见：

`04_Test/Reports/Stages/S05_UART_Ymodem/verification.md`

正式 Review：

`00_Project/03_Stages/S05_UART_Ymodem/review.md`

当前已通过 Parser、Receiver、Firmware Storage Write、Flash Sink Host Test，S04 相关回归，Keil 正常/清理构建，以及 `build_app.bat`、`flash_app.bat`、`rtt_capture.bat` 工具链 Smoke Test。

历史中间诊断曾记录 S05 专用入口初始化成功但独立 TTL `COM9` 未收到数据；该状态已被后续 CH340 `COM10` 最终板测取代，不再代表当前阶段状态。

历史下一步为核对独立 TTL 线路；后续已确认实际问题为 Sender 与 MCU 复位之间的调用顺序竞态，并完成 Tera Term 正常传输、超时中止和恢复传输。

## 2026-09-15 RTOS Thread Isolation Update

S05 Board Test has been changed to create a dedicated `s05Ymodem` thread. `appSystem` only starts the test thread and remains alive; the dedicated thread binds itself as the `service_uart` owner and performs the complete YMODEM/Storage/Slot B flow. The UART RingBuffer still has one task-context consumer.

Code verification: Keil build PASS with 0 errors and 0 warnings; YMODEM Host Tests PASS 24/24; firmware packer tests PASS 2/2.

Hardware verification is still PENDING. The next test uses only the CH340 serial path. Resolve its current COM number with `ymodem_sender devices --json`; do not use the unconnected J-Link CDC serial port.

最新 CH340 `COM10` 板测已增加并完成一次阻塞式 RX 探针：在 RX DMA 和 `service_uart` 启动前，`platform_uart_read()` 等待主机发送 `0xA5`，结果为 `result=2`、`read=0`。同时标准线程隔离版本仍记录 `tx_bytes=11`、`rx_bytes=0`，Sender 返回 `exit_code=4`。因此当前最早失败边界位于 CH340 TX 到 USART1 `PA10`/HAL 接收入口之间；临时探针已移除，最终固件已恢复为标准线程隔离版本并重新烧录。

最新运行时寄存器快照已确认：PA9/PA10 的 `MODER=Alternate Function`、`AFR[1]=AF7`，USART1/APB2 和 DMA2/AHB1 时钟均已打开，`USART1->SR=0xC0` 仅有 TXE/TC、没有 RXNE。主机发送 `0xA5` 后仍为 `rx_events=0`、`rx_bytes=0`，因此下一步应直接测量 CH340 TX 到 MCU PA10 的实际电平；当前没有证据支持继续修改 RingBuffer 或 YMODEM。

用户随后短接 MCU PA9→PA10，并完成一次 HAL 阻塞式本地回环：TX/RX status 均为 0，收到 `0x55`；标准 DMA 会话随后统计 `rx_events=11`、`rx_bytes=11`、`buffered/read=11/11`、`dropped/errors=0/0`，DMA2 Stream2 `NDTR` 从 256 变化到 245。该证据确认 USART1、DMA、UART Service、RingBuffer 和 S05 专用线程工作正常；临时 HAL 回环探针已移除并重新编译/烧录。当前只剩 CH340 外部 TX/RX 物理路径待确认。

## 2026-09-15 当前工程与参考工程 A/B 复测

开发板重新上电后，当前工程重新烧录并运行。CH340 当前端口为 `COM10`：发送单字节 `0xA5` 后，当前工程仍为 `rx_events=0`、`rx_bytes=0`；YMODEM Sender 等待初始 `C` 超时，返回 `exit_code=4`。

随后刷入参考工程 `E:\my_project_2026\Git_test\stm32f4_DMA_UART_ring_RTOS\RTT_elog_DMA_UART_ring_project\MDK-ARM\Objects\RTT_elog_DMA_UART_ring_project.hex`。参考工程 RTT 输出 `communication runtime started`，向其定义的串口命令发送 `HELP\r\n` 后，COM10 仍无响应。测试完成后已重新刷回当前工程固件。

该 A/B 测试未形成任何一个工程的 CH340 收发闭环，因此不能把失败归因于当前工程新增的 YMODEM。下一步保持代码不变，继续核对 CH340 与 USART1 `PA9/PA10` 的 TX/RX 交叉、实际引脚、共地和电平路径。

## Deferred Cross-stage Regression

S04 remains:

```text
Reset Persistence       PENDING / DEFERRED
Power-cycle Persistence PENDING / DEFERRED
```

Not an S05 blocker; must be closed before S07 closure.

## Implementation Entry

Implementation Role 已完成当前可执行的计划内实现和代码验证；下游 Verification/Review 需要先取得 USART1 真实串口物理链路，才能完成 S05 的硬件闭环门禁。

## 2026-09-15 自动化链路最终复测结论

同一块 STM32F411CE、同一 CH340 `COM10`、115200 8N1、同一当前 S05 固件下，Python Sender 和 Tera Term 均完成了 `.img` 传输。两次测试均采用：

```text
关闭串口助手
→ 上位机 Sender/Tera Term 先打开 COM10 并等待 C
→ 使用 flash_app.bat 通过 J-Link 烧录并复位目标板
→ 接收 C
→ Block 0 / ACK / C
→ Block 1...N / ACK
→ EOT NAK / EOT ACK / 空 Block 0 ACK
```

必须发送：

```text
06_Output\Packages\OTA_APP_s04_v1.1.0.img
```

不能直接发送原始：

```text
06_Output\Firmware\OTA_APP_s04_test.bin
```

原始 `.bin` 会在第一个数据 Block 的 Firmware Image Header 校验处返回 `PLATFORM_ERR_INVALID_PARAM`，表现为 Sender 卡在 Block 1 重试；这不是串口硬件、Python 字节编码或 YMODEM Block 1 格式问题。

实测结果：

```text
Python Sender：exit_code=0，55 blocks，55884 bytes，0 retries
Tera Term：宏 exit code=0
RTT：progress=55884/55884
```

后续所有自动化调用必须先执行 `ymodem_sender devices --json` 确认 CH340 端口，不使用未接线的 J-Link CDC `COM3`；Tera Term 必须在目标板烧录或复位之前打开串口。最终 Slot B 校验、超时中止和恢复传输已在本交接末尾补齐。

调用顺序错误的表现已经确认：若先烧录/复位、后启动 Sender，板端初始 `C` 会在 Sender 进入读取状态前发出，Sender 返回 `exit_code=4`，并非硬件或字节编码错误。当前自动化入口必须保持“打开串口并等待 `C` → 再烧录/复位”的顺序。

## 2026-09-15 S05 Closure

默认板测发送端固定为 Tera Term 5，入口为 `05_Tools/Scripts/send_ymodem.bat`。最终正常传输结果：宏返回 0；RTT 记录 `state=6`、`received=55884`、`bytes=55884/55884`、`retry=0`、`dropped=0`、`header_commit=1`、`Slot B validation=2`，最终会话结果 PASS。

中止回归在接收 `12288/55884` 字节后触发超时：`state=8`、`timeout=11`、`retry=10`、`header_commit=0`；随后重新烧录/复位并再次使用 Tera Term 宏传输，Slot B 校验再次通过。完整证据见验证报告；S04 Reset Persistence 与 Power-cycle Persistence 仍为跨阶段延期回归项。
