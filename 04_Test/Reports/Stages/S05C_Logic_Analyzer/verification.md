# S05C Logic Analyzer Verification

## Metadata

- Stage: `S05C_Logic_Analyzer`
- Status: `READY_FOR_REVIEW`
- Branch: `main`
- Baseline Commit: `31456f0e7020d79f31cb7dbcf006afe6fc687286`
- Verification Date: `2026-09-17`
- Coding Standard Review: `PASS`

## Scope

本阶段交付基于 `sigrok-cli` 的 SPI / I2C Logic Analyzer Workflow，包括 Config、Executor、Parser、Capture / Decode、Effective Config、Structured Result、Unified Router 和项目级断言。UART 与 GPIO Timing 按冻结设计延期。

真实板测坚持只读边界：W25Q64 只执行 JEDEC / Status / Read，AT24C02 只执行地址探测和随机读；没有执行 Flash 擦除、Flash 编程或 EEPROM 任意写入。板测临时代码已从 Application 移除。

## Verification Matrix

| Area | Result | Evidence |
| --- | --- | --- |
| Logic Analyzer doctor | PASS | `toolkit.bat logic doctor`，`sigrok-cli 0.8.0-git-f44dd91` |
| Runtime device scan | PASS | `toolkit.bat logic scan`，匹配 `fx2lafw` 设备 1 台，自动选择成功 |
| Config / Executor / Workflow contract | PASS | `05_Tools/Contracts/LogicAnalyzer/test_logic_analyzer.ps1` |
| Parser golden fixtures | PASS | `05_Tools/Tests/S05C_LogicAnalyzer/test_parser.ps1` |
| SPI project assertion | PASS | `test_spi.ps1`，JEDEC `0x9F → EF 40 17` |
| I2C project assertion | PASS | `test_i2c.ps1`，地址 `0x50`、读写方向和总线事件完整 |
| Existing Toolkit contracts | PASS | Application、Compatibility、Core、Debug、S04 isolation 全部 PASS |
| Existing Debug / Host regression | PASS | Tool sequence、CmBacktrace、Fault、GDB、Firmware、YMODEM、S04 Host 全部 PASS |
| Application build | PASS | `05_Tools/Scripts/build_app.bat`，无错误、无警告 |
| Flash / RTT cycle | PASS | `05_Tools/Scripts/run_app_cycle.bat` |
| Runtime snapshot resume | PASS | `05_Tools/toolkit.bat snapshot resume` |

## Real-board SPI Evidence

Wiring:

```text
D0 → PB12 → CS
D1 → PB13 → CLK
D3 → PB14 → MISO
D5 → PB15 → MOSI
```

采集使用 `24MHz`、`20,000ms` 时间窗口、`spi2_flash` profile。sigrok 监听器启动并进入 capture 后，才通过独立 J-Link Commander 客户端发送 `reset → run`；两类客户端未共享同一设备。

```text
Capture: 06_Output/LogicAnalyzer/20260917_115303_141_0c352bfc
Decode : 06_Output/LogicAnalyzer/20260917_120504_005_f1fbb543
```

Effective Config 记录了实际 profile、通道映射、`24MHz`、`20,000ms` 和运行时选择的 `fx2lafw:conn=4.5`。解码结果观察到：

```text
MOSI: 9F 00 00 00
MISO: FF EF 40 17
```

命令时钟周期的 `FF` 被项目断言正确排除后，W25Q64 JEDEC ID `EF 40 17` 匹配，项目断言 PASS。

## Real-board I2C Evidence

Wiring:

```text
D2 → PB6 → SCL
D4 → PB7 → SDA
```

采集使用 `1MHz`、`20,000ms` 时间窗口、`i2c_eeprom` profile；同样先启动 sigrok capture，再由独立 J-Link 客户端复位运行。

```text
Capture: 06_Output/LogicAnalyzer/20260917_115645_477_b13fea3d
Decode : 06_Output/LogicAnalyzer/20260917_120516_662_0f5a9f26
```

解码结果观察到地址 `0x50`、`WRITE` / `READ`、`START`、`REPEATED_START`、`STOP`、`ACK` 和 `NACK`；AT24C02 只读事务结构项目断言 PASS。

## Tool Runtime Strategy

工具运行按共享资源划分：不共享同一客户端、设备、端口、输出文件或构建输出的工具允许并行。J-Link Probe 仍遵循单客户端互斥；sigrok Logic Analyzer 与 J-Link、Keil、RTT 使用独立设备，在设备实例和输出路径不冲突时允许并行。同一逻辑分析仪、串口、J-Link Probe 或输出目录仍必须串行。

## Cleanup And Deferred Items

- 临时 `app_main.c` 板测代码已移除；临时 SPI 分频修改已恢复。
- 正式 Application 已重新编译、烧录并完成 RTT 冒烟；最终 RTT 仅包含基础 Application 初始化日志。
- 验证后无残留 `JLink*` 或 `sigrok-cli` 进程。
- `06_Output/LogicAnalyzer/` 为本机运行证据，不提交 Git。
- UART 与 GPIO Timing：`DEFERRED`，不是失败项。

## Conclusion

代码验证：`PASS`。

硬件验证：`PASS`（Logic Analyzer discovery、SPI/W25Q64 read-only、I2C/AT24C02 read-only 均通过）。

S05C 验证证据完整，建议交由 Review Role 进行阶段审核；本报告不直接将阶段状态改为 `CLOSED`。
