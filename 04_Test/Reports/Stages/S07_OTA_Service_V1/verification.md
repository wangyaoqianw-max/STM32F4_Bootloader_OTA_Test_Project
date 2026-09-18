# S07 OTA Service V1 Verification

## Metadata

- Stage: `S07_OTA_Service_V1`
- Status: `IN_PROGRESS / HARDWARE_ACCEPTANCE_PENDING`
- Branch: `main`
- Design Commit: `a5c4c1b`
- Implementation Commits: `9adba52`, `13d67ef`, `6ac6a49`, `bb0f96b`, `7536cf0`, `29c1f35`, `e0b8b8f`, `b9c6198`, `4e5a8e5`, `ae26497`, `a8045d2`, `23ac3db`, `2ab34f1`
- Verification Commit: `TBD (current board-acceptance documentation commit)`
- Verification Date: `2026-09-18`
- Coding Standard Review: `PASS for S07 changes`

## Scope

本报告覆盖 S07 已完成的代码、Host、Toolkit、Keil Build、正式 Application 恢复，以及 2026-09-18 在 CH340 `COM9` 上完成的真实 PA0、YMODEM、Metadata 和失败路径板级证据。LCD 文案是否按验收条件被肉眼完整确认，仍单独保留为待确认项，不用 RTT 或代码状态替代视觉验收。

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
| Factory baseline | PASS | 临时 S07 provisioning board test 严格校验后恢复：Slot A `1.0.0` VALID、Slot B EMPTY、confirmed A、pending NONE、upgrade NONE；RTT 记录 `known PENDING reset PASS source=1 committed=2 baseline=1.0.0` |
| CH340 YMODEM transfer on COM9 | PASS | 真实 PA0 启动；Python sender 在 `COM9/115200` 完成 `77468` bytes / `76` blocks / exit `0` / retries `2`，RTT/GDB 最终为 Slot B `READY_TO_INSTALL`、`77468/77468` |
| Second confirm / reset request | PASS | 用户真实按下第二次 PA0；复位后 RTT 为正常 Application/LCD 启动日志，GDB 回读 Metadata `confirmed=0/pending=1/slotA=1/slotB=1/upgrade=1/sequence=10`。复位释放时产生的按键再次被新一轮 Service 采到并显示 `FAILED/INVALID_STATE`，不改变已提交 PENDING |
| Durable Metadata PENDING | PASS | 第二次真实 PA0 后 GDB 只读回读 `pendingSlot=B`、`upgradeState=PENDING`、Slot A/B VALID、sequence `10`；未执行 Bootloader consume 或 S09/S10 安装 |
| S06 foreground / LCD full flow | PARTIAL | 多轮 RTT 均确认 foreground/Application、displayTask、Display SPI/init/start、初始渲染和 backlight 成功；LCD `RECEIVING → VERIFYING → READY/FAILED` 肉眼确认待用户回报 |
| Physical KEY_1 PA0 start/confirm | PASS | 真实 PA0 启动 YMODEM；真实第二次 PA0 触发 PENDING 提交并复位。不是 GDB 模拟按键 |
| READY reset before PENDING | PASS | 真实传输完成后 GDB 为 `state=4`、`received=expected=77468`；Toolkit GDB `monitor reset → continue` 后 RTT 捕获正常启动，GDB 为 `state=0 IDLE`，没有 PENDING 提交 |
| Interrupted transfer | PASS | 真实启动后 Ctrl+C 中止发送；GDB 为 `state=2 RECEIVING`、target B、`received=0`，Metadata 为 Slot A VALID / Slot B INVALID / pending NONE / upgrade NONE / sequence `16` |
| Bad CRC / invalid image | PASS | 临时镜像仅篡改 Payload 字节、Header 保持不变；真实 COM9 完整发送后 GDB 为 `state=7 FAILED`、`error=3`（校验失败映射）、Slot B INVALID、pending NONE、upgrade NONE、sequence `17` |
| Duplicate KEY during transfer/verify/commit | PASS | 用户在真实接收过程中第二次按 PA0；完整发送后 GDB 仍为 `state=4 READY_TO_INSTALL`，pending NONE、upgrade NONE、Slot A/B VALID、sequence `19`，未误触发 PENDING |
| SPI/I2C logic capture | NOT_REQUIRED_THIS_RUN | 当前已有代码、RTT 和 EEPROM 结果；后续若失败定位需要，再复用 `toolkit logic` |

## Evidence Details

关键 COM9 Sender 结果：

```text
{"blocks_sent":76,"bytes_sent":77468,"exit_code":0,"ok":true,"phase":"complete","port":"COM9","retries":2}
```

关键 RTT 结果：

```text
state=4 event=0 error=0 target=1 progress=100 expected=77468 received=77468
metadata_confirmed=0 pending=255 slotA=1 slotB=1 upgrade=0 sequence=19
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
硬件待验收：LCD 全流程肉眼确认、最终 Project Owner 确认
阶段状态：IN_PROGRESS，不得标记 CLOSED / PASS
```
