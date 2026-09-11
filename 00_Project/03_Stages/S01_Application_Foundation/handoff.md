# S01 Application Foundation Handoff

## Metadata

- Stage: `S01_Application_Foundation`
- Status: `IN_PROGRESS`
- Branch: `main`
- Design Commit: `1345db1d5e6cb72a81cd30255bbfaa2d25d54bc2`
- Baseline Commit: `0a3f97493560fbd3ff3a220dc7d0a433e348a95e`
- Implementation Commit: `Task 1: 35af9fb`
- Verification Commit: `Not created yet`

## Implementation Input

### Goal

建立可复用的 STM32F411 Application 基础工程，完成 Config 收口、BSP/Impl 适配、RTT + EasyLogger、FreeRTOS 基础运行、LED Blink 验证和 Keil 构建输出规范化。

### Required Reading

1. `AGENTS.md`
2. `README.md`
3. `PROJECT_CONTEXT.md`
4. `00_Project/WORKFLOW.md`
5. `00_Project/01_Requirements/项目需求V1.md`
6. `00_Project/02_Roadmap/development_roadmap.md`
7. `00_Project/03_Stages/S01_Application_Foundation/design.md`
8. `00_Project/03_Stages/S01_Application_Foundation/implementation_plan.md`
9. `03_Firmware/AGENTS.md`
10. `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`
11. `03_Firmware/00_Doc/Standards/Keil工程与构建输出规范.md`
12. `03_Firmware/Application/OTA_APP/OTA_APP.ioc`
13. `03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx`

### Allowed Changes

- `03_Firmware/Application/OTA_APP/00_Config/`
- `03_Firmware/Application/OTA_APP/01_APP/`
- S01 需要的 `02_Service/03_Platform/04_Impl/05_Vendors` 接线与最小适配
- `Core/Src/freertos.c` USER CODE 区域
- `OTA_APP.ioc` 与 `MDK-ARM/OTA_APP.uvprojx` 的必要工程配置
- `04_Test/Reports/Stages/S01_Application_Foundation/`
- 当前阶段 handoff/status/context 文档

### Prohibited Changes

- 不提前实现 W25Q64/SFUD、AT24C02、Ymodem、OTA Service、LCD UI、Bootloader、Rollback 或 Security。
- 不大规模重构 Platform/Impl 稳定接口。
- 不修改 Vendor 原始库以掩盖工程接线问题。
- 不把 S06 的正式 RTOS Task/IPC 架构提前冻结到 S01。
- 不把普通 Build Artifact 作为长期源码提交。

### Acceptance Criteria

以 `design.md` 的 Acceptance Criteria 为唯一阶段验收基准。

### Required Verification

- CubeMX regenerate
- Keil Clean Rebuild
- Build output path check
- git status check
- J-Link board flash
- FreeRTOS runtime check
- LED Blink board test
- RTT + EasyLogger output check
- dependency boundary review

## Implementation Output

- Status: `IN_PROGRESS`

### Completed Work

Task 1 completed: 已清理上一项目的 Communication、Control、Acquisition、Indicator、MPU6050 和 Display 配置，保留 S01 Status LED、LED Blink 和 Software I2C 基础参数。

### Changed Files

- `PROJECT_CONTEXT.md`
- `00_Project/05_Status/current_status.md`
- `03_Firmware/Application/OTA_APP/00_Config/project_config.h`
- `03_Firmware/Application/OTA_APP/00_Config/project_log_config.h`
- `03_Firmware/Application/OTA_APP/00_Config/README.md`

### Deviations From Plan

None. `PROJECT_DISPLAY_` 仍被未启用的 ST7789 源码引用，按计划在 Task 4 从当前 Build 排除，不恢复旧 Display Config。

### Verification Results

- Task 1 static configuration checks: `PASS`
- Code Verification: `NOT_APPLICABLE`（阶段级验证待 Task 5）
- Hardware Verification: `PENDING`（阶段级板测待 Task 5）

### Known Issues

- Task 1 已完成 Config 清理。
- 当前 BSP GPIO 仍含 KEY/LCD 等未匹配当前 `.ioc` 的绑定。
- 当前 Keil OutputDirectory 尚未调整为 `Objects\\` / `Listings\\`。

### Review Focus

- 是否真正去除了上一项目产品语义，而没有为了兼容未启用模块扩大 Config。
- 是否只对 S01 所需 Impl/BSP 做最小适配。
- 是否保持层级依赖方向。
- 是否遵守现有 Keil 构建输出规范。
- 是否具备真实板级 LED + RTT 验证证据。

## Next Action

继续执行 Task 2：对照 `OTA_APP.ioc` 和 `Core/Inc/main.h`，适配 S01 Status LED / Software I2C 的 BSP 与 Impl 绑定。
