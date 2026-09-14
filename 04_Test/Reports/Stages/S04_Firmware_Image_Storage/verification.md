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

`rtt_capture.ps1` 的 Logger 进程启动兼容修复已记录于提交 `1f756f0`。本节只证明本机工具链动作可执行，不替代持久性场景。

## Deferred Regression Items

以下场景尚未实际执行，结果保持 `PENDING`：

- Reset Persistence：`PENDING / DEFERRED`
- Power-cycle Persistence：`PENDING / DEFERRED`

Project Owner 于 2026-09-14 明确决定将这两项从 S04 阶段关闭阻塞项调整为跨阶段延期回归项。

该调整不代表测试通过，也不删除原设计中的验证要求。两项必须在以下门禁前补充真实硬件证据：

```text
Must be completed before:
S07_OTA_Service_V1 stage closure
```

可以在 S05 / S06 / S07 更早执行。

## 2026-09-14 Review 复核补充

按 Review 复核时，重新执行了 CRC、Firmware Format、Firmware Storage 和 Python pack tool Host 测试。Format Host Test 命令补充了 `04_Impl/impl_board` include path，并加入 `firmware_metadata.c`；修正后的命令全部通过。原实现计划中的示例命令已同步修正，避免文档命令与实际依赖不一致。

## 结论

代码验证：`PASS`

S04 范围内硬件主流程验证：`PASS`

跨阶段持久性回归：`PENDING / DEFERRED`

当前证据证明：Firmware Image 写入、CRC、Header-last 提交、完整回读、Metadata 双副本提交与单副本恢复，以及本机 Build/Flash/RTT 工具链动作均通过。

依据 Project Owner 的范围调整，两项 Persistence 不再阻塞 S04 关闭，但在 S07 阶段关闭前必须补测。最终阶段关闭决定见：

`00_Project/03_Stages/S04_Firmware_Image_Storage/review.md`
