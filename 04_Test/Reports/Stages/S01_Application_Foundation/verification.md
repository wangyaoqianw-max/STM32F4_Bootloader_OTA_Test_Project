# S01 Application Foundation 验证报告

## 验证元数据

- Stage：S01_Application_Foundation
- Branch：main
- 验证代码快照：5b21035
- 验证日期：2026-09-11
- Target：OTA_APP
- Keil UV4：5.38.0.0
- ARM Compiler：V5.06 update 7 (build 960)

## 验证范围

本报告覆盖 S01 的 Config 收口、BSP/Impl 适配、最小 Application 入口、Service Log、FreeRTOS 入口、Keil 工程接线和当前环境可执行的代码验证。未扩展到 S02+ 的存储、Ymodem、OTA、Bootloader、Rollback、Security、LCD UI 或正式 RTOS Task/IPC 架构。

## 验证结果

### 1. 静态与依赖边界

- Config、活跃 BSP/Impl、App → Service → Platform → Impl → Vendor 链路：PASS
- 活跃 Keil source groups、include paths 和禁止模块排除检查：PASS
- .ioc 静态核对：PASS
  - LED_1：PC13
  - I2C_SCL：PB6
  - I2C_SDA：PB7
  - TIM2 HAL Time Base、FreeRTOS SysTick 配置存在
- ST7789/LCD 等旧模块仍有源文件级残留引用，但不在 S01 active Build 中；未恢复旧 Display Config 或 LCD BSP API。

### 2. Keil Clean Rebuild

执行：

~~~
UV4.exe -c 03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx -t OTA_APP
UV4.exe -b 03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx -t OTA_APP
~~~

结果：

- Clean：PASS
- Build：PASS，0 Error(s), 6 Warning(s)
- 产物：PASS
  - 03_Firmware/Application/OTA_APP/MDK-ARM/Objects/OTA_APP.axf
  - 03_Firmware/Application/OTA_APP/MDK-ARM/Objects/OTA_APP.hex
  - 03_Firmware/Application/OTA_APP/MDK-ARM/Objects/OTA_APP.map
- 程序尺寸：Code 30980，RO-data 920，RW-data 240，ZI-data 44112
- 已知 Warning：
  - platform_gpio.c 第 33、38、43、48、139 行：5 个无符号值与零比较告警；该文件未在本任务修改。
  - elog_port.c 第 143 行：文件末尾缺少换行告警；Vendor 文件未在本任务修改。

### 3. 构建输出目录偏差

提交中的工程配置保持：

~~~
<OutputDirectory>Objects\</OutputDirectory>
<ListingPath>Listings\</ListingPath>
<GenerateListings>0</GenerateListings>
~~~

但本机 UV4 命令行 Clean/Rebuild 后会自动把 ListingPath 改为空，并在 MDK-ARM 根目录生成 startup_stm32f411xe.lst；MDK-ARM/Listings/ 未产生 listing 文件。已删除该工具生成的根目录 .lst， 并恢复工程文件中的 ListingPath=Listings\。因此：

- Objects 输出：PASS
- Listings 输出：FAIL / 待现场工具确认
- 该偏差是当前 UV4 命令行行为，尚未通过修改源码或放宽规范处理。

### 4. CubeMX、J-Link、RTT 和硬件验证

- CubeMX regenerate：PENDING。当前环境未找到 STM32CubeMX.exe；已完成 .ioc 和 USER CODE 区域静态核对。
- J-Link 下载：PENDING。当前环境未找到可用 J-Link CLI/连接设备。
- FreeRTOS 运行时：PENDING。
- LED Blink 板测：PENDING。尚未连接真实 STM32F411CEU6 硬件。
- RTT + EasyLogger 实时输出：PENDING。尚未连接目标板和 RTT Viewer。

## 阶段判定

- 代码验证：FAIL（C/C++ 编译、静态依赖和 AXF/HEX/MAP 产物均通过；但 Listings 输出验收未通过，且 CubeMX/运行时/板测尚未完成）
- 硬件验证：PENDING
- 当前阶段状态：READY_FOR_VERIFICATION

本报告不将编译结果等同于硬件通过，也不将缺少 CubeMX、J-Link、RTT 和真实板卡的环境描述为已验证。

## 后续验证步骤

1. 使用与项目规范一致的 UV4/Keil GUI Clean/Rebuild，确认 Objects/ 与 Listings/ 实际输出；若仍复现，记录 UV4 版本和完整工程设置。
2. 使用 CubeMX 打开并重新生成 OTA_APP.ioc，确认 USER CODE 区域和 Keil 工程接线未被破坏。
3. 使用 J-Link 将 Objects/OTA_APP.hex 下载到 STM32F411CEU6，复位并观察 PC13 对应 LED_1 是否按 500 ms 亮、500 ms 灭闪烁。
4. 通过 RTT Viewer 检查 Application Foundation start、日志初始化结果和 Application 初始化结果。
5. 连续复位至少三次，记录 FreeRTOS 启动、LED 和 RTT 日志均稳定后，再由 Verification/Review Role 决定是否进入下一状态。
