# S04 Firmware Image Storage Review

## Metadata

- Stage: `S04_Firmware_Image_Storage`
- Status: `NOT_STARTED`
- Branch: `codex/s04-firmware-image-storage`
- Baseline Commit: `245cd3ee550a2c2cc016e6a197609d712ef1e893`
- Design Commit: `e701910a0452952c632ad8352c6973eedf06b283`
- Implementation Plan Commit: `bc5360fa40188c189a9e19b91a29ad5d266d8220`
- Implementation Commit: `Not created yet`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Updated At: `2026-09-13`

## Review Entry Condition

只有 Stage 状态进入 `READY_FOR_REVIEW` 后才执行正式 Review。

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

## Review Decision

当前：`NOT_STARTED`

允许的最终结论：

```text
PASS
CHANGES_REQUESTED
BLOCKED
```

缺少独立 Verification 证据时不得填写 `PASS`，不得关闭 S04。
