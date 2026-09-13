# S03 EEPROM Storage Design

## Metadata

- Stage: `S03_EEPROM_Storage`
- Status: `DESIGN_APPROVED`
- Owner: `Project Owner`
- Date: `2026-09-13`
- Baseline Code Commit: `b590b3cad3c04292c41130b78cfb737d3898dd30`

## Goal

建立 AT24C02 小容量非易失存储基础能力，使 Application 能够通过现有 Software I2C 可靠读写 256 Byte EEPROM，并为后续 Firmware Metadata、升级状态、失败计数等掉电状态提供原始存储能力。

S03 只建立 **Raw Driver（原始驱动）和板级可验证能力**，不提前实现 Firmware Metadata、OTA 状态机、Bootloader 状态机或大型统一设备管理架构。

## Context

S01 已建立 Application、Platform / Impl、RTT + EasyLogger 基础；S02 已建立 W25Q64 Raw Driver 和大容量外部 Flash 存储能力。

S03 复用当前仓库中的：

- `platform_gpio` 与 PB6/PB7 BSP 绑定；
- `platform_i2c` Software I2C；
- `platform_time_delay_ms/us()`；
- RTT + EasyLogger；
- 现有错误码和 Raw Driver 编码风格。

板级硬件事实已确认：

| 项目 | 结论 |
| --- | --- |
| EEPROM | U8，AT24C02，2 Kbit = 256 Byte |
| SCL | STM32 PB6，共享 I2C SCL |
| SDA | STM32 PB7，共享 I2C SDA |
| A0/A1/A2 | 全部接 GND |
| 7-bit 地址 | `0x50` |
| WP | 接 GND，硬件永久允许写 |
| Page Size | 8 Byte |
| 内部写周期 | `tWR` 最大 5 ms |
| 总线外部上拉 | SCL/SDA 通过 R50/R51 各 4.7 kΩ 上拉到 `FLASH_VCC` |
| GPIO 初始配置 | Open Drain + `GPIO_NOPULL` + 初始 HIGH |
| Software I2C 目标速率 | 当前半周期 5 us，名义约 100 kHz |

当前生成的 `gpio.c` 已与 Software I2C 运行时契约一致：PB6/PB7 为开漏、无内部上拉、初始释放为 HIGH。外部 4.7 kΩ 上拉负责总线高电平。

## In Scope

- 为 Platform I2C 增加单次地址探测能力 `platform_i2c_probe()`；
- 新增 AT24C02 Raw Driver；
- AT24C02 初始化时执行非破坏性地址探测；
- 随机读 + 顺序读；
- 连续写入与 8 Byte Page 自动拆分；
- 每个写事务后的 ACK Polling；
- 写周期有界超时；
- 256 Byte 地址边界和参数校验；
- Project Config 中保存本板固定的 `0x50` 地址；
- RTT + EasyLogger 板级验证；
- Reset / 掉电数据保持验证。

## Out of Scope

以下内容不在 S03 实现：

- Firmware Metadata 具体结构、地址布局、版本字段；
- `PENDING / TRIAL / CONFIRMED / ROLLBACK` 等 OTA 业务状态；
- CRC、双副本、Sequence、Journal 等掉电一致性策略；
- 通用 `platform_storage_device_t` / NVM Manager；
- `platform_device_t` 与 Device Manager 的完整架构接入；
- AT24C02 专用 BSP Wrapper；
- RTOS 总线互斥；
- Hardware I2C；
- WP 软件控制；
- EEPROM Emulation on W25Q64。

这些内容在真实消费者出现后再单独设计，不为了架构完整性提前扩展 S03。

## Design

### 1. 依赖方向

```text
Application composition / test entry
        ↓
AT24C02 Raw Driver
        ↓
Platform Software I2C
        ↓
Platform GPIO / Time
        ↓
STM32 Impl
        ↓
PB6 / PB7
        ↓
AT24C02
```

AT24C02 Driver 不直接调用 STM32 HAL，不直接操作 PB6/PB7，也不拥有共享 I2C Bus 的生命周期。

### 2. Software I2C 新增能力

新增公共接口：

```c
platform_error_t platform_i2c_probe(
    platform_i2c_t *i2c,
    uint8_t address);
```

语义固定为 **一次地址探测**：

```text
START
→ 7-bit Address + Write
← ACK / NACK
STOP
```

返回语义：

- ACK：`PLATFORM_ERR_OK`；
- 地址阶段 NACK：`PLATFORM_ERR_NOT_FOUND`；
- 总线超时、BUSY、GPIO 或其它错误：沿用现有 Platform 错误返回；
- `probe()` 不负责循环等待，不承担 EEPROM 写周期策略。

实现应复用现有 START / STOP、地址发送、事务清理和错误保留逻辑，不增加 EEPROM 专用语义到 Platform I2C。

### 3. AT24C02 Raw Driver 对象

建议公共对象：

```c
#define PLATFORM_AT24C02_INITIALIZER          {0}
#define PLATFORM_AT24C02_TOTAL_SIZE_BYTES     (256U)
#define PLATFORM_AT24C02_PAGE_SIZE_BYTES      (8U)
#define PLATFORM_AT24C02_I2C_ADDRESS_MIN      (0x50U)
#define PLATFORM_AT24C02_I2C_ADDRESS_MAX      (0x57U)

typedef struct
{
    platform_i2c_t *i2c;
    uint8_t deviceAddress;
    platform_bool_t initialized;
} platform_at24c02_t;
```

容量和 Page Size 属于固定芯片属性，不作为运行时字段保存。WP 已硬件接地，不进入对象模型。

### 4. 公共 API

S03 Raw Driver 第一版只暴露：

```c
platform_error_t platform_at24c02_init(
    platform_at24c02_t *eeprom,
    platform_i2c_t *i2c,
    uint8_t deviceAddress);

platform_error_t platform_at24c02_deinit(
    platform_at24c02_t *eeprom);

platform_error_t platform_at24c02_read(
    platform_at24c02_t *eeprom,
    uint32_t address,
    uint8_t *data,
    platform_size_t dataLength);

platform_error_t platform_at24c02_write(
    platform_at24c02_t *eeprom,
    uint32_t address,
    const uint8_t *data,
    platform_size_t dataLength);
```

内存地址公共参数使用 `uint32_t`，避免调用方传入大于 255 的值时在进入 Driver 前发生 `uint8_t` 截断；Driver 在内部校验后再转换为 AT24C02 的 8-bit Word Address。

### 5. 文件边界

Raw Driver 与 W25Q64 保持同类目录风格：

```text
03_Firmware/Application/OTA_APP/
├─ 00_Config/project_config.h
├─ 03_Platform/
│  ├─ platform_mcu/i2c/platform_i2c.h/.c
│  └─ platform_bsp/at24c02/
│     ├─ platform_at24c02.h
│     └─ platform_at24c02.c
└─ ...
```

不新增 `platform_bsp_at24c02.[ch]`。本板固定地址 `0x50` 作为静态项目配置由 composition root 传入 Raw Driver；Driver 本身保留 `0x50 ~ 0x57` 的标准器件地址能力。

### 6. Init / Deinit

`platform_at24c02_init()`：

1. 检查 `eeprom` / `i2c`；
2. 拒绝重复初始化；
3. 确认 I2C 已初始化；
4. 校验 `deviceAddress` 在 AT24C02 标准范围内；
5. 保存 I2C 指针和器件地址；
6. 调用 `platform_i2c_probe()`；
7. 仅在 probe ACK 后设置 `initialized = PLATFORM_TRUE`。

初始化只做非破坏性通信检查，不自动写 EEPROM 测试数据。

`platform_at24c02_deinit()` 只释放 Raw Driver 自身状态，不调用 `platform_i2c_deinit()`，因为 Software I2C Bus 可能被其它从设备共享。

### 7. Read

指定地址读取使用 Random Read：

```text
START
→ SLA+W
→ Word Address
→ Repeated START
→ SLA+R
→ Sequential Read
→ final byte NACK
→ STOP
```

直接复用现有 `platform_i2c_write_read()`。

Read 不受 8 Byte Page 边界限制，只受 256 Byte 总容量约束。Driver 禁止顺序读取越过 `0xFF` 后依赖芯片自动回绕。

### 8. Write 与 Page 拆分

公共 `platform_at24c02_write()` 对任意合法地址和长度自动按 Page 拆分：

```text
pageRemaining = 8 - (address % 8)
chunk         = min(remaining, pageRemaining)
```

每个实际 Page Write 发送：

```text
[1 Byte Word Address] + [1..8 Byte Data]
```

因此私有临时发送缓冲区最大只需 9 Byte。

推荐私有函数：

```text
platform_at24c02_validate_initialized()
platform_at24c02_validate_range()
platform_at24c02_page_write()
platform_at24c02_wait_ready()
```

`page_write()` 不作为公共 API 暴露；上层不需要知道 EEPROM Page 边界。

完整连续写入流程：

```text
validate full range first
        ↓
calculate current page chunk
        ↓
page write
        ↓
STOP
        ↓
ACK polling until ready
        ↓
advance address / data / remaining
        ↓
repeat
```

必须先校验整个请求范围，禁止写入一部分后才发现越界。

### 9. ACK Polling

AT24C02 在内部写周期内可能对器件地址 NACK。

`platform_at24c02_wait_ready()` 使用 `platform_i2c_probe(deviceAddress)` 实现有界轮询：

```text
probe
├─ OK
│  └─ Ready
├─ NOT_FOUND
│  └─ 正常写周期 Busy，delay 后重试
└─ 其它错误
   └─ 立即返回真实总线错误
```

S03 第一版与 W25Q64 当前轮询风格一致：

```text
Poll Interval: 1 ms
Software Write Timeout: 10 ms
Datasheet tWR Max: 5 ms
```

`5 ms` 是器件规格，`10 ms` 是软件保护窗口，两者不能混为同一个概念。当前不为此额外增加 monotonic-time API。

### 10. 参数和地址边界

Read / Write 公共入口至少满足：

```text
eeprom != NULL
data != NULL
initialized == TRUE
dataLength > 0
address < 256
dataLength <= 256 - address
```

范围检查使用：

```c
dataLength > (PLATFORM_AT24C02_TOTAL_SIZE_BYTES - address)
```

避免通过 `address + dataLength` 引入整数加法溢出风险。

### 11. Project Config

本板 A0/A1/A2 全部接地，因此 composition root 使用固定：

```text
PROJECT_AT24C02_I2C_ADDRESS = 0x50
```

Platform I2C 公共 API 始终使用 7-bit 地址；不得传 `0xA0 / 0xA1`。

## Interfaces and Data Flow

初始化：

```text
BSP construct PB6/PB7 GPIO
        ↓
platform_i2c_init()
        ↓
platform_at24c02_init(..., 0x50)
        ↓
platform_i2c_probe(0x50)
        ↓
ACK → EEPROM initialized
```

读取：

```text
Application/Test
    ↓
platform_at24c02_read()
    ↓
platform_i2c_write_read()
    ↓
AT24C02 Random/Sequential Read
```

写入：

```text
Application/Test
    ↓
platform_at24c02_write()
    ↓
Page Split
    ↓
platform_i2c_write()
    ↓
ACK Polling via platform_i2c_probe()
    ↓
next page / complete
```

## Failure Handling

- I2C 初始化失败：EEPROM 初始化不继续；
- EEPROM 地址 NACK：初始化返回 `PLATFORM_ERR_NOT_FOUND`，`initialized` 保持 FALSE；
- 写周期中的 probe NACK：只在 `wait_ready()` 上下文中解释为 Busy；
- ACK Polling 超过软件保护窗口：返回 `PLATFORM_ERR_TIMEOUT`；
- 总线 timeout / busy / GPIO / I/O 错误：原样向上传递，不伪装成 EEPROM Busy；
- 地址越界：在任何 EEPROM 写入开始前返回参数错误；
- 跨页写：由 Driver 自动拆分，禁止 AT24C02 页内回绕覆盖；
- Reset / 掉电：Raw Driver 不承诺事务级原子性；CRC / 双副本 / Journal 属于后续 Metadata 层。

## Verification Strategy

S03 板测统一使用 **SEGGER RTT + EasyLogger** 输出测试过程和 PASS/FAIL；不增加 UART CLI、LCD 或其它测试基础设施。逻辑分析仪只作为出现时序问题时的辅助工具，不作为常规验收的必选入口。

板测至少覆盖：

| Test | 操作 | 预期 |
| --- | --- | --- |
| Init / Probe | 初始化 Software I2C，probe `0x50` | ACK，Driver 初始化成功 |
| Single Byte | 固定测试地址写 `0x5A` 后读回 | 完全一致 |
| In-page Write | 同一 8 Byte Page 内写入多字节 | 读回一致 |
| Cross-page Write | 例如从 `0x06` 写 10 Byte | 自动拆成 `2 + 8`，读回一致 |
| Unaligned Cross-page | 非页起始地址跨页写 | 自动拆分页边界正确 |
| Last Byte | 地址 `0xFF` 写/读 1 Byte | 成功 |
| Range Reject | 从 `0xFC` 写 5 Byte 等越界请求 | 写入前返回参数错误 |
| Reset Persistence | 写测试标记，Reset 后只读 | 数据保持 |
| Power-cycle Persistence | 写测试标记，断电再上电读取 | 数据保持 |

RTT 失败日志至少包含：测试名、地址、长度、错误码；数据比较失败时增加 expected / actual。

推荐日志形态：

```text
[S03][EEPROM] probe 0x50: OK
[S03][EEPROM] single byte: PASS
[S03][EEPROM] in-page: PASS
[S03][EEPROM] cross-page: PASS
[S03][EEPROM] boundary: PASS
[S03][EEPROM] reset persistence: PASS
[S03][EEPROM] power-cycle persistence: PASS
```

## Acceptance Criteria

S03 满足以下条件后才允许进入 Verification / Review：

1. Software I2C `probe()` 的地址 ACK/NACK 与错误传播符合设计；
2. AT24C02 `init/read/write/deinit` 行为与所有权边界符合设计；
3. 单字节、页内、跨页和非页对齐写入均可正确读回；
4. `0xFF` 合法边界可访问，所有越界请求在事务前被拒绝；
5. ACK Polling 有明确超时，不使用固定 5 ms 作为唯一完成判定，不存在无限轮询；
6. Reset 后数据保持通过；
7. 实际断电/上电后数据保持通过；
8. RTT + EasyLogger 能输出可诊断的板测证据；
9. Keil Build / Clean Rebuild 通过；
10. S03 不引入 Firmware Metadata、OTA 状态机、Device Manager 或通用 NVM 架构扩张。

## Approval

- Decision: `APPROVED`
- Approved By: `Project Owner`
- Approval Date: `2026-09-13`
- Design Commit: `Recorded by current_status.md after this document is committed`
