# S04 Firmware Image Storage Handoff

## Metadata

- Stage: `S04_Firmware_Image_Storage`
- Status: `READY_FOR_VERIFICATION`
- Branch: `main`
- Baseline Commit: `245cd3ee550a2c2cc016e6a197609d712ef1e893`
- Design Commit: `e701910a0452952c632ad8352c6973eedf06b283`
- Implementation Plan Commit: `bc5360fa40188c189a9e19b91a29ad5d266d8220`
- Review Skeleton Commit: `39602db5997c3277ff764d4a85ac029799b7ee85`
- Plan Owner Acceptance: `PASS`
- Implementation Commit: `647f32f`
- Toolchain Commit: `1f756f0`
- Verification Commit: `72a7403`
- Review Commit: `f2b6ed9`
- Updated At: `2026-09-14`

## Current Objective

S04 设计与 `implementation_plan.md` 均已由 Project Owner 批准；Task 1-8 实现、Host 验证、Keil 构建和主流程实板验证已完成，Application 工具链冒烟验证和本轮范围化 Review 也已完成，阶段仍为 `READY_FOR_VERIFICATION`。

后续如继续施工或补充验证，必须严格遵循冻结设计、计划任务顺序与范围边界。若发现设计冲突，不得自行改协议或扩展 OTA 范围，应先记录并回到设计/阻塞处理。

## Required Reading

实施前按顺序读取：

1. `AGENTS.md`
2. `README.md`
3. `PROJECT_CONTEXT.md`
4. `00_Project/WORKFLOW.md`
5. `00_Project/01_Requirements/项目需求V1.md`
6. `00_Project/02_Roadmap/development_roadmap.md`
7. `00_Project/03_Stages/S04_Firmware_Image_Storage/design.md`
8. `00_Project/03_Stages/S04_Firmware_Image_Storage/implementation_plan.md`
9. `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`
10. `03_Firmware/AGENTS.md`
11. `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`
12. `03_Firmware/00_Doc/Standards/Keil工程与构建输出规范.md`
13. `03_Firmware/Application/OTA_APP/02_Service/service_uart/service_uart.h/.c`
14. `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_w25q64.h/.c`
15. `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/at24c02/platform_at24c02.h/.c`
16. `04_Test/Board/S02_External_Flash_Driver/` 与 `04_Test/Board/S03_EEPROM_Storage/` 现有板测模式。

## Frozen Design Summary

### External Flash

```text
Slot A: 0x000000 ~ 0x07FFFF, 512 KiB
Slot B: 0x080000 ~ 0x0FFFFF, 512 KiB
Reserved: 0x100000 ~ 0x7FFFFF
```

每个 Slot：

```text
+0x0000 ~ +0x0FFF : Header Sector, 4 KiB
+0x1000 ~          : Firmware Payload
```

Payload Capacity = `508 KiB`。

### Firmware Header V1

- 固定 64 Byte；
- little-endian；
- Magic = `0x4D495746` (`FWIM`)；
- Format Version = `1`；
- Header Size = `64`；
- Version = major/minor/patch/reserved，各 `uint16_t`；
- Image Size = Payload 实际字节数；
- Payload CRC32 = CRC-32/ISO-HDLC；
- Header CRC32 覆盖 Header `0x00~0x3B`；
- V1 Reserved 全 `0x00`；
- 不允许直接持久化 C struct layout。

### CRC Common

S04 新增：

- CRC-8/SMBUS；
- CRC-16/XMODEM；
- CRC-32/ISO-HDLC；
- one-shot + streaming；
- V1 软件 bitwise implementation；
- 不依赖 HAL / RTOS / STM32 CRC Peripheral。

### AT24C02 Metadata

```text
0x00 ~ 0x7F : Copy A
0x80 ~ 0xFF : Copy B
```

每份 128 Byte，采用：

```text
Double Copy
+ uint32_t sequence
+ CRC-32/ISO-HDLC
+ commit marker
```

S04 基础字段：

- active_slot；
- confirmed_slot；
- slot_a_state；
- slot_b_state；
- confirmed_version。

S04 Slot State 只定义：

```text
EMPTY
VALID
INVALID
```

不提前引入 `PENDING / INSTALLING / TRIAL / CONFIRMED / ROLLBACK`。

### Authority Boundary

```text
W25Q64 Image Header
→ Firmware 实际 Version / Size / CRC

AT24C02 Metadata
→ active / confirmed / slot state / confirmed_version
```

EEPROM 不重复保存每个 Slot 的 image size / payload CRC。

### Application Module Boundary

计划新增：

```text
02_Service/
├─ service_common/
│  └─ crc/
└─ service_firmware/
   ├─ firmware_def
   ├─ firmware_version
   ├─ firmware_image
   ├─ firmware_metadata
   └─ firmware_storage
```

当前只有 Application 一个真实消费者，因此不提前把代码放入 `03_Firmware/Shared`。S08 Bootloader 成为第二个消费者后，再评估将稳定纯格式/算法代码抽入 `03_Firmware/Shared/Firmware`。

### Validation Boundary

必须区分：

```text
Image INVALID
≠
Validation I/O Failure
```

W25Q64 / SPI 访问失败时返回底层错误，Validation Result 保持 UNKNOWN，不得自动把 EEPROM Slot State 改成 INVALID。

`firmware_storage_validate_image()` 为只读验证操作，不自动提交 Metadata。

## Implementation Plan Task Map

正式计划：`00_Project/03_Stages/S04_Firmware_Image_Storage/implementation_plan.md`

任务顺序：

```text
Task 1  CRC Common + standard host vectors
Task 2  Firmware Version + Header V1 format
Task 3  Metadata V1 codec + double-copy selection
Task 4  Firmware Storage service + storage host stubs
Task 5  PC pack_firmware.py + cross-language contract test
Task 6  Keil production integration
Task 7  UART → fixed Slot B isolated board test
Task 8  Remove destructive test from production path + verification handoff
```

实现原则：Host Test 能覆盖的格式、CRC、Metadata 与 Storage 编排先在 PC 上验证；真实 Flash / EEPROM / UART / persistence 再由板测确认。

## S04 Board Test Contract

### Test Input

PC 工具：

```text
05_Tools/Firmware/pack_firmware.py
```

输入 APP `.bin` + version，输出：

```text
[64 Byte Header][Payload]
```

通过串口助手 raw binary send。

### Fixed Target

```text
Slot B Base     = 0x080000
Payload Address = 0x081000
```

不增加 Slot 选择 UART 命令协议。

### Existing UART Reuse

复用现有 `service_uart`：DMA RX、RingBuffer、wait event、read、data loss detection、UART error handling。

Board Test 在当前 `appSystem` Task 中执行时，通过 `platform_thread_get_current()` 获取 owner thread，不新增为了测试暴露的系统线程接口。

### Image Commit Rule

```text
Receive/validate Header in RAM
→ erase Slot B required sectors
→ write Payload first
→ streaming Payload CRC PASS
→ write Header last
→ full re-read validate
```

只有 Header 最后写入后，Image 才被视为已提交。

传输中断、UART Data Loss、UART Error、CRC mismatch 时不得写 Header；Header 保持擦除态，Slot 识别为 EMPTY。

### Diagnostics

统一：`SEGGER RTT + EasyLogger`。

至少输出：erase、Header fields、Version、Size、Expected/Calculated CRC32、receive progress、Header commit、full image validation、Metadata Copy A/B、selected sequence、metadata commit、final result。

### Retained Test Location

```text
04_Test/Board/S04_Firmware_Image_Storage/
```

验收后必须从 production `app_main()` 和正式 Keil target 移除；测试源码继续留在 `04_Test/Board`。

## Required Verification Cases

至少覆盖：

1. Valid image；
2. Payload one-byte corruption；
3. Header one-byte corruption；
4. erased Header → EMPTY；
5. invalid image_size；
6. UART interrupted transfer → Header not committed；
7. UART data loss/error → abort；
8. Metadata A valid / B invalid；
9. Metadata A invalid / B valid；
10. Metadata A/B valid sequence selection；
11. sequence wrap-around；
12. target Metadata copy incomplete commit → previous copy recovery；
13. Reset Persistence；
14. Power-cycle Persistence；
15. PC pack tool ↔ C decoder compatibility；
16. Keil normal build / clean rebuild；
17. RTT real-board evidence；
18. destructive board-test cleanup。

## Allowed Changes

- `02_Service/service_common/crc/`；
- `02_Service/service_firmware/`；
- 必要的 `project_config.h` test gate；
- `MDK-ARM/OTA_APP.uvprojx`；
- `05_Tools/Firmware/`；
- `04_Test/Host/S04_Firmware_Image_Storage/`；
- `04_Test/Board/S04_Firmware_Image_Storage/`；
- 必要的临时 `app_main.c` board-test 接入；
- S04 文档 / 状态文件；
- 后续 Verification Report。

## Prohibited Changes

- 不返工 S02/S03 已关闭 Raw Driver，除非发现明确阻塞 Bug 并升级设计；
- 不把 Firmware / Metadata 业务语义塞入 Raw Driver；
- 不实现 Ymodem；
- 不实现正式 OTA Service；
- 不实现 Bootloader 安装；
- 不实现 Trial / Confirm / Rollback；
- 不引入大型通用 Storage / Device Manager；
- 不提前把单一消费者代码移入 Shared；
- 不因一次 I/O Error 将 Slot 永久标记为 INVALID；
- 不将临时破坏性板测长期留在 production startup / Keil target。

## Current Status

```text
Design Discussion      PASS
Design Approval        PASS
Design Document        CREATED
Implementation Plan    CREATED
Plan Owner Acceptance  PASS
Stage Status           READY_FOR_VERIFICATION
Production Code        COMPLETE (S04 destructive board test removed from production startup and Keil target)
Verification           IN_PROGRESS (report created; Reset/Power-cycle persistence pending)
Review                 SCOPED PASS (persistence deferred; stage not closed)
```

## Implementation Progress

- Task 1 — CRC Common：已新增 CRC-8/SMBUS、CRC-16/XMODEM、CRC-32/ISO-HDLC 的软件 bitwise 实现和 Host Test；标准向量与流式分块计算已验证。提交记录见当前阶段分支历史。
- Task 2 — Firmware Version / Header V1：已新增固定 Slot 合同、Version 比较与校验、64 Byte Header 固定偏移编码/解码和 Header 校验；Host Test 已覆盖 fixed-offset、little-endian、erased、reserved、size 与 CRC 路径。提交记录见当前阶段分支历史。
- Task 3 — Metadata V1：已新增 128 Byte 双副本 fixed-offset 编解码、CRC32、commit marker、字段范围校验与 wrap-around-safe sequence 选择；Host Test 已覆盖 A/B 有效副本选择、序列回绕、保留区篡改、CRC 与未提交 marker 恢复。提交记录见当前阶段分支历史。
- Task 4 — Firmware Storage Service：已新增 W25Q64/AT24C02 存储编排、整镜像流式 CRC 验证、I/O 与镜像无效区分、Metadata 双副本原子提交；Host Stub 已覆盖提交中断后的旧副本恢复与 sequence 递增。提交记录见当前阶段分支历史。
- Task 5 — PC Firmware Pack Tool：已新增固定偏移 little-endian 打包工具与 Python 单元测试；Python 生成 `.img` 已由 C Host Test 验证 Header、Version、长度及 Payload CRC 合同一致。提交记录见当前阶段分支历史。
- Task 6 — Keil Production Integration：已将 CRC 与 4 个 Firmware production source 加入独立 Keil Group，并追加对应 include path；最终 normal build 与清理输出后的完整重建均为 0 errors、14 个既有 platform/vendor warnings，S04 源码无新增 warning。
- Task 7 — S04 UART → Slot B Board Test：已完成 UART Service 生命周期修正、USART1 IDLE 中断接入、64 Byte Header 分片接收、固定 Slot B 擦除、Payload 流式写入与 CRC、Header 最后提交、整镜像回读，以及 Metadata 双提交和单副本破坏恢复。干净复位/重新烧录后的 RTT 实板主流程 PASS，证据见 `04_Test/Reports/Stages/S04_Firmware_Image_Storage/verification.md` 和 `06_Output/Logs/S04_board_test_rtt_clean.log`。
- Task 8 — Board-test cleanup：已移除 `PROJECT_ENABLE_S04_BOARD_TEST`、Application 启动路径中的板测分支、正式 Keil Target 中的板测源文件及其 include path；板测源码保留在 `04_Test/Board/S04_Firmware_Image_Storage/`。

## Application Toolchain Handoff

统一入口及职责如下：

| 入口 | 职责 | 主要输出 |
| --- | --- | --- |
| `05_Tools/Scripts/build_app.bat` | 调用 Keil 编译 `OTA_APP` Target | `06_Output/Logs/OTA_APP_build.log` |
| `05_Tools/Scripts/flash_app.bat` | 使用 J-Link / SWD 下载并运行 `OTA_APP.hex` | `06_Output/Logs/OTA_APP_flash.log` |
| `05_Tools/Scripts/rtt_capture.bat [seconds]` | 使用 J-Link RTT Logger 采集 Up Channel 0 | `OTA_APP_rtt.log`、`OTA_APP_rtt_logger.log` |
| `05_Tools/Scripts/run_app_cycle.bat [seconds]` | 执行 Build → Flash → RTT Capture | 上述日志 |
| `05_Tools/Firmware/pack_firmware.py` | 生成 S04 `[64 Byte Header][Payload]` 镜像 | 用户指定 `.img` |

本机配置从 `05_Tools/Config/toolchain.local.example.bat` 复制为被 Git 忽略的
`toolchain.local.bat`，填写 `KEIL_UV4`、`JLINK_EXE`、`JLINK_RTT_LOGGER`、
`JLINK_DEVICE`、`JLINK_IF`、`JLINK_SPEED`、`JLINK_RTT_CHANNEL` 和
`RTT_CAPTURE_SECONDS`。真实路径不得提交。

2026-09-14 工具冒烟结果：`build_app.bat` 编译 0 Error / 0 Warning；
`flash_app.bat` 成功连接 STM32F411CE 并完成下载校验；`run_app_cycle.bat 10`
返回 0，RTT Logger 找到 Control Block 并捕获 415 Byte 启动日志。该结果只证明
工具链动作可用，不替代 S04 Reset / Power-cycle Persistence 验证。

## Task 7 Hardware Execution Gate

当前固件使用 `115200 8N1`，测试输入仍为 `[64 Byte Header][image_size Byte Payload]` raw binary。由于冻结流程要求先验证 Header、再擦除 Slot B，且 S04 不实现流控或正式 Transport，人工发送时必须：

1. 先发送 `.img` 的前 64 Byte；
2. 等待 RTT 输出 `erase complete, send payload bytes=...`；
3. 再发送同一 `.img` 从 offset `64` 开始的全部 Payload；
4. 收集直到 `[S04] final result=PASS` 的 RTT 日志；
5. 按 Required Verification Cases 执行中断、CRC、单副本、Reset 与 Power-cycle 场景。

如果擦除期间连续发送整个文件导致 RingBuffer Data Loss，板测会按设计 abort 且不提交 Header；这属于安全失败路径，不得视为正常传输方式。

2026-09-14 J-Link V9（S/N `602713300`，`VTref≈3.28V`）已稳定连接 STM32F411CE；COM3 为 USART1 对应 J-Link CDC UART。干净复位/重新烧录后完成 Header、Payload CRC、Header commit、整镜像回读和 Metadata 恢复验证，最终 RTT 结果为 `PASS`。第一次未复位重跑的 CRC 错误由前一次残留 64 Byte Header 造成，已通过干净重跑排除。

## Next Action

Project Owner 本轮决定暂不执行 Reset Persistence / Power-cycle Persistence；两项仍保持 `PENDING`。
本轮已完成范围化 Review：实现、接口、架构、文档和已完成验证证据为 PASS；后续如需关闭
S04，Verification Role 必须补充真实板测证据后再执行最终阶段关闭审查，当前不得将阶段标记为 `CLOSED`。

Task 1 → Task 8 已完成；本轮 Review 不改变 `READY_FOR_VERIFICATION` 状态。后续由
Verification/Review Role 根据补充的持久性证据决定最终阶段结论。
