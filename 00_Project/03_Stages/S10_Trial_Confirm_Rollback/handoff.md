# S10 Trial Confirm Rollback Handoff

## Metadata

- Stage: `S10_Trial_Confirm_Rollback`
- Workflow Status: `READY_FOR_VERIFICATION`
- Branch: `main`
- Baseline Commit: `de17162c179f3a6551c9edca0d5df35c03ffdc48`
- Actual Implementation Baseline: `79b95d1f4681c2f7b5f961785079a112a3c62492`
- Plan Baseline Ancestor: `651b3001b4c23cf4162e3367a91ae43307207bce`
- Design Commit: `223fae71d3c641cf9e36d6048d22a73815152b94`
- Design Metadata Commit: `d054f165553ed320561d726fe736f8bea7ec525e`
- Initial Handoff Commit: `6a5d2fadabf5ee7face632e90cc18e1daab02f47`
- Project Context Sync Commit: `f2a4eb1c4ca80a74a94ead818427eeb359a5d8b7`
- Current Status Sync Commit: `c4ff10d9fc345408655bc11d62c2976bb915e44f`
- Design Review Amendment Commit: `ffcf6b835d2c9dd43ee907b4770c6df43c964dc2`
- Design Review Acceptance Sync Commit: `c4f411f8a4dec926ebca0d9463ba9184ffc8af35`
- Design Approval Commit: `621df210a0fba930d05e32fe450a494b9b0c2cca`
- Implementation Plan Commit: `d63b1f8a9d1981cf1fb1e2a07a6a9af17829c1ac`
- Implementation Commits: `dfb012b`, `cd15bad`, `e245e21`, `26ce0ee`, `1ae57ef`, `d1b08b2`, `a43086c`, `352ee62`, `c237361`, `54c9827`, `b40d1b4`, `a5295c2`
- Verification Follow-up Commits: `5ca2d14`, `f0e5d88`, `25d2acc`, `0ee7f5d`, `ba6ce6f`, `7eab4cc`
- Verification Commit: `Pending final consolidated board verification`
- Review Commit: `Not created yet`
- Updated At: `2026-09-20`

## Current Role

当前角色：

```text
Verification Role
```

下一次会话允许继续：

```text
resume the consolidated S10 board-verification checklist
collect the remaining RTT / GDB / reset / power-cycle / visual evidence
record Verification Role results and hand off to Review Role
```

当前禁止：

```text
scope expansion outside approved S10 design
unapproved Metadata V3 / CONFIRMED enum / failure-threshold rollback
claiming hardware PASS without evidence
leaving temporary test code in the final production build
```

## Latest Verification Follow-up

2026-09-20 已完成不依赖人工操作的补充验证：S10 五个 Host Test、Python 回归、静态 contract 和 Application/Bootloader Build 均通过。Factory Restore 工具修复已提交为 `0ee7f5d`。用户完成恢复性断电后，目标已重新进入 Application；IWDG 寄存器、12 秒 Debug Halt/Resume 和 direct no-feed IWDG reset 证据已采集。当前硬件仍未完成 Trial Confirm 前断电和 Rollback；最新 Factory Restore 重试再次暴露 Soft-I2C BUSY / Boot halt，未形成新的 baseline PASS。

一次重复 v1.1 传输后观察到 Trial runtime `trial=1 / readyMask=0x7 / STABLE`，随后工具复位进入 `trial=0`；由于没有在自动 Confirm 前停住，也没有读到 Bootloader Rollback 决策日志，该次不能算 Rollback PASS。恢复性断电不能替代 Trial 期间真实断电。

## S10 Test Plan Execution Stop

2026-09-20 `19:07`，在用户确认破坏性 Factory Restore 后按独立测试方案执行 `20260920-185951`：

- `S10-00` 只读前置检查 `PASS`；
- `S10-01` `BLOCKED`：临时测试固件构建、烧录和 `app_v1.0.img` 的 COM9 YMODEM 传输均成功，但目标 RTT 只到 `destructive erase Slot A/B start`，RTT 捕获返回错误码 `32`；
- 失败后的正式 Application 恢复、正式 RTT、GDB halt/resume 均 `PASS`；
- 因 F0 Known-Good 基线未证明，按停止条件未执行 `S10-02` 至 `S10-09`，不得把既有 Trial/Confirm 运行时证据升级为 Rollback 通过。

本轮证据目录为 `06_Output/Logs/S10/20260920-185951/`。下一步应先定位并恢复 Factory Restore 测试固件在擦除起点后的目标板/RTT运行条件，再从 `S10-01` 重新开始；不得跳过 F0 或随机改测其他场景。

## Verification Pause and Stable Firmware State

2026-09-20 按 Project Owner 要求暂停人工板测。临时 S10 Trial 验证开关已移除，恢复脚本已删除；正式 Application 重新执行以下流程：

```text
05_Tools\toolkit.bat build application       PASS; 0 error / 0 warning
05_Tools\toolkit.bat flash application run  PASS
05_Tools\toolkit.bat rtt application 8     PASS
```

最新 RTT 已确认：

```text
OTA runtime init result=0
Application init result: 0
Display init result: 0
Display initial render result: 0
Display backlight on result: 0
```

当前只确认正式 Application 已烧录并正常完成启动初始化；LED 肉眼状态未在本轮重新验收。未继续发包、按键或断电。后续新对话从以下未完成项目继续：

```text
Trial 自动 Confirm 前真实断电
Bootloader TRIAL → ROLLBACK → NONE
Rollback 中断后的 restart-from-zero
LED / LCD 人工观察
```

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

## Implementation Progress

Project Owner 已批准 Design，Implementation Plan 已冻结。Task 0→10 已完成：同步并核对 `main`/`origin/main`、完成 Application/Bootloader Clean Build、执行 Host/Contract/Python 回归、实现 Application Trial Health/Confirm、Metadata A/B invariants、Bootloader Pending Install/Confirmed Restore 共用验证/安装核心、TRIAL/ROLLBACK 原子事务和 Boot decision，并将证据写入 `04_Test/Reports/Stages/S10_Trial_Confirm_Rollback/verification_matrix.md` 与 `verification.md`。当前转入 Verification Role；本轮已补充正式 NONE 启动、v1.1 YMODEM、Runtime Ready 和 strict Confirm 的真实 RTT/GDB 证据，但 IWDG、Rollback、复位/掉电和 LED/LCD 仍待集中执行；继续以真实文件/API 为准，不机械照抄建议命名。

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

- Status: `READY_FOR_VERIFICATION`

### Completed Work

```text
Task 0 baseline and verification matrix: dfb012b
Task 1 Application Watchdog capability: cd15bad
Task 2 Application Runtime Health: e245e21
Task 3 strict Firmware Lifecycle Confirm: 26ce0ee
Task 4 Runtime Ready / Feed / Confirm handshake: 1ae57ef
Task 5 Metadata A/B recovery invariants: d1b08b2
Task 6 Bootloader Pending Install / Confirmed Restore split: a43086c
Task 7 Atomic Rollback Metadata Transactions and Boot Decision: 352ee62
Task 8 regression fixtures and automated verification: 54c9827, b40d1b4
Task 9 board-verification checklist and evidence boundary: included in verification_matrix.md
Task 10 warning-free final build and exit evidence: a5295c2
Implementation Plan Task 0→10 is complete; stage is READY_FOR_VERIFICATION.
```

### Changed Files

```text
Application OTA_APP watchdog, health, lifecycle and runtime integration
Bootloader prevalidate/installer/metadata commit/main integration
S10 Host rollback transaction and Boot decision contracts; S09 Host fixture compatibility updates
04_Test/Reports/Stages/S10_Trial_Confirm_Rollback/verification_matrix.md
PROJECT_CONTEXT.md / 00_Project/05_Status/current_status.md
```

### Deviations From Plan

```text
The plan baseline `651b3001` is an ancestor of the synchronized clean HEAD `79b95d1`; only stage-document commits intervene. Task 5 required updating the stale S09 Host fixture from same-slot pending to the valid A-confirmed/B-pending combination. Task 6 uses the actual narrow APIs `boot_prevalidate_confirmed()`, `boot_installer_install_pending()` and `boot_installer_restore_confirmed()` while preserving the frozen architecture.
```

### Verification Results

```text
Task 0→10 evidence is recorded in `04_Test/Reports/Stages/S10_Trial_Confirm_Rollback/verification_matrix.md` and `verification.md`. Application/Bootloader Clean Builds are PASS with 0 errors / 0 warnings; Host/Contract/Python regressions are PASS; real target NONE startup and Trial/Confirm handshake evidence are PARTIAL/PASS, direct no-feed IWDG reset is PASS, while rollback, Trial reset/power-cycle and manual board evidence remain PENDING.
```

### Design Review Status

```text
Blocking Findings : 0
Important Findings: 5
Resolution         : all incorporated into design.md
Technical Result   : APPROVED
```

主要修订：

- IWDG start 前移到 HAL_Init 后、SystemClock_Config 前；
- 明确 HAL IWDG module/source 当前未接入，实施需手工加入；
- 增加 5 s Runtime Ready deadline 与 Health Feed gating；
- otaWorker 保持 Firmware Storage I/O owner；
- strict Confirm 做完整 image validation，并强制 pendingSlot != confirmedSlot。

### Known Issues

- S09 deferred board-level fault injection remains outstanding as explicitly accepted follow-up; it was not executed or counted in S10.
- S10 real board verification is partially evidenced for NONE startup, Trial/Confirm and no-feed IWDG reset; the remaining Rollback, Trial reset/power-cycle and visual scenarios are still pending for the Verification Role. Latest Factory Restore retry is blocked by Soft-I2C BUSY / Boot halt. Historical logs were not reused as S10 evidence.
- S10 Design has received formal Project Owner approval.
- Implementation Plan has been created and approved for execution.

### Verification / Review Focus

Verification and Review 优先检查：

1. Trial reset semantics 是否存在误回滚死角；
2. Watchdog Feed ownership 是否会掩盖关键失败；
3. Confirm 原子提交是否只有 TRIAL/NONE 两种持久结果；
4. Rollback destructive gate 是否一定晚于 Known-Good Image 校验和 ROLLBACK commit；
5. Installer 泛化是否保持安全窄接口；
6. Debug Freeze 是否保持现有 GDB 自动化可用；
7. S10 是否错误吸收 S09 Deferred Verification。


## Test Execution Policy

Project Owner confirmed the following S10 test policy:

```text
Automated first:
Host Test
→ Clean Build
→ Toolkit / RTT / GDB
→ automated reset / fault injection where feasible

Then consolidated manual verification:
KEY press
→ LED/LCD visual confirmation
→ real power-cycle / power-loss scenarios
```

Temporary test code is allowed when needed. It must be clearly isolated, default-disabled, and removed after its evidence is captured. Final production Clean Build must contain no temporary test-only behavior.
