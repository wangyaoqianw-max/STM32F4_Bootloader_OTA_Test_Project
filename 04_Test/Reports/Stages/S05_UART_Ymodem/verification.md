# S05 UART Ymodem 验证报告

## 元数据

- 阶段：`S05_UART_Ymodem`
- 分支：`codex/s05-uart-ymodem`
- 验证日期：2026-09-14
- 实现提交：`00cbd3a`
- 目标板：STM32F411CE
- 调试器：J-Link V9
- USART：USART1，115200 8N1；当前枚举到的 PC 端口为 J-Link CDC UART `COM3`

## 代码验证

状态：`PASS`

| 项目 | 结果 |
| --- | --- |
| YMODEM Parser Host Test | PASS |
| YMODEM Receiver Host Test | PASS |
| Firmware Storage Write Host Test | PASS |
| S04 Firmware Format Host Test 回归 | PASS |
| S05 Flash Sink Host Test | PASS |
| `test_pack_firmware.py` | 2 tests PASS |
| `ymodem_receiver.c` GCC 实际头文件语法检查 | PASS |
| `s05_ymodem_flash_sink.c` GCC 实际 Platform 头文件语法检查 | PASS |
| Keil 正常构建 | 0 errors，0 warnings |
| Keil 清理输出后完整构建 | 0 errors，14 warnings |
| Keil 工程 XML 解析 | PASS |
| `git diff --check` | PASS |

清理构建中的 14 个 warning 均来自既有 Platform/FreeRTOS/Vendor 文件；新增或修改的 S05 文件没有产生 warning。构建已成功生成 `OTA_APP.hex`，代码和链接结果有效。

Host 回归覆盖 Parser 分片、SOH/STX、CRC 和块序号，Receiver 正常/重复包/序列错误/重试/超时/CAN/EOT/Sink 失败路径，以及 Flash Sink 的 Header 边界、地址映射、Header-last 和失败原子性。

## 固件包输入

使用现有 Packer 重新生成：

```text
python 05_Tools/Firmware/pack_firmware.py \
  --input 06_Output/Firmware/OTA_APP_s04_test.bin \
  --output 06_Output/Packages/OTA_APP_s04_v1.1.0.img \
  --version 1.1.0
```

结果：

```text
源 Payload：55820 Byte
版本：1.1.0
紧凑 .img：55884 Byte = 64 Byte Header + 55820 Byte Payload
SHA-256：0E21EC936DE1D2A5342B82A616497EA978C4C3014BE081F31274555C48B7D528
```

## 工具链与板端 Smoke Test

以下入口均已实际执行：

```text
05_Tools/Scripts/build_app.bat
05_Tools/Scripts/flash_app.bat
05_Tools/Scripts/rtt_capture.bat 10
05_Tools/Scripts/run_app_cycle.bat 10
```

结果：

- Keil 编译/链接、J-Link 下载校验、复位运行和 RTT Logger 连接均通过；
- RTT 已捕获 `[S05] YMODEM_READY`；
- S05 板测 Storage 初始化和 USART1 Service 初始化均返回 `0`；
- 以上只证明板测入口和工具链动作成功，不等价于 YMODEM 文件传输通过。

## 真实 Tera Term 传输

已使用仓库入口执行：

```text
05_Tools/Scripts/send_ymodem.bat COM3 115200 06_Output/Packages/OTA_APP_s04_v1.1.0.img
```

传输结果：`PENDING`，发送器返回失败。RTT 证据 `06_Output/Logs/OTA_APP_rtt.log` 显示：

```text
[S05] YMODEM_READY
[S05] session state=8 error=2 ... file_size=0 received=0
[S05] packets received=0 accepted=0 bytes=0/0 ...
[S05] timeout=11 retry=10 cancel=0
[S05] flash payload=0 header_commit=0 error=0
```

当前证据表明板端未收到 Tera Term 经 J-Link CDC `COM3` 发送的任何 YMODEM 字节，因此没有 Block 0、Payload 写入、Header 提交或 Slot B 镜像验证证据。新增 UART 边界统计为 `rx_events=0 rx_bytes=0 buffered=0 read=0 dropped=0 errors=0 tx_bytes=11`，说明板端已经发送 11 次 `'C'`，但本轮 Tera Term 发送路径没有进入板端 UART。补充验证显示：相同 Tera Term 宏在 COM1↔COM2 回环中可以发出 `0x01`；串口助手/.NET 路径已成功触发板端接收并解析 Block 0。因此物理连接不再是未确认项，当前问题限定为 Tera Term 与 J-Link CDC 的发送路径兼容性或端口占用，不能标为硬件失败。

## 验收状态

```text
代码验证：PASS
硬件验证：PENDING
```

尚未满足阶段完成门禁：

- 真实 Tera Term → STM32 → Slot B 完整传输；
- RTT 中 Block 0、完整接收字节数、Payload 写入和 Header-last 证据；
- `firmware_storage_validate_image(Slot B) == VALID`；
- 硬件中断/取消后再次传输恢复。

当前测试暂停。独立 TTL 模块已枚举为 `COM9`，但本轮 Tera Term 发送后板端仍为 `rx_events=0 rx_bytes=0`，串口 API 探针也未读到板端初始 `'C'`；J-Link 已重新连接并完成烧录。下一步先核对 TTL 与 USART1 `PA9/PA10` 的交叉 TX/RX、共地和 3.3V 电平，再重新执行 RTT 捕获和 Sender，并以 `YMODEM session complete final result=PASS` 与 Slot B `VALID` 作为硬件通过条件。
