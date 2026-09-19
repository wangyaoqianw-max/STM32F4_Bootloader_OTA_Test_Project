# S10 Trial Confirm Rollback Handoff

## Metadata

- Stage: `S10_Trial_Confirm_Rollback`
- Workflow Status: `DESIGN_APPROVED`
- Branch: `main`
- Baseline Commit: `de17162c179f3a6551c9edca0d5df35c03ffdc48`
- Design Commit: `223fae71d3c641cf9e36d6048d22a73815152b94`
- Design Metadata Commit: `d054f165553ed320561d726fe736f8bea7ec525e`
- Initial Handoff Commit: `6a5d2fadabf5ee7face632e90cc18e1daab02f47`
- Project Context Sync Commit: `f2a4eb1c4ca80a74a94ead818427eeb359a5d8b7`
- Current Status Sync Commit: `c4ff10d9fc345408655bc11d62c2976bb915e44f`
- Design Review Amendment Commit: `ffcf6b835d2c9dd43ee907b4770c6df43c964dc2`
- Design Review Acceptance Sync Commit: `c4f411f8a4dec926ebca0d9463ba9184ffc8af35`
- Design Approval Commit: `621df210a0fba930d05e32fe450a494b9b0c2cca`
- Implementation Plan Commit: `Not created yet`
- Implementation Commit: `Not created yet`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`
- Updated At: `2026-09-19`

## Current Role

当前角色：

```text
Project Owner / S10 Implementation Planning
```

当前允许继续：

```text
implementation_plan.md preparation
implementation task decomposition
verification-plan mapping
```

当前禁止：

```text
production implementation
stage verification claims
```

只有 implementation plan 完成并同步正式上下文后，阶段才进入 `READY_FOR_IMPLEMENTATION`。

## Required Reading

后续人工或 Agent 恢复上下文时，至少按以下顺序读取：

```text
AGENTS.md
README.md
PROJECT_CONTEXT.md
00_Project/WORKFLOW.md
00_Project/02_Roadmap/development_roadmap.md
00_Project/05_Status/current_status.md

00_Project/03_Stages/S07A_RTOS_Startup_Refactor/design.md
00_Project/03_Stages/S07A_RTOS_Startup_Refactor/handoff.md

00_Project/03_Stages/S09_Firmware_Installation/design.md
00_Project/03_Stages/S09_Firmware_Installation/handoff.md
00_Project/03_Stages/S09_Firmware_Installation/review.md
04_Test/Reports/Stages/S09_Firmware_Installation/verification.md

00_Project/03_Stages/S10_Trial_Confirm_Rollback/design.md

03_Firmware/AGENTS.md
03_Firmware/00_Doc/Standards/嵌入式C代码规范.md
```

在进入实施前还必须读取 Application / Bootloader 目标目录适用的更具体 `AGENTS.md` 和真实代码。

## Upstream Input

### S07A Startup Contract

S07A 已提供：

```text
defaultTask temporary bootstrap
→ app_system_bootstrap()
→ appMainTask + otaWorker + displayTask
→ MAIN_DONE + OTA_DONE + DISPLAY_DONE
→ RUNNING / DEGRADED / FAILED
```

关键事实：

- Startup Barrier timeout = 5000 ms；
- RUNNING 与 DEGRADED 都会发布 SYSTEM_RUN；
- 因此 Trial Confirm 不能只依据 `app_startup_wait_for_decision()` 返回成功；
- Trial 必须显式要求 `systemState == APP_SYSTEM_STATE_RUNNING`。

### S09 Installation Contract

S09 已 `CLOSED / PASS`，提供：

```text
PENDING
→ Candidate validation
→ Internal APP erase/install
→ Internal CRC/vector verify
→ atomic PENDING → TRIAL
→ direct jump APP
```

冻结语义：

- `TRIAL` = 已安装并静态验证完成，但尚未运行确认；
- `confirmedSlot/confirmedVersion` 在 Trial 期间仍指向旧 Known-Good Firmware；
- W25Q64 在 Bootloader 中保持 read-only；
- Internal Flash Driver 只允许 APP Region；
- Metadata 使用双副本 + sequence + CRC + commit marker-last 原子提交。

## Frozen S10 Design Summary

### Lifecycle

```text
NONE
  ↓ OTA
PENDING
  ↓ install
TRIAL
 ├─ Health PASS + Confirm → NONE
 └─ any reset before Confirm
            ↓
         ROLLBACK
            ↓
     restore confirmedSlot
            ↓
           NONE
```

第一版明确：

- 不新增 `CONFIRMED` enum；
- 不升级 Metadata V3；
- 不使用 N 次失败阈值决定回滚；
- Trial 只有一次机会；
- Bootloader 新一轮启动看到 `TRIAL` 即表示上一轮 Trial 未 Confirm。

### Application Watchdog

Watchdog 是 Platform MCU Capability，不属于 OTA Service。

冻结方向：

```text
App / Service
→ Platform Watchdog
→ STM32 Impl
→ HAL / DBGMCU
```

关键策略：

- 不通过 CubeMX 重新生成工程；
- Application 每次启动都启用 IWDG；
- 在 RTOS Scheduler 前启动；
- 第一版 timeout 约 10 s；
- Pre-RTOS / Startup 使用少量有效 checkpoint Feed；
- Runtime 长期 Feed Owner 为 `appMainTask`；
- `appMainTask` 完成正常工作周期后才 Feed；
- 不创建独立无条件 Feed Task；
- Debug Halt 时通过 DBGMCU 冻结 IWDG；
- Confirm 后 IWDG 继续长期工作。

S10 V1 不宣称已经实现多任务持续 Heartbeat Manager。

### Trial Health

Trial Confirm 采用三层 Gate：

```text
1 Startup Health
  systemState == RUNNING

2 Runtime Ready
  MAIN / OTA / DISPLAY actually entered normal runtime

3 Observation Window
  5 s stable observation
```

第一版 Runtime Ready：

- appMainTask：至少完成一次正常前台工作周期；
- otaWorker：进入正常等待/Blocked 状态；
- displayTask：至少完成一次正常显示刷新。

不新增重复的 `app_health_component_t`；继续沿用已有 MAIN / OTA / DISPLAY 组件划分。

### Firmware Confirm

Runtime Confirm 不复用 `service_ota_confirm_install()`。

建议在：

```text
02_Service/service_firmware/
```

增加轻量 Firmware Lifecycle 能力。

严格 `firmware_confirm()` 在提交前重新检查：

```text
latest Metadata valid
upgradeState == TRIAL
pendingSlot is A/B
pending slot state == VALID
pending image Header valid
pending image version readable
```

成功事务：

```text
confirmedSlot    = pendingSlot
confirmedVersion = pending image version
pendingSlot      = NONE
upgradeState     = NONE
```

必须使用现有双副本 atomic commit，并在 commit 后 reload latest 验证。

### Bootloader Rollback

Bootloader 看到已有 `TRIAL`：

```text
read confirmedSlot + confirmedVersion
→ read-only prevalidate confirmed image
→ require Header.version == confirmedVersion
→ atomic TRIAL → ROLLBACK
→ destructive gate
→ reinstall confirmedSlot
→ Internal CRC/vector PASS
→ atomic ROLLBACK → NONE
→ jump recovered APP
```

Confirmed Image 无效时：

```text
do not erase Internal APP
do not claim rollback success
enter explicit fatal/recovery-required path
```

### Installer Reuse

禁止复制第二套 Rollback Installer。

目标结构：

```text
pending source selection ─┐
                          ├→ common image validation
confirmed source selection┘
                          ↓
                    common install core
                          ↓
               erase / copy / read-back
               / CRC / vector validation
```

保留安全入口，避免公开“任意 Slot 都可擦 APP”的宽接口。

### Rollback Power-loss Safety

Rollback 使用：

```text
persistent ROLLBACK
+
immutable External confirmedSlot
+
restart-from-zero reinstall
```

不实现 MCUboot-style sector resume。

ROLLBACK 中任意 erase/program/CRC/reset/power-loss 后，下次 Bootloader 仍从 confirmedSlot 从头恢复。

## Mature Project References

设计思想主要参考：

```text
MCUboot
ST stm32-mw-mcuboot
wolfBoot
ESP-IDF OTA rollback
```

借鉴内容：

- Test / Confirm / Revert；
- Application-controlled confirmation；
- 未 Confirm reset → rollback；
- destructive operation 前持久化恢复意图；
- update / rollback 复用底层安装能力；
- Boot/Runtime Watchdog 分阶段治理。

不直接复制 GPL 代码，也不引入 MCUboot swap/scratch/trailer 体系。

## Open Work Before Implementation

当前设计主架构已收束，但实施前仍需要完成：

1. 等待 Project Owner 明确批准 Design；
2. 批准后创建 `implementation_plan.md`；
3. Implementation Plan 中落实当前已确认的 HAL IWDG 手工接入、Pre-RTOS/Startup checkpoint、otaWorker Storage ownership、Health/Confirm handshake 和 Bootloader Installer 重构任务；
4. 实施前继续以真实文件/API 为准，不机械照抄设计中的建议命名。

上述项目允许在 Design Role 调查，但不得提前修改生产代码。

## S09 Deferred Verification Carried Forward

S09 剩余真实板补充证据：

```text
erase after
program ~25%
program ~50%
program complete
Internal CRC before/after
Metadata body / commit marker before/after
Power Loss / retry
```

这些属于：

```text
S09 supplementary verification evidence
```

即使在 S10 验证窗口执行，也必须回填 S09 Verification/Review，不得改写成 S10 Production Scope，也不得在未执行时标记 PASS。

## Tooling

优先复用现有：

```text
05_Tools\toolkit.bat build
05_Tools\toolkit.bat flash ...
05_Tools\toolkit.bat rtt ...
05_Tools\toolkit.bat snapshot ...
05_Tools\toolkit.bat fault ...
05_Tools\toolkit.bat ymodem ...
05_Tools\toolkit.bat logic ...
```

J-Link 仍保持单客户端 ownership。

S10 Watchdog Debug Freeze 验证不得破坏 S05A 已冻结的：

```text
halt → detach → remains halted

continue&
→ disconnect
→ quit
→ MCU continues running
```

## Expected Verification Scope

正式 Implementation Plan 后至少需要规划：

```text
Application + Bootloader clean build
Watchdog Pre-RTOS start
10 s approximate timeout
Debug Halt freeze
Startup RUNNING gate
Runtime Ready gate
5 s Observation
strict firmware_confirm
Trial reset → rollback
IWDG reset → rollback
software reset → rollback
power cycle → rollback
confirmed image identity gate
TRIAL → ROLLBACK atomic boundary
rollback install power-loss retry
ROLLBACK → NONE atomic boundary
S05A / S07A / S09 regression
Bootloader < 64 KiB
```

## Implementation Output

- Status: `NOT_STARTED`

### Completed Work

```text
S10 design discussion completed to DRAFT baseline.
No production implementation has started.
```

### Changed Files

```text
00_Project/03_Stages/S10_Trial_Confirm_Rollback/design.md
00_Project/03_Stages/S10_Trial_Confirm_Rollback/handoff.md
PROJECT_CONTEXT.md
00_Project/05_Status/current_status.md
```

### Deviations From Plan

```text
None.
No implementation plan exists yet.
```

### Verification Results

```text
NOT_APPLICABLE at design-only stage.
```

### Design Review Status

```text
Blocking Findings : 0
Important Findings: 5
Resolution         : all incorporated into design.md
Technical Result   : READY_FOR_OWNER_APPROVAL
```

主要修订：

- IWDG start 前移到 HAL_Init 后、SystemClock_Config 前；
- 明确 HAL IWDG module/source 当前未接入，实施需手工加入；
- 增加 5 s Runtime Ready deadline 与 Health Feed gating；
- otaWorker 保持 Firmware Storage I/O owner；
- strict Confirm 做完整 image validation，并强制 pendingSlot != confirmedSlot。

### Known Issues

- S09 deferred board-level fault injection remains outstanding as explicitly accepted follow-up.
- S10 Design has received formal Project Owner approval.
- Implementation Plan has not yet been created.

### Review Focus

正式 Design Review 优先检查：

1. Trial reset semantics 是否存在误回滚死角；
2. Watchdog Feed ownership 是否会掩盖关键失败；
3. Confirm 原子提交是否只有 TRIAL/NONE 两种持久结果；
4. Rollback destructive gate 是否一定晚于 Known-Good Image 校验和 ROLLBACK commit；
5. Installer 泛化是否保持安全窄接口；
6. Debug Freeze 是否保持现有 GDB 自动化可用；
7. S10 是否错误吸收 S09 Deferred Verification。
