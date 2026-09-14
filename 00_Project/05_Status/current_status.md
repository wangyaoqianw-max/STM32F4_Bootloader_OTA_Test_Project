# Current Project Status

## Context Metadata

- Active Stage: `S05_UART_Ymodem`
- Status: `READY_FOR_IMPLEMENTATION`
- Branch: `codex/s05-uart-ymodem`
- Baseline Commit: `8173a3da2c174294350e47d8e889cf22b9066e23`
- Design Commit: `400b8b4f4cb50672faea2c332379466bb637b3a6`
- Implementation Plan Commit: `a5417e47dc86546176ec87dff5f6c58ddfb14260`
- Design Approval Commit: `b62bdad9d1279158d4925a417ab5a0e1b4668db3`
- Implementation Commit: `Not created yet`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Last Closed Stage: `S04_Firmware_Image_Storage`
- Last Closed Stage Status: `CLOSED`
- Current Role: `Implementation Role / Ready to execute Task 1`
- Updated At: `2026-09-14`

## Current Goal

按照已获 Project Owner 批准的 `S05_UART_Ymodem` 设计和实施计划，建立 Application 侧可复用的 Ymodem Receiver 文件传输能力，并通过 Tera Term 5、现有 UART Service、Firmware Storage 和 Build/Flash/RTT 工具链形成真实板测闭环。

阶段已经通过设计门禁：

```text
DRAFT
→ DESIGN_APPROVED
→ READY_FOR_IMPLEMENTATION
```

允许开始计划内生产代码、Host Test、工具脚本和板测资产施工；设计冲突或范围扩张时必须停止并回到确认流程。

## Formal S05 Documents

- Design: `00_Project/03_Stages/S05_UART_Ymodem/design.md`
- Implementation Plan: `00_Project/03_Stages/S05_UART_Ymodem/implementation_plan.md`
- Handoff: `00_Project/03_Stages/S05_UART_Ymodem/handoff.md`

## Approved Frozen Direction

```text
Tera Term Ymodem Sender
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

关键边界：

- S05 实现 Receiver / Single File，不引入第三方黑盒 Ymodem 库；
- Ymodem 不直接依赖 HAL UART 或 W25Q64；
- Packet CRC 复用 CRC-16/XMODEM；
- 静态策略进入 `00_Config/ymodem_config.h`；
- S04 `.img` 是紧凑 `[64B Header][Payload]`，但 Slot Payload 固定从 `+0x1000` 开始；
- S05 Flash Sink 必须缓存 Header、流式写 Payload、最后提交 Header；
- Firmware Storage 增加 `write_payload()` / `write_header()` 合同感知写能力；
- S05 不更新 EEPROM Metadata、不设置 PENDING、不选择 inactive slot、不 Reset。

## PC / Board Tooling

第一版 Reference Sender：Tera Term 5 YMODEM。

本机确认 Tera Term executable：

```text
E:\APP\ProgramFile\tera_term\teraterm5\ttermpro.exe
```

该路径只允许放入被 Git 忽略的 `05_Tools/Config/toolchain.local.bat`。

板测继续复用：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Scripts/flash_app.bat
05_Tools/Scripts/rtt_capture.bat
05_Tools/Scripts/run_app_cycle.bat
```

计划新增：

```text
05_Tools/TeraTerm/send_ymodem.ttl
05_Tools/Scripts/send_ymodem.bat
```

板测主链：

```text
Build → Flash → Reset/Run → RTT Capture
→ Tera Term Ymodem Send
→ RTT protocol/storage evidence
→ firmware_storage_validate_image(Slot B)
```

## Deferred Regression

S04 remains:

```text
Reset Persistence       PENDING / DEFERRED
Power-cycle Persistence PENDING / DEFERRED
```

Not an S05 blocker; must be completed before S07 closure.

## Next Action

Implementation Role reads the approved design / plan / handoff and begins:

```text
Task 1: Tera Term Ymodem sender automation entry
```

Task 1 first verifies local Tera Term macro invocation and parameter handling. A Ymodem transfer timeout before the MCU Receiver exists is expected and must not be reported as transfer PASS.

## Blockers

No known technical blocker.
