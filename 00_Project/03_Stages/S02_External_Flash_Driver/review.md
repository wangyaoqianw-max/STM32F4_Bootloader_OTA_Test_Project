# S02 External Flash Driver Review

## Metadata

- Stage: `S02_External_Flash_Driver`
- Review State: `NOT_STARTED`
- Branch: `main`
- Design Commit: `44fddb1484441a98d336df7783170267c166f4af`
- Plan Commit: `aee30916c5c784828269668d99a2b4e63689f80e`
- Implementation Commit: `Not created yet`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`

## Review Entry Conditions

Review 仅在阶段状态达到 `READY_FOR_REVIEW` 后开始。开始 Review 前必须具备：

1. 已批准的 `design.md`；
2. 冻结的 `implementation_plan.md`；
3. 已更新的 `handoff.md` Implementation Output；
4. 实际 Implementation Commit；
5. `04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md`；
6. Keil Clean/Rebuild 结果；
7. W25Q64 板级测试证据；
8. Project Owner 对真实硬件结果的确认。

## Required Review Focus

后续 Review 至少检查：

- Platform SPI `read()` 是否保持现有 Bus / Device / Transaction 语义；
- SPI1/SPI2 是否共用 Impl，且 PCLK2/PCLK1 判断正确；
- W25Q64 Driver 是否存在 HAL 泄漏或反向依赖 Service Log；
- JEDEC/SR1/Read/Program/Erase 指令和地址序正确；
- Program 是否严格禁止单次跨 Page；
- 连续 Write 是否正确拆 Page 且没有隐式 Erase；
- Sector Erase 是否要求 4 KiB 对齐且不自动修正；
- Program/Erase 是否通过 WEL + BUSY 机制完成；
- transaction 失败路径是否仍释放 CS/activeDevice；
- 测试是否只破坏 `0x7FF000 ~ 0x7FFFFF`；
- RTT PASS 是否由实际 Read Back/Compare 支撑；
- destructive board test 是否在正常分支状态下被禁用；
- SFUD 评估是否基于已经验证的 Raw Driver 结果。

## Review Result

尚未进入 Review。不得在 Verification 完成前填写 `PASS`、`CHANGES_REQUESTED` 或 `CLOSED` 结论。
