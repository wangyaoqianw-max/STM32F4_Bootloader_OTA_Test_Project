# S10 Trial Confirm Rollback Verification Report

## Metadata

- Stage: `S10_Trial_Confirm_Rollback`
- Status: `READY_FOR_VERIFICATION`
- Branch: `main`
- Plan Baseline: `651b3001b4c23cf4162e3367a91ae43307207bce`
- Actual Implementation Baseline: `79b95d1f4681c2f7b5f961785079a112a3c62492`
- Implementation Commits: `dfb012b`, `cd15bad`, `e245e21`, `26ce0ee`, `1ae57ef`, `d1b08b2`, `a43086c`, `352ee62`, `c237361`, `54c9827`, `b40d1b4`, `a5295c2`
- Verification follow-up commits: `5ca2d14`, `f0e5d88`, `25d2acc`, `0ee7f5d`, `ba6ce6f`, `7eab4cc`
- Code Verification: `PASS`
- Hardware Verification: `PARTIAL`

## Coding Standard

```text
Coding Standard:
03_Firmware/00_Doc/Standards/嵌入式C代码规范.md
Status: READ
```

本阶段未重新运行 CubeMX。IWDG 通过 HAL module/source、Keil 工程、Platform/Impl abstraction 和 DBGMCU debug freeze 手工接入；`otaWorker` 继续持有 Firmware Storage I/O ownership。

## Implementation Result

Implementation Plan Task 0→10 已完成，核心结果如下：

- Application：Watchdog capability、Runtime Health 状态机、strict Trial Confirm、Runtime Ready/Feed/Confirm handshake。
- Metadata：保留 `NONE / PENDING / TRIAL / ROLLBACK`，强化 A/B identity 和 state invariants；Confirm 与 Rollback 均保持原子提交边界。
- Bootloader：Pending install 与 Confirmed restore 共用验证/擦写/回读核心；未 Confirm 的 Trial boot 先持久化 `ROLLBACK`，再执行 confirmed image restore。
- Reset Cause：仅用于诊断记录，不替代 Trial/Rollback 状态决策。
- 临时生产测试代码：`NONE`。最终代码中没有 TEST ONLY hook、强制失败或强制复位行为。

## Automated Verification

### Host and Contract Tests

- S02/S04/S05/S07/S07A/S09 regression Host Tests：`PASS`。
- S10 Runtime Health、Lifecycle Confirm、Metadata Invariant、Boot Recovery、Rollback Transaction Host Tests：`PASS`。
- `05_Tools/Contracts` 中 Application/Bootloader/兼容性/调试/工具流程 contract：`PASS`；本轮重新执行的 Application workflow、Runtime Health、Watchdog、Task Lifecycle、Boot Decision、Transport、Compatibility、Core、Debug、Logic Analyzer 和 S04 isolation contract 均通过。
- S10 Boot Decision contract、GDB automation、CmBacktrace/fault diagnostics、tool sequence contract：`PASS`。
- S05C parser fixture：`PASS`。
- S05C SPI/I²C 项目断言：使用解析器夹具结果执行，`test_spi.ps1` 两套 SPI 夹具 `PASS`，`test_i2c.ps1` I²C 夹具 `PASS`；真实逻辑分析仪采集仍待板测。

### Python Tests

- Firmware pack tests：`2/2 PASS`。
- YMODEM tests：`24/24 PASS`。
- S04 persistence tests：`15/15 PASS`。
- 本轮重新执行结果保持一致：Firmware pack `2/2`、YMODEM `24/24`、S04 persistence `15/15`。

### Build Verification

- Application `05_Tools\\toolkit.bat build application`：`PASS`，0 errors / 0 warnings。
- Bootloader `05_Tools\\toolkit.bat build bootloader`：`PASS`，0 errors / 0 warnings。
- Application true clean raw UV4 build：`PASS`，0 errors / 0 warnings。
- Application Total ROM：`84616 bytes (82.63 KiB)`；`configTOTAL_HEAP_SIZE=24576`，startup stack `1024 bytes`。
- Bootloader Total ROM：`22372 bytes (21.85 KiB)`，低于 `64 KiB` 限制。

### Verification Pause and Stable Firmware Recovery

2026-09-20 按用户要求暂停 S10 人工板测。临时 `S10_TEST_MANUAL_TRIAL_WINDOW_ENABLE` 开关、`S10-TEST ONLY` 标记和临时恢复脚本均已移除；在 Application 生产源文件中未发现相关测试开关或标记，正式 Application 重新构建后执行：

```text
05_Tools\toolkit.bat build application       PASS; 0 error / 0 warning
05_Tools\toolkit.bat flash application run  PASS
05_Tools\toolkit.bat rtt application 8     PASS
```

最新 RTT 证据：

```text
OTA runtime init result=0
Application init result: 0
Display init result: 0
Display initial render result: 0
Display backlight on result: 0
```

该证据表示正式 Application 已重新烧录并完成正常启动初始化；LED 肉眼现象未在本轮重新确认。之后未继续发包、按键或断电，S10 硬件验证仍保持 `PARTIAL / PENDING`，不得升级为 Rollback PASS。

### S10 Test Plan Execution (Run `20260920-185951`)

本轮按 `00_Project/03_Stages/S10_Trial_Confirm_Rollback/test_plan.md` 的固定顺序执行，用户已明确确认 Factory Restore 的破坏性操作。

- `S10-00`：`PASS`。只读前置检查确认 COM9、工具路径、J-Link 独占状态、测试镜像结构与 CRC 均满足前置条件；证据目录：`06_Output/Logs/S10/20260920-185951/S10-00/`。
- `S10-01`：`BLOCKED`。Factory Restore 使用 `app_v1.0.img`、COM9、115200 启动；临时测试固件构建和烧录 `PASS`，YMODEM 传输 `81412 bytes / 80 blocks / exit 0`，但目标 RTT 只输出到 `destructive erase Slot A/B start`，随后 RTT 捕获返回错误码 `32`。因此未取得 F0 的 Internal APP、Slot A/B、Metadata 和 Application `RUNNING/STABLE` 完整证据，不能将 Known-Good Factory Baseline 记为 `PASS`。
- 失败后的恢复动作：正式 Application 重新构建、烧录、RTT 捕获、GDB halt 快照和 GDB resume 均 `PASS`；这只证明测试失败后恢复到可运行正式固件，不替代 S10-01 基线通过。
- 依据停止条件，`S10-02` 至 `S10-09` 本轮均未执行；没有在未证明 F0 的情况下继续改变 Trial/Metadata 状态。完整输出见 `06_Output/Logs/S10/20260920-185951/S10-01/`，临时测试 RTT 原始输出见 `06_Output/Logs/S09_Factory_Restore/provision_rtt_raw.log`。

### S10-01 根因隔离与停止（Run `20260920-continue-root-cause`）

为区分 W25Q64 写路径和 Factory Restore 传输链，本轮先做了最小物理写入，再做了独立的失败后读回：

1. 直接通过正式 Application 的 W25Q64 API 擦除 Slot A，写入 64-byte Header 和 16-byte 载荷；立即读回正确，复位后再次读回仍正确，证明 W25Q64、SPI 写驱动和非易失保持能力正常。
2. Factory Restore 失败批次的 `ymodem.log` 显示数据块 0～80 均完成 ACK，随后进入 EOT；结束 Header 阶段没有完成，也没有生成 JSON 结果。该批次的 Payload 已实际写入，随后目标被恢复为正式 Application。
3. 恢复后独立 GDB 读取结果：Slot A Header 64 byte 全为 `0xFF`；Slot A `+0x1000` 读到 `0x2000E690, 0x08010281, ...`，与 `app_v1.0.img` Payload 开头一致；Slot B Header 全为 `0xFF`。证据：`06_Output/Logs/S10/20260920-continue-root-cause/factory-failed-slot-read.gdb.log`。

根因确定为 Factory Restore 的外层进程超时：`factory_restore.ps1` 给 Sender 设置 `--timeout 120`，但统一 `Invoke-ToolkitProcess` / `Complete-ToolkitProcess` 默认只等待 `60000 ms`。Sender 在结束 Header 尚未完成时被外层进程杀掉；而 `ota_firmware_sink` 使用 Header-last，故形成“Payload 已写、Slot A Header 未提交”的半成品。后续 Bootloader `confirmed prevalidate` 失败是该半成品的下游表现，不是 Rollback 擦除了 A。

因此，`baseline PASS`、Sender 已完成数据块、Sender/工具退出码以及一键流程 `[FACTORY][PASS]` 均不足以建立 F0。必须在结束 Header ACK 后独立读回 A/B Header 和 A `+0x1000`，并在正式 Application 烧录/复位后再次复读，才能进入 S10-02。

### S10-04 Trial Power-Cycle Execution (Run `20260921-121213`)

本轮按修正后的顺序执行：先用新的 External Loader 恢复 F0，再预启动 RTT 与 YMODEM Sender，解析完成后提醒第二次 PA0，随后在 v1.1 LED 闪动窗口执行真实断电。旧 Factory Restore 没有参与本轮 F0。

- F0：`PASS`。External Loader 目录 `06_Output/Logs/S10/20260921-121213/F0/ExternalLoader/` 中 Slot A Header/Payload 读回 SHA256 与源文件一致；Metadata 原始 RTT 报告 `baseline PASS`；正式 Application 启动 RTT 通过。
- OTA：`PASS`。COM9/115200 YMODEM 完成 `84680` bytes、83 blocks、retries 2、exit 0；实时 RTT 在第二次 PA0 前达到 `OTA state=4`、`progress=84680/84680`、`error=0`。
- v1.1 启动：`OBSERVED`。第二次 PA0 后实时 RTT 重新出现 Application 初始化和 `Startup state=1 main=0 ota=0 display=0`；本轮没有用固定延时判断窗口。
- 真实断电与视觉结果：`PASS for observed behavior`。用户观察到 v1.1 运行后 LED 闪动，断电上电后 LED 频率恢复为 v1.0；LCD/Bootloader 中间过程未在本轮重新完整记录。
- 上电后内部镜像：`PASS`。`06_Output/Logs/S10/20260921-121213/F3/validation.txt` 记录从 `0x08010000` 读回的 `81348` bytes 与 v1.0 payload 逐字节匹配，`FIRST_MISMATCH=-1`；上电后 Application RTT 初始化结果均为 `0`。
- Bootloader 中间证据：`MISSING`。Application RTT 控制块为 `0x2000DE04`，Bootloader RTT 控制块为 `0x200000E0`；保持单个固定地址的 J-Link RTT Logger 不能跨复位/掉电自动切换，因此没有取得 `TRIAL → ROLLBACK → restore → NONE` 的完整 Bootloader RTT 链。原始现场边界记录在 `06_Output/Logs/S10/20260921-121213/F1_F2_live_observation.md`。

本轮结论为 `PARTIAL`：最终硬件行为和 v1.0 内部镜像读回支持“已回滚到 v1.0”，但按照 S10-04 通过判据，缺少 Bootloader 决策、Confirmed restore、CRC/vector 和 Metadata 最终状态的连续证据，不能升级为完整 Rollback PASS。下一轮必须先修正 RTT 监听编排，再重复该用例；不得依靠 LED 闪烁次数单独补齐中间证据。

## Board Verification Boundary

本轮已取得部分真实目标板证据，但尚未完成完整 S10 板级验收。以下状态只依据本轮可回读的 RTT/GDB/传输结果，不把历史日志或发送端单方面成功当作通过：

| 项目 | 状态 |
| --- | --- |
| Factory Restore / Flash | `BLOCKED; root cause is outer 60 s process timeout killing the 120 s Sender before end Header commit; failed batch left Slot A Payload present but Header erased; formal recovery passed` |
| RTT capture / Reset Cause | `PARTIAL; post-power-cycle Application startup and YMODEM evidence captured; pre-Confirm Rollback RTT pending` |
| GDB target session / snapshot | `PASS for Application idle and IWDG probe; pre-Confirm Rollback breakpoint evidence pending` |
| IWDG timeout / debug freeze | `PASS; register/config, >10 s Debug Halt evidence and direct no-feed IWDG reset evidence PASS` |
| Trial runtime Ready / strict Confirm | `PASS for runtime handshake; persistent post-power-cycle state pending` |
| software reset / IWDG reset / power-cycle | `PARTIAL; recovery power-cycle and no-feed IWDG reset PASS, but Trial power-cycle before Confirm remains pending` |
| interrupted rollback and restart-from-zero | `PENDING / NOT_EXECUTED` |
| PA0 OTA/install input | `PASS; two PA0 actions reached install and Confirm flow` |
| LED / LCD manual observation | `PENDING / NOT_EXECUTED` |
| S05C real I2C/SPI capture | `PENDING / NOT_EXECUTED` |

### Real Target Evidence Collected

- Formal Application `NONE` startup after the Event Flags mapping fix: startup result fields are `PLATFORM_ERR_OK`, system state is `RUNNING`, Health is `STABLE`, and `trial=0`; GDB stopped at the FreeRTOS idle path without the previous controlled-reset loop.
- `app_v1.1.img` was rebuilt as `84680 bytes` (`84616-byte payload`, `83` YMODEM blocks). The real target transfer completed with `84680 bytes`, `retries=2`, sender exit code `0`; RTT reached `READY_TO_INSTALL` with `84680/84680` bytes.
- After the second PA0 action, GDB read `g_appHealthContext.readyMask=0x7`, `state=APP_HEALTH_STATE_STABLE`, `trial=1`, `g_appHealthConfirmResult=PLATFORM_ERR_OK`, and `g_appStartupContext.systemState=APP_SYSTEM_STATE_RUNNING`. This proves the runtime Ready/strict Confirm handshake, but does not by itself prove the persisted Metadata after a later power-cycle.
- The latest S09 Factory Restore retry opened Sender before flash but the target still reported `Soft-I2C init FAIL: BUSY` / `BOOT halt: external device init`; Sender then timed out waiting for the initial `C`. It is not counted as a new Factory Restore PASS. The earlier S09 baseline evidence remains S09-owned.
- Verification follow-up on 2026-09-20 reran the S10 Host Tests, static contracts, Python regressions and both Keil builds successfully; no production source was changed in this follow-up. Application ROM remains `84616 bytes (82.63 KiB)` and Bootloader ROM remains `22372 bytes (21.85 KiB)`.
- The Factory Restore workflow sequencing/recovery fix is committed as `0ee7f5d`. A subsequent clean Bootloader flash produced fresh RTT `Soft-I2C init FAIL: BUSY` / `BOOT halt: external device init`; GDB stopped in `diagnostics_fault_entry` with the backtrace reaching `boot_soft_i2c_init()` line 287. `DIAG_FAULT_TEST_ENABLE` remains `0` and the clean Bootloader disassembly contains no test-trigger call, so this is retained as historical board startup fault evidence rather than S10 Fault Injection evidence.
- User-performed power-cycle recovery on 2026-09-20 cleared the observed startup blockage: fresh Bootloader/Application RTT reached Application initialization, with Storage SPI, OTA UART, Display SPI and backlight all reporting `result: 0`; a subsequent GDB snapshot stopped in the FreeRTOS idle task, not a fault handler.
- Real IWDG evidence: register/config probe read `IWDG.PR=0x00000006`, `IWDG.RLR=0x000004E1`, `IWDG.SR=0`, and `DBGMCU.APB1FZ=0x00001800`; the target stayed in Application while halted for 12 seconds and resumed successfully. The direct no-feed GDB run then hit `IWDG_RESET_HANDLER_HIT` with `RCC_CSR=0x24000000` (`IWDGRSTF + PINRSTF`), recorded in `06_Output/Logs/S10_iwdg_no_feed_gdb.log`; the wrapper-only halt marker warning does not invalidate the raw reset evidence.
- A repeated v1.1 YMODEM transfer completed with `84680 bytes`, `83` blocks, `retries=2`, sender exit `0`. After install/reboot, GDB observed `trial=1`, `readyMask=0x7`, Health `STABLE`, System `RUNNING`, and Confirm result `PLATFORM_ERR_OK`; a later tool-controlled reset observed `trial=0`. Because the pre-Confirm checkpoint was not captured and no Bootloader Rollback RTT was read, this sequence is not counted as a `TRIAL → ROLLBACK` PASS; the application had likely reached its automatic Confirm window.
- In run `20260920-185951`, S10-01 Factory Restore transferred `app_v1.0.img` successfully but the temporary target RTT stopped immediately after the destructive erase start; RTT capture returned error `32`. The workflow then restored formal Application, and the follow-up formal RTT plus GDB halt/resume snapshots passed. This run is recorded as `BLOCKED`, not as a new Factory Restore baseline pass.

Verification Role still needs a Trial power-cycle before automatic Confirm, Bootloader Rollback RTT/GDB evidence, interrupted rollback/restart-from-zero, and visual LED/LCD scenarios; Review Role must decide closure afterward. The recovery power-cycle just completed is not substituted for the Trial power-cycle case. These checks are paused and will resume in a new conversation.

## S09 Deferred Boundary

S09 Deferred Fault Injection（erase/program 分段、Internal CRC、Metadata body/marker 和 Power Loss）本次未完成。最新 Factory Restore 重试的目标端 `worker result=2` / `ERROR/TIMEOUT` 证据不覆盖此前 baseline PASS，也不计入 S10；这些项目仍归属 S09 supplementary verification。

## Handoff Result

代码和自动化证据满足 Implementation Plan 的实现退出条件，阶段保持 `READY_FOR_VERIFICATION`。人工板测已按用户要求暂停，后续新对话继续剩余 Trial 断电、Rollback 和视觉验收。硬件验证仍为 `PARTIAL / PENDING`，阶段不得标记 `CLOSED`。本轮生成的构建缓存和 Python cache 仍可能存在于本地未跟踪状态，但未加入提交；本报告和 `verification_matrix.md` 是当前可回读证据入口。
