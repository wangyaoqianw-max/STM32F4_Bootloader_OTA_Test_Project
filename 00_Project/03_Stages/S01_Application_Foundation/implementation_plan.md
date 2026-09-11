# S01 Application Foundation Implementation Plan

## Metadata

- Stage: `S01_Application_Foundation`
- Design: `00_Project/03_Stages/S01_Application_Foundation/design.md`
- Baseline Commit: `0a3f97493560fbd3ff3a220dc7d0a433e348a95e`
- Status: `NOT_STARTED`

## Global Constraints

- 当前阶段仅允许实现 S01 冻结范围，不提前实现 W25Q64、AT24C02、Ymodem、OTA Service、LCD UI、Bootloader 或 Security。
- 修改 `03_Firmware` 前必须读取根 `AGENTS.md`、`03_Firmware/AGENTS.md`、嵌入式 C 代码规范和 Keil 构建输出规范。
- App 不访问 Impl；Service 不直接访问 HAL；Vendor 原始代码原则上不修改。
- 当前 CubeMX 中预启用的 FreeRTOS/UART/DMA/SPI/Software I2C 是基础能力，不等同于后续模块最终配置。
- 不删除来源和职责尚未确认的非空迁移文件；未进入 S01 的模块优先从当前 Build 排除并留待对应 Stage 处理。
- Keil 普通构建输出只进入 `MDK-ARM/Objects/` 和 `MDK-ARM/Listings/`；不得新增每次 Build 自动复制到 `06_Output/` 的规则。
- 每个 Task 完成后进行对应验证并独立提交；实际 Commit 写入 `handoff.md`。
- Agent 不能替代真实板测；硬件验证由 Project Owner 或具备硬件访问能力的执行者完成并记录。

## Tasks

### Task 1: 清理 S01 Config 与旧项目产品语义

**Goal:** 让 `00_Config` 只保留当前基础工程真正使用的静态配置，消除上一项目的任务、传感器和显示参数残留。

**Files:**

- Modify: `03_Firmware/Application/OTA_APP/00_Config/project_config.h`
- Verify: `03_Firmware/Application/OTA_APP/00_Config/project_log_config.h`
- Verify: `03_Firmware/Application/OTA_APP/00_Config/README.md`

- [ ] Step 1: 读取 `project_config.h` 的所有宏，按 S01 `design.md` 将 Communication/Control/Acquisition/Indicator/MPU6050/ST7789 等未进入 S01 的配置移除，保留 Status LED 有效电平与 LED Blink 所需时间参数；如果某迁移文件仍依赖被删除宏，先判断该文件是否属于 S01 活跃 Build，禁止为了兼容未启用模块把旧配置重新塞回 Config。
- [ ] Step 2: 检查 `project_log_config.h`，保持默认日志等级与输出开关的通用策略；除非编译依赖要求，不新增模块专用日志参数。
- [ ] Step 3: 全仓搜索 `PROJECT_COMM_`、`PROJECT_CONTROL_`、`PROJECT_ACQUISITION_`、`PROJECT_INDICATOR_`、`PROJECT_MPU6050_`、`PROJECT_DISPLAY_`，列出仍引用这些宏的文件，并将非 S01 模块标记为后续 Build 排除候选。
- [ ] Step 4: 对 Config 相关源文件执行编译/预处理检查，确认没有因为删除旧配置导致 S01 活跃代码缺少定义。
- [ ] Step 5: 提交本任务，并将 Commit 写入 `handoff.md`。

### Task 2: 适配 BSP / Impl 到当前 OTA_APP.ioc

**Goal:** 让板级物理资源绑定与当前 CubeMX 标签一致，并消除 KEY/LCD 等当前不存在资源造成的编译污染。

**Files:**

- Modify: `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/platform_bsp_gpio.h`
- Modify: `03_Firmware/Application/OTA_APP/04_Impl/impl_bsp/impl_platform_bsp_gpio.c`
- Verify: `03_Firmware/Application/OTA_APP/04_Impl/impl_board/board_types.h`
- Verify: `03_Firmware/Application/OTA_APP/04_Impl/impl_mcu/impl_platform_gpio.c`
- Verify: `03_Firmware/Application/OTA_APP/04_Impl/impl_mcu/impl_platform_gpio.h`
- Verify: `03_Firmware/Application/OTA_APP/04_Impl/impl_mcu/impl_platform_delay.c`
- Verify: `03_Firmware/Application/OTA_APP/OTA_APP.ioc`

- [ ] Step 1: 对照 `OTA_APP.ioc` 和 `Core/Inc/main.h`，确认当前实际标签为 `LED_1`、`I2C_SCL`、`I2C_SDA`，记录对应 GPIO Port/Pin；不得使用上一项目 `LED_OUT/KEY_IN/LCD_*` 宏作为当前硬件事实。
- [ ] Step 2: 将 Status LED BSP 绑定改为 `LED_1_GPIO_Port / LED_1_Pin`；保留 PB6/PB7 Software I2C 的已确认绑定；从 S01 活跃 BSP 接口和实现中移除 KEY/LCD 构造入口，除非当前 `.ioc` 已存在对应资源。
- [ ] Step 3: 检查 `impl_platform_gpio.*`、`impl_platform_delay.c`、`board_types.h` 是否引用旧工程专用头文件、句柄或硬件宏；只修正会阻断当前 STM32F411 基础工程编译的依赖，不做接口重构。
- [ ] Step 4: 编译 GPIO/Delay/BSP 相关文件，确认无 undefined macro、HAL handle 或非法层级 include；检查 App/Service/Platform 不直接 include `impl_*` 或 HAL 头文件。
- [ ] Step 5: 提交本任务，并将 Commit 写入 `handoff.md`。

### Task 3: 建立日志链和最小 Application 入口

**Goal:** 使用已有 RTT + EasyLogger 和 Service Log 建立统一启动日志，并把 LED Blink 放入 App 层而不是 CubeMX 生成文件的业务区。

**Files:**

- Create: `03_Firmware/Application/OTA_APP/01_APP/app_main.h`
- Create: `03_Firmware/Application/OTA_APP/01_APP/app_main.c`
- Verify/Modify if required: `03_Firmware/Application/OTA_APP/02_Service/service_log/service_log.c`
- Verify/Modify if required: `03_Firmware/Application/OTA_APP/02_Service/service_log/service_log.h`
- Verify: `03_Firmware/Application/OTA_APP/03_Platform/platform_middleware/platform_log.h`
- Verify/Modify if required: `03_Firmware/Application/OTA_APP/04_Impl/impl_middleware/*`
- Verify: `03_Firmware/Application/OTA_APP/05_Vendors/RTT/*`
- Verify: `03_Firmware/Application/OTA_APP/05_Vendors/easylogger/*`
- Modify: `03_Firmware/Application/OTA_APP/Core/Src/freertos.c`

- [ ] Step 1: 读取当前 `service_log`、`platform_log`、Impl Middleware、EasyLogger 和 RTT 适配代码，确认日志调用链为 `App/Service -> service_log -> platform_log -> EasyLogger -> RTT`；底层 Impl 不反向依赖 Service。
- [ ] Step 2: 创建 `app_main.h/.c`，提供单一 Application 入口；入口负责完成 S01 需要的日志初始化、Status LED 初始化和周期 Blink。使用 Platform GPIO 与 Platform/RTOS delay 能力，不直接调用 HAL。
- [ ] Step 3: 在 `freertos.c` 的 USER CODE 区域调用 App 入口，使 CubeMX 默认 Task 只承担启动载体职责；不得把具体 LED 翻转逻辑散落在生成文件中。
- [ ] Step 4: 编译并检查 RTT/EasyLogger 相关文件；确认至少可输出 Application Foundation 启动、Log 初始化和 App 初始化结果，不新增未解释 Warning。
- [ ] Step 5: 提交本任务，并将 Commit 写入 `handoff.md`。

### Task 4: 收口 Keil/CubeMX 工程接线与构建输出

**Goal:** 让 S01 活跃源文件、Include Path、FreeRTOS/Vendor 依赖和构建输出完全符合仓库规范，并确保未进入 S01 的旧模块不阻断 Build。

**Files:**

- Modify: `03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx`
- Verify/Modify if required: `03_Firmware/Application/OTA_APP/OTA_APP.ioc`
- Verify: `03_Firmware/00_Doc/Standards/Keil工程与构建输出规范.md`
- Verify: `.gitignore`
- Remove from tracking only if confirmed generated: `03_Firmware/Application/OTA_APP/MDK-ARM/JLinkLog.txt`

- [ ] Step 1: 按 S01 活跃模块重新核对 Keil Groups、Source Files 与 Include Paths；加入 `app_main`、Service Log、需要的 Platform/Impl、RTT/EasyLogger、FreeRTOS 适配；对 LCD、未使用 Service UART 等后续阶段模块不强制加入当前 Build。
- [ ] Step 2: 将 Target `OutputDirectory` 设置为 `Objects\`，`ListingPath` 设置为 `Listings\`；保持工程输入 `.uvprojx/.ioc/startup` 在版本管理范围内。
- [ ] Step 3: 核对 FreeRTOS 使用 SysTick、HAL Time Base 使用 TIM2 的现状；重新生成 CubeMX 后确认生成文件和 Keil 接线未丢失。除非实际冲突，不调整 S06 才需要冻结的 Task/IPC 架构。
- [ ] Step 4: 核对 `JLinkLog.txt` 是否仅为运行时调试日志；若确认可再生，则从 Git Index 移除并由忽略规则覆盖。不得误删 `JLinkSettings.ini`、DebugConfig 或其他尚未确认归属的工程输入。
- [ ] Step 5: 提交本任务，并将 Commit 写入 `handoff.md`。

### Task 5: Clean Rebuild、板测与阶段证据

**Goal:** 用可重复构建和真实板级行为证明 S01 基础工程可作为后续 Stage 基线。

**Files:**

- Create: `04_Test/Reports/Stages/S01_Application_Foundation/verification.md`
- Modify: `00_Project/03_Stages/S01_Application_Foundation/handoff.md`
- Modify: `00_Project/05_Status/current_status.md`
- Modify: `PROJECT_CONTEXT.md`

- [ ] Step 1: 在关闭可能占用输出文件的工具后执行 Keil `Clean Targets`，再执行 `Rebuild all target files`；记录 Keil/Compiler 版本、Target、Error/Warning 数量和生成文件。
- [ ] Step 2: 检查 `MDK-ARM/Objects/` 中存在 `.axf/.hex` 等普通构建产物，`MDK-ARM/Listings/` 中存在 `.map` 或相应 Listing；运行 `git status --short`，确认构建生成物未进入版本控制候选。
- [ ] Step 3: 使用 J-Link 下载 OTA_APP；Reset 后观察板载 LED 周期闪烁，确认 FreeRTOS 默认 Task 正常阻塞运行；连续 Reset 至少 3 次并记录结果。
- [ ] Step 4: 使用 RTT Viewer/Keil RTT 能力读取 EasyLogger 输出，记录启动日志、Application 初始化结果以及任何 Warning/Error；将代码验证与硬件验证分别标记为 PASS/FAIL/PENDING。
- [ ] Step 5: 将完整证据写入 `verification.md`；所有计划项完成后把 `handoff.md` Implementation Output 补齐，并将 `current_status.md` 更新为 `READY_FOR_VERIFICATION` 或根据实际流程进入下一合法状态后提交。

## Final Verification

必须同时检查：

```text
[ ] CubeMX 可重新生成
[ ] Keil Clean Rebuild 可重复
[ ] 0 Error；Warning 无新增未解释项
[ ] Objects/Listings 输出符合规范
[ ] git status 无构建生成物
[ ] FreeRTOS Kernel 正常启动
[ ] RTT + EasyLogger 正常输出
[ ] LED Blink 板测通过
[ ] 连续 Reset 行为稳定
[ ] App/Service/Platform/Impl/Vendor 依赖边界通过
[ ] Config 无上一项目产品级残留
[ ] 未提前实现 S02+ 功能
[ ] 验证报告落盘
```

## Completion Condition

所有任务完成、验证证据落盘并将阶段状态更新为 `READY_FOR_REVIEW`。任何真实硬件验收缺失时不得关闭 S01。
