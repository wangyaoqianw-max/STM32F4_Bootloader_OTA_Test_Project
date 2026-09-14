# S04 Firmware Image Storage Review

## Metadata

- Stage: `S04_Firmware_Image_Storage`
- Status: `SCOPED_REVIEW_COMPLETE`
- Branch: `main`
- Baseline Commit: `245cd3ee550a2c2cc016e6a197609d712ef1e893`
- Design Commit: `e701910a0452952c632ad8352c6973eedf06b283`
- Implementation Plan Commit: `bc5360fa40188c189a9e19b91a29ad5d266d8220`
- Implementation Commit: `647f32f`
- Verification Commit: `72a7403`
- Review Commit: `f2b6ed9`
- Updated At: `2026-09-14`

## Review Entry Condition

本次按 Project Owner 要求执行范围化 Review。由于 Reset Persistence 与
Power-cycle Persistence 明确暂缓，Stage 状态仍保持 `READY_FOR_VERIFICATION`；
本文件的结论只覆盖实现、代码、接口、架构、文档和已完成验证证据，不构成阶段关闭。

Review 前必须读取：

1. `00_Project/01_Requirements/项目需求V1.md`
2. `00_Project/03_Stages/S04_Firmware_Image_Storage/design.md`
3. `00_Project/03_Stages/S04_Firmware_Image_Storage/implementation_plan.md`
4. `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`
5. `04_Test/Reports/Stages/S04_Firmware_Image_Storage/verification.md`
6. S04 全部 Implementation commits / diff

## Review Focus

正式 Review 至少检查：

- Slot A/B 地址、大小、Header Sector 与 Payload Offset 是否严格符合冻结设计；
- Header V1 是否使用 fixed-offset little-endian encode/decode，而不是直接持久化 C struct layout；
- Firmware Version compare 与 Header / Payload CRC 覆盖范围是否正确；
- CRC-8/SMBUS、CRC-16/XMODEM、CRC-32/ISO-HDLC 参数是否与标准向量一致；
- CRC one-shot / streaming 是否一致；
- Metadata 双副本、sequence、CRC、commit marker 是否具备单副本损坏与提交中断恢复能力；
- Metadata / Image authority boundary 是否保持清晰；
- Image Invalid 与 Validation I/O Failure 是否严格区分；
- `firmware_storage_validate_image()` 是否只读、不隐式提交 Metadata；
- S04 UART path 是否仅为 test-only raw injection，没有提前实现 Ymodem / OTA Service；
- Slot B 是否采用 Payload-first / Header-last commit；
- UART interrupted / data loss / CRC mismatch 后是否保持 Header 未提交；
- PC `pack_firmware.py` 与 MCU/C Host decoder 是否二进制兼容；
- `03_Firmware/Shared` 是否仍遵守“已有两个真实消费者后再抽取”的约束；
- destructive Board Test 是否已退出 production runtime 与正式 Keil target；
- Keil normal build / clean rebuild、Host Test、Reset / Power-cycle 和真实 RTT 证据是否完整；
- 是否出现范围外 Device Manager / 通用 NVM / Security / Bootloader 安装等扩张。

## Review Evidence

### Implementation and Architecture

- Slot A/B 地址、Header Sector、Payload Offset 和 Payload Capacity 与冻结设计一致。
- Header、Metadata 均使用 fixed-offset little-endian 编解码，没有直接持久化 C struct layout。
- CRC 三种变体的参数、one-shot/streaming 接口和标准向量一致；Header CRC 与 Payload CRC
  覆盖范围明确。
- Metadata 双副本、sequence 回绕比较、commit marker 和旧副本恢复边界清晰；Storage
  Service 负责 I/O 编排，Image/Metadata codec 保持纯格式职责。
- `firmware_storage_validate_image()` 只读，I/O 失败返回底层错误并保持 validation
  为 `UNKNOWN`，不会隐式修改 Metadata。
- S04 UART 代码只保留在 `04_Test/Board`；正式 Application/Keil target 未包含破坏性板测。

### Verification Evidence

| 项目 | 结果 |
| --- | --- |
| CRC Host Test | PASS |
| Firmware Format Host Test | PASS（补充 `impl_board` include path 与 `firmware_metadata.c`） |
| Firmware Storage Host Test | PASS |
| Python pack tool unittest | 2 tests PASS |
| Python 生成镜像与 C decoder 兼容性 | PASS |
| Keil normal/clean rebuild | PASS，0 errors；既有 warning 已记录 |
| J-Link/RTT/Application tool smoke | PASS，详见 verification.md |
| Reset Persistence | PENDING，按用户要求本轮跳过 |
| Power-cycle Persistence | PENDING，按用户要求本轮跳过 |

审查中发现阶段计划中的 Format Host Test 示例缺少 `04_Impl/impl_board` include path
和 `firmware_metadata.c`；已同步修正 `implementation_plan.md` 和 Host README，并使用修正
后的命令重新验证通过。

## Review Decision

当前：`PASS`（仅限实现/代码/文档审查范围）

阶段关闭：`DEFERRED`。由于两项持久性场景尚未执行，Stage 仍为
`READY_FOR_VERIFICATION`，不得转换为 `CLOSED`。

允许的最终结论：

```text
PASS
CHANGES_REQUESTED
BLOCKED
```

本次存在独立 Verification 报告和主流程证据；`PASS` 不覆盖上述两项 PENDING
板测，也不替代后续正式阶段关闭审查。
