# W25Q64 STM32F411 External Loader

这是当前 STM32F411CEUx 板卡的 STM32CubeProgrammer External Loader。工程结构参考 ST 官方 `N25Q128A_STM32412G-DISCO` Loader，但接口改为当前板子的 SPI2/W25Q64JV。

## 硬件连接

```text
PB13  SPI2_SCK
PB14  SPI2_MISO
PB15  SPI2_MOSI
PB12  W25Q64_CS，低有效
```

Loader 使用 SPI Mode 0、MSB first，并校验 JEDEC ID `EF 40 17`。

## 构建

不需要在 Keil 图形界面手动创建工程。直接执行：

```powershell
05_Tools\ExternalLoader\W25Q64_STM32F411\build_external_loader.bat
```

工程文件：

```text
05_Tools/ExternalLoader/W25Q64_STM32F411/MDK-ARM/W25Q64_STM32F411.uvprojx
```

构建产生 AXF 后，脚本将其复制为：

```text
06_Output/Packages/ExternalLoader/W25Q64_STM32F411.stldr
```

`Objects/` 和 `Listings/` 是可再生的 Keil 构建输出，不属于工程输入。

## CubeProgrammer 使用

把 `.stldr` 通过 `-el` 传给 STM32CubeProgrammer，例如：

```powershell
STM32_Programmer_CLI.exe -c port=JLINK mode=HOTPLUG `
  -el .\06_Output\Packages\ExternalLoader\W25Q64_STM32F411.stldr `
  -r32 0x90000000 16
```

实际写入 Slot A/B 前必须使用物理映射镜像。工程现有 `app_v1.0.img` 是紧凑格式 `[64-byte Header][Payload]`，不能直接当作 Slot A 的线性物理镜像；Slot A 的 `0x000000~0x000FFF` 是 Header Sector，Payload 从 `0x001000` 开始。

本工具只操作 W25Q64，不写 AT24C02。AT24C02 Metadata 由 `toolkit.bat metadata baseline` 的临时目标固件负责写入和校验。

## Slot A 预烧录

使用 `preburn_w25q64.ps1` 将紧凑 `.img` 拆分为物理 Slot A 布局。脚本按“Payload 先写、Header 后写”执行，并在结束后分别读回 Header 和 Payload 做 SHA-256 比对：

```powershell
05_Tools\ExternalLoader\W25Q64_STM32F411\preburn_w25q64.ps1
```

脚本从 `05_Tools\Config\toolchain.local.bat` 读取 `STM32_PROGRAMMER_CLI`，不会把本机 CubeProgrammer 安装路径写入工程文件。

## 验证边界

Keil Clean Rebuild 成功只代表代码和制品验证通过，不代表真实板级读写通过。板级验证应单独记录 JEDEC、A/B Header、Payload 起点、擦除后的 `0xFF`、Page 写入回读和 `app_v1.0` 物理镜像结果。
