# S02 SFUD Boundary Evaluation

## Decision

`S02` 不立即集成 SFUD。当前先保留已经设计并实现的 W25Q64 Raw Driver，待 Keil 构建和真实开发板 Read Back/Compare 验证完成后，再以独立 follow-up 评估是否引入 SFUD Middleware。

本仓库当前没有 SFUD 源码或适配文件；以下结论基于 SFUD 官方公开接口定义和当前 Platform SPI/GPIO/Time 边界，不构成已经完成的 SFUD 运行验证。

参考：

- [SFUD `sfud_def.h`](https://github.com/armink/SFUD/blob/master/sfud/inc/sfud_def.h)
- [SFUD `sfud.h`](https://github.com/armink/SFUD/blob/master/sfud/inc/sfud.h)
- [SFUD 官方 README](https://github.com/armink/SFUD/blob/master/README.md)

## 1. Required SFUD Port Callbacks

SFUD 的 SPI 端口核心不是两个独立的 `read()` / `write()` 回调，而是 `sfud_spi.wr` 组合回调：一次调用接收待发送字节流和待接收缓冲区，完成一笔保持片选关系正确的总线操作。`sfud_spi` 同时包含 `lock`、`unlock` 和 `user_data`；重试延时位于 `sfud_flash.retry.delay`。

因此适配层需要：

- 一个组合 SPI 写/读回调；
- SPI 总线锁与解锁策略；
- 一个持有 Platform SPI Device/CS 绑定的 `user_data`；
- 一个可被 SFUD 无参延时回调调用的毫秒延时包装；
- CS 控制。SFUD 没有单独强制的 `set_cs` 字段，CS 可以由组合回调通过 Platform SPI Device 的事务边界管理。

## 2. Current Platform Mapping

| SFUD 需求 | 当前可复用能力 | 结论 |
| --- | --- | --- |
| `wr` 组合回调 | `platform_spi_transaction_begin()` → `platform_spi_write()` → `platform_spi_read()` → `platform_spi_transaction_end()` | 可满足；CS 由 Platform Device 事务边界控制 |
| CS | `platform_gpio_t` + `platform_spi_device_init()` 的 CS 绑定 | 可满足；不暴露 HAL Handle |
| delay | `platform_time_delay_ms(1U)` 包装为 SFUD 无参 retry delay | 可满足 |
| lock/unlock | 当前 SPI Transaction 只提供一次事务的 activeDevice 占用 | 单线程板测可用轻量包装；并发生产场景仍需先定义共享 Storage Bus 锁，不在 S02 私自扩展 |
| JEDEC/容量 | 当前 Raw Driver 初始化读取并校验 `EF 40 17` | 已覆盖底层诊断；SFUD 设备表仍需独立配置/验证 |

这些映射不需要把 HAL Handle 穿透到 SFUD，也不要求新增通用 `transfer()`。但正式 SFUD 适配需要一个独立的 Middleware Adapter，不能把 SFUD 回调直接塞进 Raw Driver。

## 3. Does SFUD Require `transfer()`?

不需要。SFUD 官方 `wr` 回调已经表达了“发送命令/地址后接收数据”的组合操作；适配层可以在一笔 Platform SPI Transaction 内依次调用现有 `platform_spi_write()` 和 `platform_spi_read()`，最后调用 `platform_spi_transaction_end()`。

这与 S02 冻结的 Bus / Device / Transaction 模型一致，因此没有理由为了 SFUD 增加 `transfer()`。只有在后续真实适配证明 SFUD 需要跨多个 `wr` 调用保持一个更长生命周期的总线锁时，才应返回 Design Role 重新评估架构边界。

## 4. SFUD and Raw Driver Semantics

SFUD 官方接口包含 `sfud_read()`、不自动擦除的 `sfud_write()`、按擦除粒度处理的 `sfud_erase()`，以及独立的 `sfud_erase_write()` / `sfud_chip_erase()`。

与当前 Raw Driver 的差异：

- Raw Driver `platform_w25q64_write()` 只做 Page 拆分，不自动 Erase；这与 `sfud_write()` 的“不执行擦除”语义接近。
- Raw Driver `platform_w25q64_page_program()` 强制禁止单次跨 Page；SFUD 适配需要确认其 Page 写拆分和目标芯片写模式配置，再用真实 Read Back 验证。
- Raw Driver `platform_w25q64_sector_erase()` 要求地址严格 4 KiB 对齐，固定使用 `0x20`，不自动向下对齐；SFUD `sfud_erase()` 按 erase granularity 对齐的行为可能影响相邻数据，不能直接视为等价安全边界。
- SFUD 提供 Chip Erase 能力，而 S02 明确禁止 Chip Erase；S02 后续若集成 SFUD，必须在项目封装层禁止或隔离该入口。
- SFUD 公开表中已有 W25Q64 相关 `EF 40 17` 参数/扩展信息，但目标 `W25Q64JVSSIQ` 仍必须以本板 JEDEC、SFDP/数据手册和真实读写擦结果为准，不能仅凭表项宣布兼容。

## 5. Follow-up Boundary

SFUD 实际 Middleware 集成延期到以下条件满足之后：

1. Keil Clean/Rebuild 完成且没有新的 S02 编译错误或未解释 warning；
2. Raw Driver 在 `0x7FF000 ~ 0x7FFFFF` 完成 JEDEC、SR1、Erase、Program、跨页 Write、边界和双启动持久化验证；
3. 明确 Storage Bus 并发锁策略；
4. 明确 SFUD 的写入、擦除和 Chip Erase 能力如何被项目 Storage 封装限制。

当前决定不是否定 SFUD，而是先把 Raw Driver 作为学习、诊断和底层验证基线，避免在没有硬件证据时同时引入第三方 Middleware 和新的并发/擦除语义。
