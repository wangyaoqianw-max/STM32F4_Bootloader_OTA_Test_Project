# S09 Firmware Installation Handoff

## Metadata

- Stage: `S09_Firmware_Installation`
- Workflow Status: `READY_FOR_IMPLEMENTATION`
- Branch: `main`
- Baseline Commit: `e4eb1ac5704971ed72ec4bdb8986e070752c5390`
- Design Commit: `4882077d6c8798900c0e12f1fc902c28682263c3`
- Implementation Plan Commit: `4882077d6c8798900c0e12f1fc902c28682263c3`
- Implementation Commit: `Not created yet`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Updated At: `2026-09-18`

## Required Reading

```text
AGENTS.md
README.md
PROJECT_CONTEXT.md
00_Project/WORKFLOW.md
00_Project/02_Roadmap/development_roadmap.md
00_Project/03_Stages/S07_OTA_Service_V1/design.md
00_Project/03_Stages/S07_OTA_Service_V1/handoff.md
00_Project/03_Stages/S08_Bootloader_Foundation/design.md
00_Project/03_Stages/S08_Bootloader_Foundation/handoff.md
00_Project/03_Stages/S09_Firmware_Installation/design.md
00_Project/03_Stages/S09_Firmware_Installation/implementation_plan.md
03_Firmware/AGENTS.md
03_Firmware/00_Doc/Standards/嵌入式C代码规范.md
```

## Upstream Input

S08 已 `CLOSED / PASS`，提供：

```text
Independent bare-metal Bootloader
+ 64 KiB / 448 KiB Internal Flash Layout
+ RTT / boot_log / CmBacktrace
+ APP Vector Validation
+ reliable APP Jump
```

S07 已 `CLOSED / PASS`，提供：

```text
External Firmware Slot A/B
+ Firmware Image V1
+ Metadata V2
+ durable PENDING
+ pendingSlot
+ Factory Provisioning precedent
```

## Frozen S09 Contract

```text
PENDING
→ validate Candidate
→ destructive gate
→ erase Internal APP
→ install
→ internal CRC/vector verify
→ atomic PENDING → TRIAL
→ jump APP
```

S09 不实现 Trial runtime confirmation、Watchdog、Failure Counter 或 Rollback。

### Bootloader Architecture

- 不迁移 Application Firmware Service；
- Bootloader 轻量复现 Header/Metadata/CRC consumer；
- Shared 继续只保留真正稳定的跨镜像事实，例如 `memory_layout.h`；
- 通过 Compatibility Test 保证 Binary Contract 一致。

### Driver Scope

```text
SPI         → minimal synchronous bus
W25Q64      → init + JEDEC + read only
Soft-I2C    → minimal bus
AT24C02     → init + read + write + ACK polling
Internal    → APP-only erase/write/read
```

初始化顺序：

```text
Bus Init
→ Device Init
→ Metadata Load
→ Boot Decision
```

### Destructive Gate

在 Internal APP erase 前必须通过：

```text
PENDING
pendingSlot valid
slot health VALID
Header valid
imageSize 1..448 KiB
External CRC PASS
Candidate MSP PASS
Candidate Reset_Handler PASS
```

### Metadata Atomic Boundary

```text
before valid TRIAL commit
→ authoritative PENDING
→ reset reinstalls

after valid TRIAL commit
→ authoritative TRIAL
→ reset does not reinstall
```

## Test Baseline

S09 固定测试固件：

```text
v1.0 = 500/500 ms Blink
v1.1 = 1000/1000 ms Blink
v1.2 = 2000/2000 ms Blink
```

Factory Restore 目标：

```text
Internal APP      = v1.0
Slot A            = v1.0 VALID
Slot B            = EMPTY
confirmedSlot     = A
confirmedVersion  = 1.0.0
pendingSlot       = NONE
upgradeState      = NONE
Metadata          = V2
```

## Tooling

优先复用：

```text
05_Tools\toolkit.bat build
05_Tools\toolkit.bat flash ...
05_Tools\toolkit.bat rtt ...
05_Tools\toolkit.bat ymodem ...
05_Tools\toolkit.bat snapshot ...
05_Tools\toolkit.bat logic ...
```

本阶段允许按计划增加 `toolkit.bat factory restore`，但不得建立第二套 Provisioning/Build/Flash 工具链。

RTT 用于稳定状态检查点；GDB/J-Link 用于 reset/fault injection；Logic Analyzer 用于必要的 SPI/I2C 底层证据。

## Implementation Notes

- 实施前先核实真实文件/API，不机械照抄计划中的建议命名；
- 修改 `.c/.h` 前必须读取并声明代码规范；
- `pack_firmware.py` 当前 compact `.img` 与 Slot physical layout 不相同；
- W25Q64 Candidate 在 Bootloader install 中必须保持只读；
- `confirmedSlot/confirmedVersion` 到 S10 Confirm 前保持旧值；
- TRIAL 是“安装完成、等待运行确认”，不是“正在安装”。

## Implementation Output

施工后填写：

- Actual implementation commits；
- Actual file/API decisions；
- Build / Host Test；
- Board normal installation；
- Fault injection evidence；
- Bootloader size；
- Known issues / deviations；
- Verification handoff。