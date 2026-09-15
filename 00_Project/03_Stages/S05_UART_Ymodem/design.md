# S05 UART Ymodem Design

## 1. Stage Metadata

- Stage: `S05_UART_Ymodem`
- Status: `CLOSED`
- Branch: `codex/s05-uart-ymodem`
- Baseline Commit: `8173a3da2c174294350e47d8e889cf22b9066e23`
- Prerequisites: `S02_External_Flash_Driver` CLOSED, `S04_Firmware_Image_Storage` CLOSED
- Updated At: `2026-09-15`

## 2. Goal

建立 Application 侧可复用的 Ymodem Receiver 文件传输能力，使 PC 能通过 OTA 专用 UART 将 S04 定义的 Firmware Image V1 `.img` 文件可靠传输到 STM32，并通过 Firmware Storage 写入固定测试 Slot B。

S05 重点解决：

```text
PC Ymodem Sender
        ↓
UART byte stream
        ↓
service_uart
        ↓
Ymodem Packet Parser
        ↓
Ymodem Receiver State Machine
        ↓
Ymodem Sink
        ↓
Firmware Storage
        ↓
W25Q64 Slot B
```

阶段结束时必须能够证明：

- MCU 能与独立 PC Ymodem Sender 正常握手；
- Block 0 文件名、文件大小、修改时间和文件权限可安全解析；
- SOH 128 Byte / STX 1 KiB Packet 均可接收；
- Packet CRC-16/XMODEM、Block Number、ACK / NAK、Retry、Timeout、Cancel、EOT 可正确处理；
- 完整 `.img` 可按 S04 Slot 物理布局写入 External Flash；
- 传输完成后，Flash 中 Firmware Image 可通过现有 `firmware_storage_validate_image()` 验证；
- 中断或失败传输不能被误认为有效 Firmware。

S05 建立的是“可靠文件运输能力”，不建立正式 OTA 业务状态机。

## 3. Scope Boundary

### 3.1 Included

- Application Ymodem Receiver；
- Single-file Ymodem Session；
- Receiver 主动发送 `'C'` 请求 CRC 模式；
- Block 0 filename / filesize / modification time / file mode；
- SOH 128 Byte Packet；
- STX 1024 Byte Packet；
- Block Number / complement validation；
- CRC-16/XMODEM Packet validation；
- ACK / NAK；
- Duplicate Packet recognition；
- Sequence Error；
- Timeout / Retry；
- Remote CAN / Local Cancel；
- EOT handshake；
- Empty Block 0 Session End；
- Parser / Receiver / Sink 分层；
- Ymodem 编译期静态策略进入 `00_Config`；
- Firmware Storage 增加写入 Header / Payload 的逻辑能力；
- S05 Test Flash Sink 固定写 Slot B；
- Tera Term 5 Ymodem Sender 自动化入口；
- Host Test、Keil Build、RTT 板测与异常恢复验证。

### 3.2 Excluded

- Ymodem Sender on STM32；
- 正式 Batch 多文件业务；
- Bluetooth OTA Transport；
- USB CDC Transport；
- Application OTA Service；
- Inactive Slot 动态选择；
- Firmware Upgrade / Downgrade Policy；
- EEPROM `PENDING` / Upgrade Metadata 更新；
- Reset 请求；
- Bootloader 安装；
- Trial / Confirm / Rollback；
- FreeRTOS OTA 并发模型；
- SHA / AES / HMAC / Digital Signature；
- 通用 Protocol Manager / Transport Manager / Device Manager。

## 4. Stable Inputs From Previous Stages

### 4.1 UART Service

S05 直接复用现有 `service_uart`：

```text
UART DMA RX
→ SPSC RingBuffer
→ service_uart_read()
→ service_uart_wait_event()
→ service_uart_write()
→ error / data-loss status
```

Ymodem 不直接调用 STM32 HAL UART，不重新创建 DMA RX，不自行维护第二套 UART RingBuffer。

### 4.2 CRC Common

Packet CRC 直接复用 S04：

```text
CRC-16/XMODEM
poly   = 0x1021
init   = 0x0000
refin  = false
refout = false
xorout = 0x0000
```

Ymodem 不复制第三方 CRC 实现。

### 4.3 Firmware Storage Contract

S04 已冻结：

```text
Slot A: 0x000000 ~ 0x07FFFF
Slot B: 0x080000 ~ 0x0FFFFF

Per Slot:
+0x0000 ~ +0x0FFF : Header Sector
+0x1000 ~          : Payload
```

Firmware Header V1 固定 64 Byte；Payload 最大 508 KiB。

### 4.4 Firmware File Format

`05_Tools/Firmware/pack_firmware.py` 输出紧凑传输文件：

```text
.img file
├─ Header[64]
└─ Payload[image_size]
```

因此：

```text
Ymodem file_size = 64 + header.image_size
```

但 Slot 物理存储不是紧凑布局。

S05 禁止把整个 `.img` 从 `slot_base + 0` 连续原样写入 Flash，否则 Payload 会错误落在 `+0x0040`，破坏 S04 `PAYLOAD_OFFSET = 0x1000` 合同。

## 5. Architecture

### 5.1 Module Structure

建议新增：

```text
03_Firmware/Application/OTA_APP/02_Service/service_ymodem/
├─ ymodem_def.h
├─ ymodem_parser.h
├─ ymodem_parser.c
├─ ymodem_receiver.h
├─ ymodem_receiver.c
└─ ymodem_sink.h
```

阶段静态配置：

```text
03_Firmware/Application/OTA_APP/00_Config/
└─ ymodem_config.h
```

S05 板测 Sink 不进入生产 Ymodem 模块：

```text
04_Test/Board/S05_UART_Ymodem/
├─ app_s05_ymodem_test.h
├─ app_s05_ymodem_test.c
├─ s05_ymodem_flash_sink.h
└─ s05_ymodem_flash_sink.c
```

### 5.2 Responsibility Boundary

```text
service_uart
→ UART 字节搬运、DMA/RingBuffer、事件、错误和 data loss

ymodem_parser
→ byte stream → Packet / EOT / CAN
→ Packet framing
→ Block complement validation
→ CRC-16/XMODEM validation

ymodem_receiver
→ Session 状态机
→ Block 0 / expected block / duplicate block
→ ACK / NAK / 'C' / CAN
→ timeout / retry
→ file lifecycle

ymodem_sink
→ begin / write / end / abort
→ 不包含 Ymodem 协议动作

firmware_storage
→ Slot 逻辑地址与 Flash 物理布局
→ erase / Header write / Payload write / image validation

S07 OTA Service
→ 将来负责 Inactive Slot、版本策略、Metadata PENDING、Reset
```

## 6. Ymodem Protocol Subset

### 6.1 Control Bytes

协议常量属于 `ymodem_def.h`，不是项目 Config：

```text
SOH = 0x01
STX = 0x02
EOT = 0x04
ACK = 0x06
NAK = 0x15
CAN = 0x18
'C' = 0x43
```

### 6.2 Packet Layout

128 Byte Packet：

```text
SOH | Block No | ~Block No | Data[128] | CRC_H | CRC_L
```

总长度：133 Byte。

1 KiB Packet：

```text
STX | Block No | ~Block No | Data[1024] | CRC_H | CRC_L
```

总长度：1029 Byte。

CRC 仅覆盖 Data 区；CRC 高字节先发送。

### 6.3 Block Number

Parser 只检查：

```text
(uint8_t)(block + block_complement) == 0xFF
```

Receiver 再检查协议序号：

```text
block == expected_block
→ 新 Packet

block == (uint8_t)(expected_block - 1)
→ Duplicate Packet，重新 ACK，不重复写入

其他
→ Sequence Error，NAK / Retry
```

Block Number 按 8-bit 自然回卷；不使用文件级 uint8 序号限制文件大小。

### 6.4 Block 0

Block 0 Data：

```text
filename '\0' [space] filesize [space] moddate [space] mode [space] serial
```

S05 必须：

- 在 Packet Data 长度范围内 bounded search 第一个 `\0`；
- filename 不允许越过 `YMODEM_CFG_FILENAME_MAX_LEN`；
- 接受文件名后的一个可选空格；
- filesize 按 ASCII 十进制安全转换为 `uint32_t`；
- moddate 和 mode 按 ASCII 八进制安全转换为 `uint32_t`；
- S05 要求 filesize、moddate 和 mode 按顺序出现，不允许跳过中间字段；
- serial 为可选 ASCII 八进制字段，出现时必须完整解析；
- 拒绝非数字、非法进制字符、空字段、数值溢出、0 Byte 和超过允许大小的文件；
- 严格校验已定义字段，最后一个字段之后除 `\0` 外不得出现未解析数据；
- Empty Block 0 只在 `WAIT_END_HEADER` 状态表示 Session End。

S05 Single-file 模式收到第二个非空 Block 0 时拒绝继续接收第二文件。

## 7. Parser Design

Parser 是增量字节解析器，不假设一次 `service_uart_read()` 等于一个 Ymodem Packet。

### 7.1 Parser States

第一版保持最小状态：

```text
WAIT_START
COLLECT_PACKET
```

`WAIT_START`：

- `SOH` → expected = 133；
- `STX` → expected = 1029；
- `EOT` → emit EOT；
- `CAN` → emit CAN candidate；
- 其他字节 → 忽略或计入 noise statistics。

`COLLECT_PACKET`：

- 所有字节都视为 Packet 内容，包括值等于 SOH / STX / EOT / CAN 的 Payload Byte；
- 收齐 expected bytes 后统一验证；
- Packet 内 CRC 错误后丢弃整个候选 Packet，不在 Payload 内搜索 SOH / STX 重同步。

### 7.2 Parser Interface Direction

第一版优先采用逐 Byte Feed，避免一个批量 feed 输入同时包含多个 Event 时产生剩余输入所有权问题：

```c
platform_error_t ymodem_parser_feed_byte(
    ymodem_parser_t *parser,
    uint8_t byte,
    ymodem_parser_event_t *event,
    ymodem_packet_t *packet);
```

Parser context 必须跨 UART read 调用保留半包。

Parser 最大内部 Packet Buffer：1029 Byte。

## 8. Receiver State Machine

冻结状态：

```text
UNINITIALIZED
IDLE
WAIT_HEADER
RECEIVE_DATA
WAIT_EOT_CONFIRM
WAIT_END_HEADER
FINISHED
ABORTED
ERROR
```

正常流程：

```text
IDLE
 │ start
 │ send 'C'
 ▼
WAIT_HEADER
 │ valid Block 0
 │ parse full Block 0 metadata
 │ sink.begin()
 │ ACK + 'C'
 ▼
RECEIVE_DATA
 │ valid Block N
 │ sink.write(valid_length)
 │ ACK
 │
 │ EOT and received_size == file_size
 ▼
WAIT_EOT_CONFIRM
 │ NAK
 │ second EOT
 │ ACK + 'C'
 ▼
WAIT_END_HEADER
 │ Empty Block 0
 │ sink.end()
 │ ACK
 ▼
FINISHED
```

如果外部 Sender 与经典双 EOT 细节存在兼容差异，可以在不破坏状态边界的前提下增加兼容分支，但不得把不同 EOT 行为散落到 Parser。

## 9. Timeout / Retry / Cancel

### 9.1 Timeout

静态默认值进入 `ymodem_config.h`，具体数值在实现与 Tera Term 联调时验证后冻结。

至少覆盖：

- WAIT_HEADER timeout；
- RECEIVE_DATA packet-start timeout；
- COLLECT_PACKET inter-byte timeout；
- WAIT_EOT_CONFIRM timeout；
- WAIT_END_HEADER timeout。

行为：

```text
WAIT_HEADER timeout
→ resend 'C'

RECEIVE_DATA timeout
→ NAK

WAIT_END_HEADER timeout
→ resend 'C'
```

连续失败达到 `YMODEM_CFG_MAX_RETRY` 后进入 ERROR 并调用 Sink abort。

Retry Counter 表示当前等待目标的连续失败次数；成功接收预期 Packet 后清零。

### 9.2 Cancel

Remote Cancel：协议边界收到 CAN 后进入取消流程；实现可根据 Tera Term 实际兼容性决定是否要求连续两个 CAN。

Local Cancel：提供 Receiver Cancel API；主动取消发送 CAN 序列、调用 Sink abort 并进入 ABORTED。

Payload 内的 `0x18` 只是数据，不得触发取消。

## 10. Sink Contract

Ymodem Receiver 不直接依赖 W25Q64：

```c
typedef struct
{
    platform_error_t (*begin)(void *context,
                              const char *filename,
                              uint32_t fileSize);
    platform_error_t (*write)(void *context,
                              const uint8_t *data,
                              uint32_t length);
    platform_error_t (*end)(void *context);
    void (*abort)(void *context);
    void *context;
} ymodem_sink_t;
```

Receiver 只管理文件字节流，不知道 Slot A/B、Metadata 或 OTA 状态。

## 11. Firmware Storage Write Extension

S05 不采用“任意 Slot-relative Raw Write”作为正式上层接口，而增加符合 S04 Firmware Contract 的逻辑写接口：

```c
platform_error_t firmware_storage_write_payload(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    uint32_t payloadOffset,
    const uint8_t *data,
    uint32_t length);

platform_error_t firmware_storage_write_header(
    firmware_storage_t *storage,
    firmware_slot_t slot,
    const uint8_t rawHeader[FIRMWARE_IMAGE_HEADER_SIZE]);
```

约束：

- `write_payload()` 的 offset 相对 Payload 起点，而不是 Slot Base；
- 实际地址统一由 Storage 转换为 `slot_base + FIRMWARE_PAYLOAD_OFFSET + payloadOffset`；
- 使用 `length > capacity - offset` 形式避免整数加法溢出；
- `write_header()` 只写固定 64 Byte Header；
- Header-last commit 由 S05 Sink / 后续 OTA 编排保证；
- Raw Driver 不新增 Firmware 业务语义。

现有 `firmware_storage_erase_slot()`、`firmware_storage_validate_image()` 保持职责不变。

## 12. S05 Flash Sink

S05 Test Flash Sink 固定目标 Slot B，只用于 Stage 验证。

`.img` 输入为紧凑 `[64B Header][Payload]`，Sink 必须完成逻辑到物理布局转换。

### 12.1 begin()

- 保存 Ymodem `file_size`；
- 检查 `file_size > 64`；
- 检查 `file_size <= 64 + FIRMWARE_SLOT_PAYLOAD_CAPACITY`；
- 初始化 Header 缓存和累计长度；
- 此时不提交 Metadata。

### 12.2 write()

前 64 Byte：

- 跨多个 Ymodem Data Packet 时累计 Header；
- Header 收齐后调用现有 Header decode/validation；
- 检查 `file_size == 64 + header.imageSize`；
- 校验成功后调用 `firmware_storage_erase_slot()`；
- 暂不写 Header。

后续 Payload：

```text
compact .img offset 64
→ Slot payload offset 0
→ physical slot_base + 0x1000
```

通过 `firmware_storage_write_payload()` 流式写入。

### 12.3 end()

只有以下条件全部满足才提交 Header：

- Ymodem received_size == Block 0 file_size；
- Header 已完整且合法；
- Payload 已完整写入；
- Payload written size == header.imageSize；
- 必要的写后检查通过。

然后：

```text
firmware_storage_write_header()
→ firmware_storage_validate_image()
```

只有最终 image validation 为 VALID，S05 文件接收才判定 PASS。

### 12.4 abort()

- 不提交 Header；
- 不写 EEPROM Metadata；
- 不标记 Slot VALID / PENDING；
- 已写入的 Payload 可留在 Flash，下一次 begin 重新 erase；
- 因 Header 未提交，半成品不能通过正常 image validation。

## 13. Config Policy

新增 `00_Config/ymodem_config.h`，只放编译期策略，不放协议固定常量。

建议字段：

```text
YMODEM_CFG_FILENAME_MAX_LEN
YMODEM_CFG_PACKET_TIMEOUT_MS
YMODEM_CFG_INTERBYTE_TIMEOUT_MS
YMODEM_CFG_MAX_RETRY
YMODEM_CFG_UART_READ_BUFFER_SIZE
YMODEM_CFG_SINGLE_FILE_ONLY
```

以下不得进入 Config：

```text
SOH/STX/EOT/ACK/NAK/CAN/'C'
128/1024 Packet protocol sizes
CRC-16/XMODEM polynomial
Firmware Slot physical contract
```

## 14. PC Sender Tooling

S05 参考 Sender 固定为 Tera Term 5 的 YMODEM Sender。

官方能力：

- `ttermpro.exe /C=<n> /BAUD=<speed> /M=<macro>` 可启动串口和宏；
- TTL Macro `ymodemsend <filename>` 执行 Ymodem Sender；
- `result = 1` 表示 transfer success，`0` 表示 failure。

参考：

- https://teratermproject.github.io/manual/5/en/commandline/teraterm.html
- https://teratermproject.github.io/manual/4/en/macro/command/ymodemsend.html

仓库计划增加：

```text
05_Tools/TeraTerm/send_ymodem.ttl
05_Tools/Scripts/send_ymodem.bat
```

本机 Tera Term 安装路径只允许写入被 Git 忽略的：

```text
05_Tools/Config/toolchain.local.bat
```

版本化 `toolchain.local.example.bat` 只增加空变量，例如：

```bat
set "TERATERM_EXE="
```

不得把当前开发机的绝对路径提交到仓库。

`send_ymodem.bat` 第一版接受：

```text
COM number
baud rate
firmware .img path
```

Task 1 先验证：

- 本机配置加载成功；
- Tera Term executable 可找到；
- 宏参数可正确传递；
- 无 Receiver 时得到预期 transfer failure / timeout，而不是脚本自身崩溃。

真正 transfer PASS 必须等最小 MCU Receiver 实现后再验证。

## 15. Receiver Public Interface Direction

第一版 Receiver 可以直接依赖已经存在且稳定的 `service_uart_t`，不提前增加通用 Transport Interface。

建议对象：

```text
ymodem_receiver_t
├─ config
├─ context
│  ├─ state
│  ├─ parser
│  ├─ expectedBlock
│  ├─ retryCount
│  ├─ block0Metadata
│  │  ├─ filename
│  │  ├─ fileSize
│  │  ├─ modificationTime
│  │  ├─ fileMode
│  │  └─ serialNumber
│  ├─ receivedSize
│  ├─ fileStarted
│  └─ lastError
└─ statistics
```

建议 API：

```c
platform_error_t ymodem_receiver_init(...);
platform_error_t ymodem_receiver_start(...);
platform_error_t ymodem_receiver_process(...);
platform_error_t ymodem_receiver_cancel(...);
platform_error_t ymodem_receiver_get_status(...);
platform_error_t ymodem_receiver_get_statistics(...);
```

S06 引入 RTOS 后，可以改变 `process()` 的调用上下文，不改变 Ymodem 协议核心。

## 16. Statistics / Diagnostics

第一版至少保留：

```text
packet_received
packet_accepted
bytes_received
bytes_written
crc_error_count
sequence_error_count
duplicate_packet_count
timeout_count
retry_count
cancel_count
```

RTT / EasyLogger 应输出：

- Session begin；
- Block 0 完整元数据（filename / filesize / moddate / mode / serial）；
- Header validation；
- transfer progress；
- retry reason；
- cancel / timeout；
- EOT / session finish；
- final Firmware Image validation result。

高频 Packet 正常日志不得默认输出到过高等级，避免日志本身影响 UART 传输时序。

## 17. Validation Strategy

### 17.1 Host Test

必须覆盖：

- SOH 128 packet；
- STX 1 KiB packet；
- fragmented feed；
- multiple packets across arbitrary UART chunks；
- Block complement error；
- CRC error；
- valid Block 0；
- missing NUL；
- invalid / overflow filesize；
- expected packet；
- duplicate packet；
- sequence error；
- EOT；
- CAN；
- timeout / retry reset / retry exceeded；
- Sink begin/write/end/abort call order；
- Firmware Storage payload/header write bounds；
- Header-last failure behavior。

### 17.2 Board Test

至少验证：

1. Tera Term 发送合法 `.img`；
2. STM32 完成 Ymodem Session；
3. Block 0 完整元数据正确；
4. SOH / STX 可兼容；
5. Slot B Header / Payload 落在正确物理区域；
6. `firmware_storage_validate_image()` 返回 VALID；
7. Ymodem file_size 与 `64 + image_size` 一致；
8. Cancel 后可再次启动新 Session；
9. 中途停止 Sender / Timeout 后可恢复；
10. 失败传输不提交 Header；
11. RTT 无 data-loss / fatal UART error。

### 17.3 End-to-end Acceptance

最终链路：

```text
pack_firmware.py
→ firmware.img
→ send_ymodem.bat
→ Tera Term YMODEM
→ STM32 service_uart
→ service_ymodem
→ S05 Flash Sink
→ firmware_storage
→ W25Q64 Slot B
→ firmware_storage_validate_image()
→ VALID
```

如果 Ymodem transfer 报 success 但最终 Firmware Image validation 失败，则 S05 验收失败。

## 18. Deferred Items

S04 跨阶段延期回归仍保持：

```text
Reset Persistence       PENDING / DEFERRED
Power-cycle Persistence PENDING / DEFERRED
```

不阻塞 S05，但必须在 S07 关闭前完成。

S05 不把这些结果改写成 PASS。

## 19. Stage Completion Criteria

S05 可以进入 Verification / Review 的最低条件：

- Tera Term Ymodem 自动化发送入口可被本地 Agent / 人工稳定调用；
- Parser / Receiver Host Test 通过；
- Firmware Storage 新写接口 Host Test 通过；
- Keil normal build / clean rebuild 通过；
- PC → Ymodem → STM32 → Slot B 完整板测通过；
- 最终 `firmware_storage_validate_image()` 返回 VALID；
- Cancel / Timeout / retry / duplicate 等主要异常路径有验证证据；
- 失败传输不提交有效 Header；
- 阶段验证报告明确区分 Host / Build / Hardware Evidence；
- Review 无未解决的阶段内阻塞问题。

## 20. Design Decisions Summary

```text
Ymodem is implemented, not imported as a black-box library.
Receiver-only / single-file for S05.
Parser / Receiver / Sink are separated.
Existing service_uart remains transport owner.
Existing CRC-16/XMODEM is reused.
Ymodem does not know Slot or OTA Metadata.
Firmware Storage gains logical Header/Payload write APIs.
Compact .img is translated into the existing non-contiguous Slot layout.
Header is committed last.
Tera Term 5 is the independent PC reference Sender.
Machine-specific Tera Term path remains local-only.
```

## 21. Board Test Task Ownership Adjustment (2026-09-15)

S05 Board Test uses a dedicated `s05Ymodem` Platform Thread.

```text
appSystem
  └─ create s05Ymodem and keep the application task alive

s05Ymodem
  ├─ initialize Storage and UART Service
  ├─ own service_uart consumer context
  ├─ run YMODEM Receiver state machine
  ├─ write Slot B through S05 Flash Sink
  └─ validate the final image
```

The `service_uart` RingBuffer remains a single-consumer path: UART event delivery is performed from ISR context, and `s05Ymodem` performs the read and YMODEM feed. The UART read loop and YMODEM parser are not assigned to separate competing consumers.

## 22. Final Stage Closure (2026-09-15)

S05 Verification / Review 已通过。默认板测发送端为 `05_Tools/Scripts/send_ymodem.bat` 调用 Tera Term 5；必须先打开 CH340 串口并进入等待 `C` 状态，再通过 J-Link 烧录或复位目标板。Python Sender 保留为 Host 和诊断辅助工具，不作为 S05 默认板测入口。

最终正常传输、传输中止后的恢复传输以及 Slot B 镜像校验结果见 `04_Test/Reports/Stages/S05_UART_Ymodem/verification.md`。S05 板测入口已从正式 Application 启动和 Keil 生产 target 移除，仍保留在 `04_Test/Board/S05_UART_Ymodem` 供后续回归使用。
