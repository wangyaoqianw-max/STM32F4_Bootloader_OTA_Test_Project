# ADR-0001: External Firmware Slots and Metadata Lifecycle

## Status

Accepted

## Context

STM32F411 的 Internal Flash 当前承载正在执行的 Application。W25Q64 上的 Slot A/B 用于保存 OTA Firmware Image，不能描述为 MCU 可直接执行的 A/B Application 分区。S07 需要在不混淆镜像健康状态和升级生命周期的前提下，记录已确认镜像与下一次待安装镜像。

此前 Metadata 中的 `activeSlot` 不能同时表达“当前确认版本”和“下一次待安装目标”。S07 还需要保留 AT24C02 双副本、sequence、CRC32 和 commit-marker-last 的原子提交语义。

## Decision

1. Internal Flash 是当前执行区；External Flash 的 Slot A/B 是 Firmware Image Storage，不是可直接执行的 CPU 分区。
2. Metadata 使用 `confirmedSlot` 表示最后确认成功的 External Firmware Image，使用 `pendingSlot` 表示 Bootloader 下一次需要安装的目标。
3. 删除业务字段 `activeSlot`，并使用独立的 `slotAState`、`slotBState` 表示镜像健康，使用 `upgradeState` 表示 `NONE/PENDING/TRIAL/ROLLBACK` 生命周期。
4. S07 只负责稳定态下载、验证和 `NONE → PENDING` 的 Metadata 原子提交，然后请求 Reset；Internal Flash Installation、Trial、Confirm 和 Rollback execution 由后续阶段负责。
5. Metadata 保持双 128 Byte Copy、sequence、CRC32、commit-marker-last；V1 记录只读兼容，读取旧格式不主动写回迁移。

## Consequences

- `otaWorker` 和 `service_ota` 不得把 External Slot 当作执行分区，也不得实现 S09/S10 的安装和试运行逻辑。
- 后续 Bootloader 必须消费 `pendingSlot + upgradeState=PENDING`，并实现 Trial/Confirm/Rollback 生命周期。
- S07 的板级验收必须分别证明 Slot 镜像有效性、Metadata 原子持久化和后续 Bootloader 消费；仅有 Build 或当前 Application 重启不能替代这些证据。
