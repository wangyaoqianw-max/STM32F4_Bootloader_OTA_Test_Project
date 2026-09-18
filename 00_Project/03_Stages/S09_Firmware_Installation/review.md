# S09 Firmware Installation Review

## Metadata

- Stage: `S09_Firmware_Installation`
- Status: `NOT_STARTED`
- Branch: `main`
- Updated At: `2026-09-18`

## Review Gate

Review Role 在 S09 进入 `READY_FOR_REVIEW` 后填写。

审核至少核对：

- 冻结设计与实际实现一致；
- Bootloader/Application Binary Contract 兼容；
- W25Q64 Bootloader Driver 保持 read-only；
- Internal Flash Driver 无法覆盖 Bootloader Region；
- Candidate pre-validation 在 erase 前完成；
- PENDING→TRIAL 原子边界和 fault injection 证据成立；
- TRIAL 不重复安装；
- confirmedSlot/confirmedVersion 未提前更新；
- Build、Board Test、Bootloader size 和文档证据完整；
- 未提前实现 S10 Trial Confirm / Watchdog / Rollback。
