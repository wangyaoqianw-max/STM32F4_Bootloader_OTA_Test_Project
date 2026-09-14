# S05 UART YMODEM Host Tests

本目录保存 S05 可在 Host 上验证的 Firmware Storage 写入边界测试。

## Firmware Storage Write Test

在仓库根目录执行：

```powershell
gcc -std=c99 -Wall -Wextra -Werror `
  -I04_Test/Host/S05_UART_Ymodem/stubs `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_common `
  -I03_Firmware/Application/OTA_APP/04_Impl/impl_board `
  -I03_Firmware/Application/OTA_APP/02_Service/service_common/crc `
  -I03_Firmware/Application/OTA_APP/02_Service/service_firmware `
  -o "$env:TEMP\s05_firmware_storage_write_host_test.exe" `
  04_Test/Host/S05_UART_Ymodem/s05_firmware_storage_write_host_test.c `
  03_Firmware/Application/OTA_APP/02_Service/service_common/crc/crc.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_version.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_image.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_metadata.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_storage.c

& "$env:TEMP\s05_firmware_storage_write_host_test.exe"
```

S04 的完整 Firmware Storage 回归测试仍按 `04_Test/Host/S04_Firmware_Image_Storage/README.md` 执行。
