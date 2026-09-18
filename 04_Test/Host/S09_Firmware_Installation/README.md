# S09 Bootloader Contract Host Test

本目录验证 Bootloader 的独立 Firmware Header、Metadata V2 与 CRC 合同。测试直接编译 `03_Firmware/Bootloader/OTA_Bootloader/Firmware` 下的生产源码，不把 Application Firmware Service 链入 Bootloader。

在仓库根目录执行：

```powershell
gcc -std=c99 -Wall -Wextra -Werror `
  -I03_Firmware/Bootloader/OTA_Bootloader/Firmware `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_common `
  -I03_Firmware/Application/OTA_APP/04_Impl/impl_board `
  -I03_Firmware/Application/OTA_APP/02_Service/service_common/crc `
  -I03_Firmware/Application/OTA_APP/02_Service/service_firmware `
  -o "$env:TEMP\s09_contract_host_test.exe" `
  04_Test/Host/S09_Firmware_Installation/s09_contract_host_test.c `
  03_Firmware/Bootloader/OTA_Bootloader/Firmware/boot_crc32.c `
  03_Firmware/Bootloader/OTA_Bootloader/Firmware/boot_image.c `
  03_Firmware/Bootloader/OTA_Bootloader/Firmware/boot_metadata.c `
  03_Firmware/Application/OTA_APP/02_Service/service_common/crc/crc.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_version.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_metadata.c

& "$env:TEMP\s09_contract_host_test.exe"
```

预校验门禁测试使用同样的 Bootloader Firmware/Boot 源文件，并通过只读外部存储替身覆盖 Header、Payload CRC、向量和 PENDING 条件：

```powershell
gcc -std=c99 -Wall -Wextra -Werror -DSTM32F411xE -DUSE_HAL_DRIVER `
  -I03_Firmware/Bootloader/OTA_Bootloader/Firmware `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/Bus/SPI `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/Bus/SoftI2C `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/W25Q64 `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/AT24C02 `
  -I03_Firmware/Bootloader/OTA_Bootloader/Core/Inc `
  -I03_Firmware/Bootloader/OTA_Bootloader/Config `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/STM32F4xx_HAL_Driver/Inc `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/CMSIS/Device/ST/STM32F4xx/Include `
  -I03_Firmware/Bootloader/OTA_Bootloader/Drivers/CMSIS/Include `
  -I03_Firmware/Bootloader/OTA_Bootloader/Boot `
  -I03_Firmware/Shared `
  -o "$env:TEMP\s09_prevalidate_host_test.exe" `
  04_Test/Host/S09_Firmware_Installation/s09_prevalidate_host_test.c `
  03_Firmware/Bootloader/OTA_Bootloader/Firmware/boot_crc32.c `
  03_Firmware/Bootloader/OTA_Bootloader/Firmware/boot_image.c `
  03_Firmware/Bootloader/OTA_Bootloader/Firmware/boot_metadata.c `
  03_Firmware/Bootloader/OTA_Bootloader/Boot/boot_validate.c `
  03_Firmware/Bootloader/OTA_Bootloader/Boot/boot_prevalidate.c

& "$env:TEMP\s09_prevalidate_host_test.exe"
```

测试覆盖：

- PC Packer `app_v1.1.img` Header 固定字节向量与 Header CRC；
- Bootloader CRC-32/ISO-HDLC 标准向量及分块计算；
- Application Metadata V2 → Bootloader decode；
- Bootloader Metadata record → Application decode；
- 双 Metadata Copy 的 sequence 选择与未提交 marker 拒绝。

本测试不证明真实 SPI/I2C、Flash 擦写或板级安装结果；这些由 S09 后续 Driver、Installer 和板测任务提供证据。
