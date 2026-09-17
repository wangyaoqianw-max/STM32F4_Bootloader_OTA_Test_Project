# S07 OTA Service V1 Verification

## Metadata

- Stage: `S07_OTA_Service_V1`
- Status: `IN_PROGRESS / HARDWARE_ACCEPTANCE_PENDING`
- Branch: `main`
- Design Commit: `a5c4c1b`
- Implementation Commits: `9adba52`, `13d67ef`, `6ac6a49`, `bb0f96b`, `7536cf0`, `29c1f35`, `e0b8b8f`, `b9c6198`, `4e5a8e5`, `ae26497`, `a8045d2`, `23ac3db`, `2ab34f1`
- Verification Commit: `e60273f`
- Verification Date: `2026-09-17`
- Coding Standard Review: `PASS for S07 changes`

## Scope

本报告覆盖 S07 已完成的代码、Host、Toolkit、Keil Build、正式 Application 恢复，以及当前可获得的 COM9 板级证据。真实 PA0 物理按键和失败路径板测按用户安排留待后续，不用代码验证替代硬件结论。

S07 仍严格止于：

```text
下载 → 非确认 External Slot → 镜像验证 → READY_TO_INSTALL
→ 第二次确认 → Metadata PENDING → Reset request
```

Internal Flash Installation、Trial、Confirm 和 Rollback execution 不属于本阶段。

## Code Verification

| Area | Result | Evidence |
| --- | --- | --- |
| Metadata V1/V2 and dual-copy atomicity | PASS | `s07_metadata_host_test.c`、S04 persistence/storage tests |
| Production dynamic sink / Header-last | PASS | `s07_ota_firmware_sink_host_test.c` |
| OTA Service state machine | PASS | `s07_service_ota_host_test.c`，含 abort/retry、PENDING 和 invalid transition |
| IRQ / BSP Key / Worker / Display contracts | PASS | S07 Host contract tests，`gcc -std=c99 -Wall -Wextra -Werror` |
| Existing S04/S05 regression | PASS | Firmware image/storage, storage write, YMODEM parser/receiver and historical sink tests |
| Python / Toolkit contracts | PASS | Firmware pack 2、Python YMODEM 24、S04 persistence 15 及 Toolkit contracts |
| Production dependency isolation | PASS | `03_Firmware/Application/OTA_APP` 无 S05 Board Test sink 依赖 |
| Keil Application link | PASS | `OTA_APP.build_log.htm`: `0 Error(s), 13 Warning(s)`；警告来自既有 GPIO/UART/FreeRTOS 边界检查，未引入 S07 链接错误 |
| Formal Flash / RTT smoke | PASS | `05_Tools\toolkit.bat flash run`、`05_Tools\toolkit.bat rtt 5` 返回 PASS；仅作为正式镜像恢复和启动冒烟 |
| Diff / source style | PASS | 已按工程 C 规范复核 S07 新增文件；`git diff --check` PASS |

说明：统一 Toolkit 将 Keil 有警告结果映射为非零退出码，因此 `toolkit.bat build` 在本次全量重建中报告 WARN；Keil 产物日志明确为 0 errors。没有为消除无关历史警告扩大本阶段范围。

## Hardware Verification

| Acceptance item | Result | Evidence / limitation |
| --- | --- | --- |
| Factory baseline | PASS | Task 8 临时板测：Slot A `1.0.0` VALID、Metadata confirmed A、pending NONE、upgrade NONE；COM9 传输 67664 bytes / 67 blocks / retries 2 |
| CH340 YMODEM transfer on COM9 | PASS (simulated start path) | Python sender：77456 bytes / 76 blocks / exit 0 / retries 2；sender 完成 EOT/Block 0；RTT `READY_TO_INSTALL`, target Slot B, `dropped=0`, `errors=0` |
| Second confirm / reset request | PASS (GDB-simulated BSP EXTI call) | 通过 GDB 调用公开 BSP EXTI forwarding，Session 进入 reset 流程；GDB clean continue/disconnect，随后 RTT 为正常重启日志。不是物理 PA0 按键证据 |
| Durable Metadata PENDING | PASS (direct EEPROM read) | 临时只读 S04 板测恢复并回读：copy A/B valid=1、selected copy=1、sequence=6、confirmed A、pending B、Slot A/B VALID、upgrade `PENDING`、confirmed version `1.0.0`；临时代码已完整恢复 |
| S06 foreground / LCD full flow | PENDING | 有启动和 Display init RTT 冒烟；未完成人工 LCD 全流程观察 |
| Physical KEY_1 PA0 start/confirm | PENDING | 本轮未进行真实按键操作，不能以 GDB 模拟替代 |
| READY reset before PENDING | PENDING | 尚未完成真实板上的中途复位验收 |
| Interrupted transfer | PENDING | 尚未完成本阶段独立的真实失败路径板测 |
| Bad CRC / invalid image | PENDING | 尚未完成真实板测 |
| Duplicate KEY during transfer/verify/commit | PENDING | 尚未完成真实板测 |
| SPI/I2C logic capture | NOT_REQUIRED_THIS_RUN | 当前已有代码、RTT 和 EEPROM 结果；后续若失败定位需要，再复用 `toolkit logic` |

## Evidence Details

关键 COM9 Sender 结果：

```text
{"blocks_sent":76,"bytes_sent":77456,"exit_code":0,"ok":true,"phase":"complete","port":"COM9","retries":2}
```

关键 RTT 结果：

```text
OTA state=4 event=0 target=1 progress=77456/77456 error=0
OTA UART rx_events=703 rx_bytes=79502 buffered=79502 read=79502 dropped=0 errors=0 tx_bytes=87
```

Task 10 期间发现并修复了一个真实生产问题：进度达到 100% 后，每个剩余 payload 字节仍重复发布终止进度事件，可能填满 Display Queue，导致尾部 Block 0 无法完成。修复为同一 Session 只发布一次 `100%`，提交 `2ab34f1`；Service Host Test、Keil link 和 COM9 完整传输随后通过。

Metadata 原始回读：

```text
[S04-PERSIST] SNAPSHOT image_validation=2 image_size=77392 image_crc=0x76FB27BF metadata_a_valid=1 metadata_b_valid=1 selected_copy=1 sequence=6 confirmed_slot=0 pending_slot=1 slot_a_state=1 slot_b_state=1 upgrade_state=1 confirmed_version=1.0.0
```

该回读证明 S07 的 `PENDING` 数据已按双副本/sequence/CRC/commit marker 规则持久化；它不表示 Bootloader 已消费该状态，也不表示 S09/S10 已实现。

## Code / Hardware Result

```text
代码验证：PASS
硬件验证：PARTIAL
硬件待验收：真实 PA0 按键、LCD 全流程观察、中途复位、interrupted transfer、bad CRC、重复按键和最终 Project Owner 确认
阶段状态：IN_PROGRESS，不得标记 CLOSED / PASS
```
