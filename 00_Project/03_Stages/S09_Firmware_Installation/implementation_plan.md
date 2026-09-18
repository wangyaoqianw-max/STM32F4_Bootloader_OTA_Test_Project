# S09 Firmware Installation Implementation Plan

## Metadata

- Stage: `S09_Firmware_Installation`
- Workflow Status: `READY_FOR_IMPLEMENTATION`
- Branch: `main`
- Baseline Commit: `e4eb1ac5704971ed72ec4bdb8986e070752c5390`
- Updated At: `2026-09-18`

## Required Reading

实施前按顺序读取：

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
00_Project/03_Stages/S09_Firmware_Installation/handoff.md
03_Firmware/AGENTS.md
03_Firmware/00_Doc/Standards/嵌入式C代码规范.md
```

修改任何 `.c/.h` 前必须明确记录：

```text
Coding Standard: 03_Firmware/00_Doc/Standards/嵌入式C代码规范.md
Status: READ
```

随后调查真实代码和工具入口，不凭计划猜测 API 名称或文件位置。

## Execution Rules

- 允许在 `main` 分支执行；
- 优先复用 `05_Tools` 已有 Build/Flash/RTT/GDB/Ymodem/Logic Analyzer 能力；
- 不建立第二套工具链；
- 不为了架构完整性引入 Application 五层结构、Manager Framework 或无需求抽象；
- 不修改已关闭 S07/S08 的冻结语义，除非发现真实合同冲突并停止施工；
- 硬件验证必须基于真实板证据，不得用 Build/Host Test 代替；
- 每个 Task 完成后保持可编译、可解释，并记录必要提交与验证结果。

## Task 0 - Prepare Test Assets and Factory Baseline Restore

### Goal

建立可重复、可恢复的 S09 板测起点。

### Work

1. 核对当前 `project_config.h` 的 v1.0 Blink 为 500/500 ms；
2. Clean Build v1.0，并保存 raw bin；
3. 临时配置 1000/1000 ms，Clean Build 并保存 v1.1；
4. 临时配置 2000/2000 ms，Clean Build 并保存 v1.2；
5. 恢复 500/500 ms 并确认源码 clean；
6. 确认每个 bin 非空且 `<= 448 KiB`；
7. 核对 `pack_firmware.py`：compact `.img=[Header][Payload]` 不得被误当作 Slot 线性布局；
8. 建立 `toolkit.bat factory restore`，复用现有 Toolkit 与正式 storage/metadata 能力；
9. Factory Restore 明确为 destructive operation；
10. Factory Restore 后回读验证 Internal APP、Slot A、Slot B、Metadata。

### Baseline

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

### Acceptance

- v1.0/v1.1/v1.2 bin 可重复生成；
- Application config 已恢复 v1.0；
- Factory Restore 可重复执行；
- Reset / power-cycle 后 baseline 持久；
- repo 不遗留未预期生产源码修改。

## Task 1 - Freeze Bootloader Binary Contract and Host Compatibility Tests

### Goal

在不迁移 Application Firmware Service 的前提下建立 Bootloader 轻量合同。

### Work

1. 建立 Bootloader Firmware contract 常量/类型；
2. 实现轻量 CRC-32/ISO-HDLC；
3. 实现 Header V1 decode/validate；
4. 实现 Metadata V2 decode/select/encode-uncommitted 所需最小逻辑；
5. 不依赖 Application `platform_error` / Platform Object；
6. 使用固定 raw test vector 验证 Header/Metadata 字节偏移；
7. 验证 PC Packer → Bootloader Header decode；
8. 验证 Application Metadata → Bootloader decode；
9. 验证 Bootloader Metadata record → Application decode（可使用 Host Test/fixture）。

### Acceptance

- Header V1 / Metadata V2 / CRC 对同一 fixture 结果一致；
- Bootloader 不链接 Application Firmware Service；
- 不把 Application Platform 依赖迁入 Shared。

## Task 2 - Implement Minimal Bus and External Device Drivers

### Goal

建立 S09 Bootloader 所需最小外部存储访问链。

### Work

1. 调查 CubeMX 已生成 SPI2/GPIO 配置；
2. 建立 SPI Bus 最小同步能力；
3. 建立 Soft-I2C 最小能力；
4. W25Q64 只实现 init / JEDEC check / read；
5. AT24C02 实现 init / read / write / ACK polling；
6. 初始化顺序固定 Bus → Device；
7. 增加明确失败码与 RTT 诊断；
8. 不加入 W25Q64 erase/program/SFUD。

### Verification

- W25Q64 JEDEC read；
- Slot A/B Header read；
- AT24C02 Copy A/B read；
- Metadata V2 decode；
- 必要时用现有 Logic Analyzer 验证 SPI/I2C。

### Acceptance

- Bootloader 可稳定读取 Candidate 和 Metadata；
- W25Q64 保持 read-only；
- Device Driver 不重复初始化 Bus。

## Task 3 - Implement APP-only Internal Flash Driver

### Goal

建立严格受限的 Internal Application Flash 擦写能力。

### Work

1. 复核 STM32F411 Sector 4~7 geometry；
2. API 使用 APP-relative offset；
3. `erase_app()` 固定擦 S4~S7；
4. overflow-safe range check；
5. WORD program 为主并处理非 4 Byte 尾部；
6. 局部 read-back verify；
7. 所有路径正确 Unlock/Lock；
8. 复核 HAL Program Parallelism 与 Voltage Range；
9. 不在 Driver 中加入 CRC、Metadata、Slot 语义。

### Negative Test

必须证明任何错误 offset/length 都不能触及：

```text
0x08000000 ~ 0x0800FFFF
```

### Acceptance

- APP Region erase/write/read-back PASS；
- Bootloader Region protection PASS；
- 失败返回值可定位 erase/program/verify。

## Task 4 - Implement Pending Candidate Pre-validation

### Goal

在破坏 Internal APP 前完成所有可完成验证。

### Work

实现：

```text
load Metadata
→ require PENDING
→ validate pendingSlot
→ require pending slot health VALID
→ read/validate Header
→ imageSize > 0
→ imageSize <= APP_FLASH_SIZE
→ streaming External CRC
→ read first 8 payload bytes
→ validate MSP / Reset_Handler
```

重构 S08 vector validator，使同一核心规则可用于：

- Internal APP vector；
- External Candidate vector values。

### Acceptance

以下场景都不得调用 Internal APP erase：

- invalid magic；
- invalid Header CRC；
- invalid Payload CRC；
- imageSize 0；
- imageSize > 448 KiB；
- invalid MSP；
- invalid Reset_Handler；
- external read failure。

## Task 5 - Implement boot_installer Transaction

### Goal

完成 W25Q64 → Internal Flash 的可靠同步安装。

### Work

1. 增加 `boot_installer`；
2. Candidate 只来自 `metadata.pendingSlot`；
3. 通过 Task 4 destructive gate 后才 erase；
4. 使用约 256 Byte bounded buffer 流式读取/写入；
5. 每块写后 local read-back；
6. 完成后计算 Internal Whole-image CRC；
7. 再执行 Internal vector validation；
8. 汇总 installer result，并输出稳定 RTT checkpoint；
9. 失败时保持 Metadata PENDING；
10. 安装期间不修改 External Candidate。

### Acceptance

- v1.1 Candidate 能完整写入 Internal APP；
- Internal CRC 与 Header payloadCRC 一致；
- Internal vector PASS；
- 写入/CRC/Vector 失败不得产生 TRIAL。

## Task 6 - Implement Atomic PENDING to TRIAL Commit

### Goal

在安装完成并验证后建立唯一可靠的 TRIAL 原子边界。

### Work

Bootloader Metadata commit 固定：

```text
load A+B
→ select latest
→ sequence + 1
→ target opposite copy
→ invalidate target marker
→ write body+CRC
→ read-back body
→ write commit marker last
→ read-back marker
→ reload A+B
→ verify new copy latest
```

状态变化仅：

```text
upgradeState: PENDING → TRIAL
```

保持：

```text
confirmedSlot
confirmedVersion
pendingSlot
slotAState
slotBState
```

符合冻结语义。

### Acceptance

- Commit 前 reset → latest remains PENDING；
- Commit 后 reset → latest = TRIAL；
- TRIAL 时不重复安装；
- confirmed information 未提前更新。

## Task 7 - Integrate Boot Main Decision Flow

### Goal

将 S09 能力接入 S08 已验证启动链，保持 boot_main 精简。

### Flow

```text
diagnostics init
→ bus init
→ device init
→ metadata load
→ if PENDING: boot_installer_run()
→ NONE/TRIAL: validate Internal APP
→ jump
```

`ROLLBACK` 不在 S09 实现正式恢复逻辑；必须明确记录并保持 S10 ownership。

### Rules

- installer logic 不塞进 `main.c`；
- `boot_main` 只编排 Boot decision；
- S08 cleanup/jump contract 不回归；
- TRIAL reset 不进入 installer。

### Acceptance

- NONE 正常启动现有 APP；
- PENDING 执行安装；
- TRIAL 直接启动已安装 APP；
- S08 invalid vector reject 继续有效。

## Task 8 - Board Verification and Fault Injection

### Goal

证明安装事务对 Reset/Power Loss 的状态边界成立。

### Required Normal Flow

```text
factory restore
→ v1.0 running
→ Ymodem v1.1 to Slot B
→ confirm install
→ PENDING
→ reset
→ Bootloader install
→ TRIAL
→ v1.1 running
```

### Fault Injection Checkpoints

至少覆盖：

1. Candidate validation 后、erase 前；
2. erase 后；
3. program 约 25%；
4. program 约 50%；
5. program complete；
6. Internal CRC 前/后；
7. Metadata body write 后、commit marker 前；
8. commit marker 后；
9. jump 前。

### Expected Invariant

```text
Before valid TRIAL commit
→ PENDING
→ reboot reinstalls

After valid TRIAL commit
→ TRIAL
→ reboot does not reinstall
```

### Tooling

优先：

```text
toolkit
RTT
GDB + J-Link
```

Logic Analyzer 只在 SPI/I2C 行为需要额外证据时使用。

### Acceptance

- 正常 v1.0 → v1.1 PASS；
- destructive gate 之前中断不破坏旧 APP；
- erase/program 阶段中断后可重新安装；
- Metadata commit 原子边界 PASS；
- repeated reset/power-cycle 行为符合状态合同。

## Task 9 - Regression, Size, Documentation and Handoff

### Work

1. Bootloader Clean Build；
2. Application Clean Build；
3. 检查 Bootloader `.map/.bin < 64 KiB`；
4. 回归 S08 jump/CmBacktrace/RTT；
5. 回归 S07 Ymodem/PENDING 关键链；
6. 更新 S09 verification report；
7. 更新 S09 handoff；
8. 同步 `PROJECT_CONTEXT.md`、`current_status.md`、Roadmap；
9. Review 前确认 S10 能直接接手 TRIAL/Confirm/Watchdog/Rollback。

### Completion Gate

只有以下均满足才能进入 Verification / Review：

- Task 0~8 已完成；
- Build PASS；
- Host compatibility PASS；
- Normal installation PASS；
- Fault injection PASS；
- 文档与代码一致；
- 无 S10 scope creep。

## Planned Task Order

```text
Task 0  Test Assets / Factory Restore
   ↓
Task 1  Binary Contract
   ↓
Task 2  Bus + W25Q64 + AT24C02
   ↓
Task 3  Internal Flash
   ↓
Task 4  Candidate Pre-validation
   ↓
Task 5  Installer
   ↓
Task 6  PENDING → TRIAL Atomic Commit
   ↓
Task 7  Boot Main Integration
   ↓
Task 8  Board / Fault Injection
   ↓
Task 9  Regression / Docs / Handoff
```

Task 2 与 Task 3 在接口冻结后可以视实际实现情况独立推进，但主代理必须统一检查接口、依赖和最终集成。
