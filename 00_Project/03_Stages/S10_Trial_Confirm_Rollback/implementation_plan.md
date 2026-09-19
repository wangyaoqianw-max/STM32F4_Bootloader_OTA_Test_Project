# S10 Trial Confirm Rollback Implementation Plan

## Metadata

- Stage: `S10_Trial_Confirm_Rollback`
- Workflow Status: `READY_FOR_IMPLEMENTATION`
- Branch: `main`
- Baseline Commit: `651b3001b4c23cf4162e3367a91ae43307207bce`
- Design Approval Commit: `621df210a0fba930d05e32fe450a494b9b0c2cca`
- Status: `NOT_STARTED`
- Updated At: `2026-09-19`

## Required Reading

实施前按顺序读取：

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
00_Project/03_Stages/S09_Firmware_Installation/implementation_plan.md
00_Project/03_Stages/S09_Firmware_Installation/handoff.md
00_Project/03_Stages/S09_Firmware_Installation/review.md
04_Test/Reports/Stages/S09_Firmware_Installation/verification.md

00_Project/03_Stages/S10_Trial_Confirm_Rollback/design.md
00_Project/03_Stages/S10_Trial_Confirm_Rollback/implementation_plan.md
00_Project/03_Stages/S10_Trial_Confirm_Rollback/handoff.md

03_Firmware/AGENTS.md
03_Firmware/00_Doc/Standards/嵌入式C代码规范.md
03_Firmware/00_Doc/Standards/Keil工程与构建输出规范.md
```

修改任何项目自研 `.c/.h` 前必须明确记录：

```text
Coding Standard:
03_Firmware/00_Doc/Standards/嵌入式C代码规范.md
Status: READ
```

随后以真实代码/API/工程文件为准，不机械照抄本计划中的建议命名。

## Execution Rules

- 允许在 `main` 分支执行；
- 允许使用现有 `05_Tools` Build / Flash / RTT / GDB / Ymodem / Logic Analyzer / Factory Restore 能力；
- 不建立第二套工具链；
- S10 IWDG 采用手工 HAL 接入，不运行 CubeMX regeneration；
- Vendor 源码不做风格化修改；仅启用必要 HAL module / build source；
- `main.c`、`freertos.c` 只修改 USER CODE 安全区域；
- 不新增 `CONFIRMED` lifecycle enum，不升级 Metadata V3；
- 不新增 Failure Counter 作为 rollback gate；
- 不复制第二套 Rollback Installer；
- W25Q64 在 Bootloader 继续保持 read-only；
- Firmware Storage I/O 继续由 otaWorker 执行上下文独占；
- 临时测试代码、Fault Hook、测试宏允许加入工程，但必须：
  1. 与正式业务代码有明确隔离；
  2. 默认关闭；
  3. 完成对应测试后移除；
  4. 最终 Clean Build 前确认生产工程无临时测试路径残留。
- 自动化测试优先执行；需要人工按键、肉眼观察或真实断电的场景在自动测试完成后统一集中执行；
- 每个 Task 完成后保持 Application / Bootloader 可编译，不累计多个未验证大改动。

## Test Strategy

S10 验证按以下顺序组织：

```text
Host / Static / Contract
        ↓
Application + Bootloader Clean Build
        ↓
Automated Board Test
  Toolkit / RTT / GDB / Reset
        ↓
Automated Fault Injection where feasible
        ↓
Consolidated Manual Board Test
  KEY / LED / LCD / real power cut
        ↓
Remove temporary test code
        ↓
Final Regression
```

原则：

- 能由脚本、GDB、RTT、软件 Reset、Metadata 注入完成的场景不要求人工重复操作；
- 人工操作统一放到最后一轮，避免开发过程中频繁按键/观察/拔电；
- 自动化测试通过不替代必须人工确认的真实现象；
- S09 Deferred Fault Injection 若在 S10 执行，证据仍回填 S09，不改变阶段归属。

---

## Task 0 - Freeze Baseline, Test Hooks and Verification Matrix

### Goal

建立 S10 可重复施工起点，并把自动/人工测试边界先固化。

### Work

1. 记录 `main@651b3001` clean baseline；
2. 执行 Application / Bootloader Clean Build；
3. 确认 Factory Restore、Ymodem、RTT、GDB snapshot/fault workflow 可用；
4. 建立 S10 verification matrix，将每个验收项分类为：
   - HOST_AUTO；
   - BOARD_AUTO；
   - BOARD_MANUAL；
5. 设计统一临时 Test Hook 入口，优先使用 compile-time config / fault injection，不把测试逻辑散落在业务状态机；
6. 临时测试宏默认 `0`，并记录最终移除检查；
7. 核对 S09 Deferred Fault Injection 列表，标记哪些可在 S10 自动化窗口补测。

### Acceptance

- 两工程 baseline Build PASS；
- 自动/人工测试清单明确；
- 测试钩子不会改变正式默认行为；
- 当前正式源码无未识别临时代码。

---

## Task 1 - Add Application Watchdog Platform Capability

### Goal

手工接入 STM32F411 IWDG，并通过 Platform / Impl 提供最小可靠接口。

### Files / Areas

预计涉及：

```text
03_Platform/platform_mcu/watchdog/
04_Impl/impl_mcu/
00_Config/project_config.h
Core/Inc/stm32f4xx_hal_conf.h
MDK-ARM/OTA_APP.uvprojx
Core/Src/main.c
Core/Src/freertos.c or USER CODE startup checkpoint
```

具体文件名实施时按现有目录命名规范确认。

### Work

1. 新增 Platform Watchdog API：
   ```text
   start(timeoutMs)
   feed()
   ```
2. Impl 使用 STM32 HAL IWDG；
3. 手工启用 `HAL_IWDG_MODULE_ENABLED`；
4. 将 `stm32f4xx_hal_iwdg.c` 加入 Keil Application build；
5. 不运行 CubeMX regeneration；
6. 根据约 32 kHz nominal LSI 计算约 10 s Prescaler/Reload；
7. 参数做 range check，不把 HAL handle 暴露到 Platform；
8. 在 Debug 配置中启用 IWDG freeze：
   ```text
   CPU Halt → IWDG frozen
   Continue → counter resumes
   ```
9. 在 `HAL_Init()` 后、`SystemClock_Config()` 前通过 USER CODE 区启动 IWDG；
10. 增加少量 Pre-RTOS / startup checkpoint：
    - peripherals / diagnostics ready；
    - RTOS objects/defaultTask ready 或 scheduler 后 bootstrap 首个安全点；
11. 不在每个 `MX_xxx_Init()` 后机械 Feed；
12. `Error_Handler()` 不增加无限 Feed。

### Automated Verification

- Clean Build 证明 HAL IWDG 正确链接；
- map/project 检查 IWDG source 已编译；
- RTT/GDB 读取关键寄存器证明 IWDG active；
- 临时故障路径停止 Feed，约 10 s 后观察 IWDG Reset；
- GDB breakpoint 停留超过 watchdog timeout，不发生 Reset；
- Continue 后 watchdog 恢复计数。

### Acceptance

- 正常启动不误 Reset；
- IWDG 在 RTOS 前已启动；
- Debug Freeze PASS；
- 不依赖 CubeMX；
- 现有 GDB workflow 无回归。

---

## Task 2 - Implement Application Runtime Health State

### Goal

建立轻量 `app_health`，负责 Trial Health / Runtime Ready / Observation / Feed permission，不承担 Storage I/O。

### Architecture

```text
Startup
  ↓
app_health
  ├─ firmware mode: NONE / TRIAL
  ├─ MAIN_RUNTIME_READY
  ├─ OTA_RUNTIME_READY
  ├─ DISPLAY_RUNTIME_READY
  ├─ ready deadline
  ├─ observation deadline
  └─ feed / confirm / reset decision
```

不重新定义 MAIN/OTA/DISPLAY component enum。

### Work

1. 在 Application system/runtime 合适位置新增轻量 Health Context；
2. 状态至少能够表达：
   ```text
   STARTUP
   WAIT_RUNTIME_READY
   OBSERVING
   CONFIRM_REQUIRED / CONFIRMING
   STABLE
   FAILED
   ```
3. Runtime Ready deadline = 5 s；
4. Observation Window = 5 s；
5. Trial 只有 `APP_SYSTEM_STATE_RUNNING` 可以进入 Ready/Observation；
6. Trial + DEGRADED / FAILED → Health FAIL；
7. NONE 固件保持现有 DEGRADED 运行语义；
8. Feed permission 由 Health State 给出：
   - valid startup progress → 可受控 Feed；
   - WAIT_RUNTIME_READY 未超时 → 可 Feed；
   - OBSERVING → 可 Feed；
   - CONFIRMING → 不允许 appMainTask 无条件无限 Feed；
   - FAILED → 不 Feed；
   - STABLE → 正常长期 Feed；
9. 明确 timeout wrap-around safe；
10. Health 模块不直接调用 W25Q64/EEPROM、不依赖 `app_ota_runtime` 私有对象。

### Host / Automated Verification

覆盖：

- NONE boot；
- TRIAL boot；
- RUNNING；
- DEGRADED；
- one/multiple Runtime Ready missing；
- 5 s Ready timeout；
- all Ready → Observation；
- 5 s Observation → Confirm request；
- explicit failure；
- Feed permission transition。

### Acceptance

- 不存在永久 TRIAL；
- Health Policy 与 Storage I/O 解耦；
- 无独立 Watchdog Task。

---

## Task 3 - Add Strict Firmware Lifecycle Confirmation

### Goal

在 `service_firmware` 建立严格 Runtime Confirm 事务，并保持 otaWorker 为 Storage I/O owner。

### Files / Areas

预计：

```text
02_Service/service_firmware/firmware_lifecycle.[ch]
02_Service/service_firmware/firmware_storage.[ch]
01_APP/runtime/app_ota_runtime.[ch]
01_APP/task/app_ota_worker.[ch]
01_APP/contract/app_runtime_contract.h
```

### Work

1. 新增 Firmware Lifecycle 语义，不复用 `service_ota_confirm_install()`；
2. Lifecycle 接收已初始化 `firmware_storage_t *`，但不拥有底层 Driver；
3. Strict Confirm 每次重新读取 latest Metadata；
4. 前置条件：
   ```text
   upgradeState == TRIAL
   confirmedSlot = A/B
   pendingSlot = A/B
   pendingSlot != confirmedSlot
   pending slot state == VALID
   ```
5. 对 pending image 执行完整只读验证：
   - Header；
   - imageSize；
   - Internal APP capacity compatibility；
   - Payload CRC；
   - Version；
6. 构造：
   ```text
   confirmedSlot    = pendingSlot
   confirmedVersion = pending header.version
   pendingSlot      = NONE
   upgradeState     = NONE
   ```
7. 复用现有双 Copy Metadata atomic commit；
8. Commit 后重新 load latest 并逐字段确认；
9. app_ota_runtime 提供窄的 Lifecycle 执行入口，不公开 Raw Storage 给其他 Task；
10. otaWorker 增加内部 Notification / Command 支持 Trial Confirm；
11. Confirm transaction 必须在 otaWorker 上下文执行；
12. Confirm 完成后把结果回报给 `app_health`；
13. Confirm 失败进入 Health FAIL / controlled reset。

### Host Verification

至少覆盖：

- state != TRIAL；
- pending NONE；
- pending == confirmed；
- slot INVALID；
- Header invalid；
- size overflow；
- Payload CRC invalid；
- commit body failure；
- commit marker failure；
- reload verify failure；
- normal TRIAL → NONE；
- Confirm 后 confirmedSlot/version 正确。

### Acceptance

- appMainTask/app_health 不直接碰 Firmware Storage；
- Confirm 掉电语义只有完整 TRIAL 或完整 NONE；
- Candidate 成为 confirmed 前再次完成完整 Image Validation。

---

## Task 4 - Integrate Runtime Ready, Watchdog Feed and Confirm Handshake

### Goal

把 MAIN / OTA / DISPLAY 真实长期运行路径接入 Health，不改变 S07A Startup 职责。

### Work

1. `app_system_bootstrap()` 初始化 Health infrastructure；
2. 不把 runtime health 塞进 `app_startup_report_*()`；
3. appMainTask：
   - SYSTEM_RUN 后完成一个 LED 正常工作周期；
   - report MAIN_RUNTIME_READY；
   - 后续每个工作周期根据 Health Feed permission 决定是否 `platform_watchdog_feed()`；
   - Health 请求 Confirm 时只发起一次 otaWorker confirm request；
   - Health FAIL 时优先请求 controlled reset；
4. otaWorker：
   - Startup decision 成功后；
   - Firmware Runtime/Lifecycle 状态已经可读；
   - 进入正常 command wait loop 前 report OTA_RUNTIME_READY；
   - 接收 Trial Confirm command 并在本 Task 执行 strict confirm；
5. displayTask：
   - Startup decision 成功后；
   - 进入永久 display queue wait 前 report DISPLAY_RUNTIME_READY；
   - initial render 不作为 Runtime Ready；
6. Trial Runtime Ready 5 s 超时；
7. 三者全部 Ready 后开始 5 s Observation；
8. Confirm 成功 → Health STABLE；
9. Confirm 失败 / Ready timeout / Trial DEGRADED → reset 或停止 Feed；
10. Stable NONE 状态下 appMainTask 继续长期 Feed。

### Automated Verification

通过 RTT / GDB / temporary test hook 验证：

- Ready 位顺序；
- Ready deadline；
- Observation timer；
- Confirm request only once；
- Confirm success；
- Confirm failure；
- FAILED 后不继续合法 Feed；
- NONE 状态长期运行不 Reset。

### Acceptance

- 三个 Runtime Ready 都发生在 SYSTEM_RUN 后；
- appMainTask 是唯一长期 Feed owner；
- otaWorker 是 Storage transaction owner；
- 没有 circular dependency / Raw Storage shared access。

---

## Task 5 - Strengthen Metadata A/B Recovery Invariants

### Goal

让 Application 与 Bootloader 都拒绝破坏 Rollback 基础的不合法 Metadata 组合。

### Work

1. 在 Application / Bootloader Metadata 或 lifecycle destructive gate 中增加跨字段检查：
   ```text
   PENDING/TRIAL/ROLLBACK:
   confirmedSlot = A/B
   pendingSlot = A/B
   confirmedSlot != pendingSlot
   ```
2. confirmed slot 必须处于 VALID；
3. pending slot 在非 NONE lifecycle 必须 VALID；
4. 不改变 Metadata V2 raw layout；
5. 不改变已有 sequence / CRC / marker 规则；
6. 对已有 Factory baseline / stable NONE 保持兼容；
7. 扩展 Application ↔ Bootloader Metadata compatibility Host Test。

### Acceptance

- pending == confirmed 被拒绝；
- Metadata V2 binary fixture 不变化；
- S04/S07/S09 正常 fixture 继续 PASS。

---

## Task 6 - Refactor Bootloader Prevalidate and Installer for Rollback Reuse

### Goal

把 S09 Candidate-only 安装链泛化为安全的 Pending Install + Confirmed Restore，共用验证与复制核心。

### Work

1. 从 `boot_prevalidate_candidate()` 中抽取通用 slot image validation；
2. 保留安全入口：
   ```text
   prevalidate pending candidate
   prevalidate confirmed recovery image
   ```
3. Pending path：
   - require PENDING；
   - source = pendingSlot；
   - enforce pending != confirmed；
4. Confirmed recovery path：
   - require TRIAL/ROLLBACK recovery semantics；
   - source = confirmedSlot；
   - require slot VALID；
   - Header.version == confirmedVersion；
5. 共同验证：
   - Header；
   - size；
   - vector；
   - Payload CRC；
6. `boot_installer_run()` 解耦 Candidate-only selection；
7. 建立两个窄入口：
   ```text
   install pending
   restore confirmed
   ```
8. 两者共用 private install core：
   - erase APP；
   - bounded chunk copy；
   - local read-back；
   - Internal whole CRC；
   - Internal vector；
9. 不公开“任意 Slot 直接安装”接口；
10. External Flash 仍完全只读。

### Host Verification

- Pending A/B；
- Confirmed A/B；
- wrong state；
- pending == confirmed；
- confirmed version mismatch；
- invalid confirmed CRC/vector；
- common installer source selection；
- Internal APP erase 不可在 prevalidate failure 前发生。

### Acceptance

- 只有一个 Internal Flash install core；
- Rollback source 不能被任意调用方选择；
- S09 Pending 正常链无回归。

---

## Task 7 - Implement Atomic Rollback Metadata Transactions and Boot Decision

### Goal

完成 `TRIAL → ROLLBACK → NONE` 的可重启事务。

### Work

1. 重构 `boot_metadata_commit` 的 private atomic core；
2. 保留现有 `boot_metadata_commit_trial()` 行为；
3. 新增语义入口：
   ```text
   rollback_begin:
   TRIAL → ROLLBACK

   rollback_complete:
   ROLLBACK → NONE
   pendingSlot = NONE
   ```
4. rollback_begin 保持：
   - confirmedSlot；
   - confirmedVersion；
   - pendingSlot；
   - slot states；
5. rollback_complete 保持：
   - confirmedSlot；
   - confirmedVersion；
   - slot states；
6. Boot main 决策改为：
   ```text
   PENDING  → install pending → TRIAL → jump
   TRIAL    → validate confirmed → commit ROLLBACK
              → restore confirmed
              → verify
              → commit NONE
              → jump
   ROLLBACK → validate confirmed
              → restart full restore
              → verify
              → commit NONE
              → jump
   NONE     → normal jump
   ```
7. Confirmed image validation 必须发生在 Internal erase 前；
8. ROLLBACK commit 必须发生在 Internal erase 前；
9. ROLLBACK 中断后采用 restart-from-zero；
10. Known-Good image invalid 时不 erase Internal APP，进入 explicit halt/recovery-required；
11. 加入 Reset Cause snapshot/log：
    - POR/BOR；
    - PIN；
    - Software；
    - IWDG；
12. Reset Cause 不参与 rollback decision；
13. 保持 S08 jump cleanup contract。

### Host Verification

覆盖：

- TRIAL → rollback_begin；
- ROLLBACK → complete；
- body/marker atomic boundary；
- power-loss model；
- confirmed image invalid；
- repeated ROLLBACK restore；
- NONE / PENDING / TRIAL / ROLLBACK boot decision。

### Acceptance

- destructive restore 前状态一定已持久为 ROLLBACK；
- ROLLBACK 任意中断后可重启；
- confirmedSlot/version 不被 rollback 修改。

---

## Task 8 - Automated Regression and Board Verification

### Goal

在不要求人工按键/肉眼观察/拔电的前提下，把可以自动化的 S10 验证尽量全部完成。

### Priority

优先完成本 Task，再进入 Task 9 人工验收。

### Automated Scope

#### Host / Contract

```text
S04 Firmware/Metadata tests
S07 OTA Service tests
S07A Startup tests
S09 Bootloader contract/installer tests
S10 Health tests
S10 Lifecycle Confirm tests
S10 Rollback tests
```

#### Build

```text
Application Clean Build
Bootloader Clean Build
0 error / 0 warning
Bootloader < 64 KiB
```

#### Board Automation

优先使用：

```text
05_Tools\toolkit.bat
RTT
GDB + J-Link
software reset
factory restore
Python Ymodem sender where suitable
temporary test/fault hooks
```

自动验证至少包括：

1. IWDG pre-RTOS start；
2. stable NONE 长期 Feed；
3. IWDG timeout reset；
4. Debug Halt freeze > timeout；
5. Continue 后 watchdog resume；
6. Trial Runtime Ready PASS；
7. Runtime Ready timeout；
8. Observation 5 s；
9. strict Confirm；
10. EEPROM latest Metadata = NONE/new confirmed；
11. Trial software reset → rollback；
12. ROLLBACK automatic restore；
13. confirmed CRC/version invalid → destructive gate reject；
14. rollback begin/complete marker boundary where automated fault hook supports；
15. reset during rollback program where GDB/software reset injection is reliable；
16. S05A GDB contract regression；
17. S07A Startup regression；
18. S09 Pending install regression。

### Temporary Test Code

允许：

- compile-time fault point；
- forced Ready missing；
- forced Confirm failure；
- forced reset checkpoint；
- test-only RTT checkpoint；
- temporary Metadata corruption helper。

要求：

```text
default OFF
clearly marked TEST ONLY
after evidence captured → remove
final production build → no temporary hook
```

### Acceptance

- 所有可自动化场景先完成；
- 失败项先修复，不把已知问题推迟到人工验收；
- 只把确实依赖人工动作的项目留给 Task 9。

---

## Task 9 - Consolidated Manual Board Verification

### Goal

一次性完成需要人工参与的板级场景，避免开发过程中反复按键/观察/拔电。

### Manual Scope

根据自动测试结果统一执行：

1. Factory baseline：
   ```text
   v1.0 running
   Slot A confirmed
   Slot B inactive
   NONE
   ```
2. 人工按 PA0：
   - start OTA；
   - 必要时第二次确认 install；
3. 发送 v1.1 image；
4. LCD 肉眼确认必要 OTA 状态；
5. v1.1 Trial 运行行为肉眼确认；
6. 5 s Runtime/Observation 后 Confirm，确认设备继续正常工作；
7. 再制造 Trial failure，确认 Rollback 后恢复 v1.0 LED 行为；
8. Trial 期间真实 power-cycle；
9. Rollback install 期间真实断电，在可控且不会误伤 PC/Probe 的条件下执行；
10. 恢复上电后确认 ROLLBACK 重启恢复；
11. 必要时检查 Reset Cause RTT；
12. 如补做 S09 Deferred Fault Injection，将证据回填 S09 verification。

### Acceptance

- 人工交互正常链 PASS；
- Trial 未 Confirm 真实断电后 Rollback PASS；
- Rollback 中断恢复 PASS；
- LED/LCD 现象与 Metadata/RTT 证据一致。

---

## Task 10 - Remove Test Hooks, Final Regression, Documentation and Handoff

### Goal

确保最终仓库只留下正式 S10 能力和正式测试资产，不遗留临时验证逻辑。

### Work

1. 移除全部临时生产代码 Test Hook / forced fault；
2. 若保留通用 Fault Injection 能力，必须：
   - 明确为正式 Diagnostics/Test infrastructure；
   - 默认关闭；
   - 有文档和独立配置；
3. Application Clean Build；
4. Bootloader Clean Build；
5. Host Test 全回归；
6. Toolkit 自动回归；
7. 检查 Bootloader `.map/.bin < 64 KiB`；
8. 检查 Application stack/heap 基线无异常回归；
9. 检查 `git diff --check`；
10. Coding Standard Review；
11. 创建：
    ```text
    04_Test/Reports/Stages/S10_Trial_Confirm_Rollback/verification.md
    ```
12. S09 Deferred 测试若执行则单独回填：
    ```text
    04_Test/Reports/Stages/S09_Firmware_Installation/verification.md
    ```
13. 更新 S10 handoff；
14. 更新 PROJECT_CONTEXT / current_status / Roadmap；
15. 进入 `READY_FOR_VERIFICATION`，不得直接跳到 CLOSED。

### Completion Gate

只有以下均满足才能结束 Implementation Role：

- Task 0~7 正式代码完成；
- Task 8 自动化验证已完成并处理所有已知失败；
- Task 9 必须人工执行的场景已完成，或明确标记硬件验证 PENDING；
- 临时测试代码已移除；
- Application / Bootloader Clean Build PASS；
- Host Test PASS；
- Bootloader size < 64 KiB；
- 文档与代码一致；
- S09 Deferred evidence 未混入 S10 完成声明；
- handoff 已记录实际 Commit / verification 状态。

---

## Planned Task Order

```text
Task 0  Baseline / Verification Matrix
   ↓
Task 1  Watchdog Platform + HAL Integration
   ↓
Task 2  Runtime Health State
   ↓
Task 3  Strict Firmware Lifecycle Confirm
   ↓
Task 4  Runtime Ready / Feed / Confirm Handshake
   ↓
Task 5  Metadata A/B Invariants
   ↓
Task 6  Bootloader Prevalidate + Installer Reuse
   ↓
Task 7  Rollback Transactions + Boot Decision
   ↓
Task 8  Automated Test / Fault Injection
   ↓
Task 9  Consolidated Manual Board Verification
   ↓
Task 10 Remove Test Hooks / Regression / Handoff
```

## Parallelism Guidance

在不违反资源所有权的前提下：

- Task 1 的 Watchdog Platform/Impl 与 Task 5 的 Metadata Host Contract 可在接口冻结后独立推进；
- Task 2/3/4 紧密耦合，主代理统一接口和集成；
- Task 6/7 紧密耦合，主代理统一 Bootloader API；
- Host Test 可与不共享构建输出的文档/fixture 工作并行；
- Keil Build、J-Link/GDB/RTT、同一串口、同一 Logic Analyzer 实例遵守现有资源互斥；
- 不允许多个执行者同时修改同一文件或同一紧耦合模块。

## Implementation Exit State

实施计划完成并同步正式上下文后：

```text
DESIGN_APPROVED
→ READY_FOR_IMPLEMENTATION
```

实际开始生产代码施工时再进入：

```text
READY_FOR_IMPLEMENTATION
→ IN_PROGRESS
```
