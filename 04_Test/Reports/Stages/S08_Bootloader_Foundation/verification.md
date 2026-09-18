# S08 Bootloader Foundation Verification

## Metadata

- Stage: `S08_Bootloader_Foundation`
- Status: `READY_FOR_VERIFICATION`
- Implementation Commits: `8ec0045`
- Verification Commit: `pending`
- Branch: `main`
- Verification Date: `2026-09-18`

## Scope

本报告验证 S08 的 Internal Flash Layout、Bootloader 诊断基础和 Application Jump。S09 的 External Flash / EEPROM / Metadata consume / Internal Flash Installation，以及 S10 的 Trial / Confirm / Rollback 均未实施。

## Code Verification

| 项目 | 结果 | 证据 |
| --- | --- | --- |
| `05_Tools\\toolkit.bat sync-s08` | PASS | CubeMX 生成后重新应用 Keil IROM/OCR、Include、Define、Boot source groups 和 Application VTOR |
| Keil Bootloader Build | PASS | `05_Tools\\toolkit.bat build bootloader`，0 error / 0 warning |
| Keil Application Build | PASS | `05_Tools\\toolkit.bat build application`，0 error / 0 warning |
| S08 PowerShell parser checks | PASS | sync / target / build / flash / RTT / run / snapshot / fault / toolkit |
| Debug workflow contract | PASS | `05_Tools\\Contracts\\Debug\\test_debug_workflows.ps1` |
| Fault diagnostic contract | PASS | `05_Tools\\Debug\\CmBacktrace\\test_fault_diagnostics.ps1` |
| CmBacktrace integration contract | PASS | `05_Tools\\Debug\\CmBacktrace\\test_cm_backtrace_integration.ps1` |
| C/H/ASM style check | PASS | S08 自研文件无 Tab、无超过 120 列的代码行；公共 API 和文件头按嵌入式 C 规范复核 |
| Bootloader dependency boundary | PASS | Target 工程无 FreeRTOS / EasyLogger build dependency；Fault Injection 默认关闭 |

## Flash Layout / Map Evidence

| Image | Base / Limit | Map result |
| --- | --- | --- |
| Bootloader | `0x08000000` / `0x00010000` | LR size `0x00002da8`，ER size `0x00002d80`，未超过 64 KiB |
| Application | `0x08010000` / `0x00070000` | LR size `0x00013f20`，ER size `0x00013d68`，未超过 448 KiB |

关键符号：

- Bootloader `__Vectors = 0x08000000`，`Reset_Handler = 0x08000241`；
- Application `__Vectors = 0x08010000`，`Reset_Handler = 0x08010281`；
- Application `VTOR` 源码使用共享 `APP_BASE_ADDR`；
- 两个 Flash 区间相邻但不重叠；
- Bootloader map：`03_Firmware/Bootloader/OTA_Bootloader/MDK-ARM/Objects/OTA_Bootloader.map`；
- Application map：`03_Firmware/Application/OTA_APP/MDK-ARM/Listings/OTA_APP.map`。

## Board Evidence

| 项目 | 结果 | 证据 |
| --- | --- | --- |
| Final Bootloader / Application Flash | PASS | `toolkit.bat flash application prepare`、`flash bootloader prepare`、`flash bootloader run` |
| RTT Application after Boot jump | PASS | `06_Output/Logs/OTA_Bootloader_rtt.log`，观察到 EasyLogger、`appMainTask`、`otaWorker`、`displayTask` 初始化及 Display backlight |
| Valid APP vector / jump | PASS | `06_Output/Logs/S08_bootloader_gdb_handoff.log`；MSP `0x2000E690`、Reset `0x08010281`，命中 `boot_jump_to_app` |
| APP GDB runtime snapshot | PASS | `06_Output/Logs/OTA_APP_gdb_snapshot.log`；PC 位于 FreeRTOS `prvIdleTask`，`MSP=0x2000E670`、`PSP=0x20000C28`、`PRIMASK=0` |
| CmBacktrace controlled Fault | PASS | `06_Output/Logs/OTA_Bootloader_fault_gdb.log` / `OTA_Bootloader_fault_rtt.log`；`CFSR=0x00000400`、Fault PC `0x080022AA`，GDB/RTT 交叉一致 |
| Invalid MSP reject | PASS | 受控 RAM vector test：`MSP_INVALID`，未进入 Jump |
| Invalid Reset_Handler reject | PASS | 受控 RAM vector test：`RESET_INVALID`，未进入 Jump |
| Erased vector reject | PASS | `06_Output/Logs/OTA_Bootloader_rtt.log`：`APP validation FAIL: ERASED` |
| GDB/J-Link cleanup | PASS | Snapshot halt/resume、Flash、RTT 和 Fault workflow 均通过统一入口，测试后无 GDB/J-Link client 残留 |

## Pending Owner Checks

以下项目属于需要 Project Owner 在目标板上最终确认的硬件验收，不用代码结果替代：

- 真实断电再上电后的 Bootloader → Application 稳定启动；
- Application LED / LCD 基础行为的现场确认；
- 至少一个 Application 外设中断路径在 Jump 后的现场确认。

重复 Reset 已通过统一 J-Link 运行路径完成；本报告暂不把它等同于真实 Power-cycle。

## Verification Status

```text
代码验证：PASS
硬件验证：PENDING
```

S08 实施范围内的代码、构建、Flash Layout、RTT、CmBacktrace、向量拒绝和合法 Jump 已完成。完成阶段关闭前，补齐上面的 Project Owner 硬件检查并形成 Review 结论。
