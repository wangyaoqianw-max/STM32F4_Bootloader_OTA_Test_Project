# W25Q64 STM32F411 External Loader 实施计划

## 目标

在 `05_Tools` 下创建独立 Keil External Loader 工程，构建 `W25Q64_STM32F411.stldr`，并保留后续 S10 物理读回验证入口。

## 步骤

1. 创建 `05_Tools/ExternalLoader/W25Q64_STM32F411/` 的工程、源码和文档目录。
2. 参考官方 `N25Q128A_STM32412G-DISCO` 的 `Dev_Inf` 和 Loader API，写入 W25Q64 几何信息。
3. 实现 STM32F411 SPI2/PB12～PB15 裸机驱动，以及 W25Q64 的 JEDEC、状态、读、页写、Sector Erase、Chip Erase 和等待 Ready。
4. 生成 Keil `.uvprojx`，使用 STM32F411CEUx、ARMCC 和 RAM Loader 链接布局；输出目录固定为 `MDK-ARM/Objects` 与 `MDK-ARM/Listings`。
5. 用本机 Keil 执行 Clean Rebuild，检查 0 error、入口符号、`StorageInfo` 地址和输出段布局。
6. 将 AXF 复制为 `.stldr` 到 `06_Output/Packages/ExternalLoader/`，补充 README 中的安装和命令行用法。
7. 在不覆盖当前用户改动的前提下运行 `git diff --check` 和工作区检查，记录代码验证结果；真实板测单独标为 PENDING，除非用户另行要求执行。

## 停止条件

- 工具链不可用或 Keil 无法识别新工程：停止并保留日志；
- 链接段无法同时满足 `StorageInfo` 和 SRAM Loader：停止，不用 Application 工程替代；
- 构建通过但 STLDR 无法被 CubeProgrammer 识别：先修正制品布局，不进入破坏性板测。

## 验证命令

```text
UV4.exe -b <project>.uvprojx -t W25Q64_STM32F411 -o <log>
fromelf.exe --text -a <project>.axf
STM32_Programmer_CLI.exe -c port=JLINK mode=HOTPLUG -el <loader>.stldr ...
```

## 当前状态

- 设计：已获用户批准
- 实现：已完成
- 构建：PASS；Keil Clean Rebuild 为 0 error / 0 warning，已生成 `.stldr`
- 硬件：PASS；STM32CubeProgrammer 已完成 JEDEC/擦除/写入/读回验证，S10 F0 已使用该 Loader 建立并复读 Slot A/B
