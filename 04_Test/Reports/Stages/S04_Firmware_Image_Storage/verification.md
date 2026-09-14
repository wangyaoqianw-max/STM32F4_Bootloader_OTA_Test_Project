# S04 Firmware Image Storage 验证报告

## 元数据

- 阶段：`S04_Firmware_Image_Storage`
- 分支：`main`
- 验证日期：2026-09-14
- 目标板：STM32F411CE
- 调试器：J-Link V9，S/N `602713300`
- UART：USART1，115200 8N1，J-Link CDC UART `COM3`
- 镜像：`OTA_APP_s04_v1.1.0.img`

## 代码验证

状态：`PASS`

| 项目 | 结果 |
| --- | --- |
| CRC Host Test | PASS |
| Firmware Format Host Test | PASS |
| Firmware Storage Host Test | PASS |
| Python pack tool unittest | 2 tests PASS |
| Python 生成镜像的 C 解码兼容性 | PASS |
| Keil normal build | 0 errors，14 个既有 warning |
| Keil 清理输出后完整重建 | 0 errors，14 个既有 warning |
| `git diff --check`（按仓库 CRLF 规则） | PASS |

Host 测试使用临时目录 `s04_host_verification` 编译，不产生仓库内测试可执行文件。

## 真实板测

日志：`06_Output/Logs/S04_board_test_rtt_clean.log`

干净复位并重新烧录后，按“先 64 Byte Header、等待擦除完成、再发送 Payload”的顺序执行：

- Header validation：`VALID`
- Header：Version `1.1.0`，Payload `55820` Byte
- Slot B 擦除：PASS
- Payload 接收进度：`55820/55820`
- Payload CRC：expected `0x3ED1C72E`，calculated `0x3ED1C72E`
- Header 最后提交：PASS
- 完整镜像回读验证：PASS
- Metadata 双副本提交：PASS
- 破坏最新副本后的单副本恢复：PASS
- 最终结果：`PASS`

第一次未复位重跑出现 CRC `0x8C76B70F`，根因为前一次未完成解析的 64 Byte Header 残留在 DMA/RingBuffer 中，被当作本次 Payload 前缀；重新烧录复位后的干净测试已排除该测试残留并通过。

## Application Toolchain Smoke Test

2026-09-14 在同一 STM32F411CE / J-Link V9 / SWD 4 MHz 环境下完成：

| 入口 | 结果 | 证据 |
| --- | --- | --- |
| `05_Tools/Scripts/build_app.bat` | PASS | Keil 0 Error、0 Warning |
| `05_Tools/Scripts/flash_app.bat` | PASS | J-Link 连接、下载、校验和运行成功 |
| `05_Tools/Scripts/rtt_capture.bat 10` | Logger 连接 PASS | 找到 RTT Control Block；无新日志时按设计返回无 payload |
| `05_Tools/Scripts/run_app_cycle.bat 10` | PASS | 返回 0，捕获 RTT 启动日志 415 Byte |

`rtt_capture.ps1` 的 Logger 进程启动兼容修复已记录于提交 `1f756f0`。本节只证明
本机工具链动作可执行，不替代 S04 持久性场景。

## 尚未执行项

- Reset Persistence：`PENDING`（Project Owner 本轮决定暂不执行）
- Power-cycle Persistence：`PENDING`（Project Owner 本轮决定暂不执行）

当前报告证明 S04 固件镜像写入、CRC、Header-last 提交、回读、Metadata 恢复链路和本机工具链动作通过；上述两项持久性场景本轮不执行，因此仍不能作为已验证证据。

## 2026-09-14 Review 复核补充

按 Review 复核时，重新执行了 CRC、Firmware Format、Firmware Storage 和 Python pack
tool Host 测试。Format Host Test 命令补充了 `04_Impl/impl_board` include path，并加入
`firmware_metadata.c`；修正后的命令全部通过。原实现计划中的示例命令已同步修正，避免
文档命令与实际依赖不一致。

## 结论

代码验证：`PASS`

硬件验证：`PENDING`（S04 主流程 PASS，Reset/Power-cycle Persistence 尚待补测）

本报告不关闭阶段，交由 Verification/Review 流程继续处理。
