# S01 Application Foundation Handoff

## Metadata

- Stage: `S01_Application_Foundation`
- Status: `DRAFT`
- Branch: `main`
- Design Commit: `Not created yet`
- Baseline Commit: `0a3f97493560fbd3ff3a220dc7d0a433e348a95e`
- Implementation Commit: `Not created yet`
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

- Status: `NOT_COMPLETED`

### Completed Work

Not started.

### Changed Files

Not started.

### Deviations From Plan

None.

### Verification Results

- Code Verification: `NOT_APPLICABLE`
- Hardware Verification: `NOT_APPLICABLE`

### Known Issues

- 当前 `project_config.h` 仍含上一项目产品级配置。
- 当前 BSP GPIO 仍含 KEY/LCD 等未匹配当前 `.ioc` 的绑定。
- 当前 Keil OutputDirectory 尚未调整为 `Objects\\` / `Listings\\`。

### Review Focus

- 是否真正去除了上一项目产品语义，而没有为了兼容未启用模块扩大 Config。
- 是否只对 S01 所需 Impl/BSP 做最小适配。
- 是否保持层级依赖方向。
- 是否遵守现有 Keil 构建输出规范。
- 是否具备真实板级 LED + RTT 验证证据。
