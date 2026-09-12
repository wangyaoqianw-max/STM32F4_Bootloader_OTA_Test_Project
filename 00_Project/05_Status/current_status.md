# Current Project Status

## Context Metadata

- Active Stage: `S02_External_Flash_Driver`
- Status: `READY_FOR_VERIFICATION`
- Branch: `codex/s02-external-flash-driver`
- Baseline Code Commit: `5ac069f19c7f401f56e8fa5aad00c92a76aaaedf`
- Design Commit: `44fddb1484441a98d336df7783170267c166f4af`
- Plan Commit: `aee30916c5c784828269668d99a2b4e63689f80e`
- Implementation Commit: `e2d1ad53e9b24f2cb55fb2a663f34d0095bd276b`
- Current Role: `Verification`
- Updated At: `2026-09-12`

## Current Goal

执行 `S02_External_Flash_Driver`：先补齐 SPI2 所需的 Platform SPI `read()`、SPI1/SPI2 多实例和 APB 时钟判断，再实现并验证 W25Q64 Raw Driver V1。

本阶段目标是得到一个行为明确、可诊断、经过真实硬件 Read Back 验证的 External SPI Flash 基础能力，而不是提前实现 OTA Slot、Firmware Metadata 或 Bootloader 安装逻辑。

## Completed

### Previous Stages

- `S00_Template_Restructure`：`CLOSED`。
- `S01_Application_Foundation`：`CLOSED`。
- S01 已建立 App / Service / Platform / Impl / Vendor / Config 基线、FreeRTOS 基础运行、RTT + EasyLogger、LED Blink、Keil 构建规范和板测基线。

### S02 Design

- W25Q64/SPI2 硬件软件接口已确认。
- SPI1/SPI2 当前运行频率均调整为 12.5 MHz。
- 设计冻结：继续复用现有 SPI Bus / Device / Transaction，只增加同步 `read()`，不增加 `transfer()`。
- 设计冻结：SPI1/SPI2 共用单一 STM32 SPI Impl，通过 Context 绑定 `hspi1/hspi2`。
- 设计冻结：SPI1 使用 PCLK2，SPI2 使用 PCLK1 计算实际 SCK。
- 设计冻结：W25Q64 Driver 位于 `platform_bsp/w25q64`，不直接依赖 HAL。
- Raw Driver V1 命令：`0x9F / 0x05 / 0x06 / 0x03 / 0x02 / 0x20`。
- Read 可跨 Page/Sector；原子 Page Program 不可跨 256 Byte Page。
- 连续 Write 自动拆 Page，但不自动 Erase。
- Sector Erase 要求 4 KiB 对齐，不自动修正地址。
- Program/Erase 每笔重新 Write Enable、校验 WEL、轮询 BUSY。
- S02 专用可破坏测试 Sector：`0x7FF000 ~ 0x7FFFFF`。
- RTT + EasyLogger 为板测主要观测手段；PASS/FAIL 必须来自真实 Read Back/Compare。
- `design.md` 已获 Project Owner 批准。
- `implementation_plan.md`、`handoff.md`、`review.md` 已建立。

### S02 Implementation

- Task 1-5 已按独立本地提交完成：SPI read/SPI2、多实例与 PCLK 校验；W25Q64 初始化、诊断、读取、编程、擦除、跨页写入与边界测试；App Storage SPI Bus 编排；破坏性板测门禁和双启动持久化逻辑。
- `PROJECT_S02_FLASH_BOARD_TEST_ENABLE` 提交值为 `0U`，普通启动不会自动擦除或写入测试 Sector；当前工作区可临时改为 `1U` 进行板测。
- 日志初始化已放入 `freertos.c` 的 CubeMX 用户初始化区；`defaultTask` 只创建 `app_system` 后退出，App 通过 Platform Thread API 使用独立任务栈运行。
- 实现提交范围：`f89a394`、`70f179e`、`42217e2`、`86ecbb3`、`82573b2`、`e2d1ad5`。

## Implementation Sequence

1. Task 1：Platform SPI `read()` + SPI2 multi-instance + PCLK1/PCLK2 修正。
2. Task 2：Flash CS/Storage SPI BSP + W25Q64 init/JEDEC/SR1/Read。
3. Task 3：Write Enable/BUSY + Page Program + Sector Erase。
4. Task 4：跨页 Write + 地址/Page/Sector 边界负向测试。
5. Task 5：RTT 板测入口 + Reset persistence + destructive test 收口。
6. Task 6：Verification evidence 输入 + SFUD 边界评估。

详细执行步骤以 `00_Project/03_Stages/S02_External_Flash_Driver/implementation_plan.md` 为唯一计划基准。

## Verification Expectations

最终至少验证：

- Keil Clean/Rebuild；
- JEDEC ID=`EF 40 17`；
- SR1/BUSY/WEL；
- 4 KiB Sector Erase 后完整 Read Back 全 `0xFF`；
- 单 Page Program + Compare；
- 从 `0x7FF0F0` 连续写 300 Byte 并 Read Back Compare；
- 越界访问、跨页原子 Program、未对齐 Erase 被拒绝；
- Reset 后数据保持；
- RTT/EasyLogger 输出每个 Test Case 的地址、长度、返回码和实际 PASS/FAIL。

## Verification Status

- 代码验证：`PASS`，统一 GCC 静态语法检查、工程 XML/源路径检查和差异检查已完成。
- Keil Build：`PASS`，通过 `05_Tools\Scripts\build_app.bat` 调用 Keil UV4，0 Error、1 Warning（既有 Platform Thread 适配警告，暂不处理）；Clean/Rebuild：`NOT_RUN`。
- 硬件验证：`PENDING`，当前无开发板、J-Link、RTT 或逻辑分析仪证据。
- Verification Input：[S02 verification.md](../../04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md)
- SFUD Evaluation：[sfud_evaluation.md](../03_Stages/S02_External_Flash_Driver/sfud_evaluation.md)

## Known Non-blocking Items

- S01 遗留 `platform_gpio.c` 5 个既有 Warning；后续作为代码质量技术债务处理。
- Vendor `elog_port.c` 1 个文件末尾换行 Warning。
- CK02AT Datasheet/API 延后到安全阶段。
- Ymodem 资料最晚在 S05 前补齐。
- LCD/CTP 细节延后到 S11。
- SFUD 实际集成方式在 Raw Driver 板测通过后评估；若需要改变批准的 SPI 抽象，则返回 Design Role。

## Blockers

实现无已知代码阻塞；阶段关闭仍受 Keil Clean/Rebuild 和真实硬件证据约束。

## Next Action

进入 Verification Role：补做 Keil Clean/Rebuild 和两次启动板测，回填 `04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md` 的真实结果；证据完整后交 Review Role，不得直接标记 S02 `CLOSED`。

## Required Reading for Implementation

1. `AGENTS.md`
2. `README.md`
3. `PROJECT_CONTEXT.md`
4. `00_Project/WORKFLOW.md`
5. `00_Project/01_Requirements/项目需求V1.md`
6. `00_Project/02_Roadmap/development_roadmap.md`
7. `00_Project/03_Stages/S02_External_Flash_Driver/design.md`
8. `00_Project/03_Stages/S02_External_Flash_Driver/implementation_plan.md`
9. `00_Project/03_Stages/S02_External_Flash_Driver/handoff.md`
10. `03_Firmware/AGENTS.md`
11. `03_Firmware/00_Doc/Standards/嵌入式C代码规范.md`
12. `02_Hardware/Hardware_Software_Interface/W25Q64JVSSIQ_外部Flash硬件软件接口参考.md`
13. `03_Firmware/Application/OTA_APP/OTA_APP.ioc`
14. 当前 SPI Platform/Impl 与 BSP GPIO/SPI 代码。

## Prohibited During S02 Implementation

- 不实现 Chip Erase。
- 不引入 Dual/Quad、DMA/Interrupt SPI 或异步 Flash API。
- 不在 Raw Driver 自动 Erase 或自动修正 Sector 地址。
- 不提前实现 Firmware Image/A-B Slot/Metadata/Ymodem/OTA Service/Bootloader。
- 不用 RTT 文本替代真实 Read Back/Compare 验收。
