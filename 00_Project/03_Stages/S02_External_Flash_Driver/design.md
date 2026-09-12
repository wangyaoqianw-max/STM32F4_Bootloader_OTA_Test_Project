# S02 External Flash Driver Design

## Metadata

- Stage: `S02_External_Flash_Driver`
- Status: `DESIGN_APPROVED`
- Owner: `Project Owner / Design`
- Date: `2026-09-12`
- Approved At: `2026-09-12`

## Goal

建立 STM32F411 Application 对板载 W25Q64JV External SPI Flash 的可靠原始访问能力，并完成从 SPI Platform/Impl 基础能力、板级绑定、W25Q64 Raw Driver 到真实硬件验证的完整闭环，为后续 Firmware Image Storage、Ymodem 接收和 OTA Service 提供稳定的大容量非易失存储基础。

本阶段优先完成并验证 W25Q64 Raw Driver，再在 Raw Driver 行为和板级链路稳定后评估 SFUD 接入。Raw Driver 用于掌握 SPI NOR Flash 的协议、页编程、扇区擦除、状态轮询、地址边界和异常行为，并作为 SFUD 适配时的诊断基线。

本阶段不定义 OTA Slot、Firmware Metadata、A/B 分区和 Firmware Image 格式，这些属于后续 `S04_Firmware_Image_Storage`。

## Context

当前 Application 已完成 `S01_Application_Foundation`，保持 `App / Service / Platform / Impl / Vendor / Config` 分层。

已确认硬件与工程事实：

- MCU：STM32F411CEU6；
- External Flash：Winbond W25Q64JVSSIQ，64 Mbit / 8 MiB；
- 地址范围：`0x000000 ~ 0x7FFFFF`，24-bit 地址；
- Page Size：256 Byte；
- Sector Size：4 KiB；
- Erase 后数据：`0xFF`；
- W25Q64 使用 Standard SPI，不使用 Dual / Quad SPI；
- SPI2：PB13=SCK、PB14=MISO、PB15=MOSI；
- PB12=`FLASH_CS`，GPIO 软件片选，低电平有效，启动默认高电平；
- IO2/IO3 不作为 Quad IO 使用；
- SPI2：Master、2-line Full Duplex、8-bit、MSB First、Mode 0、Software NSS；
- APB1=50 MHz，SPI2 Prescaler=/4，实际 SPI2 Clock=12.5 MHz；
- SPI1 当前 Prescaler=/8，实际 SPI1 Clock=12.5 MHz；
- V1 使用 `0x03 Read Data`，当前 12.5 MHz 不需要 Fast Read；
- 当前 Platform SPI 已有 Bus / Device / Transaction，但仓库基线只有 `write()`；
- 当前 STM32 SPI Impl 只构造 SPI1，实际 SPI Clock 计算固定使用 PCLK2，S02 必须修正。

## In Scope

1. 扩展 Platform SPI，同步阻塞事务增加 `read()`。
2. STM32 SPI Impl 支持 SPI1/SPI2 两个实例，共用一套 Ops/Lifecycle。
3. 修正 SPI1→PCLK2、SPI2→PCLK1 的实际时钟计算。
4. 保持现有 Bus / Device / Transaction 模型，不新建 SPI 框架。
5. 在 `03_Platform/platform_bsp/w25q64/` 增加 W25Q64 Raw Driver 与板级构造。
6. 支持 JEDEC ID、SR1、Read Data、Page Program、连续 Write、4 KiB Sector Erase。
7. 支持 BUSY 轮询、Write Enable、WEL 校验和超时。
8. 支持 Flash 范围、Page 边界和 Sector 对齐校验。
9. 使用专用测试 Sector 完成真实硬件 Read / Program / Erase / Read Back 验证。
10. RTT + EasyLogger 作为板测主要观测和证据展示手段。
11. Raw Driver 板测通过后评估 SFUD 接入和职责边界。

## Out of Scope

- OTA Firmware Slot A/B、Firmware Header、Version、CRC、Metadata、Upgrade State；
- Ymodem / OTA UART Transport；
- Bootloader 侧 External Flash 复用；
- Dual / Quad SPI、DMA SPI、Interrupt SPI、异步 API；
- Fast Read、32 KiB / 64 KiB Block Erase；
- Chip Erase 公共 API；
- Security Register、Deep Power-down；
- Wear Leveling、文件系统、KV Storage；
- Raw Driver 自动 Erase、自动分区或 OTA 业务状态判断；
- 运行时动态重配 SPI。S02 继续采用 CubeMX 固定配置 + Platform 校验。

## Design

### 1. 分层与目录

```text
03_Platform/
└─ platform_bsp/
   └─ w25q64/
      ├─ platform_w25q64.h
      ├─ platform_w25q64.c
      ├─ platform_bsp_w25q64.h
      └─ platform_bsp_w25q64.c
```

依赖：

```text
Application / future Storage Service
              ↓
      W25Q64 Raw Driver
              ↓
     platform_spi_device_t
              ↓
        Platform SPI
              ↓
         Impl SPI2
              ↓
        STM32 HAL SPI
```

约束：`platform_w25q64.*` 不访问 `hspi2`、`SPI2`、PB12 或 `HAL_SPI_xxx()`；板级引脚和静态参数由 `platform_bsp_w25q64.*` 提供；Storage/OTA 上层不直接操作 WEL/BUSY/Opcode。

### 2. SPI Platform 最小扩展

`platform_spi_bus_ops_t` 增加：

```c
platform_error_t (*read)(
    platform_spi_bus_t *bus,
    uint8_t *data,
    platform_size_t dataLength);
```

Facade：

```c
platform_error_t platform_spi_read(
    platform_spi_device_t *device,
    uint8_t *data,
    platform_size_t dataLength);
```

行为与 `platform_spi_write()` 对称：NULL、长度 0、Device 未初始化、Bus 未 STARTED、不是 activeDevice 都必须返回可诊断错误。Bus 构造时要求 `ops->read != NULL`。

S02 不增加 `transfer()`；W25Q64 的 command/header 写入后读取数据由同一 transaction 内的 `write()` + `read()` 表达。

### 3. SPI Impl 多实例

继续使用单一 `impl_platform_spi.c`：

```text
g_spi1Context → &hspi1
g_spi2Context → &hspi2
```

公开：

```c
impl_platform_spi1_construct(...)
impl_platform_spi2_construct(...)
```

SPI1/SPI2 共用 Lifecycle/Ops，不复制实例文件。新增 `stm32_spi_read()`，第一版使用阻塞 HAL SPI。

时钟计算：

```text
SPI1 → HAL_RCC_GetPCLK2Freq() → 100 MHz / 8 = 12.5 MHz
SPI2 → HAL_RCC_GetPCLK1Freq() →  50 MHz / 4 = 12.5 MHz
```

### 4. W25Q64 数据模型

```c
typedef struct
{
    platform_spi_device_t spiDevice;
    platform_gpio_t cs;
    platform_spi_device_config_t spiConfig;
    platform_gpio_level_t csActiveLevel;
    uint8_t manufacturerId;
    uint8_t memoryType;
    uint8_t capacityId;
    platform_bool_t initialized;
} platform_w25q64_t;
```

固定几何信息使用常量：Total=8 MiB、Address=`0x000000~0x7FFFFF`、Page=256 Byte、Sector=4096 Byte。

### 5. BSP 静态构造

`platform_bsp_w25q64_construct_flash()` 填充：Mode 0、MSB First、8-bit、CS Low Active、FLASH_CS/PB12。

`spiConfig.maxClockHz` 表达 Device/当前命令允许的最大时钟，不表达当前 Prescaler 结果。V1 `0x03 Read Data` 可按 50 MHz 约束；实际运行仍是 12.5 MHz。

### 6. 初始化与反初始化

```text
validate flash / spiBus
→ SPI Bus 必须 STARTED
→ configure CS output/inactive
→ platform_spi_device_init
→ wait_ready(init timeout)
→ Read JEDEC ID
→ 校验 EF 40 17
→ 保存 ID
→ initialized = TRUE
```

任一步失败都回滚已建立资源，保持 `initialized = FALSE`。`deinit()` 只释放 W25Q64 自己的 SPI Device 和 CS GPIO，不停止共享 SPI Bus。

### 7. Transaction 规则

只要 `platform_spi_transaction_begin()` 成功，后续无论成功或失败都必须执行 `platform_spi_transaction_end()`。

错误优先级：操作失败时保留第一个操作错误；操作成功但 end 失败时返回 end 错误。

允许建立最小私有 helper：

```text
finish_transaction()
command_read()
```

不建立通用 Command Engine。

### 8. Raw Driver V1 命令

| Command | Opcode | Purpose |
| --- | ---: | --- |
| Read JEDEC ID | `0x9F` | 识别 EF 40 17 |
| Read Status Register-1 | `0x05` | BUSY/WEL |
| Write Enable | `0x06` | Program/Erase 前置 |
| Read Data | `0x03` | 连续读取 |
| Page Program | `0x02` | 单页编程 |
| Sector Erase | `0x20` | 4 KiB 擦除 |

V1 不暴露 Chip Erase `0x60 / 0xC7`。

### 9. BUSY / WEL / Timeout

- `write_enable()`、`wait_ready()` 为私有 API；
- `0x06` 后读取 SR1，必须确认 `WEL==1`；
- Program/Erase 通过 `SR1.BUSY` 轮询完成，不用固定 Delay 代替；
- 轮询间隔建议 1 ms；
- Page Program timeout：10 ms；Sector Erase timeout：500 ms；Init Ready timeout：1000 ms；
- 每一笔修改操作都重新 Write Enable。

### 10. Read Data 与范围

公开：

```c
platform_w25q64_read(flash, address, data, dataLength)
```

Read 可跨 Page/Sector，只受整个 8 MiB 范围限制，不隐式执行 `wait_ready()`。

统一 24-bit 地址编码，MSB First。范围校验使用：

```text
length > 0
address < TOTAL_SIZE
length <= TOTAL_SIZE - address
```

避免 `address + length` 整数溢出。

### 11. Page Program 与连续 Write

原子 `platform_w25q64_page_program()` 必须满足：

```text
0 < length <= 256
(address % 256) + length <= 256
```

禁止跨 Page，防止页内 wrap-around。

流程：`wait_ready → write_enable → WEL check → 0x02+address+data → end → wait_ready`。

`platform_w25q64_write()` 只做 Page 拆分：

```text
pageOffset    = address % 256
pageRemaining = 256 - pageOffset
chunkLength   = min(remaining, pageRemaining)
```

例如 `0xF0` 写 300 Byte：`16 + 256 + 28`。`write()` 不自动 Erase，也不预读验证是否为 `0xFF`。

### 12. Sector Erase

`platform_w25q64_sector_erase(flash, sectorAddress)` 只支持 `0x20` 4 KiB Sector Erase。

要求：

```text
sectorAddress % 4096 == 0
```

不自动向下对齐。流程：`validate → wait_ready → write_enable → WEL check → 0x20+address → end → wait_ready`。

正式 Driver 不自动 Read Back 4 KiB；Read Back 属于测试/更高层策略。

### 13. Public API Boundary

公开：

```text
Lifecycle
├─ platform_w25q64_init()
└─ platform_w25q64_deinit()

Diagnostics
├─ platform_w25q64_read_jedec_id()
└─ platform_w25q64_read_status1()

Data
├─ platform_w25q64_read()
├─ platform_w25q64_page_program()
├─ platform_w25q64_write()
└─ platform_w25q64_sector_erase()
```

私有：`write_enable()`、`wait_ready()`、`validate_range()`、`encode_address()`、`command_read()`、`finish_transaction()`、`rollback_init()`。

### 14. Error Semantics

继续使用 `platform_error_t`。必须可诊断 NULL、未初始化、Bus 状态错误、地址越界、Page 跨页、Sector 未对齐、HAL Busy/Timeout/I/O Error、JEDEC ID 不匹配和 BUSY timeout。

SPI Data Operation 失败后仍需尽力结束 transaction，避免 CS 长期 Low 或 `activeDevice` 永久占用。

### 15. Logging / Diagnostics

Raw Driver 不依赖 Service Log，避免 Platform/BSP 反向依赖 Service。

板测/Application 测试代码使用 RTT + EasyLogger 输出：初始化、JEDEC ID、SR1、测试 Sector、Erase/Program 地址和长度、返回码、Read Back Compare、边界测试、Timeout 和 Reset 后 persistence 结果。

RTT 日志用于观测和证据展示，不能替代数据校验：Erase PASS 必须建立在完整 4 KiB Read Back 全 `0xFF`；Program PASS 必须建立在 Read Back 与源 Buffer 实际比较一致。

### 16. SFUD Boundary

Raw Driver 板测通过后再评估 SFUD：复用已经验证的 SPI/CS/Delay Platform 适配；比较 SFUD 与 Raw Driver 的 Read/Write/Erase 行为；Raw Driver 保留为学习、诊断和底层验证参考。Storage/OTA 最终依赖 SFUD 还是项目 Storage 封装，在 SFUD 实际验证后决定。

## Hardware Verification

S02 使用专用可破坏测试 Sector：

```text
0x7FF000 ~ 0x7FFFFF
```

S04 正式分区时必须重新纳入分区规划。

按风险从低到高测试：

1. JEDEC ID=`EF 40 17`；
2. SR1 连续读取稳定；
3. 记录测试 Sector 原内容；
4. Erase `0x7FF000`；
5. 完整 Read Back 4 KiB，逐字节全 `0xFF`；
6. 单 Page Program + Read Back Compare；
7. `testSector + 0xF0` 写 300 Byte，验证 `16 + 256 + 28`；
8. Read Back 300 Byte 与源数据比较；
9. Flash 尾部合法 Read；
10. 越界 Read/Write 被拒绝；
11. 原子 Page Program 跨页被拒绝；
12. 未 4 KiB 对齐 Erase 被拒绝；
13. MCU Reset 后数据保持；
14. RTT 异常时用逻辑分析仪抽查 CS/SCK/MOSI/MISO。

RTT + EasyLogger 输出每个 Case 的地址、长度、返回码和 PASS/FAIL；PASS/FAIL 必须由代码实际检查结果产生。

## Acceptance Criteria

- [ ] Platform SPI `read()` 与 transaction 模型集成；
- [ ] SPI1/SPI2 共用 Impl，多实例构造正常；
- [ ] SPI1 PCLK2 / SPI2 PCLK1 时钟计算正确；
- [ ] Clean Rebuild 通过；
- [ ] JEDEC ID=`EF 40 17`；
- [ ] SR1/BUSY/WEL 行为符合预期；
- [ ] `0x03 Read Data` 连续读取正确；
- [ ] Sector Erase 后 4 KiB Read Back 全 `0xFF`；
- [ ] 单 Page Program + Read Back Compare 通过；
- [ ] 非 Page 对齐起始地址写入正确；
- [ ] 跨 Page Write 拆分和 Read Back Compare 通过；
- [ ] 越界访问、跨页原子 Program、未对齐 Erase 均被拒绝；
- [ ] Reset 后数据保持；
- [ ] 错误和 Timeout 可通过 RTT 诊断；
- [ ] 测试代码不调用 Chip Erase；
- [ ] Raw Driver 结果足以作为 SFUD 适配基线。

## Implementation Constraints

- 不修改 Vendor 原始库；
- 不修改 OTA、Bootloader、Firmware Metadata 等范围外接口；
- HAL Handle 不越过 Impl 边界；
- 不用固定 Delay 替代 Program/Erase BUSY 判断；
- `write()` 不隐式 Erase；
- Sector Erase 不自动修正地址；
- 不提供 Chip Erase V1 公共 API；
- 第一版使用阻塞 SPI；
- 日志不替代 Read Back / Compare。

## Design Decisions Summary

1. 复用现有 Platform SPI Bus / Device / Transaction。
2. S02 只增加 `read()`，不增加 `transfer()`。
3. SPI1/SPI2 共用一个 Impl，通过 Context 绑定 HAL Handle。
4. W25Q64 位于 `platform_bsp/w25q64`，不依赖 HAL。
5. Raw Driver 先于 SFUD，用作学习、验证和诊断基线。
6. Read 可跨 Page/Sector；原子 Page Program 不得跨 256 Byte Page。
7. 连续 `write()` 自动 Page 拆分，但不自动 Erase。
8. Sector Erase 要求 4 KiB 对齐，不自动修正。
9. 每次 Program/Erase 前 Write Enable 并验证 WEL。
10. Program/Erase 通过 SR1.BUSY 轮询完成。
11. 板测使用 `0x7FF000` 专用 Sector。
12. RTT + EasyLogger 用作主要观测和证据展示，验收结论来自真实数据校验。
