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
