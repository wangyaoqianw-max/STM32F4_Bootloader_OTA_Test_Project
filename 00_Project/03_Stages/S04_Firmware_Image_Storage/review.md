# S04 Firmware Image Storage Review

## Metadata

- Stage: `S04_Firmware_Image_Storage`
- Status: `CLOSED`
- Branch: `main`
- Baseline Commit: `245cd3ee550a2c2cc016e6a197609d712ef1e893`
- Design Commit: `e701910a0452952c632ad8352c6973eedf06b283`
- Implementation Plan Commit: `bc5360fa40188c189a9e19b91a29ad5d266d8220`
- Implementation Commit: `647f32f`
- Verification Commit: `72a7403`
- Scoped Review Commit: `f2b6ed9`
- Supplementary Regression Commit: `6f2fad5`
- Closure Decision: `PASS`
- Updated At: `2026-09-16`

## Review Entry Condition

S04 已完成实现、Host Test、Keil Build、Firmware Image 主流程真实板测、Application Toolchain 冒烟验证以及范围化 Review。

原设计中的以下两个持久性测试在阶段首次关闭时曾作为延期回归项：

- Reset Persistence；
- Power-cycle Persistence。

Project Owner 于 2026-09-16 安排补充真实板测。两项测试均已完成，结果如下：

```text
Reset Persistence       PASS
Power-cycle Persistence PASS
```

补充验证使用只读测试固件、固定 `_SEGGER_RTT` 地址和自动重连监听器；测试完成后已将临时入口从正式 Application/Keil target 移除，仅保留 `04_Test/Board` 测试资产。

## Review Focus

正式 Review 检查：

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
- Keil normal build / clean rebuild、Host Test 与真实 RTT 主流程证据是否完整；
- 是否出现范围外 Device Manager / 通用 NVM / Security / Bootloader 安装等扩张。

## Review Evidence

### Implementation and Architecture

- Slot A/B 地址、Header Sector、Payload Offset 和 Payload Capacity 与冻结设计一致。
- Header、Metadata 均使用 fixed-offset little-endian 编解码，没有直接持久化 C struct layout。
- CRC 三种变体的参数、one-shot/streaming 接口和标准向量一致；Header CRC 与 Payload CRC 覆盖范围明确。
- Metadata 双副本、sequence 回绕比较、commit marker 和旧副本恢复边界清晰；Storage Service 负责 I/O 编排，Image/Metadata codec 保持纯格式职责。
- `firmware_storage_validate_image()` 只读，I/O 失败返回底层错误并保持 validation 为 `UNKNOWN`，不会隐式修改 Metadata。
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
| Slot B Firmware 主流程实板测试 | PASS |
| Header-last commit / full re-read validation | PASS |
| Metadata 双副本提交与单副本恢复 | PASS |
| Reset Persistence | PASS；GDB `monitor reset → continue& → disconnect` 后事件前后快照一致 |
| Power-cycle Persistence | PASS；断电/上电后捕获启动标志和新快照，事件前后快照一致 |

审查中发现阶段计划中的 Format Host Test 示例缺少 `04_Impl/impl_board` include path 和 `firmware_metadata.c`；已同步修正 `implementation_plan.md` 和 Host README，并使用修正后的命令重新验证通过。

## Project Owner Scope Decision

本次补充验证关闭了原先的延期回归项：

```text
Before:
Cross-stage deferred regression item

After:
Completed cross-stage regression item
```

调整理由：

- S04 的 Firmware Image / Slot / CRC / Metadata 存储合同和主链路已完成验证；
- Reset / Power-cycle 后持久化字段保持一致，补充证据已落盘；
- 临时板测代码已退出正式 Keil target，避免影响生产 Application 构建。

## Final Review Decision

Review：`PASS`

Stage：`CLOSED`

```text
Implementation       PASS
Architecture         PASS
Host Verification    PASS
Build Verification   PASS
Main Board Flow      PASS
Review               PASS

Reset Persistence       PASS
Power-cycle Persistence PASS

S04_Firmware_Image_Storage → CLOSED
```

S04 可以作为 S05 的正式前置阶段使用。

下一阶段建议：`S05_UART_Ymodem` Design Stage。
