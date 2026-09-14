# S05 UART Ymodem Handoff

## Metadata

- Stage: `S05_UART_Ymodem`
- Status: `IN_PROGRESS`
- Branch: `codex/s05-uart-ymodem`
- Baseline Commit: `8173a3da2c174294350e47d8e889cf22b9066e23`
- Design Commit: `400b8b4f4cb50672faea2c332379466bb637b3a6`
- Implementation Plan Commit: `a5417e47dc86546176ec87dff5f6c58ddfb14260`
- Project Owner Approval: `PASS / 2026-09-14`
- Implementation Commit: `00cbd3a`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Updated At: `2026-09-14`

## Current Role

Implementation Role，计划内实现已完成；当前等待真实串口物理链路完成 Verification。

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

当前已通过 Parser、Receiver、Firmware Storage Write、Flash Sink Host Test，S04 相关回归，Keil 正常/清理构建，以及 `build_app.bat`、`flash_app.bat`、`rtt_capture.bat` 工具链 Smoke Test。

板端已确认 S05 专用入口初始化 Storage/UART 并输出 `[S05] YMODEM_READY`。使用由 `OTA_APP_s04_test.bin`（55820 Byte，版本 `1.1.0`）生成的 `OTA_APP_s04_v1.1.0.img`（55884 Byte）尝试发送时，当前 `COM3` 为 J-Link CDC UART，板端记录 `received=0` 后按超时策略退出；尚未取得 Block 0、Header-last 和 Slot B `VALID` 证据。因此阶段保持 `IN_PROGRESS`，硬件传输和后续恢复测试保持 `PENDING`。

下一步：确认 USART1 PA9/PA10 与 PC 串口的 TX/RX/GND 物理连接，重新执行真实传输；成功后补写 Verification Commit 和 Review 输入。

## Deferred Cross-stage Regression

S04 remains:

```text
Reset Persistence       PENDING / DEFERRED
Power-cycle Persistence PENDING / DEFERRED
```

Not an S05 blocker; must be closed before S07 closure.

## Implementation Entry

Implementation Role 已完成当前可执行的计划内实现和代码验证；下游 Verification/Review 需要先取得 USART1 真实串口物理链路，才能完成 S05 的硬件闭环门禁。
