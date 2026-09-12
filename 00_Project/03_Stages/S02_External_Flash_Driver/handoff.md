# S02 External Flash Driver Handoff

## Metadata

- Stage: `S02_External_Flash_Driver`
- Status: `READY_FOR_VERIFICATION`
- Branch: `codex/s02-external-flash-driver`
- Design Commit: `44fddb1484441a98d336df7783170267c166f4af`
- Plan Commit: `aee30916c5c784828269668d99a2b4e63689f80e`
- Baseline Code Commit: `5ac069f19c7f401f56e8fa5aad00c92a76aaaedf`
- Implementation Commit: `e2d1ad53e9b24f2cb55fb2a663f34d0095bd276b`
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`

## Implementation Input

### Goal

在现有 STM32F411 Application 基础上建立可靠的 W25Q64 Raw Driver V1：补齐 SPI2 同步读取能力，完成 JEDEC ID、SR1、Read、Page Program、跨页 Write、4 KiB Sector Erase、范围/对齐校验，并使用 RTT + EasyLogger 和真实 Read Back/Compare 完成板级验证。

### Required Reading

1. `AGENTS.md`
2. `README.md`
3. `PROJECT_CONTEXT.md`
4. `00_Project/WORKFLOW.md`
5. `00_Project/01_Requirements/项目需求V1.md`
6. `00_Project/02_Roadmap/development_roadmap.md`
7. `00_Project/03_Stages/S02_External_Flash_Driver/design.md`
8. `00_Project/03_Stages/S02_External_Flash_Driver/implementation_plan.md`
9. `00_Project/03_Stages/S01_Application_Foundation/handoff.md`
10. `00_Project/03_Stages/S01_Application_Foundation/review.md`
11. `03_Firmware/AGENTS.md`
12. `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`
13. `02_Hardware/Hardware_Software_Interface/W25Q64JVSSIQ_外部Flash硬件软件接口参考.md`
14. `03_Firmware/Application/OTA_APP/OTA_APP.ioc`
15. 当前 SPI Platform/Impl、BSP GPIO/SPI、App Main 代码。

### Frozen Hardware Facts

- W25Q64JVSSIQ：8 MiB，地址 `0x000000 ~ 0x7FFFFF`。
- Page：256 Byte；Sector：4096 Byte。
- SPI2：PB13 SCK、PB14 MISO、PB15 MOSI。
- `FLASH_CS`：PB12，软件 CS，Low Active。
- SPI2：Mode 0、MSB First、8-bit、Full Duplex、Software NSS。
- APB1=50 MHz，SPI2 Prescaler=/4，实际 SCK=12.5 MHz。
- SPI1：APB2=100 MHz，Prescaler=/8，实际 SCK=12.5 MHz。
- W25Q64 V1 使用 Standard SPI，不使用 Dual/Quad。
- S02 可破坏测试 Sector：`0x7FF000 ~ 0x7FFFFF`。

### Allowed Changes

- `03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/spi/`
- `03_Firmware/Application/OTA_APP/04_Impl/impl_mcu/impl_platform_spi.*`
- `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/platform_bsp_spi.h`
- `03_Firmware/Application/OTA_APP/04_Impl/impl_bsp/impl_platform_bsp_spi.c`
- `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/platform_bsp_gpio.h`
- `03_Firmware/Application/OTA_APP/04_Impl/impl_bsp/impl_platform_bsp_gpio.c`
- 新增 `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/`
- S02 需要的 `project_config.h`、`app_main.c` 和 Keil 工程接线。
- `04_Test/Board/S02_External_Flash_Driver/`
- `04_Test/Reports/Stages/S02_External_Flash_Driver/`
- S02 阶段文档、status/context 和 SFUD 评估文档。

### Prohibited Changes

- 不修改 Vendor 原始库。
- 不引入 Chip Erase 公共 API。
- 不引入 Fast Read、Dual/Quad、DMA SPI、Interrupt SPI 或异步 Flash API。
- 不新增 `transfer()`，除非实际实现证明 approved design 无法完成并返回 Design Role。
- 不在 Raw Driver 内自动 Erase、自动向下对齐 Sector、自动分区或判断 OTA 状态。
- 不提前实现 Firmware Image、A/B Slot、Metadata、Ymodem、OTA Service、Bootloader 或 Security。
- 不把 HAL Handle 暴露到 Platform/BSP Driver。
- 不用固定 Delay 替代 BUSY 完成判断。
- 不把 RTT 打印的“OK”本身当作硬件验证证据。

### Required Public Interfaces

```c
platform_error_t platform_spi_read(
    platform_spi_device_t *device,
    uint8_t *data,
    platform_size_t dataLength);

platform_error_t impl_platform_spi2_construct(
    platform_spi_bus_t *bus,
    const char *name,
    uint32_t caps);

platform_error_t platform_bsp_spi_construct_storage_bus(
    platform_spi_bus_t *bus);

platform_error_t platform_bsp_w25q64_construct_flash(
    platform_w25q64_t *flash);

platform_error_t platform_w25q64_init(
    platform_w25q64_t *flash,
    platform_spi_bus_t *spiBus);

platform_error_t platform_w25q64_deinit(platform_w25q64_t *flash);

platform_error_t platform_w25q64_read_jedec_id(
    platform_w25q64_t *flash,
    platform_w25q64_jedec_id_t *jedecId);

platform_error_t platform_w25q64_read_status1(
    platform_w25q64_t *flash,
    uint8_t *status);

platform_error_t platform_w25q64_read(
    platform_w25q64_t *flash,
    uint32_t address,
    uint8_t *data,
    platform_size_t dataLength);

platform_error_t platform_w25q64_page_program(
    platform_w25q64_t *flash,
    uint32_t address,
    const uint8_t *data,
    platform_size_t dataLength);

platform_error_t platform_w25q64_write(
    platform_w25q64_t *flash,
    uint32_t address,
    const uint8_t *data,
    platform_size_t dataLength);

platform_error_t platform_w25q64_sector_erase(
    platform_w25q64_t *flash,
    uint32_t sectorAddress);
```

### Acceptance Criteria

以 `design.md` 的 Acceptance Criteria 为准，至少包括：

- SPI read、SPI2 multi-instance、PCLK1/PCLK2 时钟判断正确；
- Clean Rebuild 无 S02 新错误；
- JEDEC ID=`EF 40 17`；
- SR1/BUSY/WEL 可解释；
- Read、Sector Erase、Page Program、跨页 Write 均通过真实 Read Back；
- 越界、跨页原子 Program、未对齐 Erase 被拒绝；
- Reset 后数据保持；
- RTT/EasyLogger 可诊断过程和失败，但不替代实际校验；
- Raw Driver 结果可作为 SFUD 适配基线。

## Implementation Output

- Status: `READY_FOR_VERIFICATION`
- Completed Work: `Task 1-5` completed as five feature commits, followed by one verification-time startup correction. Platform SPI now supports blocking read and SPI2 multi-instance construction; W25Q64 Raw Driver covers JEDEC/SR1/Read/WEL/BUSY/Page Program/cross-page Write/4 KiB Sector Erase; App owns Storage SPI Bus lifecycle; destructive board test is explicitly gated and defaults to `0U`.
- Changed Files: Platform SPI/Impl/BSP, W25Q64 Raw Driver/BSP, App/config, Keil project wiring, and `04_Test/Board/S02_External_Flash_Driver/`.
- Deviations From Plan: App invocation/gating was intentionally kept in Task 5 to avoid Task 2 creating an un-gated destructive startup path. No approved design interface was expanded; no SFUD source was added.
- Known Issues: Automated Keil Build now passes through `05_Tools\Scripts\build_app.bat` with 0 Error and 1 existing warning; Clean/Rebuild, development board, J-Link, RTT terminal and logic analyzer evidence remain pending.
- Verification Evidence: [S02 verification input](../../../04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md)
- SFUD Evaluation: [SFUD boundary evaluation](sfud_evaluation.md); actual middleware integration deferred.

### Verification-time Startup Correction

- 日志服务初始化已放入 `freertos.c` 的 CubeMX `USER CODE BEGIN Init` 区域。
- `defaultTask` 仅负责创建 `app_system` 后退出，`app_system` 通过 Platform Thread API 运行 `app_main`。
- `app_system.c` 不直接依赖 CMSIS-RTOS；对应 FreeRTOS Thread Impl 已加入 Keil 工程。
- 自动 Keil Build 结果为 `0 Error、1 Warning`；warning 暂按用户要求保留。

施工完成后由 Implementation Role 更新本节，不修改已冻结的设计结论来迁就实现。

## Next Action

由 Verification Role 继续执行 Keil Clean/Rebuild 和真实开发板验证，重点记录 JEDEC/SR1、Erase 全 `0xFF`、Program/跨页 Write Read Back、边界拒绝及双启动持久化。当前不得直接关闭 S02；硬件证据完整后再交 Review Role。
