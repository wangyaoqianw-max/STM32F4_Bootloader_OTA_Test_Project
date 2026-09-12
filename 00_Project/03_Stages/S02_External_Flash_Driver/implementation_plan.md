# S02 External Flash Driver Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有 STM32F411 Application 分层基础上补齐 SPI2 读能力并实现可验证的 W25Q64 Raw Driver V1，完成 JEDEC ID、SR1、Read、Page Program、跨页 Write、4 KiB Sector Erase、边界校验和 RTT 板测闭环。

**Architecture:** 保留现有 `Platform SPI Bus / Device / Transaction` 模型，只增加同步阻塞 `read()`；SPI1/SPI2 共用 STM32 Impl，通过 Context 绑定不同 HAL Handle。W25Q64 位于 `03_Platform/platform_bsp/w25q64/`，不直接依赖 HAL；板级测试使用独立 Test Harness + RTT/EasyLogger 展示结果，PASS/FAIL 由 Read Back/Compare 实际校验产生。

**Tech Stack:** STM32F411CEU6、STM32CubeF4 HAL、Keil MDK-ARM、Platform/Impl 分层、W25Q64JV Standard SPI、RTT + EasyLogger、FreeRTOS 基础运行环境。

**Spec:** `00_Project/03_Stages/S02_External_Flash_Driver/design.md`

## Global Constraints

- SPI1 与 SPI2 当前实际时钟均为 12.5 MHz；SPI1 使用 PCLK2，SPI2 使用 PCLK1。
- S02 只增加同步阻塞 `platform_spi_read()`，不增加 `transfer()`、DMA SPI、Interrupt SPI 或异步 API。
- W25Q64 Raw Driver 不访问 `hspi2`、`SPI2`、PB12 或 `HAL_SPI_xxx()`。
- W25Q64 V1 使用 `0x9F / 0x05 / 0x06 / 0x03 / 0x02 / 0x20`；不提供 Chip Erase。
- Read 可跨 Page/Sector；单次 Page Program 不得跨 256 Byte Page。
- `platform_w25q64_write()` 可自动拆 Page，但不得隐式 Erase。
- Sector Erase 地址必须 4 KiB 对齐，Driver 不自动向下对齐。
- Program / Erase 前必须 Write Enable 并确认 WEL；完成状态通过 SR1.BUSY 轮询，不用固定 Delay 替代。
- S02 专用可破坏测试 Sector：`0x7FF000 ~ 0x7FFFFF`。
- RTT + EasyLogger 只负责观测和证据展示；Erase/Program PASS 必须来自真实 Read Back 校验。
- 不修改 Vendor 原始库，不提前实现 OTA Slot、Firmware Metadata、Ymodem、Bootloader 或 Security。

---

## File Map

### Modify

- `03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/spi/platform_spi.h`：增加 SPI `read` Bus Op 与 public facade。
- `03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/spi/platform_spi.c`：增加 `platform_spi_read()`，Bus 构造要求 `ops->read`。
- `03_Firmware/Application/OTA_APP/04_Impl/impl_mcu/impl_platform_spi.h`：增加 SPI2 构造入口并修正文档语义。
- `03_Firmware/Application/OTA_APP/04_Impl/impl_mcu/impl_platform_spi.c`：增加 SPI2 Context、阻塞读、PCLK1/PCLK2 判断、共用构造 helper。
- `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/platform_bsp_spi.h`：增加 Storage SPI Bus 构造入口。
- `03_Firmware/Application/OTA_APP/04_Impl/impl_bsp/impl_platform_bsp_spi.c`：Storage Bus 绑定 `impl_platform_spi2_construct()`。
- `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/platform_bsp_gpio.h`：增加 Flash CS GPIO 构造入口。
- `03_Firmware/Application/OTA_APP/04_Impl/impl_bsp/impl_platform_bsp_gpio.c`：绑定 `FLASH_CS_GPIO_Port / FLASH_CS_Pin`。
- `03_Firmware/Application/OTA_APP/00_Config/project_config.h`：增加 S02 Flash 测试/器件静态配置。
- `03_Firmware/Application/OTA_APP/01_APP/app_main.c`：只负责构造/启动 Storage SPI Bus 并调用阶段板测入口；不把 W25Q64 协议逻辑写入 App。
- `03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx`：加入 W25Q64 Driver 与 S02 Board Test 源文件/Include Path。

### Create

- `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_w25q64.h`
- `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_w25q64.c`
- `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_bsp_w25q64.h`
- `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_bsp_w25q64.c`
- `04_Test/Board/S02_External_Flash_Driver/s02_flash_board_test.h`
- `04_Test/Board/S02_External_Flash_Driver/s02_flash_board_test.c`
- `00_Project/03_Stages/S02_External_Flash_Driver/sfud_evaluation.md`
- `04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md`（Verification Role 写最终证据；Implementation 只准备测试入口，不预填 PASS）。

---

### Task 1: Extend Platform SPI for Read and SPI2 Multi-instance Support

**Files:**
- Modify: `03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/spi/platform_spi.h`
- Modify: `03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/spi/platform_spi.c`
- Modify: `03_Firmware/Application/OTA_APP/04_Impl/impl_mcu/impl_platform_spi.h`
- Modify: `03_Firmware/Application/OTA_APP/04_Impl/impl_mcu/impl_platform_spi.c`
- Modify: `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/platform_bsp_spi.h`
- Modify: `03_Firmware/Application/OTA_APP/04_Impl/impl_bsp/impl_platform_bsp_spi.c`

**Interfaces:**
- Consumes: existing `platform_spi_bus_t`, `platform_spi_device_t`, transaction lifecycle and HAL-owned `hspi1/hspi2`.
- Produces:
  - `platform_error_t platform_spi_read(platform_spi_device_t *device, uint8_t *data, platform_size_t dataLength);`
  - `platform_error_t impl_platform_spi2_construct(platform_spi_bus_t *bus, const char *name, uint32_t caps);`
  - `platform_error_t platform_bsp_spi_construct_storage_bus(platform_spi_bus_t *bus);`

- [ ] **Step 1: Add a compile-time failing call site for `platform_spi_read()` and SPI2 constructor**

Temporarily add a local compile probe in the implementation branch or test harness that references the intended signatures:

```c
static void s02_spi_compile_probe(platform_spi_device_t *device,
                                  platform_spi_bus_t *bus)
{
    uint8_t byte = 0U;

    (void)platform_spi_read(device, &byte, 1U);
    (void)impl_platform_spi2_construct(bus,
                                       "storage_spi_bus",
                                       PLATFORM_DEVICE_CAP_NONE);
}
```

Run Keil Clean/Rebuild. Expected before implementation: compile failure because `platform_spi_read()` and/or `impl_platform_spi2_construct()` are not declared.

- [ ] **Step 2: Extend `platform_spi_bus_ops_t` and public facade**

Add `read` after `write`:

```c
platform_error_t (*read)(
    platform_spi_bus_t *bus,
    uint8_t *data,
    platform_size_t dataLength);
```

Add public declaration:

```c
platform_error_t platform_spi_read(
    platform_spi_device_t *device,
    uint8_t *data,
    platform_size_t dataLength);
```

Update `platform_spi_bus_init()` validation so `params->ops->read == NULL` is rejected.

Implement `platform_spi_read()` symmetrically with `platform_spi_write()`:

```c
if (data == NULL) {
    return PLATFORM_ERR_NULL_POINTER;
}
if (dataLength == 0U) {
    return PLATFORM_ERR_INVALID_PARAM;
}

result = platform_spi_validate_device(device);
if (result != PLATFORM_ERR_OK) {
    return result;
}

bus = device->bus;
if (bus->device.object.state != PLATFORM_OBJECT_STARTED) {
    return PLATFORM_ERR_INVALID_STATE;
}
if (bus->activeDevice != device) {
    return PLATFORM_ERR_INVALID_STATE;
}

return bus->ops->read(bus, data, dataLength);
```

- [ ] **Step 3: Add SPI2 Context and common constructor**

In `impl_platform_spi.c` keep one implementation file:

```c
static stm32_spi_impl_context_t g_spi1Context = { &hspi1 };
static stm32_spi_impl_context_t g_spi2Context = { &hspi2 };
```

Add private common constructor:

```c
static platform_error_t stm32_spi_construct(
    platform_spi_bus_t *bus,
    const char *name,
    uint32_t caps,
    stm32_spi_impl_context_t *context)
{
    platform_spi_bus_init_params_t params;

    if ((bus == NULL) || (name == NULL) || (context == NULL)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    params.name = name;
    params.caps = caps;
    params.lifecycle = &g_stm32SpiLifecycleOps;
    params.ops = &g_stm32SpiOps;
    params.implContext = context;

    return platform_spi_bus_init(bus, &params);
}
```

`impl_platform_spi1_construct()` and `impl_platform_spi2_construct()` only选择不同 Context。

- [ ] **Step 4: Add blocking read and keep HAL status mapping unified**

Add:

```c
static platform_error_t stm32_spi_read(
    platform_spi_bus_t *bus,
    uint8_t *data,
    platform_size_t dataLength)
{
    platform_error_t result;
    stm32_spi_impl_context_t *context = NULL;

    if ((data == NULL) || (dataLength == 0U)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }
    if (dataLength > STM32_SPI_HAL_MAX_TRANSFER_SIZE) {
        return PLATFORM_ERR_OVERFLOW;
    }

    result = stm32_spi_get_context(bus, &context);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    return stm32_spi_map_hal_status(HAL_SPI_Receive(
        context->halSpi,
        data,
        (uint16_t)dataLength,
        STM32_SPI_BLOCKING_TIMEOUT_MS));
}
```

Add it to `g_stm32SpiOps`。

- [ ] **Step 5: Fix actual SPI clock source selection**

Add helper:

```c
static platform_error_t stm32_spi_get_peripheral_clock_hz(
    const SPI_HandleTypeDef *halSpi,
    uint32_t *clockHz)
{
    if ((halSpi == NULL) || (clockHz == NULL)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (halSpi->Instance == SPI1) {
        *clockHz = HAL_RCC_GetPCLK2Freq();
    } else if (halSpi->Instance == SPI2) {
        *clockHz = HAL_RCC_GetPCLK1Freq();
    } else {
        return PLATFORM_ERR_NOT_SUPPORTED;
    }

    return (*clockHz == 0U) ?
           PLATFORM_ERR_NOT_INITIALIZED : PLATFORM_ERR_OK;
}
```

`stm32_spi_get_actual_clock_hz()` 调用该 helper，再除 Prescaler。不要继续固定 PCLK2。

- [ ] **Step 6: Add board-level Storage SPI bus constructor**

Public BSP interface:

```c
platform_error_t platform_bsp_spi_construct_storage_bus(
    platform_spi_bus_t *bus);
```

Implementation:

```c
return impl_platform_spi2_construct(bus,
                                    "storage_spi_bus",
                                    PLATFORM_DEVICE_CAP_NONE);
```

Display Bus 保持 SPI1，不改既有语义。

- [ ] **Step 7: Clean Rebuild and remove temporary compile probe**

Run Keil Clean/Rebuild. Expected: `0 Error`; existing explained warnings may remain unchanged. Confirm no new warning from S02 changes. Remove the temporary compile probe after the real API compiles.

- [ ] **Step 8: Commit Task 1**

```bash
git add 03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/spi \
        03_Firmware/Application/OTA_APP/04_Impl/impl_mcu/impl_platform_spi.* \
        03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/platform_bsp_spi.h \
        03_Firmware/Application/OTA_APP/04_Impl/impl_bsp/impl_platform_bsp_spi.c
git commit -m "feat: extend platform spi for storage bus"
```

---

### Task 2: Add W25Q64 BSP Binding and Read-only Identification Path

**Files:**
- Modify: `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/platform_bsp_gpio.h`
- Modify: `03_Firmware/Application/OTA_APP/04_Impl/impl_bsp/impl_platform_bsp_gpio.c`
- Modify: `03_Firmware/Application/OTA_APP/00_Config/project_config.h`
- Create: `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_w25q64.h`
- Create: `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_w25q64.c`
- Create: `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_bsp_w25q64.h`
- Create: `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_bsp_w25q64.c`
- Modify: `03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx`

**Interfaces:**
- Consumes: Task 1 `platform_spi_read()` and Storage SPI Bus.
- Produces:
  - `platform_w25q64_t`
  - `platform_w25q64_init()` / `platform_w25q64_deinit()`
  - `platform_w25q64_read_jedec_id()`
  - `platform_w25q64_read_status1()`
  - `platform_w25q64_read()`
  - `platform_bsp_w25q64_construct_flash()`

- [ ] **Step 1: Write a failing board-test skeleton for JEDEC ID**

Create `04_Test/Board/S02_External_Flash_Driver/s02_flash_board_test.h/.c` with intended entry point:

```c
platform_error_t s02_flash_board_test_run(platform_spi_bus_t *spiBus);
```

Initial test body should try to construct/init and check JEDEC ID:

```c
if ((id.manufacturerId != 0xEFU) ||
    (id.memoryType != 0x40U) ||
    (id.capacityId != 0x17U)) {
    return PLATFORM_ERR_IO;
}
```

Add test source/include path to Keil and call from App only after Storage SPI Bus is STARTED. Expected before W25Q64 implementation: compile failure because W25Q64 APIs do not exist.

- [ ] **Step 2: Add Flash CS logical GPIO binding**

In BSP GPIO contract add:

```c
platform_error_t platform_bsp_gpio_construct_flash_cs(
    platform_gpio_t *gpio);
```

In Impl BSP add context:

```c
static impl_platform_gpio_context_t g_flashCsContext = {
    FLASH_CS_GPIO_Port,
    FLASH_CS_Pin
};
```

and construct it through `impl_platform_gpio_construct()` with name `"flash_cs_gpio"`。

- [ ] **Step 3: Add W25Q64 public types/constants**

In `platform_w25q64.h` define at least:

```c
#define PLATFORM_W25Q64_INITIALIZER          {0}
#define PLATFORM_W25Q64_TOTAL_SIZE_BYTES     (8U * 1024U * 1024U)
#define PLATFORM_W25Q64_PAGE_SIZE_BYTES      (256U)
#define PLATFORM_W25Q64_SECTOR_SIZE_BYTES    (4096U)
#define PLATFORM_W25Q64_ADDRESS_MAX          (0x7FFFFFU)

typedef struct
{
    uint8_t manufacturerId;
    uint8_t memoryType;
    uint8_t capacityId;
} platform_w25q64_jedec_id_t;

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

Declare lifecycle/diagnostic/read APIs exactly as frozen in `design.md`。

- [ ] **Step 4: Implement BSP W25Q64 static construction**

`platform_bsp_w25q64_construct_flash()` should start from `PLATFORM_W25Q64_INITIALIZER`, construct CS GPIO, then set:

```c
constructed.spiConfig.mode = PLATFORM_SPI_MODE_0;
constructed.spiConfig.bitOrder = PLATFORM_SPI_BIT_ORDER_MSB_FIRST;
constructed.spiConfig.dataBits = 8U;
constructed.spiConfig.maxClockHz = PROJECT_FLASH_SPI_MAX_CLOCK_HZ;
constructed.csActiveLevel = PLATFORM_GPIO_LEVEL_LOW;
```

Add to `project_config.h`:

```c
#define PROJECT_FLASH_SPI_MAX_CLOCK_HZ       (50000000U)
#define PROJECT_FLASH_TEST_SECTOR_ADDRESS    (0x7FF000U)
```

`50 MHz` is the Device/command limit used for Platform validation, not the actual 12.5 MHz runtime clock.

- [ ] **Step 5: Implement transaction helpers, SR1 and JEDEC ID**

Private command constants:

```c
#define PLATFORM_W25Q64_CMD_READ_JEDEC_ID    (0x9FU)
#define PLATFORM_W25Q64_CMD_READ_STATUS1     (0x05U)
#define PLATFORM_W25Q64_CMD_READ_DATA        (0x03U)
```

Implement `finish_transaction()` so `transaction_end()` always executes after a successful begin and the first operation error wins.

Implement `command_read()` as:

```text
begin → write command/header → read response → end
```

Implement raw JEDEC read usable before `flash->initialized == TRUE`; public JEDEC API requires initialized object.

- [ ] **Step 6: Implement init/deinit rollback**

`platform_w25q64_init()` must:

```text
validate constructed object and STARTED bus
→ configure CS output/inactive
→ platform_spi_device_init
→ wait for ready using SR1
→ read raw JEDEC ID
→ require EF 40 17
→ save ID
→ initialized = TRUE
```

Any failure after CS configuration must rollback SPI Device/GPIO and leave `initialized = FALSE`。

`deinit()` must not stop/deinit the shared SPI Bus.

- [ ] **Step 7: Implement range validation, 24-bit address and Read Data**

Private range check:

```c
if (dataLength == 0U) {
    return PLATFORM_ERR_INVALID_PARAM;
}
if (address >= PLATFORM_W25Q64_TOTAL_SIZE_BYTES) {
    return PLATFORM_ERR_INVALID_PARAM;
}
if (dataLength > (PLATFORM_W25Q64_TOTAL_SIZE_BYTES - address)) {
    return PLATFORM_ERR_INVALID_PARAM;
}
```

Encode address MSB-first and send header `[0x03, A23:A16, A15:A8, A7:A0]`，then read arbitrary valid length; do not impose Page boundaries on Read.

- [ ] **Step 8: Keil Clean/Rebuild and non-destructive board test**

Expected RTT sequence must include at least:

```text
[S02] storage spi start result=0
[S02] w25q64 init result=0
[S02] jedec=EF 40 17 PASS
[S02] sr1=xx PASS
```

Do not erase or program yet. If JEDEC is wrong, inspect CS/SCK/MOSI/MISO with logic analyzer before changing Driver assumptions.

- [ ] **Step 9: Commit Task 2**

```bash
git add 03_Firmware/Application/OTA_APP/00_Config/project_config.h \
        03_Firmware/Application/OTA_APP/03_Platform/platform_bsp \
        03_Firmware/Application/OTA_APP/04_Impl/impl_bsp/impl_platform_bsp_gpio.c \
        03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx \
        04_Test/Board/S02_External_Flash_Driver
git commit -m "feat: add w25q64 identification and read path"
```

---

### Task 3: Add Write Enable, Busy Polling, Page Program and Sector Erase

**Files:**
- Modify: `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_w25q64.c`
- Modify: `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_w25q64.h`
- Modify: `04_Test/Board/S02_External_Flash_Driver/s02_flash_board_test.c`

**Interfaces:**
- Consumes: Task 2 W25Q64 object, SR1 read, transaction helpers and Read Data.
- Produces:
  - `platform_w25q64_page_program()`
  - `platform_w25q64_sector_erase()`
  - private `write_enable()` / `wait_ready()`

- [ ] **Step 1: Extend board tests first with expected erase/program checks**

Add destructive test cases only against `PROJECT_FLASH_TEST_SECTOR_ADDRESS`:

```c
result = platform_w25q64_sector_erase(
    &flash,
    PROJECT_FLASH_TEST_SECTOR_ADDRESS);
```

After erase, read complete 4096 Byte in bounded chunks and fail if any byte is not `0xFF`。

Add single-page pattern test, e.g. 64 Byte deterministic data:

```c
for (index = 0U; index < sizeof(tx); index++) {
    tx[index] = (uint8_t)(0xA5U ^ index);
}
```

Expected before Task 3 implementation: compile failure on missing Program/Erase APIs.

- [ ] **Step 2: Implement Write Enable with WEL verification**

Add opcodes/masks:

```c
#define PLATFORM_W25Q64_CMD_WRITE_ENABLE      (0x06U)
#define PLATFORM_W25Q64_SR1_BUSY_MASK         (0x01U)
#define PLATFORM_W25Q64_SR1_WEL_MASK          (0x02U)
```

`write_enable()` flow:

```text
begin → write 0x06 → end → read SR1 → require WEL==1
```

Do not expose it publicly.

- [ ] **Step 3: Implement `wait_ready()` with timeout and 1 ms polling**

Use `platform_time` elapsed-time capability already available in Platform. If only delay API is available, maintain an elapsed counter advanced by each successful 1 ms delay rather than depending on HAL tick from BSP.

Behavior:

```c
for (elapsedMs = 0U; elapsedMs < timeoutMs; elapsedMs++) {
    result = platform_w25q64_read_status1_raw(flash, &status);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
    if ((status & PLATFORM_W25Q64_SR1_BUSY_MASK) == 0U) {
        return PLATFORM_ERR_OK;
    }
    result = platform_time_delay_ms(1U);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }
}
return PLATFORM_ERR_TIMEOUT;
```

Use 10 ms Program timeout、500 ms Sector Erase timeout、1000 ms Init Ready timeout.

- [ ] **Step 4: Implement strict Page Program**

Add `0x02` and validate:

```c
pageOffset = address % PLATFORM_W25Q64_PAGE_SIZE_BYTES;

if ((dataLength == 0U) ||
    (dataLength > PLATFORM_W25Q64_PAGE_SIZE_BYTES) ||
    (dataLength > (PLATFORM_W25Q64_PAGE_SIZE_BYTES - pageOffset))) {
    return PLATFORM_ERR_INVALID_PARAM;
}
```

Then:

```text
wait_ready
→ write_enable
→ begin
→ write 0x02 + 24-bit address
→ write data
→ end
→ wait_ready(10 ms)
```

A successful return means Flash has completed internal Program and is Ready.

- [ ] **Step 5: Implement strict 4 KiB Sector Erase**

Validate range and exact alignment:

```c
if ((sectorAddress >= PLATFORM_W25Q64_TOTAL_SIZE_BYTES) ||
    ((sectorAddress % PLATFORM_W25Q64_SECTOR_SIZE_BYTES) != 0U)) {
    return PLATFORM_ERR_INVALID_PARAM;
}
```

Then:

```text
wait_ready
→ write_enable
→ begin
→ write 0x20 + 24-bit address
→ end
→ wait_ready(500 ms)
```

Never auto-align and never add Chip Erase.

- [ ] **Step 6: Run destructive board tests and capture RTT evidence**

Expected logical evidence:

```text
[S02] erase sector=0x7FF000 result=0
[S02] erase readback all_ff=1 PASS
[S02] page program addr=0x7FF000 len=64 result=0
[S02] page compare PASS
```

The code must calculate PASS/FAIL from data; do not print PASS merely because APIs returned `OK`.

- [ ] **Step 7: Commit Task 3**

```bash
git add 03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64 \
        04_Test/Board/S02_External_Flash_Driver/s02_flash_board_test.c
git commit -m "feat: add w25q64 program and sector erase"
```

---

### Task 4: Add Continuous Cross-page Write and Boundary Tests

**Files:**
- Modify: `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_w25q64.c`
- Modify: `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_w25q64.h`
- Modify: `04_Test/Board/S02_External_Flash_Driver/s02_flash_board_test.c`

**Interfaces:**
- Consumes: Task 3 strict single-page Program.
- Produces: `platform_w25q64_write()` with automatic Page splitting but no Erase.

- [ ] **Step 1: Write the cross-page test before `write()` implementation**

After erasing the test Sector, prepare 300 Byte deterministic source data and write from `PROJECT_FLASH_TEST_SECTOR_ADDRESS + 0xF0U`。

The test expectation is:

```text
chunk1 = 16
chunk2 = 256
chunk3 = 28
```

The external behavioral assertion is 300 Byte Read Back equals source; the test must not rely only on internal chunk logs.

Expected before implementation: compile failure because `platform_w25q64_write()` is missing.

- [ ] **Step 2: Implement automatic Page splitting**

Use:

```c
while (remaining > 0U) {
    pageOffset = currentAddress % PLATFORM_W25Q64_PAGE_SIZE_BYTES;
    pageRemaining = PLATFORM_W25Q64_PAGE_SIZE_BYTES - pageOffset;
    chunkLength = (remaining < pageRemaining) ? remaining : pageRemaining;

    result = platform_w25q64_page_program(
        flash,
        currentAddress,
        currentData,
        chunkLength);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    currentAddress += (uint32_t)chunkLength;
    currentData += chunkLength;
    remaining -= chunkLength;
}
```

Validate the entire range once before the loop. Do not Erase or pre-read for `0xFF`.

- [ ] **Step 3: Add explicit negative boundary cases**

Board test must assert non-`OK` for:

```text
Read starting at 0x800000
Read crossing 0x7FFFFF
Write crossing 0x7FFFFF
page_program(address=...F0, length=32) when that crosses Page
sector_erase(0x7FF001)
```

Also assert a legal tail read ending exactly at `0x7FFFFF` returns `OK`.

- [ ] **Step 4: Re-run erase + cross-page + boundary suite**

Expected RTT summary should contain distinct results, for example:

```text
[S02] cross-page write addr=0x7FF0F0 len=300 PASS
[S02] out-of-range read rejected PASS
[S02] out-of-range write rejected PASS
[S02] atomic page-cross rejected PASS
[S02] unaligned erase rejected PASS
```

- [ ] **Step 5: Commit Task 4**

```bash
git add 03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64 \
        04_Test/Board/S02_External_Flash_Driver/s02_flash_board_test.c
git commit -m "feat: add w25q64 cross page write and boundaries"
```

---

### Task 5: Integrate S02 Board Test Entry and Verify Reset Persistence

**Files:**
- Modify: `03_Firmware/Application/OTA_APP/01_APP/app_main.c`
- Modify: `03_Firmware/Application/OTA_APP/00_Config/project_config.h`
- Modify: `04_Test/Board/S02_External_Flash_Driver/s02_flash_board_test.c`
- Modify: `03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx`

**Interfaces:**
- Consumes: complete Raw Driver V1.
- Produces: reproducible board-validation entry with RTT output, without making destructive erase/write run forever on every normal boot.

- [ ] **Step 1: Keep App orchestration thin**

`app_main.c` may own the Storage SPI Bus object and lifecycle:

```c
static platform_spi_bus_t g_storageSpiBus = PLATFORM_SPI_BUS_INITIALIZER;
```

Initialization order:

```text
service_log_init
→ existing Application basic init
→ platform_bsp_spi_construct_storage_bus
→ platform_spi_bus_lifecycle_init
→ platform_spi_bus_lifecycle_start
→ optional S02 board test entry
```

Do not put W25Q64 opcodes, erase loops or comparison logic directly in `app_main.c`。

- [ ] **Step 2: Gate destructive board testing explicitly**

Add a temporary stage configuration:

```c
#define PROJECT_S02_FLASH_BOARD_TEST_ENABLE   (1U)
```

When enabled, run the full destructive suite once per boot. Before leaving implementation for final verification/review, set it back to `0U` or otherwise ensure destructive tests are not part of the normal long-term runtime path.

- [ ] **Step 3: Add reset-persistence mode**

After the cross-page write case succeeds, the test should log the exact address/pattern expected after reset. On the next reset, run a non-destructive persistence check before the next erase.

A simple deterministic pattern is sufficient; the second-boot check must Read Back and compare actual bytes. Expected:

```text
[S02] persistence addr=0x7FF0F0 len=300 PASS
```

- [ ] **Step 4: Clean Rebuild and full board run**

Required runtime checks:

```text
JEDEC ID EF 40 17
SR1 readable
Erase 4 KiB + all-FF Read Back
Single-page Program + Compare
Cross-page 300 Byte Write + Compare
Range/alignment rejection cases
Reset persistence
```

Use RTT/EasyLogger as the primary observation channel. Use logic analyzer only when transaction-level diagnosis is needed.

- [ ] **Step 5: Disable destructive autorun after evidence is collected**

Set `PROJECT_S02_FLASH_BOARD_TEST_ENABLE` to `0U` for the normal branch state, while retaining the board-test source for reproducibility. Rebuild again and verify normal Application still starts and existing LED/log baseline remains functional.

- [ ] **Step 6: Commit Task 5**

```bash
git add 03_Firmware/Application/OTA_APP/01_APP/app_main.c \
        03_Firmware/Application/OTA_APP/00_Config/project_config.h \
        03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx \
        04_Test/Board/S02_External_Flash_Driver
git commit -m "test: add S02 external flash board validation"
```

---

### Task 6: Record Verification Evidence and Evaluate SFUD Boundary

**Files:**
- Create/Modify: `04_Test/Reports/Stages/S02_External_Flash_Driver/verification.md`
- Create: `00_Project/03_Stages/S02_External_Flash_Driver/sfud_evaluation.md`
- Modify after implementation: `00_Project/03_Stages/S02_External_Flash_Driver/handoff.md`
- Modify after implementation: `00_Project/05_Status/current_status.md`
- Modify after implementation: `PROJECT_CONTEXT.md`

**Interfaces:**
- Consumes: Task 1-5 implementation commits and actual board evidence.
- Produces: S02 verification input and explicit SFUD follow-up decision. This task does not invent PASS results before hardware testing.

- [ ] **Step 1: Run final Clean/Rebuild and record exact build result**

Record compiler result, new/existing warning distinction, and generated firmware artifact path. If new S02 warning exists, treat it as a defect rather than silently merging it into the S01 known warnings.

- [ ] **Step 2: Record board evidence case-by-case**

`verification.md` must contain actual observed values for:

```text
JEDEC ID
SR1 sample
Test Sector
Erase all-FF result
Single-page Program address/length/compare
Cross-page address/length/compare
Boundary rejection cases
Reset persistence
RTT log evidence summary
Logic analyzer evidence if used
```

Do not mark a case PASS without corresponding code check or hardware observation.

- [ ] **Step 3: Write SFUD evaluation against the proven Raw Driver baseline**

`sfud_evaluation.md` must answer concretely:

1. SFUD port requires which callbacks for SPI write/read, CS and delay;
2. existing Platform SPI/GPIO/Time can satisfy each callback without HAL leakage;
3. whether SFUD requires `transfer()` or whether current write/read transaction mapping is sufficient;
4. how SFUD erase/write semantics compare with Raw Driver V1;
5. whether S02 should integrate SFUD immediately or defer actual middleware integration to a follow-up change.

If SFUD requires a new abstraction that contradicts the approved design, stop and return to Design Role instead of forcing the change into implementation.

- [ ] **Step 4: Update handoff/status/context only from real results**

Set implementation output, commits, known issues and next action based on actual work. Normal workflow after implementation is `READY_FOR_VERIFICATION`; only Verification Role may turn real evidence into verification conclusions, and only Review Role may close S02.

- [ ] **Step 5: Commit documentation/evidence**

```bash
git add 04_Test/Reports/Stages/S02_External_Flash_Driver \
        00_Project/03_Stages/S02_External_Flash_Driver \
        00_Project/05_Status/current_status.md \
        PROJECT_CONTEXT.md
git commit -m "docs: record S02 flash driver verification inputs"
```

---

## Plan Self-check

This plan covers all approved S02 Raw Driver requirements: SPI read, SPI2 multi-instance, APB clock correction, BSP CS/Bus binding, JEDEC/SR1, Read, WEL/BUSY, Program, Sector Erase, cross-page Write, range/alignment behavior, RTT diagnostics, destructive-test containment, reset persistence and SFUD evaluation. No Chip Erase, OTA partitioning, metadata, Ymodem, Bootloader or security implementation is included.
