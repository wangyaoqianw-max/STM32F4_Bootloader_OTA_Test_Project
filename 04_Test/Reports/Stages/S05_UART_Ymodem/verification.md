# S05 UART Ymodem 验证报告

## 元数据

- 阶段：`S05_UART_Ymodem`
- 分支：`codex/s05-uart-ymodem`
- 验证日期：2026-09-15
- 实现提交：`00cbd3a`
- 目标板：STM32F411CE
- 调试器：J-Link V9
- USART：USART1，115200 8N1；CH340 外部串口端口为 `COM10`

## 代码验证

状态：`PASS`

| 项目 | 结果 |
| --- | --- |
| YMODEM Parser Host Test | PASS |
| YMODEM Receiver Host Test | PASS |
| Tera Term Block 0 元数据兼容回归 | PASS |
| Firmware Storage Write Host Test | PASS |
| S04 Firmware Format Host Test 回归 | PASS |
| S05 Flash Sink Host Test | PASS |
| `test_pack_firmware.py` | 2 tests PASS |
| `ymodem_receiver.c` GCC 实际头文件语法检查 | PASS |
| `s05_ymodem_flash_sink.c` GCC 实际 Platform 头文件语法检查 | PASS |
| Keil 生产 target 构建 | 0 errors，14 warnings（既有 Platform/FreeRTOS/Vendor 文件） |
| Keil 清理输出后完整构建 | 0 errors，14 warnings |
| Keil 工程 XML 解析 | PASS |
| `git diff --check`（S05 源代码/测试/文档） | PASS |

构建中的 14 个 warning 均来自既有 Platform/FreeRTOS/Vendor 文件；新增或修改的 S05 文件没有产生 warning。构建已成功生成 `OTA_APP.hex`，代码和链接结果有效。

Host 回归覆盖 Parser 分片、SOH/STX、CRC 和块序号，Receiver 正常/重复包/序列错误/重试/超时/CAN/EOT/Sink 失败路径，以及 Flash Sink 的 Header 边界、地址映射、Header-last 和失败原子性。

## 2026-09-15 Block 0 兼容性修复增量

Host Test 新增 Tera Term 实际发送格式：文件名后使用空格分隔的文件大小、修改时间和权限字段。Receiver 现按十进制解析文件大小、按八进制解析修改时间和权限，并保存可选发送方序号；Block 0 全部字段统一保存在 `ymodem_receiver_block0_metadata_t` 中。字段缺失、跳过、非法进制、溢出或未知尾部数据均拒绝。

本轮新增 Receiver Host Test、S05 Host 回归、S04 Firmware Format Host Test、`test_pack_firmware.py` 和 Keil 构建均通过。Keil 构建结果为 `0 Error(s), 0 Warning(s)`。

本轮差异复核中，S05 源代码、测试和阶段文档通过 `git diff --check`；全量检查仅报告 CubeMX 生成文件的 CRLF 行尾差异，以及 `OTA_APP.uvprojx` 末尾空行，不涉及本轮 C 源码逻辑。

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

## 2026-09-15 当前工程与参考工程 A/B 串口复测

开发板重新上电后，当前工程重新完成 J-Link 连接、复位、烧录和运行。使用 CH340 `COM10`、115200、8N1 测试：

```text
当前工程 RTT                         PASS
当前工程 YMODEM_READY                PASS
当前工程主机发送 0xA5                MCU 未收到
当前工程 YMODEM Sender 初始 C         FAIL, exit_code=4
当前工程 RTT rx_bytes                0
```

随后将参考工程
`E:\my_project_2026\Git_test\stm32f4_DMA_UART_ring_RTOS\RTT_elog_DMA_UART_ring_project`
的已生成 HEX 刷入同一块板。参考工程 RTT 输出 `communication runtime started`，其串口配置为 USART1、PA9/PA10、115200 8N1。向同一 `COM10` 发送该工程定义的 `HELP\r\n` 命令，仍未收到任何响应。

```text
参考工程烧录                         PASS
参考工程 RTT                         PASS
参考工程 HELP CRLF 响应              FAIL, no response
```

测试完成后已将当前工程固件重新刷回开发板。A/B 结果表明：同一 CH340 外部链路下，当前工程的 YMODEM 初始 `C` 失败，参考工程的普通串口命令也未形成收发闭环；当前没有证据支持继续修改 YMODEM、RingBuffer 或 RTOS 线程。最早失败边界仍位于 CH340 与 MCU USART1 PA9/PA10 之间，硬件串口链路保持 `PENDING`。

```text
代码验证：PASS
硬件验证：PENDING
当前固件：已恢复为当前工程
```

## 2026-09-15 PA9-PA10 本地 UART 回环

用户临时短接 MCU `PA9`（USART1_TX）与 `PA10`（USART1_RX）后，增加一次仅用于诊断的 HAL 阻塞式回环探针。测试在 `service_uart_start()` 之前执行，随后已移除该探针。

```text
HAL loopback TX status = 0
HAL loopback RX status = 0
received byte          = 0x55
UART rx_events         = 11
UART rx_bytes          = 11
UART buffered/read     = 11/11
UART dropped/errors    = 0/0
DMA2 Stream2 NDTR      = 256 -> 245
```

该结果证明当前固件的 USART1、PA9/PA10 复用、DMA RX、UART Service 和 RingBuffer 均可以正常工作；此前的 `rx_bytes=0` 仅发生在 CH340 外部链路输入场景。当前剩余问题不是 YMODEM 或 UART 模块移植漏洞，而是 CH340 到 MCU 串口引脚之间的外部连接路径尚未打通。

```text
代码验证：PASS
硬件本地回环：PASS
CH340 外部串口链路：PENDING
```

尚未满足阶段完成门禁：

- 真实 Tera Term → STM32 → Slot B 完整传输；
- RTT 中 Block 0、完整接收字节数、Payload 写入和 Header-last 证据；
- `firmware_storage_validate_image(Slot B) == VALID`；
- 硬件中断/取消后再次传输恢复。

当前测试暂停。独立 TTL 模块已枚举为 `COM9`，但本轮 Tera Term、串口 API 以及直接发送单字节 `0x01` 后板端均为 `rx_events=0 rx_bytes=0`，串口 API 也未读到板端初始 `'C'`；J-Link 已重新连接并完成烧录。下一步先核对 TTL TX 到 USART1 `PA10` 的信号路径、交叉 TX/RX、共地和 3.3V 电平，再重新执行 RTT 捕获和 Sender，并以 `YMODEM session complete final result=PASS` 与 Slot B `VALID` 作为硬件通过条件。

2026-09-15 兼容性修复后的板测尝试仍记录 `rx_events=0 rx_bytes=0`，因此本次未进入 Block 0 解析路径，不能作为硬件修复结果。代码验证保持 `PASS`，硬件验证保持 `PENDING`。

2026-09-15 使用独立 TTL 模块 `COM9` 重新执行整体数据流：J-Link 烧录校验通过，RTT 捕获到 `[S05] YMODEM_READY`，Tera Term 宏已打开 `COM9` 但返回 `ERRORLEVEL=1`。RTT 最终仍为 `rx_events=0 rx_bytes=0`、`packets received=0`、`file_size=0`，说明本次未到达 MCU Block 0 解析入口；协议代码验证保持 `PASS`，硬件验证保持 `PENDING`。

## 2026-09-15 RTOS 线程隔离与阻塞 RX 探针

本轮使用 CH340 当前枚举端口 `COM10`，不使用 J-Link CDC `COM3`。

测试结果：

```text
Keil build                         PASS, 0 Error(s), 0 Warning(s)
J-Link flash                       PASS
S05 dedicated thread start         PASS, result=0
S05 YMODEM_READY                   PASS, thread=s05Ymodem
Sender initial C                  FAIL, exit_code=4
MCU UART statistics                tx_bytes=11, rx_events=0, rx_bytes=0
Host raw 0xA5 write                MCU did not report reception
Blocking Platform UART RX probe    result=2, read=0
```

阻塞 RX 探针在 `service_uart_start()` 和 RX DMA 启动之前执行，直接调用 Platform UART 接收接口。该探针仍然超时，说明字节尚未到达 `HAL_UART_Receive()`，因此当前问题不在 RTOS 线程、YMODEM Parser、DMA、RingBuffer 或 Receiver 状态机。

本轮诊断后的结论是：

```text
代码验证：PASS
硬件验证：PENDING
当前最早失败边界：CH340 TX -> USART1 PA10 -> HAL_UART_Receive
```

## 2026-09-15 UART 运行时寄存器快照

本轮仅增加运行时寄存器日志，没有修改 UART、DMA、RingBuffer、YMODEM 或任务调度逻辑。使用 CH340 `COM10`，烧录和 RTT 捕获均成功；主机在板端进入 `YMODEM_READY` 后发送单字节 `0xA5`。

运行时快照：

```text
GPIOA MODER   = 0xA8288800  PA9/PA10 mode = Alternate Function
GPIOA AFR[1]  = 0x00000770  PA9/PA10 AF   = AF7 USART1
GPIOA PUPDR   = 0x64000000  PA9/PA10 pull = None
USART1 BRR    = 0x00000364
USART1 CR1    = 0x0000201C
USART1 CR3    = 0x00000041
USART1 SR     = 0x000000C0  TXE/TC set, RXNE not set
USART1 clock  = APB2ENR bit enabled
DMA2 clock    = AHB1ENR bit enabled
DMA2 Stream2  = circular RX, enabled, NDTR=256 at startup
```

本次 RTT 仍为 `rx_events=0`、`rx_bytes=0`、`packets received=0`，主机也未读到初始 `C`。运行时配置已证明 PA9/PA10 和 USART1 外设复用正确；发送 `0xA5` 后没有出现 RXNE，当前最早失败边界继续位于 CH340 TX 到 MCU PA10 实际输入电平之间。RingBuffer 和 YMODEM 仍未进入故障边界。

```text
代码验证：PASS
硬件验证：PENDING
```

## 2026-09-15 Python Sender 与 Tera Term CH340 端到端复测

前面的失败测试使用了原始 `OTA_APP_s04_test.bin`。该文件只有 Payload，不包含 S05 Flash Sink 要求的 64 Byte Firmware Image Header。板端已经收到 Block 0 并解析文件名和文件大小，但在第一个数据 Block 写入前进行 Header 校验时返回 `error=3`（`PLATFORM_ERR_INVALID_PARAM`），因此 Sender 只看到 Block 1 无 ACK/NAK。

本次先启动上位机 Sender，再通过 `05_Tools\Scripts\flash_app.bat` 烧录并复位目标板，使用 CH340 `COM10`、115200、8N1 和正确的：

```text
06_Output\Packages\OTA_APP_s04_v1.1.0.img
```

### Python Sender

```text
[RX] C
[TX] Block 0
[RX] ACK
[RX] C
[TX] Block 1 ... Block 55
[RX] ACK（全部数据块）
[TX] EOT
[RX] NAK
[TX] EOT
[RX] ACK
[RX] C
[TX] Block 0 (end)
[RX] ACK
[DONE] Transfer successful bytes=55884 blocks=55 retries=0
```

机器结果：

```json
{"blocks_sent":55,"bytes_sent":55884,"exit_code":0,"ok":true,"phase":"complete","retries":0}
```

### Tera Term

使用仓库入口和同一个 `.img`：

```text
05_Tools\Scripts\send_ymodem.bat COM10 115200 06_Output\Packages\OTA_APP_s04_v1.1.0.img
```

宏退出码为 `0`。随后 RTT 捕获到完整接收进度：

```text
[S05] progress=16384/55884
[S05] progress=32768/55884
[S05] progress=49152/55884
[S05] progress=55884/55884
```

这次结果同时证明：

```text
CH340 外部串口链路：PASS
Python Sender 协议传输：PASS
Tera Term YMODEM 传输：PASS
正确文件格式：必须使用 .img
固定启动顺序：Sender/Tera Term 打开串口后，再烧录或复位 MCU
```

本次 RTT 捕获已证明完整 Payload 接收进度，但未单独捕获最终 `Slot B validation result` 行；中断取消恢复和第二次连续传输仍属于后续硬件回归项。

### 首次失败的调用顺序

首次 `exit_code=4` 的直接原因是自动化调用顺序错误：目标板已经复位并发送初始 `C`，Sender 尚未打开 COM10 或尚未进入读取状态，导致该握手字节被错过。后续将调用顺序固定为：

```text
启动 Sender/Tera Term 并打开 COM
→ 确认进入等待 C
→ 烧录或复位 MCU
→ 开始 YMODEM 传输
```

该顺序调整后，Python Sender 和 Tera Term 均能收到初始 `C` 并完成传输。

## 2026-09-15 Tera Term 最终板级验收

本轮按固定顺序执行：关闭其他串口工具，启动仓库内 Tera Term 宏并打开 CH340 `COM10`，确认宏进入等待状态后，再执行 `05_Tools\Scripts\flash_app.bat` 通过 J-Link 烧录/复位；烧录结束后立即启动 RTT 捕获。发送文件为：

```text
06_Output\Packages\OTA_APP_s04_v1.1.0.img
```

Tera Term 入口：

```text
05_Tools\Scripts\send_ymodem.bat COM10 115200 06_Output\Packages\OTA_APP_s04_v1.1.0.img
```

### 正常传输结果

```text
Tera Term macro exit code       0
YMODEM state                    6 (FINISHED)
filename                        OTA_APP_s04_v1.1.0.img
file_size / received            55884 / 55884
packets received / accepted    57 / 57
bytes received / written        55884 / 55884
CRC / sequence / duplicate      0 / 0 / 0
timeout / retry / cancel        0 / 0 / 0
UART dropped / errors           0 / 0
flash payload / header commit   55820 / 1
Slot B validation               result=0, validation=2 (VALID)
final result                    PASS
```

RTT 同时记录进度 `16384/55884`、`32768/55884`、`49152/55884`、`55884/55884`，证明完整数据流已到达接收端。该结果确认 Tera Term、CH340、USART1、DMA/RingBuffer、YMODEM Receiver、Flash Sink、Slot B 写入和最终 Firmware 校验链路均通过。

### 传输中止与恢复结果

第二轮测试在传输过程中停止 Tera Term 进程，接收端得到：

```text
YMODEM state                    8 (ERROR)
error                           2 (PLATFORM_ERR_TIMEOUT)
received                        12288 / 55884
timeout / retry / cancel        11 / 10 / 0
flash payload / header commit   0 / 0
```

该结果证明未完成传输不会提交 Header。随后重新按“先启动 Tera Term，再烧录/复位”的顺序执行正常传输，宏返回 0，Slot B 再次得到 `validation=2 (VALID)`，证明失败后可以恢复新会话。

### 验收结论

```text
代码验证：PASS
硬件验证：PASS
阶段状态：CLOSED
默认板测发送端：Tera Term 5 / 05_Tools\Scripts\send_ymodem.bat
```

首次 `exit_code=4` 已确认是调用顺序错误：目标板先复位并发送初始 `C`，Sender 后打开串口而错过握手。该问题已通过固定自动化顺序解决，不需要继续修改 YMODEM 协议或 UART 初始化。S04 Reset Persistence、Power-cycle Persistence 仍按计划延期至 S07 关闭前完成。
