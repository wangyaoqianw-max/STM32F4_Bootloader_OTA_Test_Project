# S07A RTOS Startup Host Test

本目录验证 Startup Context、DONE Barrier、DEGRADED 裁决和广播式
`SYSTEM_RUN`/`SYSTEM_ABORT` 合同。测试直接编译 `01_APP/system/app_startup.c`，不进入 Keil 固件工程。

在仓库根目录执行：

```powershell
gcc -std=c99 -Wall -Wextra -Werror `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_common `
  -I03_Firmware/Application/OTA_APP/04_Impl/impl_board `
  -I03_Firmware/Application/OTA_APP/03_Platform/platform_os `
  -I03_Firmware/Application/OTA_APP/01_APP `
  -I03_Firmware/Application/OTA_APP/01_APP/system `
  -o "$env:TEMP\s07a_startup_contract_host_test.exe" `
  04_Test/Host/S07A_RTOS_Startup/s07a_startup_contract_host_test.c `
  03_Firmware/Application/OTA_APP/01_APP/system/app_startup.c

& "$env:TEMP\s07a_startup_contract_host_test.exe"
```

该测试不覆盖真实 CMSIS-RTOS2 调度、板级 Task List、Heap 或硬件验收；这些证据由 S07A Verification Report 记录。
