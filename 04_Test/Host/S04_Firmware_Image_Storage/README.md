# S04 Firmware Image Storage Host Tests

本目录保存 S04 可在 Host 上独立验证的数据格式、CRC 与存储编排测试。

`s04_crc_host_test.c` 覆盖 CRC-8/SMBUS、CRC-16/XMODEM、CRC-32/ISO-HDLC 的标准向量，以及一次性和分块计算的一致性。

## 本地验证入口

以下命令在仓库根目录执行。格式测试需要同时编译 `firmware_metadata.c`，并加入
`impl_board` include path，因为 `platform_types.h` 通过该路径取得底层 Board 类型定义。

```powershell
gcc -std=c99 -Wall -Wextra -Werror `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_common `
  -I03_Firmware/Application/OTA_APP/04_Impl/impl_board `
  -I03_Firmware/Application/OTA_APP/02_Service/service_common/crc `
  -I03_Firmware/Application/OTA_APP/02_Service/service_firmware `
  -o "$env:TEMP\s04_firmware_format_host_test.exe" `
  04_Test/Host/S04_Firmware_Image_Storage/s04_firmware_format_host_test.c `
  03_Firmware/Application/OTA_APP/02_Service/service_common/crc/crc.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_version.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_image.c `
  03_Firmware/Application/OTA_APP/02_Service/service_firmware/firmware_metadata.c

& "$env:TEMP\s04_firmware_format_host_test.exe"
python -m unittest 05_Tools/Firmware/test_pack_firmware.py -v
```

Storage Host Test 的完整命令和 PC 打包工具的交叉解码命令以同阶段
`implementation_plan.md` 为准。Reset Persistence 与 Power-cycle Persistence
属于真实板测场景，本轮按 Project Owner 决定跳过，不能以 Host Test 或编译结果替代。
