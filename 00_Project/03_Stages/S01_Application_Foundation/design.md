# S01 Application Foundation Design

## Metadata

- Stage: `S01_Application_Foundation`
- Status: `DESIGN_APPROVED`
- Owner: `Project Owner / Design`
- Date: `2026-09-11`

## Goal

建立一个可复用、可重新生成、可稳定构建和板级验证的 STM32F411 Application 基础工程，完成通用分层骨架、基础 Platform/Impl 适配、RTT + EasyLogger、FreeRTOS 基础运行环境和 LED Blink 验证，为后续 External Flash、EEPROM、Ymodem、OTA Service 和 Bootloader 阶段提供稳定基线。

本阶段不是 OTA 功能阶段。完成后，工程应在删除 OTA、W25Q64、Ymodem 等项目语义后仍然成立，并能够作为个人 STM32F411 Application 基础工程继续复用。

## Context

当前 `03_Firmware/Application/OTA_APP/` 已由 CubeMX/Keil 创建，并复制了上一项目的部分 Config、Platform、Impl、Service 和 Vendor 文件。

当前已确认：

- MCU 为 STM32F411CEU6；
- CubeMX 已启用 FreeRTOS、USART1 + RX/TX DMA、SPI1 和 PB6/PB7 Software I2C GPIO；
- HAL Time Base 使用 TIM2，FreeRTOS 使用 SysTick；
- `05_Vendors` 已存在 SEGGER RTT 与 EasyLogger；
- `project_config.h` 仍包含上一项目的 Communication/Control/Acquisition/Indicator/MPU6050/ST7789 等产品级配置；
- `impl_platform_bsp_gpio.c` 仍包含当前 `.ioc` 不存在的 KEY/LCD GPIO 绑定；
- Keil `OTA_APP.uvprojx` 当前输出目录仍为 `OTA_APP\\`，未符合仓库已冻结的 `Objects\\` / `Listings\\` 构建规范；
- 当前实现基线 Commit：`0a3f97493560fbd3ff3a220dc7d0a433e348a95e`。

## In Scope

1. 保留 `00_Config / 01_APP / 02_Service / 03_Platform / 04_Impl / 05_Vendors` 分层结构，并验证依赖方向。
2. 清理 `project_config.h` 中上一项目的产品级语义，仅保留 S01 实际需要的基础配置。
3. 保留并验证 `project_log_config.h` 的通用日志策略。
4. 适配当前 STM32F411 板级 GPIO 绑定，至少建立 Status LED 与已确认 Software I2C GPIO 的正确资源映射。
5. 验证已迁移的通用 MCU/RTOS Platform/Impl 代码不存在旧工程硬编码依赖；只对实际编译阻塞项做最小修改。
6. 接入 RTT + EasyLogger，并通过 Service Log 提供统一日志出口。
7. 建立最小 Application 入口，使 `main.c` / CubeMX 默认 Task 不直接承载业务逻辑。
8. 使用板载 LED 完成 Firmware V1.0 行为验证：周期闪烁，并输出基础启动日志。
9. 保留 FreeRTOS 基础环境并验证 Kernel 正常启动；不在本阶段冻结最终 Task/IPC 架构。
10. 按 `03_Firmware/00_Doc/Standards/Keil工程与构建输出规范.md` 修正 Keil 输出目录并验证 Clean Rebuild。
11. 清理或停止跟踪明确属于构建/调试生成物的文件；不得误删工程输入。

## Out of Scope

- W25Q64/SFUD 功能实现；
- AT24C02 功能驱动与 NVM 数据模型；
- Firmware Image、A/B Slot、Metadata、Version Contract；
- Ymodem、OTA UART 接收链、OTA Service；
- LCD/CTP UI 和 ST7789 功能接入；
- 最终 RTOS Task 划分、Queue/Notification/Mutex 并发设计；
- Bootloader、Internal Flash 安装、Trial/Confirm/Rollback；
- CRC/SHA/AES/HMAC/Signature/CK02AT/RDP；
- 为未进入当前 Stage 的模块提前修改接口或冻结参数。

## Design

### 1. 基础工程定位

S01 采用“常用能力预启用、具体模块后续再冻结”的基础工程策略。CubeMX 中已经开启的 FreeRTOS、USART1、DMA、SPI1、Software I2C GPIO 可以保留，但这些配置在 S01 只表示基础能力存在，不表示后续 W25Q64、Ymodem 或 LCD 的最终参数已经确认。

### 2. 分层边界

```text
Application
  ↓
Service
  ↓
Platform
  ↓
Impl
  ↓
Vendor / STM32 HAL / FreeRTOS
```

约束：

- App 可以访问 Service 和 Platform，不访问 Impl；
- Service 不直接访问 STM32 HAL；
- Platform 声明稳定能力和数据类型；
- Impl 负责当前 MCU、RTOS、板级物理资源绑定；
- Vendor 原始库原则上不修改；
- `00_Config` 只保存当前产品/工程的静态配置，不承载运行时状态。

### 3. Config 收口

`project_config.h` 从上一项目配置收缩为 S01 所需内容，至少包括：

- Status LED 有效电平；
- LED Blink 周期或 ON/OFF 时间；
- 其他只有在 S01 实际被代码使用时才保留。

不得继续保留未在 S01 使用的 Communication Task、Control Task、Acquisition Task、Indicator Task、MPU6050、ST7789 等参数作为“备用配置”。这些参数在对应 Stage 再引入。

`project_log_config.h` 当前结构可复用，只在编译依赖需要时做最小适配。

### 4. BSP / Impl 适配

`impl_platform_bsp_gpio.c` 只绑定当前 `.ioc` 中已存在且 S01 需要/已确认的资源：

- `LED_1_GPIO_Port / LED_1_Pin` → Status LED；
- `I2C_SCL_GPIO_Port / I2C_SCL_Pin` → Software I2C SCL；
- `I2C_SDA_GPIO_Port / I2C_SDA_Pin` → Software I2C SDA。

上一项目的 `KEY_IN`、`LCD_CS`、`LCD_DC`、`LCD_RST`、`LCD_BL` 不作为 S01 活跃绑定。非本阶段模块可以保留源码作为待后续评估材料，但不得因为被复制进仓库就强制加入当前 Keil Build。

MCU 级 `impl_platform_gpio.*`、`impl_platform_delay.c`、UART/SPI/RTOS 通用实现以“可编译、无旧工程硬编码依赖”为目标，不进行无关重构。

### 5. 日志

日志链固定为：

```text
App / Service
   ↓
service_log
   ↓
platform_log
   ↓
EasyLogger
   ↓
SEGGER RTT
```

底层 Impl 不为了普通初始化日志反向依赖 Service。S01 至少输出：

- Application Foundation 启动；
- Log 初始化结果；
- Application 初始化结果；
- 可选的关键错误信息。

### 6. Application 最小入口

新增最小 `app_main` 模块。CubeMX/FreeRTOS 生成入口只负责完成 Vendor 初始化并调用 App 层入口，不长期承载 LED 业务实现。

S01 的 LED Blink 可以运行在 CubeMX 默认 Task 中，由 App 层调用 Platform GPIO 与 Platform/RTOS delay 能力实现。这个默认 Task 只是基础工程运行载体，不等同于 S06 的最终任务架构。

### 7. RTOS 边界

S01 只验证：

- FreeRTOS Kernel 可以启动；
- HAL Time Base 与 RTOS Tick 不冲突；
- 已迁移的基础 RTOS 抽象能够编译；
- 默认 Task 可以阻塞延时，不使用 Busy Loop。

S06 再设计正式 Task、IPC、ISR/Task 并发边界和后台 OTA 运行模型。

### 8. 构建输出

严格复用仓库现有构建规范：

```text
MDK-ARM/Objects/   -> .o/.d/.crf/.axf/.hex/.bin 等普通 Build Artifact
MDK-ARM/Listings/  -> .map/.lst 等分析输出
06_Output/         -> 仅在需要交付、打包或临时导出时复制制品
```

Keil Target 配置必须改为：

```text
Output Directory  = .\\Objects\\
Listing Directory = .\\Listings\\
```

S01 不新增“每次 Build 自动复制到 06_Output”的规则。

## Interfaces and Data Flow

### Runtime

```text
Reset
 ↓
HAL / Clock / GPIO / DMA / Peripheral Init
 ↓
FreeRTOS Kernel
 ↓
Default Task
 ↓
app_main
 ├─ service_log init → EasyLogger → RTT
 └─ Status LED → Platform GPIO → Impl GPIO → HAL
```

### Build

```text
CubeMX .ioc
 ↓ Generate
Core / Drivers / Middlewares
 ↓
Keil OTA_APP.uvprojx
 ↓ Clean Rebuild
Objects + Listings
 ↓
Board Flash / Debug
```

## Failure Handling

- CubeMX 重新生成后出现工程文件丢失或 Include 路径失效：视为 S01 未通过，不通过手工临时修改生成文件掩盖问题。
- 迁移文件引用旧项目宏、句柄、模块：优先在 Config/BSP/Impl 边界修正；若属于未进入 S01 的功能模块，则从当前 Build 排除，不提前实现。
- EasyLogger/RTT 初始化失败：记录为基础设施故障，LED Blink 仍可作为最小运行诊断；不得把日志故障误判为 MCU 未运行。
- FreeRTOS 未启动或 Task 不运行：分别检查 HAL Time Base、SysTick、NVIC priority、FreeRTOS 配置和 Task 创建，不用 Busy Loop 替代。
- Keil 出现随机 `.o` 写入失败/I/O error：按现有《Keil工程与构建输出规范》排查，不修改业务源码规避。
- 板级 LED 行为与设计不一致：优先核对 `.ioc` 标签、GPIO Port/Pin、有效电平和原理图，不直接反转上层业务语义。

## Verification Strategy

代码验证：

1. 检查 S01 活跃代码不再引用上一项目的 MPU6050/ST7789/Acquisition/Indicator 等配置；
2. 检查 App/Service/Platform 不直接包含 Impl/HAL 的非法依赖；
3. CubeMX 重新生成后检查自研文件和 Keil 工程接线仍存在；
4. Keil 执行 Clean Targets + Rebuild all target files；
5. 目标为 0 Error，Warning 必须逐项确认，不允许新增未解释 Warning；
6. 验证 `.axf/.hex` 等进入 `Objects/`，`.map/.lst` 进入 `Listings/`；
7. `git status --short` 不出现构建生成物。

硬件验证：

1. J-Link 下载 OTA_APP；
2. Reset 后 FreeRTOS 正常运行；
3. 板载 LED 按 S01 配置稳定周期闪烁；
4. RTT 能看到 EasyLogger 启动与 Application 基础日志；
5. 连续 Reset 多次，行为稳定一致。

完整验证证据写入 `04_Test/Reports/Stages/S01_Application_Foundation/`。

## Acceptance Criteria

- [ ] `OTA_APP.ioc` 可重新生成代码，且不破坏自研分层代码。
- [ ] Keil 可以 Clean Rebuild，构建结果满足阶段 Warning/Error 要求。
- [ ] `MDK-ARM/Objects/` 与 `MDK-ARM/Listings/` 输出符合仓库规范。
- [ ] 构建生成物不进入 Git status。
- [ ] `project_config.h` 不再携带上一项目的产品级配置残留。
- [ ] Status LED BSP 与当前 `LED_1` CubeMX 资源一致。
- [ ] RTT + EasyLogger 正常输出基础启动日志。
- [ ] FreeRTOS Kernel 与默认 Task 正常运行，无 Busy Loop。
- [ ] LED Blink 板测通过，并能连续 Reset 稳定复现。
- [ ] App / Service / Platform / Impl / Vendor 依赖边界检查通过。
- [ ] S01 不实现 W25Q64、AT24C02、Ymodem、OTA、LCD UI、Bootloader 或 Security 功能。
- [ ] 验证报告已写入 `04_Test/Reports/Stages/S01_Application_Foundation/`。

## Approval

- Decision: `APPROVED`
- Approved By: `Project Owner`
- Design Commit: `1345db1d5e6cb72a81cd30255bbfaa2d25d54bc2`
