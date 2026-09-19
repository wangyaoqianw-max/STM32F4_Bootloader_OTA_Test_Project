# S10 Trial Confirm Host Test

本目录验证 Application Firmware Lifecycle 的严格 Runtime Confirm 事务。测试通过替换 `firmware_storage_*` 接口，验证状态门禁、完整镜像校验、Metadata 原子提交以及提交后的 latest Metadata 回读；不进入 Keil 工程，也不访问真实外设。

在仓库根目录执行：

```powershell
$gcc = 'C:\MinGW\bin\gcc.exe'
$out = Join-Path $env:TEMP 's10_lifecycle_host_test.exe'
& $gcc -std=c11 -Wall -Wextra -Werror `
  -I03_Firmware/Application/OTA_APP/02_Service/service_firmware `
  -I03_Firmware/Application/OTA_APP/02_Service/service_common/crc `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_common `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_bsp `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64 `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/at24c02 `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/gpio `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/spi `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/i2c `
  -I03_Firmware/Application/OTA_APP/04_Impl/impl_board `
  -o $out `
  04_Test/Host/S10_Trial_Confirm_Rollback/s10_lifecycle_host_test.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_lifecycle.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_version.c
& $out
Remove-Item -LiteralPath $out -Force
```

覆盖：

- `TRIAL`、Slot A/B、pending `VALID` 与 pending != confirmed 门禁；
- Header、容量、Payload CRC 和 Version 校验失败；
- Metadata commit 失败与 commit 后回读不一致；
- 正常 `TRIAL → NONE` 以及 confirmed Slot/Version 更新。

Runtime Health 状态机测试：

```powershell
$gcc = 'C:\MinGW\bin\gcc.exe'
$out = Join-Path $env:TEMP 's10_health_host_test.exe'
& $gcc -std=c11 -Wall -Wextra -Werror `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_common `
  -I03_Firmware/Application/OTA_APP/04_Impl/impl_board `
  -I03_Firmware/Application/OTA_APP/01_APP `
  -I03_Firmware/Application/OTA_APP/01_APP/system `
  -o $out `
  04_Test/Host/S10_Trial_Confirm_Rollback/s10_health_host_test.c `
  03_Firmware/Application/OTA_APP/01_APP/system/app_health.c
& $out
Remove-Item -LiteralPath $out -Force
```

另有 `05_Tools/Contracts/Application/test_s10_runtime_health_integration.ps1` 检查 Health 初始化、三个 Runtime Ready、单次 Confirm 请求、otaWorker 执行和窄 Runtime 状态接口均已接入。

Metadata A/B 生命周期不变量测试：

```powershell
$gcc = 'C:\MinGW\bin\gcc.exe'
$out = Join-Path $env:TEMP 's10_metadata_invariant_host_test.exe'
& $gcc -std=c11 -Wall -Wextra -Werror `
  -I03_Firmware/Bootloader/OTA_Bootloader/Firmware `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_common `
  -I03_Firmware/Application/OTA_APP/04_Impl/impl_board `
  -I03_Firmware/Application/OTA_APP/02_Service/service_common/crc `
  -I03_Firmware/Application/OTA_APP/02_Service/service_firmware `
  -o $out `
  04_Test/Host/S10_Trial_Confirm_Rollback/s10_metadata_invariant_host_test.c `
  03_Firmware/Bootloader/OTA_Bootloader/Firmware/boot_crc32.c `
  03_Firmware/Bootloader/OTA_Bootloader/Firmware/boot_metadata.c `
  03_Firmware/Application/OTA_APP/02_Service/service_common/crc/crc.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_version.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_metadata.c
& $out
Remove-Item -LiteralPath $out -Force
```

该测试同时验证 Application/Bootloader 编码结果一致，并验证两端都拒绝
`pendingSlot == confirmedSlot`、无效 confirmed Slot 基线和无效 pending Slot，
同时保留 Factory/stable `NONE` 记录。

Bootloader Pending Install / Confirmed Restore 测试：

```powershell
$gcc = 'C:\MinGW\bin\gcc.exe'
$out = Join-Path $env:TEMP 's10_boot_recovery_host_test.exe'
& $gcc -std=c11 -Wall -Wextra -Werror -DSTM32F411xE -DUSE_HAL_DRIVER `
  -I03_Firmware/Bootloader/OTA_Bootloader/Firmware `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/Bus/SPI `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/Bus/SoftI2C `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/W25Q64 `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/AT24C02 `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/InternalFlash `
  -I03_Firmware/Bootloader/OTA_Bootloader/Core/Inc `
  -I03_Firmware/Bootloader/OTA_Bootloader/Config `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/STM32F4xx_HAL_Driver/Inc `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/CMSIS/Device/ST/STM32F4xx/Include `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/CMSIS/Include `
  -I03_Firmware/Bootloader/OTA_Bootloader/Boot `
  -I03_Firmware/Shared `
  -o $out `
  04_Test/Host/S10_Trial_Confirm_Rollback/s10_boot_recovery_host_test.c `
  03_Firmware/Bootloader/OTA_Bootloader/Firmware/boot_crc32.c `
  03_Firmware/Bootloader/OTA_Bootloader/Firmware/boot_image.c `
  03_Firmware/Bootloader/OTA_Bootloader/Firmware/boot_metadata.c `
  03_Firmware/Bootloader/OTA_Bootloader/Boot/boot_validate.c `
  03_Firmware/Bootloader/OTA_Bootloader/Boot/boot_prevalidate.c `
  03_Firmware/Bootloader/OTA_Bootloader/Boot/boot_installer.c
& $out
Remove-Item -LiteralPath $out -Force
```

该测试验证 Pending/Confirmed 来源选择、共享验证/安装核心、Confirmed version、
Payload CRC、Vector、同 Slot 门禁，以及 prevalidate 失败不得擦除 Internal APP。

Rollback Metadata 原子事务测试：

```powershell
$gcc = 'C:\MinGW\bin\gcc.exe'
$out = Join-Path $env:TEMP 's10_rollback_transaction_host_test.exe'
& $gcc -std=c11 -Wall -Wextra -Werror -DSTM32F411xE -DUSE_HAL_DRIVER `
  -I03_Firmware/Bootloader/OTA_Bootloader/Firmware `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/Bus/SoftI2C `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/AT24C02 `
  -I03_Firmware/Bootloader/OTA_Bootloader/Core/Inc `
  -I03_Firmware/Bootloader/OTA_Bootloader/Config `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/STM32F4xx_HAL_Driver/Inc `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/CMSIS/Device/ST/STM32F4xx/Include `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/CMSIS/Include `
  -I03_Firmware/Bootloader/OTA_Bootloader/Boot `
  -o $out `
  04_Test/Host/S10_Trial_Confirm_Rollback/s10_rollback_transaction_host_test.c `
  03_Firmware/Bootloader/OTA_Bootloader/Firmware/boot_crc32.c `
  03_Firmware/Bootloader/OTA_Bootloader/Firmware/boot_metadata.c `
  03_Firmware/Bootloader/OTA_Bootloader/Boot/boot_metadata_commit.c
& $out
Remove-Item -LiteralPath $out -Force
```

Boot decision 静态契约：

```powershell
& .\05_Tools\Contracts\Bootloader\test_s10_boot_decision_contract.ps1
```

该测试验证 `TRIAL → ROLLBACK → restore → NONE` 的调用顺序、`ROLLBACK`
重启后的完整恢复路径，以及 Reset Cause 仅用于诊断。
