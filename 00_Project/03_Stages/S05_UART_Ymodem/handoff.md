# S05 UART Ymodem Handoff

## Metadata

- Stage: `S05_UART_Ymodem`
- Status: `DRAFT`
- Branch: `codex/s05-uart-ymodem`
- Baseline Commit: `8173a3da2c174294350e47d8e889cf22b9066e23`
- Design Commit: `400b8b4f4cb50672faea2c332379466bb637b3a6`
- Implementation Plan Commit: `a5417e47dc86546176ec87dff5f6c58ddfb14260`
- Implementation Commit: `Not created yet`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Updated At: `2026-09-14`

## Current Role

Design Role。

Design 和 Implementation Plan 已落盘，但尚未经过 Project Owner 正式批准，因此不得开始生产代码施工。

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

计划新增：

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

## Deferred Cross-stage Regression

S04 remains:

```text
Reset Persistence       PENDING / DEFERRED
Power-cycle Persistence PENDING / DEFERRED
```

Not an S05 blocker; must be closed before S07 closure.

## Next Action

Project Owner reviews:

1. `design.md`；
2. `implementation_plan.md`；
3. this handoff。

If approved, synchronize status to `READY_FOR_IMPLEMENTATION` and begin Task 1. Until then remain `DRAFT`.
