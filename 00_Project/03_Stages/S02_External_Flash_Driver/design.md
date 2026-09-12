# S02 External Flash Driver Design

## Metadata

- Stage: `S02_External_Flash_Driver`
- Status: `DRAFT`
- Owner: `Project Owner / Design`
- Date: `2026-09-12`

## Goal

建立 STM32F411 Application 对板载 W25Q64JV External SPI Flash 的可靠原始访问能力，并完成从 SPI Platform/Impl 基础能力、板级绑定、W25Q64 Raw Driver 到真实硬件验证的完整闭环，为后续 Firmware Image Storage、Ymodem 接收和 OTA Service 提供稳定的大容量非易失存储基础。

本阶段优先完成并验证 W25Q64 Raw Driver，再在 Raw Driver 行为已经明确、硬件链路已经验证的基础上评估 SFUD 接入。Raw Driver 的目的不是替代成熟 Flash Middleware，而是用于掌握 SPI NOR Flash 的真实协议、页编程、扇区擦除、状态轮询、地址边界和异常行为，同时作为后续 SFUD 移植时的板级诊断基线。

本阶段不定义 OTA Slot、Firmware Metadata、A/B 分区和 Firmware Image 格式，这些属于后续 `S04_Firmware_Image_Storage`。

## Context

当前 Application 工程已完成 `S01_Application_Foundation`，并保留 App / Service / Platform / Impl / Vendor / Config 分层结构。

当前已确认的硬件和工程事实：

- MCU：STM32F411CEU6；
- External Flash：Winbond W25Q64JVSSIQ，容量 64 Mbit / 8 MiB；
- W25Q64 地址范围：`0x000000 ~ 0x7FFFFF`，使用 24-bit 地址；
- Page Size：256 Byte；
- Sector Size：4 KiB；
- Erase 后数据：`0xFF`；
- W25Q64 使用 Standard SPI，不使用 Dual / Quad SPI；
- SPI2 用于 W25Q64：PB13=SCK、PB14=MISO、PB15=MOSI；
- PB12=`FLASH_CS`，GPIO 软件片选，低电平有效，启动默认保持高电平；
- W25Q64 IO2/IO3 未连接 MCU，不使用 Quad 指令；
- SPI2 当前配置为 Master、2-line Full Duplex、8-bit、MSB First、Mode 0、Software NSS；
- APB1=50 MHz，SPI2 Prescaler=/4，因此实际 SPI2 Clock=12.5 MHz；
- SPI1 当前也已调整为 12.5 MHz，但仍保留给显示相关用途；
- `0x03 Read Data` 在当前 12.5 MHz 下无需 Fast Read；
- 当前 Platform SPI 已具备 Bus / Device / Transaction 模型，但仓库 `main` 上仍只有 `write()`，并且 Impl 当前只构造 SPI1；
- 当前 `impl_platform_spi.c` 的实际 SPI Clock 计算固定使用 PCLK2，增加 SPI2 前必须改为按 SPI Instance 区分 APB1/PCLK1 与 APB2/PCLK2。

## In Scope

1. 扩展 Platform SPI，使同步阻塞事务支持 `read()`。
2. 扩展 STM32 SPI Impl，使同一套实现支持 SPI1 和 SPI2 两条独立 Bus。
3. 修正 SPI 实际时钟计算：SPI1 使用 PCLK2，SPI2 使用 PCLK1。
4. 保持现有 Platform SPI Bus / Device / Transaction 模型，不重构为新的 SPI 框架。
5. 在 `03_Platform/platform_bsp/w25q64/` 增加 W25Q64 Raw Driver 和当前板级静态构造。
6. 支持 W25Q64 JEDEC ID、Status Register-1、Read Data、Page Program、连续 Write、4 KiB Sector Erase。
7. 支持 BUSY 轮询、Write Enable、WEL 校验和操作超时。
8. 支持完整 Flash 地址范围检查、Page 边界检查、Sector 对齐检查。
9. 通过专用测试 Sector 完成真实硬件 Read / Program / Erase / Read Back 验证。
10. 使用 RTT + EasyLogger 输出板测过程和结果，作为硬件验收的诊断与证据展示手段。
11. Raw Driver 验证通过后评估 SFUD 的接入方式，明确 SFUD 与自研 Raw Driver 的职责边界。

## Out of Scope

- OTA Firmware Slot A/B 分区；
- Firmware Header、Version、CRC、Metadata 与 Upgrade State；
- Ymodem、UART Firmware Transport；
- Bootloader 侧 Flash 驱动移植；
- Dual / Quad SPI；
- DMA SPI、Interrupt SPI 和异步 Flash API；
- Fast Read、32 KiB / 64 KiB Block Erase；
- Chip Erase 公共 API；
- Security Register、Deep Power-down 等扩展命令；
- Flash Wear Leveling、文件系统或 KV Storage；
- 在 Raw Driver 内自动 Erase、自动分区或自动判断 OTA 业务状态；
- 为多个 SPI Device 动态切换 HAL 配置。S02 Phase 1 继续采用“校验 CubeMX 固定配置”的策略。

## Design

### 1. 分层位置与依赖方向

W25Q64 属于具体板级器件驱动，放置在 Platform BSP：

```text
03_Platform/
└─ platform_bsp/
   └─ w25q64/
      ├─ platform_w25q64.h
      ├─ platform_w25q64.c
      ├─ platform_bsp_w25q64.h
      └─ platform_bsp_w25q64.c
```

依赖方向：

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

约束：

- `platform_w25q64.*` 不直接访问 `hspi2`、`SPI2`、PB12 或 `HAL_SPI_xxx()`；
- W25Q64 Driver 只依赖 Platform SPI / GPIO / Time 能力；
- 当前板级引脚和 SPI 静态参数由 `platform_bsp_w25q64.*` 构造；
- 上层 Storage/OTA 不直接操作 WEL、BUSY 或 W25Q64 指令码。

### 2. SPI Platform 最小扩展

保留现有 Bus / Device / Transaction 模型，仅给 `platform_spi_bus_ops_t` 增加同步阻塞 `read()`：

```c
platform_error_t (*read)(
    platform_spi_bus_t *bus,
    uint8_t *data,
    platform_size_t dataLength);
```

公开 facade：

```c
platform_error_t platform_spi_read(
    platform_spi_device_t *device,
    uint8_t *data,
    platform_size_t dataLength);
```

行为与现有 `platform_spi_write()` 对称：

- `data == NULL` → `PLATFORM_ERR_NULL_POINTER`；
- `dataLength == 0` → `PLATFORM_ERR_INVALID_PARAM`；
- Device 未初始化 → `PLATFORM_ERR_NOT_INITIALIZED`；
- Bus 未 STARTED → `PLATFORM_ERR_INVALID_STATE`；
- 当前 `activeDevice != device` → `PLATFORM_ERR_INVALID_STATE`；
- HAL 状态继续使用既有统一映射。

S02 不增加 `transfer()`。当前 W25Q64 V1 的 `write command/header → read data` 可以由同一 transaction 中的 `write()` + `read()` 表达，避免提前引入没有实际需求的通用 Full-duplex Transfer API。

### 3. SPI Impl 多实例支持

继续使用单一 `impl_platform_spi.c`，SPI1/SPI2 共用生命周期和 Bus Ops：

```text
g_spi1Context → &hspi1
g_spi2Context → &hspi2
```

公开构造入口：

```c
impl_platform_spi1_construct(...)
impl_platform_spi2_construct(...)
```

不复制 `impl_platform_spi1.c / impl_platform_spi2.c`。

新增 `stm32_spi_read()`，内部使用阻塞 `HAL_SPI_Receive()`；本阶段不增加 DMA 或中断模式。

实际时钟计算必须按实例选择外围总线：

```text
SPI1 → PCLK2
SPI2 → PCLK1
```

当前工程预期：

```text
SPI1: 100 MHz / 8 = 12.5 MHz
SPI2:  50 MHz / 4 = 12.5 MHz
```

`applyConfig()` 继续只校验当前 CubeMX/HAL 固定配置是否满足目标 Device，不在运行时重新配置 SPI。

### 4. W25Q64 数据模型

建议对象：

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

语义：

- `spiDevice / cs`：运行上下文；
- `spiConfig / csActiveLevel`：静态设置；
- JEDEC ID 与 `initialized`：运行数据和状态。

固定几何信息使用常量，不在对象中重复保存：

```text
Total Size = 8 MiB
Address    = 0x000000 ~ 0x7FFFFF
Page Size  = 256 Byte
Sector     = 4096 Byte
```

### 5. BSP 静态构造

`platform_bsp_w25q64_construct_flash()` 负责当前开发板静态信息：

```text
SPI Mode     = Mode 0
Bit Order    = MSB First
Data Bits    = 8
CS Active    = Low
CS GPIO      = FLASH_CS / PB12
```

`spiConfig.maxClockHz` 表示 Device/当前所用命令允许的最大时钟，而不是“当前实际运行时钟”。对 V1 使用的 `0x03 Read Data`，可按 50 MHz 能力约束；当前实际运行 12.5 MHz 由 CubeMX Prescaler 决定。

### 6. 初始化与反初始化

初始化接口：

```c
platform_w25q64_init(flash, spiBus)
```

建议流程：

```text
验证 flash / spiBus
        ↓
确认 SPI Bus 已 STARTED
        ↓
配置 CS GPIO 为输出且保持 inactive
        ↓
platform_spi_device_init()
        ↓
读取 SR1 / 等待 BUSY 清零
        ↓
Read JEDEC ID (0x9F)
        ↓
校验 EF 40 17
        ↓
保存 JEDEC ID
        ↓
initialized = TRUE
```

初始化成功的语义是“预期 W25Q64JV 已响应且基础 SPI 通信链路可用”，不是仅保存指针。

若 GPIO、SPI Device、状态读取或 JEDEC 校验任一步失败，必须回滚已建立资源，最终保持 `initialized = FALSE`。

`deinit()` 仅释放 W25Q64 自己拥有的 SPI Device 描述符和 CS GPIO，不停止或反初始化共享 SPI Bus。

### 7. SPI Transaction 规则

只要 `platform_spi_transaction_begin()` 成功，不论后续操作成功或失败，都必须执行 `platform_spi_transaction_end()`。

错误优先级：

```text
业务/数据操作失败
→ 仍然结束 transaction
→ 返回最先发生的操作错误

业务操作成功但 transaction_end 失败
→ 返回 transaction_end 错误
```

建议私有 helper：

```c
platform_w25q64_finish_transaction(...)
platform_w25q64_command_read(...)
```

`command_read()` 只服务真实重复场景：

```text
begin
 ↓
write command/header
 ↓
read response/data
 ↓
end
```

不建立通用 Command Engine 或 Command Descriptor 框架。

### 8. Raw Driver V1 命令集

V1 使用以下 Standard SPI 指令：

| Command | Opcode | Purpose |
| --- | ---: | --- |
| Read JEDEC ID | `0x9F` | 识别 Manufacturer / Memory Type / Capacity |
| Read Status Register-1 | `0x05` | 读取 BUSY / WEL |
| Write Enable | `0x06` | 修改操作前置 |
| Read Data | `0x03` | 连续读取 |
| Page Program | `0x02` | 单页编程 |
| Sector Erase 4 KiB | `0x20` | 最小擦除单位 |

V1 不暴露 Chip Erase `0x60 / 0xC7`。

JEDEC ID 预期：

```text
EF 40 17
```

SR1 关键位：

```text
BUSY = bit0
WEL  = bit1
```

### 9. BUSY、WEL 与超时策略

`write_enable()` 和 `wait_ready()` 保持为 Driver 私有能力，不向 Storage/OTA 上层公开。

Write Enable 成功必须读取 SR1 确认 `WEL == 1`，不能只以 `0x06` SPI 发送成功作为成功条件。

Program / Erase 完成通过轮询 `SR1.BUSY` 判断，不使用固定 Delay 代替完成判断。

建议轮询周期：1 ms。

建议超时：

```text
Page Program timeout  = 10 ms
Sector Erase timeout  = 500 ms
Init Ready timeout    = 1000 ms
```

每一笔修改操作都重新执行 Write Enable。不得假设 WEL 在多次 Program / Erase 之间持续有效。

### 10. Read Data

公开接口：

```c
platform_w25q64_read(
    flash,
    address,
    data,
    dataLength)
```

Transaction：

```text
CS Low
0x03
A23:A16
A15:A8
A7:A0
Read N Bytes
CS High
```

规则：

- Read 不受 256 Byte Page 边界限制；
- 可跨 Page、跨 Sector 连续读取；
- 只受整个 8 MiB 地址范围限制；
- 普通 Read 不隐式执行 `wait_ready()`；
- 修改类 API 返回成功时必须已经等待 Flash Ready，因此正常调用链不应让普通 Read 承担状态同步职责。

### 11. 地址与范围校验

统一使用 24-bit 地址编码 helper，按 MSB First 发送：

```text
0x123456 → 12 34 56
```

范围校验避免使用可能溢出的 `address + length`，采用：

```text
length > 0
address < TOTAL_SIZE
length <= TOTAL_SIZE - address
```

越界操作必须在发送 SPI 指令前被拒绝。

### 12. Page Program 与连续 Write

底层原子操作：

```c
platform_w25q64_page_program(...)
```

必须满足：

```text
0 < length <= 256
(address % 256) + length <= 256
```

禁止单次 Page Program 跨 Page，防止 W25Q64 页内 wrap-around 导致已有数据被覆盖。

完整流程：

```text
wait_ready
 ↓
write_enable
 ↓
确认 WEL
 ↓
0x02 + 24-bit address + data
 ↓
transaction_end
 ↓
wait_ready(Page Program timeout)
```

`page_program()` 返回 `OK` 表示内部 Program 已结束、Flash 已重新 Ready。

连续写接口：

```c
platform_w25q64_write(...)
```

只负责根据当前 Page 剩余空间自动拆分：

```text
pageOffset    = address % 256
pageRemaining = 256 - pageOffset
chunkLength   = min(remaining, pageRemaining)
```

例如从 `0x0000F0` 写 300 Byte：

```text
16 Byte + 256 Byte + 28 Byte
```

`write()` 不自动 Erase，也不检查目标区域是否全部为 `0xFF`。Raw Driver 不猜测上层存储策略。

### 13. Sector Erase

公开接口：

```c
platform_w25q64_sector_erase(
    flash,
    sectorAddress)
```

只支持 4 KiB `0x20 Sector Erase`。

调用者必须传入 Sector 起始地址：

```text
sectorAddress % 4096 == 0
```

驱动不自动将任意地址向下对齐，因为 Erase 是破坏性操作；传错地址应直接返回参数错误，而不是静默擦除调用者未明确指定的 Sector。

流程：

```text
validate aligned address
 ↓
wait_ready
 ↓
write_enable
 ↓
确认 WEL
 ↓
0x20 + 24-bit address
 ↓
transaction_end
 ↓
wait_ready(Sector Erase timeout)
```

正式 Driver 不在擦除后自动读取整个 4 KiB 做 `0xFF` 校验；Read Back 属于测试或更高层完整性策略。

### 14. Public API Boundary

Raw Driver V1 对上层公开：

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

以下保持私有：

```text
write_enable()
wait_ready()
validate_range()
encode_address()
command_read()
finish_transaction()
rollback_init()
```

### 15. Error Semantics

Driver 继续使用统一 `platform_error_t`，不新建 W25Q64 专属错误体系。

至少保证：

- NULL 指针可诊断；
- 未初始化对象可诊断；
- SPI Bus 状态错误可诊断；
- 地址越界可诊断；
- Page Program 跨页可诊断；
- Sector 地址未对齐可诊断；
- HAL/SPI Busy、Timeout、I/O Error 可透传为统一 Platform 错误；
- JEDEC ID 不匹配导致初始化失败；
- BUSY 超时导致修改操作失败，不误报成功。

发生 SPI Data Operation 错误后必须尽力结束 transaction，避免 CS 长期保持低电平或 `activeDevice` 永久占用 Bus。

### 16. Logging / Diagnostics

W25Q64 Raw Driver 本身不依赖 Service Log，避免 Platform/BSP 反向依赖 Service。

板级验收和 Application 测试代码可以使用 RTT + EasyLogger 输出诊断信息。建议至少输出：

```text
W25Q64 init start/result
JEDEC ID: EF 40 17
SR1 value / BUSY / WEL
Test Sector address
Erase start/result/elapsed
Program address/length/result
Cross-page chunk information（调试级可选）
Read Back compare result
Boundary test result
Timeout/error code
Reset 后 persistence check result
```

RTT 日志用于观察和保留板测证据，但不能替代真实数据校验。例如“Erase PASS”必须建立在 Read Back 全部为 `0xFF` 的检查结果上，“Program PASS”必须建立在 Read Back 与源 Buffer 比对一致的结果上，而不是仅依据 SPI API 返回 `OK`。

### 17. SFUD Boundary

Raw Driver 的第一目标是让当前板级 SPI NOR 链路和器件行为可被独立理解、调试和验证。

Raw Driver 板测通过后再接入 SFUD，届时：

- SFUD 使用经过验证的 SPI/CS/Delay Platform 适配；
- 比较 SFUD Read / Write / Erase 与 Raw Driver 行为；
- Raw Driver 可保留为学习、诊断和底层验证参考；
- OTA/Storage 最终是否直接依赖 SFUD 或项目封装后的 Storage 接口，在 SFUD 实际集成后决定；
- 不在 Raw Driver V1 中为了模拟 SFUD 提前增加复杂抽象。

## Data Flow

### Read

```text
Caller
  ↓
platform_w25q64_read
  ↓
Range Validation
  ↓
SPI Transaction Begin
  ↓
0x03 + Address
  ↓
SPI Read Data
  ↓
Transaction End
```

### Program

```text
Caller
  ↓
platform_w25q64_write
  ↓
Page Split
  ↓
page_program
  ↓
Ready → WREN → WEL Check
  ↓
0x02 + Address + Data
  ↓
BUSY Poll
```

### Erase

```text
Caller
  ↓
sector_erase
  ↓
Alignment Check
  ↓
Ready → WREN → WEL Check
  ↓
0x20 + Sector Address
  ↓
BUSY Poll
```

## Hardware Verification

S02 Raw Driver 板测使用一个明确的专用测试 Sector。S04 正式分区前，暂定使用 W25Q64 最后一个 Sector：

```text
0x7FF000 ~ 0x7FFFFF
```

该区域在 S02 中视为允许破坏的数据区；进入 S04 正式 A/B Slot / Metadata 分区设计后必须重新纳入分区规划，不再默认作为自由测试区。

板测按风险由低到高执行：

1. 初始化并读取 JEDEC ID，要求为 `EF 40 17`；
2. 连续读取 SR1，确认返回稳定且 BUSY/WEL 可解释；
3. 读取测试 Sector 当前内容，记录但不据此判定通过；
4. 擦除 `0x7FF000` Sector；
5. 读取完整 4 KiB，逐字节确认全部为 `0xFF`；
6. 在测试 Sector 内执行单 Page Program，并 Read Back Compare；
7. 从 `testSector + 0xF0` 写入 300 Byte，验证自动拆分为 `16 + 256 + 28`；
8. 连续 Read Back 300 Byte，与源数据逐字节比较；
9. 测试 Flash 尾部合法 Read；
10. 测试越界 Read/Write 被拒绝；
11. 测试 Page Program 跨页原子 API 被拒绝；
12. 测试未 4 KiB 对齐 Sector Erase 被拒绝；
13. MCU Reset 后再次读取测试数据，确认非易失保持；
14. 使用逻辑分析仪抽查 JEDEC ID 或一次 Read/Program transaction 的 CS/SCK/MOSI/MISO 时序，如 RTT 结果异常则优先用于定位硬件链路。

RTT + EasyLogger 作为上述测试的主要运行时观测接口，输出每个 Case 的地址、长度、返回码和 PASS/FAIL；最终 PASS/FAIL 必须由代码实际校验结果产生。

## Acceptance Criteria

S02 Raw Driver V1 至少满足：

- [ ] Platform SPI `read()` 与现有 transaction 模型集成完成；
- [ ] SPI1/SPI2 共用 Impl，多实例构造正常；
- [ ] SPI1 PCLK2 / SPI2 PCLK1 时钟计算正确；
- [ ] Clean Rebuild 通过；
- [ ] W25Q64 初始化稳定，JEDEC ID=`EF 40 17`；
- [ ] SR1 可读取，BUSY/WEL 行为符合预期；
- [ ] `0x03 Read Data` 连续读取正确；
- [ ] 4 KiB Sector Erase 完成后 Read Back 全部为 `0xFF`；
- [ ] 单 Page Program + Read Back Compare 通过；
- [ ] 非 Page 对齐起始地址写入正确；
- [ ] 跨 Page Write 自动拆分并 Read Back Compare 通过；
- [ ] 地址越界被拒绝；
- [ ] 原子 Page Program 跨页被拒绝；
- [ ] 未对齐 Sector Erase 被拒绝；
- [ ] Reset 后数据保持；
- [ ] 错误返回和 Timeout 可通过 RTT 日志诊断；
- [ ] 测试代码不会调用 Chip Erase；
- [ ] Raw Driver 验证结果足以作为后续 SFUD 适配基线。

## Implementation Constraints

- 不直接修改 Vendor 原始库实现；
- 不为了 S02 修改 OTA、Bootloader、Firmware Metadata 等范围外接口；
- 不将 HAL Handle 暴露到 Platform/BSP Driver；
- 不用固定 Delay 替代 Program / Erase 的 BUSY 完成判断；
- 不允许 Raw Driver 在 `write()` 中隐式擦除；
- 不允许 Sector Erase 自动向下对齐调用者错误地址；
- 不提供 Chip Erase V1 公共 API；
- 第一版使用阻塞 SPI，优先保证行为清晰、稳定、可验证；
- 日志只用于诊断和展示，不替代 Read Back / Compare 等实际验收逻辑。

## Design Decisions Summary

1. 继续复用现有 Platform SPI Bus / Device / Transaction，不重新设计 SPI 架构。
2. S02 只给 SPI 增加 `read()`，暂不增加 `transfer()`。
3. SPI1/SPI2 使用同一 Impl，通过 Context 绑定不同 HAL Handle。
4. W25Q64 驱动位于 `platform_bsp/w25q64`，不直接依赖 HAL。
5. Raw Driver 先于 SFUD，用于学习、验证和建立诊断基线。
6. Read 可跨 Page/Sector；Page Program 原子操作不得跨 256 Byte Page。
7. 连续 `write()` 负责自动 Page 拆分，但绝不自动 Erase。
8. Sector Erase 要求调用者传入 4 KiB 对齐起始地址，不自动修正。
9. 每次 Program/Erase 前重新 Write Enable，并验证 WEL。
10. Program/Erase 通过 SR1.BUSY 轮询确认完成。
11. 板测使用专用测试 Sector `0x7FF000`，避免误擦未知区域。
12. RTT + EasyLogger 用作板测主要观测和证据展示，但通过结论必须来自真实数据校验。
