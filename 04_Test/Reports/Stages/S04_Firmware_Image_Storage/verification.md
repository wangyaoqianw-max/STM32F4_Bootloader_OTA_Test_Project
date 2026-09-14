# S04 Firmware Image Storage 验证报告

## 元数据

- 阶段：`S04_Firmware_Image_Storage`
- 分支：`codex/s04-firmware-image-storage`
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

## 尚未执行项

- Reset Persistence：`PENDING`
- Power-cycle Persistence：`PENDING`

当前报告证明 S04 固件镜像写入、CRC、Header-last 提交、回读和 Metadata 恢复链路通过；上述两项持久性场景需在下一次板测窗口补充。

## 结论

代码验证：`PASS`

硬件验证：`PENDING`（S04 主流程 PASS，Reset/Power-cycle Persistence 尚待补测）

本报告不关闭阶段，交由 Verification/Review 流程继续处理。
