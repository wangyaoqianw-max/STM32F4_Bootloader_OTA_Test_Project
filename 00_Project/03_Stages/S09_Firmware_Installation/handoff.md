# S09 Firmware Installation Handoff

## Metadata

- Stage: `S09_Firmware_Installation`
- Workflow Status: `IN_PROGRESS`
- Branch: `main`
- Baseline Commit: `e4eb1ac5704971ed72ec4bdb8986e070752c5390`
- Design Commit: `4882077d6c8798900c0e12f1fc902c28682263c3`
- Implementation Plan Commit: `4882077d6c8798900c0e12f1fc902c28682263c3`
- Implementation Commits: `6e3fef5`, `c8e0c07`, `1f5b4d9`, `fbe8e37`, `e4d6816`, `69c426a`, `28feb5a`, `53acd8e`
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

### Actual Implementation

- Task 0 建立 v1.0/v1.1/v1.2 固件资产、compact image 检查和 Factory Restore 工作流；
- Task 1 建立独立 Bootloader Firmware Header/Metadata/CRC 合同及 PC Packer 兼容 Host Test；
- Task 2 建立 SPI2、Soft-I2C、W25Q64 read-only、AT24C02 最小读写和 ACK polling；
- Task 3/4 建立 APP-only Internal Flash、Candidate pre-validation 和复用 S08 规则的 vector 校验；
- Task 5 建立 256 Byte bounded installer、分块 read-back、Internal whole-image CRC 和最终 vector 校验；
- Task 6 建立 commit marker 最后写入的 `PENDING → TRIAL` 原子提交；
- Task 7 将设备初始化、Metadata decision、安装和跳转接入 S08 Boot Main；`ROLLBACK` 明确保持 S10 ownership；
- 已按 C 代码规范补齐新增 `.c/.h` 的文件头、公开 API、类型、边界和硬件约束注释，未对无关模块做格式化重构。

### Code Verification Evidence

- Contract Host Test：PASS；
- Pre-validation Host Test：PASS，invalid Header/CRC/vector/PENDING 场景不链接 destructive API；
- Installer Host Test：PASS，destructive gate、256 Byte 分块、read-back 和写入失败注入通过；
- Metadata Commit Host Test：PASS，commit marker 顺序、故障注入和错误 Slot 无写入通过；
- Bootloader Keil Build：PASS，0 error / 0 warning；`Total ROM Size = 21104 bytes`，`.bin = 11688 bytes`，均小于 64 KiB；
- Application Keil Build：PASS，最终正式工程 0 error / 0 warning；
- `git diff --check`：PASS；Bootloader 中未发现 W25Q64 写入/擦除生产 API；
- 完整证据索引见 `04_Test/Reports/Stages/S09_Firmware_Installation/verification.md`。

### Board Evidence and Pending Work

- 既有真实板自动 GDB/Ymodem 流程已完成 v1.1 发送，复位后 Bootloader RTT 捕获 `Metadata ... state=TRIAL` 与 `TRIAL state: skip reinstall`；
- 既有 Bootloader 自动 RTT 基线捕获 `W25Q64 JEDEC EF 40 17`、外部设备初始化成功、Metadata `NONE` 和合法 APP Jump；
- Factory Restore 曾有一次历史成功基线证据，见 `06_Output/Logs/S09_Factory_Restore/provision_rtt_raw.log`；
- 今晚自动重试未通过：发送端未收到初始 `C`，板端后续 RTT 为 `worker result=2`，对应外部 Slot 擦除超时。证据见 `provision_rtt_failure_20260918.log`、`ymodem_failure_20260918.log`；
- 依赖人工按键或人工断电/复位配合的正常安装、安装中断、Power Loss 和完整 Fault Injection 暂不宣称通过，留待下一次板测；
- 当前不满足 Task 9 Completion Gate，阶段保持 `IN_PROGRESS`，不得进入 `CLOSED`。

### Handoff to Next Role

- 当前建议角色：先由 Project Owner/人工板测恢复 Factory Restore 基线，再进入 Verification；
- 首要补测：Factory baseline → v1.0 → v1.1 Ymodem → PENDING → Bootloader install → TRIAL → v1.1；
- 随后补测：Metadata marker 前后 reset、安装中断、Power Loss/retry；
- S10 仍直接接手 TRIAL runtime confirmation、Watchdog、Failure Counter 和 Rollback。
