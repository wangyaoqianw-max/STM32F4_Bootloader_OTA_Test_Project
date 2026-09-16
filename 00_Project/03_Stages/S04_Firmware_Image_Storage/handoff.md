# S04 Firmware Image Storage Handoff

## Metadata

- Stage: `S04_Firmware_Image_Storage`
- Status: `CLOSED`
- Branch: `main`
- Baseline Commit: `245cd3ee550a2c2cc016e6a197609d712ef1e893`
- Design Commit: `e701910a0452952c632ad8352c6973eedf06b283`
- Implementation Plan Commit: `bc5360fa40188c189a9e19b91a29ad5d266d8220`
- Plan Owner Acceptance: `PASS`
- Implementation Commit: `647f32f`
- Toolchain Commit: `1f756f0`
- Verification Commit: `72a7403`
- Scoped Review Commit: `f2b6ed9`
- Supplementary Regression Commit: `6f2fad5`
- Final Closure Decision: `PASS`
- Updated At: `2026-09-16`

## Stage Result

S04 已完成并正式关闭。

已完成能力：

```text
Firmware Image Contract
+ A/B Slot Layout
+ 64 Byte Header V1
+ Firmware Version
+ CRC Common
+ AT24C02 Metadata Double Copy
+ Firmware Storage Service
+ PC Firmware Pack Tool
+ UART Test-only Slot B Injection
+ RTT / EasyLogger Board Evidence
+ Build / Flash / RTT Local Toolchain
```

实现、Host Test、Keil Build、主流程真实板测和 Review 均通过。

## Frozen Contracts For Downstream Stages

### W25Q64 Slot Layout

```text
Slot A: 0x000000 ~ 0x07FFFF  (512 KiB)
Slot B: 0x080000 ~ 0x0FFFFF  (512 KiB)
Reserved: 0x100000 ~ 0x7FFFFF
```

Per Slot：

```text
+0x0000 ~ +0x0FFF : Header Sector
+0x1000 ~          : Firmware Payload
Payload Capacity  : 508 KiB
```

### Firmware Header V1

- 64 Byte；
- little-endian；
- Magic `0x4D495746` (`FWIM`)；
- Format Version = 1；
- Version = major/minor/patch/reserved；
- image_size；
- payload_crc32；
- header_crc32；
- Header CRC covers bytes `0x00~0x3B`；
- Payload CRC covers exactly `image_size` payload bytes；
- V1 reserved fields must be zero；
- persistent format uses fixed offsets, not raw C struct layout。

### CRC Common

```text
CRC-8/SMBUS
CRC-16/XMODEM
CRC-32/ISO-HDLC
```

支持 one-shot 和 streaming；V1 为纯软件 bitwise implementation。

### Metadata V1

AT24C02：

```text
Copy A: 0x00 ~ 0x7F
Copy B: 0x80 ~ 0xFF
```

机制：

```text
Double Copy
+ uint32_t sequence
+ CRC-32/ISO-HDLC
+ commit marker
```

S04 Slot State：

```text
EMPTY
VALID
INVALID
```

Authority boundary：

```text
W25Q64 Image Header
→ Firmware actual Version / Size / CRC

AT24C02 Metadata
→ active / confirmed / slot state / confirmed_version
```

`firmware_storage_validate_image()` 为只读验证；I/O failure 与 image invalid 必须区分。

## Implementation Outputs

生产代码：

```text
03_Firmware/Application/OTA_APP/02_Service/service_common/crc/
03_Firmware/Application/OTA_APP/02_Service/service_firmware/
```

Host Test：

```text
04_Test/Host/S04_Firmware_Image_Storage/
```

Board Test：

```text
04_Test/Board/S04_Firmware_Image_Storage/
```

Board Test 已退出 production startup 和正式 Keil target，仅作为阶段测试资产保留在
`04_Test/Board/S04_Firmware_Image_Storage/`。如需复测，应临时重新加入 Keil target 并启用测试入口；提交态不启用该入口。

Firmware pack tool：

```text
05_Tools/Firmware/pack_firmware.py
```

## Verification Summary

已验证：

- CRC 标准向量和 streaming；
- Header fixed-offset / little-endian / CRC；
- Metadata fixed-offset / CRC / commit marker / sequence wrap-around；
- Metadata 双副本选择和单副本恢复；
- Firmware Storage Host Stub；
- Python pack tool 与 C decoder 合同一致；
- Keil normal build / clean rebuild；
- Slot B Firmware 主流程真实板测；
- Payload streaming CRC；
- Header-last commit；
- 完整镜像回读验证；
- Metadata 双提交和破坏最新副本后的恢复；
- J-Link Flash / RTT automation smoke test。

详细证据：

`04_Test/Reports/Stages/S04_Firmware_Image_Storage/verification.md`

## Persistence Regression

以下两项跨阶段回归已于 2026-09-16 完成真实硬件验证：

```text
Reset Persistence       PASS
Power-cycle Persistence PASS
```

Reset 测试通过 GDB `monitor reset → continue& → disconnect → quit` 验证，复位前后快照一致。
Power-cycle 测试由操作者实际断电/上电，自动 RTT 监听器通过固定 RTT 地址、无数据 watchdog 和
1/2/4 秒退避重连捕获上电启动标志，并要求启动标志之后出现新快照，事件前后快照一致。

Power-cycle 证据日志：

```text
06_Output/Logs/S04_power_cycle_persistence.log
06_Output/Logs/S04_power_cycle_persistence_chunk_*.log
06_Output/Logs/S04_power_cycle_persistence_logger_*.log
```

本次自动化还记录并处理了 `RTT_DATA_STALLED` 和 `TARGET_UNAVAILABLE` 两类重连事件。
两项 Persistence 已完成，不再属于 S07 关闭前的待办回归。

上述两项已完成，不再作为 `PENDING` 项维护。

## Application Toolchain

统一入口：

| Tool | Purpose |
| --- | --- |
| `05_Tools/Scripts/build_app.bat` | Keil OTA_APP Build |
| `05_Tools/Scripts/flash_app.bat` | J-Link SWD Flash + Run |
| `05_Tools/Scripts/rtt_capture.bat [seconds]` | RTT Channel 0 Capture |
| `05_Tools/Scripts/run_app_cycle.bat [seconds]` | Build → Flash → RTT |
| `05_Tools/Firmware/pack_firmware.py` | Generate Firmware Image V1 |

机器路径由被 Git 忽略的 `05_Tools/Config/toolchain.local.bat` 管理。

## S05 Handoff Input

下一阶段：

`S05_UART_Ymodem`

S05 可以直接依赖：

- existing `service_uart` DMA + RingBuffer + error/data-loss handling；
- CRC-16/XMODEM；
- S04 Firmware Image binary contract；
- W25Q64 Raw Driver；
- `service_firmware` / `firmware_storage`；
- `pack_firmware.py`；
- Build / Flash / RTT local automation。

S05 仍需正式讨论和冻结：

- Ymodem protocol source / reference；
- Ymodem module layer and ownership；
- Block 0 / SOH / STX / EOT / ACK / NAK / CAN；
- timeout / retry / cancel；
- 128 Byte / 1 KiB packet；
- whether Ymodem transports raw `.bin` or S04 `.img`；
- Ymodem 与 `firmware_storage` 的边界；
- PC-side Ymodem test tool；
- board verification cases。

S04 不提前替 S05 决定这些设计。

## Final State

```text
S04_Firmware_Image_Storage
Implementation : PASS
Verification   : PASS within closed S04 scope
Review         : PASS
Stage          : CLOSED

Persistence Regression:
- Reset Persistence       PASS
- Power-cycle Persistence PASS
```

下一次对话可以从 `S05_UART_Ymodem` Design Stage 开始。
