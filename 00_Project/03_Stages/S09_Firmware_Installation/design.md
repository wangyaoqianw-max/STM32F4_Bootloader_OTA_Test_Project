# S09 Firmware Installation Design

## Metadata

- Stage: `S09_Firmware_Installation`
- Workflow Status: `READY_FOR_IMPLEMENTATION`
- Branch: `main`
- Baseline Commit: `e4eb1ac5704971ed72ec4bdb8986e070752c5390`
- Design Owner: Project Owner
- Updated At: `2026-09-18`

## 1. Goal

在 S08 已验证的独立裸机 Bootloader 基础上，实现从 W25Q64 Pending Slot 到 STM32F411 Internal Flash Application Region 的可靠固件安装。

S09 的职责链固定为：

```text
S07 Application OTA
→ External Flash Candidate
→ Metadata PENDING
→ Reset

S09 Bootloader
→ Consume PENDING
→ Validate Candidate
→ Install to Internal Flash
→ Verify Installed APP
→ Metadata PENDING → TRIAL
→ Jump APP

S10
→ Trial / Confirm / Watchdog / Rollback
```

S09 不把“已安装”误认为“已确认运行成功”。新固件只有在 S10 完成运行确认后才能成为 confirmed firmware。

## 2. Upstream Contracts

### 2.1 Internal Flash Layout

沿用 S08 已冻结布局：

```text
Bootloader  : 0x08000000 ~ 0x0800FFFF, 64 KiB
Application : 0x08010000 ~ 0x0807FFFF, 448 KiB
```

STM32F411CE Sector 4~7 恰好构成完整 Application Region：

```text
Sector 4 : 64 KiB
Sector 5 : 128 KiB
Sector 6 : 128 KiB
Sector 7 : 128 KiB
Total    : 448 KiB
```

S09 Internal Flash Driver 只允许操作 Application Region，禁止提供可表达 Bootloader Region 的通用擦写接口。

### 2.2 External Firmware Layout

沿用 S04/S07：

```text
Slot A: 0x000000 ~ 0x07FFFF
Slot B: 0x080000 ~ 0x0FFFFF

Per Slot:
+0x0000 ~ +0x0FFF : Header Sector
+0x1000 ~          : Firmware Payload
Payload Capacity   : 508 KiB
```

Firmware Image Header V1 固定 64 Byte，Payload 从 `slot_base + 0x1000` 开始。

现有 PC packer 输出 compact image：

```text
[64 Byte Header][Payload]
```

它不是 Slot 内部的线性物理布局。Factory Provisioning / Restore 不得把 compact `.img` 从 slot base 直接线性写入 W25Q64；必须按 Header Sector 与 Payload Offset 合同映射。

### 2.3 Metadata Lifecycle

沿用 S07 已冻结生命周期，不在 S09 重新定义第二套状态机：

```text
NONE → PENDING → TRIAL → NONE
                 ↓
             ROLLBACK → NONE
```

S09 只负责：

```text
PENDING → TRIAL
```

S09 对已有状态的处理：

```text
NONE
→ Validate Internal APP
→ Jump

PENDING
→ Install Candidate
→ Verify
→ TRIAL
→ Jump

TRIAL
→ Do not reinstall
→ Validate Internal APP
→ Jump

ROLLBACK
→ S10 owns rollback execution
```

## 3. Architecture Principles

### 3.1 Lean Bootloader

Bootloader 保持独立、精简和可审查，不复制 Application 的完整五层架构。

建议结构：

```text
OTA_Bootloader/
├─ Boot/
│  ├─ boot_main
│  ├─ boot_jump
│  ├─ boot_validate
│  └─ boot_installer
├─ Firmware/
│  ├─ boot_firmware_def
│  ├─ boot_image
│  ├─ boot_metadata
│  └─ boot_crc32
├─ Drivers/
│  ├─ Bus/
│  │  ├─ SPI/
│  │  └─ SoftI2C/
│  ├─ W25Q64/
│  ├─ AT24C02/
│  └─ InternalFlash/
├─ Config/
├─ Diagnostics/
└─ Vendor/
```

最终目录名可按当前工程风格微调，但职责边界不得改变。

### 3.2 Share Contract, Not Application Implementation

S09 不把 Application 的 `firmware_image / firmware_metadata / firmware_storage / firmware_version` 迁入 Shared。

冻结原则：

```text
共享持久化数据协议
≠
共享 Application 业务实现
```

Bootloader 独立实现轻量解析与状态访问，仅保证与 Application / PC Tool 的 Binary Contract 完全一致。

`03_Firmware/Shared/memory_layout.h` 继续作为 Internal Flash Layout 的共享事实来源。Firmware/Metadata 的 Bootloader 实现不得依赖 Application `platform_error`、Platform Device Object 或 Firmware Storage Service。

必须增加 Compatibility Test，确保 Application、Bootloader、PC Tool 对 Header V1、Metadata V2、CRC 和 Slot Layout 的解释一致。

### 3.3 Mature Project References

本阶段只参考成熟 Bootloader 的设计思想，不直接复制其实现：

- MCUboot：Primary/Secondary、Test/Confirm、状态在操作完成后提交、Boot Policy 与 Flash backend 分离；
- wolfBoot：External Update → Internal Boot、Update/Testing/Success 生命周期、HAL 与更新逻辑分离；
- OpenBLT：STM32F4 Sector Geometry、Flash Region Bounds、Bootloader Region 保护。

本项目不引入 MCUboot swap/scratch/trailer/TLV，也不引入 wolfBoot/OpenBLT 的 GPL 源码。

## 4. Initialization Order

启动依赖顺序冻结为：

```text
MCU / HAL Init
    ↓
Bus Init
├─ SPI
└─ Soft-I2C
    ↓
Device Init
├─ W25Q64
└─ AT24C02
    ↓
Metadata Load
    ↓
Boot Decision
```

规则：

1. Bus 只负责通信机制，不知道具体设备；
2. Device Driver 不重复初始化 Bus；
3. 不引入 Bus Manager / Device Manager；
4. 初始化失败采用 fail-fast，记录明确 Boot Log；
5. Internal Flash 是 MCU 内部资源，不要求单独 Device Init。

## 5. Minimal Driver Scope

### 5.1 SPI Bus

只提供 Bootloader 所需的同步 SPI 基础能力。

不得承载 W25Q64、Firmware Slot 或 OTA 语义。

### 5.2 W25Q64

S09 中 W25Q64 是 immutable firmware source：

```text
W25Q64
├─ init
├─ JEDEC/device check
└─ read
```

不实现：

```text
Page Program
Sector Erase
Chip Erase
Write Enable
SFUD
```

Bootloader 在安装期间不得修改 Candidate Slot。

### 5.3 Soft-I2C / AT24C02

Soft-I2C 提供最小总线能力。

AT24C02 必须支持：

```text
init / ACK probe
read
write
ACK polling
```

AT24C02 不能裁成只读，因为 S09 安装完成后必须原子提交 `PENDING → TRIAL`。

### 5.4 Internal Flash

Internal Flash Driver 只负责机械操作，不知道 Slot、Metadata、PENDING 或 TRIAL。

建议公共能力：

```c
boot_internal_flash_erase_app();
boot_internal_flash_write(offset, data, length);
boot_internal_flash_read(offset, data, length);
```

采用 APP-relative offset，调用者不能表达 Bootloader 地址。

规则：

- `erase_app()` 固定擦除 Sector 4~7；
- 使用 `FLASH_VOLTAGE_RANGE_3` 对应当前约 3.3 V 板级供电；
- 正常 Payload 以 WORD 编程为主；
- 尾部 1~3 Byte 必须安全处理，不修改实际 imageSize / CRC 语义；
- 边界检查使用 overflow-safe 形式；
- 写入过程执行局部 read-back；
- Whole-image CRC 不放进 Internal Flash Driver。

具体 Program Parallelism 以 STM32F411 Reference Manual 与当前 HAL 实际约束为准，实施前复核。

## 6. Lightweight Firmware Components

### 6.1 boot_firmware_def

保存 Bootloader 使用的稳定合同常量和轻量类型：

- Slot A/B/NONE；
- Slot base/size/payload offset；
- Header V1 magic/size/format；
- Metadata V2 地址/大小/magic/commit marker；
- Upgrade State；
- Version 基础字段。

不要引入 Application Platform 类型。

### 6.2 boot_crc32

实现与 Application 完全一致的 CRC-32/ISO-HDLC。

第一版优先简单、可审查的实现；不要求为了速度引入大型查表。

### 6.3 boot_image

Bootloader 只消费 Image，不生产 Image。

最小职责：

```text
read/decode Header
validate Header
validate External Payload CRC
extract version / imageSize / payloadCRC
```

不需要 Application 的 Header encode、OTA sink、Slot write/erase 等接口。

### 6.4 boot_metadata

Bootloader 只实现 Metadata V2 所需最小能力：

```text
decode committed copy
validate copy
select latest copy
load
atomic commit
```

S09 Factory Baseline 必须统一写入 Metadata V2，因此 Bootloader 第一版不要求承担历史 Metadata V1 migration。

如果实施调查发现现有正式 baseline 仍存在 V1 依赖，必须停止并回到设计确认，不得静默改变 Binary Contract。

## 7. Candidate Pre-validation and Destructive Gate

Internal APP erase 是 S09 的 destructive gate。

在跨过该 Gate 前，必须完成所有非破坏性检查：

```text
Metadata = PENDING
→ pendingSlot is A/B
→ pending slot state = VALID
→ read Header
→ Header format / reserved / CRC valid
→ imageSize > 0
→ imageSize <= APP_FLASH_SIZE (448 KiB)
→ External Payload CRC PASS
→ Candidate MSP valid
→ Candidate Reset_Handler valid
→ ALLOW INTERNAL ERASE
```

特别注意：

```text
External Slot Payload Capacity ≈ 508 KiB
Internal APP Capacity          = 448 KiB
```

因此 `imageSize > 448 KiB` 必须在任何 Internal Flash erase 之前拒绝。

Candidate Vector Validation 复用 S08 相同规则。建议将 S08 Validator 重构为“验证给定 MSP / Reset_Handler”的公共核心，再由 Internal APP 和 External Candidate 两条路径分别读取向量后调用。

## 8. Installation Transaction

S09 Installer 采用同步、阻塞、可审查的事务流程：

```text
Metadata PENDING
      ↓
Validate Candidate Header
      ↓
Validate External Payload CRC
      ↓
Validate Candidate Vector
      ↓
====== Destructive Gate ======
      ↓
Erase Internal APP S4~S7
      ↓
W25Q64 → bounded RAM buffer → Internal Flash
      ↓
Per-chunk local read-back
      ↓
Internal Whole-image CRC
      ↓
Internal APP Vector Validation
      ↓
Atomic Metadata PENDING → TRIAL
      ↓
Jump APP
```

建议第一版安装 Buffer 约 256 Byte；最终大小放入 Bootloader Config，根据 Stack/RAM 和实测调整。

不引入：

- DMA；
- RTOS；
- Queue；
- Double Buffer；
- Resume-from-sector-N 安装状态。

### 8.1 Power-loss Strategy

S09 采用 restart-from-zero，不实现复杂 resumable swap。

理由：

```text
External Candidate = durable immutable source
Internal APP       = disposable install target while PENDING
Metadata           = remains PENDING until complete verification
```

因此 TRIAL commit 前任何 reset/power loss：

```text
next boot
→ read PENDING
→ revalidate Candidate
→ erase APP
→ reinstall from beginning
```

## 9. Metadata Atomic Commit

沿用 Application 已验证的双副本语义，不重新发明协议。

AT24C02：

```text
Copy A: 0x00 ~ 0x7F
Copy B: 0x80 ~ 0xFF
```

Commit 顺序：

```text
load A + B
→ select latest valid copy
→ build new Metadata in RAM
→ sequence = old + 1
→ select opposite copy
→ invalidate target commit marker
→ write target body + CRC
→ read-back verify
→ write COMMIT marker LAST
→ read-back marker
→ reload both copies
→ verify new copy is latest and fields match
```

`PENDING → TRIAL` 时只改变 upgrade lifecycle：

```text
confirmedSlot    = old confirmed slot
confirmedVersion = old confirmed version
pendingSlot      = candidate
upgradeState     = TRIAL
slot states      = unchanged
```

不得在 S09 提前更新 confirmedSlot / confirmedVersion。

### 9.1 Atomic Boundary

```text
Before valid TRIAL commit
→ authoritative state remains PENDING
→ reset causes reinstall

After valid TRIAL commit
→ authoritative state is TRIAL
→ reset must not reinstall
```

TRIAL 必须表示：

> Candidate 已完整安装并通过静态验证，可以进入试运行。

TRIAL 绝不表示“正在安装”。

## 10. boot_installer Boundary

`boot_installer` 负责安装事务和状态转换，但不负责选择 inactive Slot。

Candidate 唯一来源：

```text
metadata.pendingSlot
```

S07 Application 已负责：

```text
select inactive slot
→ download
→ validate
→ pendingSlot = target
→ PENDING
```

Bootloader 只机械执行：

```text
Metadata tells which Slot
→ validate it
→ install it
```

建议 installer 具有自身聚合结果类型，用于 RTT 和上层决策区分 Header、External CRC、Erase、Program、Internal CRC、Metadata Commit 等失败阶段。

## 11. Failure Semantics

### 11.1 Failure Before Internal Erase

例如：

- Metadata invalid；
- Candidate Header invalid；
- imageSize invalid；
- External CRC invalid；
- Candidate Vector invalid；
- W25Q64 read failure。

必须：

```text
do not erase Internal APP
do not commit TRIAL
log explicit failure
```

S09 不擅自清除 PENDING、修改 Slot health 或重定义 Recovery Policy；这些异常策略在 S10/Recovery 阶段继续完善。

### 11.2 Failure After Internal Erase

例如：

- Internal erase error；
- program error；
- W25Q64 mid-copy read error；
- local read-back error；
- Internal CRC failure；
- Internal vector failure。

此时 Internal APP 不可信，但：

```text
Metadata remains PENDING
External Candidate remains intact
```

Reset 后从头重新安装。

### 11.3 Metadata Commit Failure

Internal APP 即使已经静态验证成功，只要 TRIAL commit 未被重新 load 并确认有效，就不得把安装事务视为成功。

## 12. Test Assets and Factory Baseline

S09 在正式安装测试前建立固定测试资产：

```text
v1.0 = LED 500 ms ON / 500 ms OFF
v1.1 = LED 1000 ms ON / 1000 ms OFF
v1.2 = LED 2000 ms ON / 2000 ms OFF
```

仅临时修改静态 Blink Config 生成版本，不修改 `app_main_task` 业务逻辑；生成后恢复 v1.0 配置并保持源码 clean。

输出：

```text
06_Output/Firmware/app_v1.0.bin
06_Output/Firmware/app_v1.1.bin
06_Output/Firmware/app_v1.2.bin
```

必要时同时生成 Firmware Package，但必须遵守 compact package 与 Slot physical layout 的映射规则。

建立统一 Factory Restore：

```text
05_Tools\toolkit.bat factory restore
```

目标 baseline：

```text
Internal APP      = v1.0
Slot A            = v1.0 VALID
Slot B            = EMPTY
confirmedSlot     = A
confirmedVersion  = 1.0.0
pendingSlot       = NONE
upgradeState      = NONE
Metadata format   = V2
```

Factory Restore 优先复用已有 Toolkit + 临时 provisioning firmware/board test + 正式 W25Q64/AT24C02/Firmware Storage 代码，不通过 J-Link 直接模拟 SPI/I2C 原始写设备。

该操作是 destructive operation，必须明确提示会重置 Internal APP、Slot A/B 与 Metadata。

## 13. Verification Strategy

复用现有工具，不建立第二套测试系统：

```text
Toolkit
→ build / flash / factory restore / ymodem

RTT
→ Boot decision / image validation / install / metadata state

GDB + J-Link
→ breakpoint / memory inspect / reset / fault injection

Logic Analyzer
→ SPI/I2C 疑难问题补充证据
```

RTT 至少提供稳定检查点：

```text
metadata PENDING + pending slot
candidate header PASS
candidate CRC PASS
candidate vector PASS
internal erase start/PASS
install start/complete
internal CRC PASS
internal vector PASS
metadata PENDING -> TRIAL
metadata commit PASS
jump APP
```

错误日志必须能区分失败阶段，而不是只输出通用 FAIL。

### 13.1 Fault Injection

重点验证 atomic boundary：

```text
Reset before TRIAL commit
→ Metadata PENDING
→ next boot reinstalls

Reset after TRIAL commit
→ Metadata TRIAL
→ next boot does not reinstall
→ validate/jump installed APP
```

建议覆盖：

- External validation 后、erase 前；
- erase 后；
- program 约 25% / 50%；
- program 完成后；
- Internal CRC 前后；
- Metadata body write 后、commit marker 前；
- commit marker 后；
- Jump 前。

优先通过现有 GDB/J-Link 外部打断，不在生产代码中堆叠大量 Fault Injection 宏。

Logic Analyzer 只在需要确认 SPI read、AT24C02 write/ACK polling 或 Metadata commit bus behavior 时使用。

## 14. Non-goals

S09 不实现：

- Trial runtime confirmation；
- `firmware_confirm()`；
- Watchdog / IWDG；
- Reset Cause policy；
- Failure Counter；
- Automatic Rollback；
- Recovery Image restore；
- Anti-rollback security policy；
- SHA / AES / HMAC / Signature / CK02AT；
- External Flash erase/write in Bootloader；
- Ymodem in Bootloader；
- FreeRTOS / EasyLogger；
- MCUboot-style swap/scratch/trailer framework。

以上属于 S10 或后续安全阶段。

## 15. Acceptance Criteria

必须满足：

1. Bootloader 能初始化 SPI/Soft-I2C 后再初始化 W25Q64/AT24C02；
2. W25Q64 Bootloader Driver 保持 read-only；
3. AT24C02 只实现 Metadata 所需最小 read/write/ACK polling；
4. Internal Flash Driver 永远不能操作 Bootloader Region；
5. Bootloader Header/Metadata/CRC 与 Application/PC Binary Contract 兼容；
6. `imageSize > 448 KiB` 在 erase 前被拒绝；
7. Header/External CRC/Vector invalid 时不擦 Internal APP；
8. PENDING Candidate 可完整安装到 Internal APP；
9. program 过程存在局部 read-back；
10. 安装后 Internal Whole-image CRC PASS；
11. 安装后 Internal Vector Validation PASS；
12. Internal 验证完成前不得提交 TRIAL；
13. PENDING→TRIAL 使用双副本 atomic commit + commit marker last；
14. TRIAL commit 后重新 load 能选中新副本；
15. TRIAL 中 confirmedSlot/confirmedVersion 保持旧 confirmed firmware；
16. TRIAL reset 不重复安装；
17. TRIAL commit 前 reset 保持 PENDING 并从头重新安装；
18. External Candidate 在安装期间不被 Bootloader 修改；
19. Factory Restore 能恢复统一 v1.0 baseline；
20. v1.0 → v1.1 正常安装板测通过；
21. fault injection 覆盖 destructive gate、program、metadata commit 原子边界；
22. Toolkit / RTT / GDB 现有能力无回归；
23. Bootloader/Application Clean Build 通过；
24. Bootloader size 仍小于 64 KiB；
25. S10 Trial/Confirm/Watchdog/Rollback 仍未提前实现。
