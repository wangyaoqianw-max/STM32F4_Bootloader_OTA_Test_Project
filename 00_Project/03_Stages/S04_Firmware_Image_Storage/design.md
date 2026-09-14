# S04 Firmware Image Storage Design

## 1. Stage Metadata

- Stage: `S04_Firmware_Image_Storage`
- Status: `DESIGN_APPROVED`
- Branch: `codex/s04-firmware-image-storage`
- Baseline Commit: `245cd3ee550a2c2cc016e6a197609d712ef1e893`
- Prerequisites: `S02_External_Flash_Driver` CLOSED, `S03_EEPROM_Storage` CLOSED
- Updated At: `2026-09-13`

## 2. Goal

建立 Firmware Image、External Flash A/B Slot、Firmware Header、Firmware Version、CRC32、AT24C02 Metadata 双副本以及镜像验证的基础存储模型，使 Application 能够在不实现正式 OTA Transport 的前提下：

- 识别 Slot A / Slot B；
- 识别 Firmware Header / Version / Size / CRC；
- 区分 EMPTY / VALID / INVALID 镜像；
- 可靠读取和提交基础 Metadata；
- 通过 UART 测试注入一个打包 Firmware Image 至固定 Slot B；
- 通过 RTT + EasyLogger 输出真实板测证据；
- 为后续 Application OTA Service 与 Bootloader 建立稳定的数据契约。

S04 不实现正式 OTA 下载、Firmware 安装、Trial / Confirm / Rollback。

## 3. Scope Boundary

### 3.1 Included

- W25Q64 Slot A / Slot B 固定分区；
- 每个 Slot 独立 Header Sector；
- Firmware Image Header V1 二进制格式；
- Firmware Version V1 数据模型和比较规则；
- CRC Common：CRC-8/SMBUS、CRC-16/XMODEM、CRC-32/ISO-HDLC；
- Header CRC32 / Payload CRC32；
- AT24C02 Metadata V1 双副本模型；
- Metadata sequence / CRC / commit marker；
- Metadata Copy A / Copy B 选择与提交规则；
- Slot EMPTY / VALID / INVALID 基础状态；
- Application `service_firmware` 模块边界；
- S04 Test-only UART raw image injection；
- PC Firmware pack tool；
- RTT + EasyLogger 板测。

### 3.2 Excluded

- UART / Ymodem 正式 Firmware Transport；
- Retry / Cancel / Packet CRC / Ymodem 状态机；
- Application OTA Service；
- FreeRTOS OTA 并发模型；
- Bootloader Internal Flash 安装；
- `PENDING / INSTALLING / TRIAL / CONFIRMED / ROLLBACK` OTA 状态机；
- Watchdog / Boot Failure Counter；
- SHA / AES / HMAC / Digital Signature；
- Device Manager / `platform_storage_device_t` / 通用 NVM Manager；
- W25Q64 EEPROM Emulation。

## 4. Existing Storage Baseline

S04 直接复用已经关闭并验证的 Raw Driver：

```text
W25Q64 Raw Driver
├─ 8 MiB address space
├─ read
├─ page program / cross-page write
└─ 4 KiB sector erase

AT24C02 Raw Driver
├─ 256 Byte address space
├─ read
├─ 8 Byte page split write
└─ bounded ACK polling
```

S04 不向 Raw Driver 注入 Firmware / OTA 业务语义。

## 5. W25Q64 A/B Slot Layout

W25Q64 总容量：`8 MiB`，地址范围 `0x000000 ~ 0x7FFFFF`。

冻结布局：

```text
0x000000 ┌─────────────────────────────┐
         │ Slot A                      │
         │ Size = 0x080000 = 512 KiB  │
0x07FFFF └─────────────────────────────┘

0x080000 ┌─────────────────────────────┐
         │ Slot B                      │
         │ Size = 0x080000 = 512 KiB  │
0x0FFFFF └─────────────────────────────┘

0x100000 ┌─────────────────────────────┐
         │ Reserved                    │
         │ 7 MiB                       │
0x7FFFFF └─────────────────────────────┘
```

常量：

```text
SLOT_A_BASE          = 0x000000
SLOT_B_BASE          = 0x080000
SLOT_SIZE            = 0x080000
HEADER_SECTOR_SIZE   = 0x001000
PAYLOAD_OFFSET       = 0x001000
SLOT_PAYLOAD_CAPACITY= 0x07F000 = 508 KiB
```

每个 Slot：

```text
slot_base + 0x0000 ~ +0x0FFF
→ Header Sector，4 KiB

slot_base + 0x1000 ~
→ Firmware Payload
```

Header Sector 独立存在，Header 本身只占前 64 Byte，其余空间保持擦除态或保留。

Firmware Payload 地址永远由固定 `PAYLOAD_OFFSET = 0x1000` 决定，不使用 `header_size` 推导 Payload 地址。

## 6. Firmware Version Contract

版本使用固定整数结构，不使用字符串：

```text
firmware_version_t
├─ uint16_t major
├─ uint16_t minor
├─ uint16_t patch
└─ uint16_t reserved
```

- 总大小：8 Byte；
- V1 `reserved` 必须为 `0`；
- 比较顺序：`major → minor → patch`；
- Version Compare 只比较版本，不决定是否允许升级；
- 正常升级 / 回滚的允许策略由后续 OTA / Boot Policy 决定。

## 7. Firmware Image Header V1

### 7.1 Binary Contract

Header 固定 64 Byte，little-endian。

```text
Offset  Size  Field
-----------------------------------------
0x00    4     magic
0x04    2     format_version
0x06    2     header_size
0x08    2     version.major
0x0A    2     version.minor
0x0C    2     version.patch
0x0E    2     version.reserved
0x10    4     image_size
0x14    4     payload_crc32
0x18    36    reserved
0x3C    4     header_crc32
-----------------------------------------
Total         64 Byte
```

冻结值：

```text
magic          = 0x4D495746  // little-endian bytes: "FWIM"
format_version = 1
header_size    = 64
reserved       = all 0x00 in V1
```

### 7.2 Image Size

`image_size` 只表示原始 Firmware Payload（APP `.bin`）长度，不包括：

- Header；
- Header Sector padding；
- Slot padding。

必须满足：

```text
image_size > 0
image_size <= SLOT_PAYLOAD_CAPACITY
```

未来 Bootloader 安装前还必须检查 Internal Flash APP Capacity；该检查不属于 S04。

### 7.3 CRC Coverage

Header CRC：

```text
CRC-32/ISO-HDLC(Header[0x00 ... 0x3B])
```

即覆盖前 60 Byte，排除 `header_crc32` 自身。

Payload CRC：

```text
CRC-32/ISO-HDLC(Payload[0 ... image_size - 1])
```

只覆盖真实 Firmware Payload。

### 7.4 Encoding / Decoding

禁止把编译器 C `struct` layout 直接作为持久化或跨工具协议格式。

必须通过固定 offset 的 raw buffer encode / decode：

```text
64 Byte raw buffer
↕
firmware_image_encode_header()
firmware_image_decode_header()
```

这样 PC 工具、Application 和未来 Bootloader 使用同一二进制合同，不依赖结构体 padding / alignment。

## 8. Firmware Header Authority

W25Q64 Image Header 是 Firmware 镜像物理属性的权威来源：

```text
Firmware Version
Image Size
Payload CRC32
Header CRC32
```

EEPROM 不重复保存每个 Slot 的 `image_size / payload_crc32`。

原则：

```text
Image Header
→ "这个 Firmware 是什么"

EEPROM Metadata
→ "系统当前信任谁、准备怎样使用这些 Slot"
```

## 9. CRC Common Module

S04 增加独立 CRC Common，不将 CRC 实现绑定到 Firmware 模块。

建议位置：

```text
03_Firmware/Application/OTA_APP/02_Service/service_common/crc/
├─ crc.h
└─ crc.c
```

冻结算法：

| Algorithm | Polynomial | Init | RefIn | RefOut | XorOut |
| --- | --- | --- | --- | --- | --- |
| CRC-8/SMBUS | `0x07` | `0x00` | false | false | `0x00` |
| CRC-16/XMODEM | `0x1021` | `0x0000` | false | false | `0x0000` |
| CRC-32/ISO-HDLC | `0x04C11DB7` | `0xFFFFFFFF` | true | true | `0xFFFFFFFF` |

用途：

```text
CRC-8/SMBUS
→ 通用基础能力，S04 不强制业务使用

CRC-16/XMODEM
→ 为 S05 Ymodem Packet CRC 预留

CRC-32/ISO-HDLC
→ Firmware Header
→ Firmware Payload
→ EEPROM Metadata
```

V1 使用纯软件 bitwise implementation，不依赖 STM32 CRC Peripheral、HAL 或 RTOS。

CRC API 同时提供：

- one-shot calculate；
- incremental / streaming init-update-finalize。

Firmware Payload CRC 必须支持分块读取，不允许为了 CRC 一次性把几百 KiB Firmware 全部放入 RAM。

## 10. AT24C02 Metadata V1

### 10.1 Double-copy Layout

AT24C02 共 256 Byte，均分为两个 128 Byte Metadata Copy：

```text
0x00 ~ 0x7F  Metadata Copy A
0x80 ~ 0xFF  Metadata Copy B
```

单 Copy 固定格式：

```text
Offset  Size  Field
-----------------------------------------
0x00    4     magic
0x04    2     format_version
0x06    2     metadata_size
0x08    4     sequence
0x0C    1     active_slot
0x0D    1     confirmed_slot
0x0E    1     slot_a_state
0x0F    1     slot_b_state
0x10    8     confirmed_version
0x18    96    reserved
0x78    4     metadata_crc32
0x7C    4     commit_marker
-----------------------------------------
Total         128 Byte
```

冻结值：

```text
magic          = 0x444D5746  // little-endian bytes: "FWMD"
format_version = 1
metadata_size  = 128
reserved       = all 0x00 in V1
commit_marker  = 0x54494D43  // little-endian bytes: "CMIT"
```

Metadata CRC：

```text
CRC-32/ISO-HDLC(Metadata[0x00 ... 0x77])
```

即排除：

```text
metadata_crc32
commit_marker
```

### 10.2 Slot Identity

```text
FIRMWARE_SLOT_A    = 0
FIRMWARE_SLOT_B    = 1
FIRMWARE_SLOT_NONE = 0xFF
```

`NONE` 用于尚未建立有效 active / confirmed 关系的阶段。

### 10.3 Basic Slot State

S04 只定义镜像基础状态：

```text
EMPTY
VALID
INVALID
```

含义：

```text
EMPTY
→ Header 前 64 Byte 全部为 0xFF，当前无已提交镜像

VALID
→ 最近一次完整 Header + Payload 验证通过

INVALID
→ Slot 存在数据，但明确无法作为合法 Firmware Image
```

S04 不把以下 OTA 工作流状态塞入 Slot State：

```text
DOWNLOADING
PENDING
INSTALLING
TRIAL
CONFIRMED
ROLLBACK
```

这些属于后续 `upgrade_state` / OTA 状态机。

### 10.4 Confirmed Version

EEPROM 保存 `confirmed_version`，作为未来 Boot / OTA Version Policy 的稳定版本基准。

它不取代 Header 中的真实 `firmware_version`。

后续策略示例：

```text
Normal OTA
→ candidate_version 与 confirmed_version 比较

Rollback
→ 允许明确记录的 confirmed_slot，即使版本更低
```

具体升级 / 降级策略不在 S04 实现。

## 11. Metadata Atomicity Model

AT24C02 Raw Driver 能保证 Page Split 和单次写周期完成，但不提供跨多字节 Metadata 的掉电原子性。

因此 S04 采用：

```text
Double Copy
+ sequence
+ metadata_crc32
+ commit_marker
```

提交规则：

```text
当前最新 Copy = A, sequence = N
目标 Copy = B

1. 将 B.commit_marker 写为 INVALID / 非 COMMITTED
2. 构造新 Metadata，sequence = N + 1
3. 写入 B Body + metadata_crc32
4. Read Back
5. 校验格式、字段和 metadata_crc32
6. 最后单独写 commit_marker = COMMITTED
7. 再读 commit_marker 确认
```

发生掉电时：

```text
Body 写入前 / 中途掉电
→ 目标 Copy 未提交
→ 旧 Copy 仍有效

CRC 写完但 commit 前掉电
→ 目标 Copy 未提交
→ 旧 Copy 仍有效

commit 完成后掉电
→ 两份可能都有效
→ 使用 sequence 选择较新 Copy
```

读取规则：

```text
A valid, B invalid → A
A invalid, B valid → B
A valid, B valid   → newer sequence
A invalid, B invalid → Metadata unavailable / uninitialized
```

`sequence` 使用 `uint32_t`，比较实现需要考虑 wrap-around，不简单依赖长期 `a > b`。

## 12. Image Validation Model

`firmware_storage_validate_image()` 必须区分：

```text
镜像明确无效
≠
验证过程无法完成
```

建议语义：

```text
Function return
→ 本次验证流程是否成功执行完成

Validation Result
→ Firmware Image 本身的判定
```

可能的 Image Result：

```text
EMPTY
VALID
INVALID_MAGIC
INVALID_FORMAT_VERSION
INVALID_HEADER_SIZE
INVALID_RESERVED
INVALID_VERSION
INVALID_SIZE
INVALID_HEADER_CRC
INVALID_PAYLOAD_CRC
UNKNOWN
```

其中：

```text
W25Q64 read failure
SPI timeout
Driver state error
```

属于 Validation I/O Error：

- Result 保持 `UNKNOWN`；
- 返回底层错误；
- 不得因此把 EEPROM Slot State 永久改成 `INVALID`。

Image Validation 本身是只读操作，不自动修改 Metadata。

## 13. Validation Order

```text
1. Validate slot parameter
2. Resolve slot_base
3. Read Header 64 Byte
4. If all 0xFF → EMPTY
5. Decode Header
6. Check magic
7. Check format_version
8. Check header_size
9. Check reserved fields
10. Check firmware_version format
11. Check image_size range
12. Calculate Header CRC32
13. Compare header_crc32
14. Stream-read Payload
15. Streaming CRC-32/ISO-HDLC
16. Compare payload_crc32
17. VALID
```

## 14. Application Module Boundary

S04 当前只存在 Application 消费者，因此按照仓库 `Shared` 规则，不提前把单一使用方代码放入 `03_Firmware/Shared`。

建议当前新增：

```text
03_Firmware/Application/OTA_APP/02_Service/
├─ service_common/
│  └─ crc/
│     ├─ crc.h
│     └─ crc.c
│
└─ service_firmware/
   ├─ firmware_def.h
   ├─ firmware_version.h/.c
   ├─ firmware_image.h/.c
   ├─ firmware_metadata.h/.c
   └─ firmware_storage.h/.c
```

职责：

```text
firmware_def
→ 固定数据类型、Slot、State、Binary Contract 常量

firmware_version
→ version validation / compare

firmware_image
→ Header encode/decode / format validation

firmware_metadata
→ Metadata encode/decode / CRC / sequence / copy selection

firmware_storage
→ 使用 W25Q64 / AT24C02 Raw Driver 完成实际存储访问与整镜像验证
```

`firmware_image / firmware_metadata / firmware_version` 尽量保持平台无关。

`firmware_storage` 是 Application 当前对 Raw Driver 的存储编排层。

S08 Bootloader 真正成为第二个消费者后，再评估把稳定的纯逻辑迁移到：

```text
03_Firmware/Shared/Firmware/
```

Shared 不直接绑定 Application Platform Driver。

## 15. Driver Ownership

`service_firmware` 不拥有 W25Q64 / AT24C02 Raw Driver 的底层总线生命周期。

建议通过 config / dependency injection 绑定已经初始化的：

```text
platform_w25q64_t *
platform_at24c02_t *
```

Firmware Service 不擅自 stop/deinit 共享 SPI / I2C Bus。

## 16. S04 Test-only Firmware Injection

S04 可以复用现有 `service_uart` 作为板测数据注入通道，但不能提前实现 S05 的正式 Ymodem Transport。

### 16.1 PC Image Package

PC 工具建议：

```text
05_Tools/Firmware/pack_firmware.py
```

输入：

```text
APP.bin
+ version major.minor.patch
```

输出测试镜像：

```text
firmware_x.y.z.img

[64 Byte Header]
[image_size Byte Payload]
```

传输文件不包含 Header Sector 剩余的 `0xFF` padding。

PC 工具必须与 MCU 使用完全相同的 little-endian Binary Contract 和 CRC-32/ISO-HDLC 参数。

### 16.2 Fixed Test Target

S04 板测固定写入：

```text
Slot B
slot_base   = 0x080000
payload_base= 0x081000
```

不为测试设计额外的 Slot 选择 UART 命令协议。

### 16.3 Test Write Order

冻结写入顺序：

```text
1. Board Test 等待 Header 64 Byte
2. Decode + Validate received Header in RAM
3. Erase Slot B Header Sector + required Payload sectors
4. Receive Payload by existing service_uart
5. Stream-write Payload to Slot B + 0x1000
6. Streaming CRC32 while receiving
7. If UART data loss / UART error / CRC mismatch → abort
8. Do not commit Header
9. If Payload complete and CRC matches → write Header last
10. Call firmware_storage_validate_image(SLOT_B)
11. Re-read W25Q64 and recalculate Payload CRC
12. Output result through RTT + EasyLogger
```

Header 最后写入相当于 Image Commit：

```text
传输中断 / Payload 不完整
→ Header Sector 保持 erased
→ Image 识别为 EMPTY
```

这样半成品不能被误识别成有效 Firmware Image。

## 17. Board Test Location

S04 destructive / temporary board test 最终保留于：

```text
04_Test/Board/S04_Firmware_Image_Storage/
```

板测期间可以临时接入 Keil target；验收完成后必须退出生产 Application 正常启动路径和正式 target。

## 18. RTT + EasyLogger Evidence

至少输出：

```text
Slot B erase start/result
Header received
Firmware version
Image size
Expected payload CRC32
Payload receive progress
Calculated payload CRC32
Header commit result
Image validation sub-results
Metadata Copy A/B status
Metadata selected sequence
Metadata commit result
Final board-test result
```

失败日志至少包含：

```text
testcase / stage
offset or address
expected
actual
platform error
validation result
```

## 19. Required Verification Cases

至少包含：

1. Valid Firmware Image → `VALID`；
2. Payload 修改 1 Byte → `INVALID_PAYLOAD_CRC`；
3. Header 修改 1 Byte → `INVALID_HEADER_CRC` 或对应格式错误；
4. Header 前 64 Byte 全 `0xFF` → `EMPTY`；
5. 非法 `image_size` → `INVALID_SIZE`；
6. UART 发送中途停止 → Header 不提交 → Slot B 保持 `EMPTY`；
7. UART RingBuffer Data Loss / UART Error → abort，Header 不提交；
8. Metadata Copy A valid / B invalid → 选择 A；
9. Metadata Copy A invalid / B valid → 选择 B；
10. Metadata A/B 均 valid → 选择较新 sequence；
11. Metadata 目标 Copy 写入中断 / commit marker 未完成 → 旧 Copy 仍可恢复；
12. Metadata Reset Persistence；
13. Metadata Power-cycle Persistence；
14. Keil normal build + clean/rebuild；
15. RTT + EasyLogger 提供真实硬件证据。

## 20. Acceptance Criteria

S04 可以进入 Verification 的最低条件：

- A/B Slot 地址与边界固定且检查正确；
- Header V1 encode/decode 与 PC pack tool 一致；
- CRC-32/ISO-HDLC PC 与 MCU 结果一致；
- CRC 支持 Streaming；
- Slot B 能通过 UART test-only path 写入真实 Firmware Image；
- Header 最后提交，中断传输不会产生假 VALID 镜像；
- `EMPTY / VALID / INVALID` 判定正确；
- Image Invalid 与 Validation I/O Error 明确区分；
- Metadata 双副本、CRC、sequence、commit marker 正常；
- 任一单副本损坏时可恢复另一份；
- Reset / Power-cycle 后 Metadata 可恢复；
- 测试代码最终退出生产启动路径；
- 不引入范围外 OTA / Bootloader / Device Manager 功能。

## 21. Deferred Decisions

延后到后续 Stage：

- `PENDING / TRIAL / CONFIRMED / ROLLBACK` Metadata 字段；
- `pending_slot` / failure counter / rollback request；
- 正常升级 downgrade policy；
- Ymodem Transport；
- Bootloader Shared 抽取与 Boot Policy；
- Internal Flash APP Capacity 最终安装检查；
- Security mechanism。

## 22. Design Approval

上述设计已由 Project Owner 在 S04 Design Discussion 中逐项确认。

当前状态：`DESIGN_APPROVED`。

下一步：生成 `implementation_plan.md` 后，才进入 `READY_FOR_IMPLEMENTATION`，不得直接施工生产代码。
