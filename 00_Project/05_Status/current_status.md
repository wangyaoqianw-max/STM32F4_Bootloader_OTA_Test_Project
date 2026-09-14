# Current Project Status

## Context Metadata

- Active Stage: `S04_Firmware_Image_Storage`
- Status: `CLOSED`
- Branch: `main`
- Baseline Commit: `245cd3ee550a2c2cc016e6a197609d712ef1e893`
- Design Commit: `e701910a0452952c632ad8352c6973eedf06b283`
- Implementation Plan Commit: `bc5360fa40188c189a9e19b91a29ad5d266d8220`
- Implementation Commit: `647f32f`
- Toolchain Commit: `1f756f0`
- Verification Commit: `72a7403`
- Scoped Review Commit: `f2b6ed9`
- Last Closed Stage: `S04_Firmware_Image_Storage`
- Last Closed Stage Status: `CLOSED`
- Next Planned Stage: `S05_UART_Ymodem`
- Current Role: `Stage Closed / Ready for S05 Design`
- Updated At: `2026-09-14`

## Current Goal

`S04_Firmware_Image_Storage` 已完成实现、验证、Review 和 Project Owner 最终关闭决定。

阶段状态：

```text
READY_FOR_IMPLEMENTATION
→ READY_FOR_VERIFICATION
→ SCOPED REVIEW
→ OWNER SCOPE DECISION
→ CLOSED
```

S04 已成为 S05 的正式前置阶段。

当前没有启动 S05 正式 Stage 文档；下一次对话可从 `S05_UART_Ymodem` Design Role 开始。

## S04 Delivered Capabilities

```text
Firmware Image Contract
+ A/B Slot Layout
+ Header V1
+ Firmware Version
+ CRC Common
+ Metadata Double Copy
+ Firmware Storage Service
+ PC pack_firmware.py
+ UART Test-only Slot B Injection
+ RTT Board Evidence
+ Build / Flash / RTT Automation
```

核心存储合同继续作为后续阶段稳定输入，不在 S05 中随意重定义。

## Deferred Regression

以下两项没有实际执行，不得标记为 PASS：

```text
Reset Persistence       PENDING / DEFERRED
Power-cycle Persistence PENDING / DEFERRED
```

Project Owner 已批准将其从 S04 关闭阻塞项调整为跨阶段延期回归项。

硬门禁：

```text
Must be completed before:
S07_OTA_Service_V1 stage closure
```

S05 / S06 不被这两项阻塞；可以在任意更早的合适板测窗口补测。

## Formal S04 Documents

- Design: `00_Project/03_Stages/S04_Firmware_Image_Storage/design.md`
- Implementation Plan: `00_Project/03_Stages/S04_Firmware_Image_Storage/implementation_plan.md`
- Handoff: `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`
- Review: `00_Project/03_Stages/S04_Firmware_Image_Storage/review.md`
- Verification: `04_Test/Reports/Stages/S04_Firmware_Image_Storage/verification.md`

## Stable Downstream Inputs

S05 可直接复用：

- existing `service_uart`；
- CRC-16/XMODEM；
- CRC-32/ISO-HDLC；
- Firmware Header V1 / Version / Image Size / CRC contract；
- W25Q64 Slot A/B layout；
- `service_firmware` / `firmware_storage`；
- `pack_firmware.py`；
- `build_app.bat`；
- `flash_app.bat`；
- `rtt_capture.bat`；
- `run_app_cycle.bat`。

## Next Action

启动新的设计对话：

```text
Stage: S05_UART_Ymodem
Initial Status: DRAFT
Role: Design Role
```

先讨论设计，不直接施工。

优先需要冻结：

1. Ymodem 参考协议来源；
2. Ymodem 模块所属层级与边界；
3. packet / block state machine；
4. CRC-16/XMODEM 复用；
5. timeout / retry / cancel；
6. `.bin` vs S04 `.img` transport format；
7. Ymodem 与 `firmware_storage` 的职责边界；
8. PC 端工具与板测验收。

## Blockers

S04 无关闭阻塞项。

S05 尚未创建正式 Stage，因此当前仅等待下一轮 Design Discussion。
