# S03 EEPROM Storage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现并验证 AT24C02 EEPROM Raw Driver，使 Application 能通过现有 Software I2C 完成可靠的初始化、读写、跨页处理和掉电保持测试。

**Architecture:** 保持当前轻量架构，不引入 Device Manager 或通用 Storage/NVM 框架。`platform_i2c` 只新增通用单次地址探测 `probe()`；AT24C02 Driver 负责器件地址、容量、8 Byte Page、ACK Polling 和写周期策略；Application 测试入口负责组装对象并通过 RTT + EasyLogger 输出板测证据。

**Tech Stack:** STM32F411CEU6, C, STM32 HAL underneath Platform/Impl, Software I2C, AT24C02, SEGGER RTT, EasyLogger, Keil MDK.

**Spec:** `00_Project/03_Stages/S03_EEPROM_Storage/design.md`

## Metadata

- Stage: `S03_EEPROM_Storage`
- Design Commit: `a2a77a6a01d3219f8a1a095ce922b5a81cb6d771`
- Baseline Code Commit: `b590b3cad3c04292c41130b78cfb737d3898dd30`
- Status: `COMPLETED`

## Global Constraints

- 不修改已关闭 S02 的结论或把 EEPROM 逻辑塞入 W25Q64 Driver。
- 不在 S03 引入 Firmware Metadata、OTA 状态机、Bootloader 状态机、Device Manager、`platform_storage_device_t` 或通用 NVM Manager。
- AT24C02 Driver 不直接调用 STM32 HAL GPIO，不直接操作 PB6/PB7。
- Platform I2C 公共地址参数保持 7-bit 语义；本板 AT24C02 地址为 `0x50`。
- PB6/PB7 保持 Open Drain + `GPIO_NOPULL`；外部 R50/R51 4.7 kΩ 上拉负责高电平。
- AT24C02 总容量 256 Byte，Page Size 8 Byte，`tWR` 最大 5 ms。
- ACK Polling 使用 1 ms 间隔、10 ms 软件保护窗口；NACK 仅在写后 `wait_ready()` 上下文解释为器件 Busy。
- 所有越界写必须在首个 I2C 写事务前拒绝。
- 板测统一使用 RTT + EasyLogger；逻辑分析仪作为问题定位辅助，不是正常路径的强制依赖。

---

### Task 1: Add generic Software I2C address probe

**Files:**
- Modify: `03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/i2c/platform_i2c.h`
- Modify: `03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/i2c/platform_i2c.c`

**Interfaces:**
- Consumes: 已有 Software I2C START/STOP、地址发送、总线校验、错误清理能力。
- Produces:
```c
platform_error_t platform_i2c_probe(
    platform_i2c_t *i2c,
    uint8_t address);
```

- [x] **Step 1: 在头文件中声明 `platform_i2c_probe()` 并明确 7-bit 地址及返回语义。**
- [x] **Step 2: 在 `.c` 中复用现有事务 helper，实现 `START → SLA+W → ACK/NACK → STOP`，不发送任何数据字节。**
- [x] **Step 3: 确认地址 NACK 返回 `PLATFORM_ERR_NOT_FOUND`，其它总线错误保持原错误，不被转换。**
- [x] **Step 4: 编译检查现有 I2C API 无回归。**
  - Run: `05_Tools/Scripts/build_app.bat`
  - Expected: Keil normal build succeeds with no new errors.
- [x] **Step 5: 提交本任务，并将 Commit 写入 `handoff.md`。**

### Task 2: Add AT24C02 Raw Driver core

**Files:**
- Create: `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/at24c02/platform_at24c02.h`
- Create: `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/at24c02/platform_at24c02.c`
- Modify: `03_Firmware/Application/OTA_APP/00_Config/project_config.h`
- Modify: `03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx`

**Interfaces:**
- Consumes: `platform_i2c_probe()`, `platform_i2c_write()`, `platform_i2c_write_read()`, `platform_time_delay_ms()`.
- Produces:
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

- [x] **Step 1: 创建头文件，定义 256 Byte 容量、8 Byte Page、`0x50~0x57` 地址范围、运行时对象和四个公共 API。**
- [x] **Step 2: 在 `project_config.h` 中增加本板静态地址 `PROJECT_AT24C02_I2C_ADDRESS (0x50U)`，不把板级固定地址硬编码成 Driver 唯一地址。**
- [x] **Step 3: 实现参数/初始化状态校验与 `init/deinit`。**
  - `init()` 只在 probe 成功后设置 `initialized = PLATFORM_TRUE`。
  - `deinit()` 不调用 `platform_i2c_deinit()`。
- [x] **Step 4: 实现 `read()`，用 1 Byte Word Address + `platform_i2c_write_read()` 完成 Random/Sequential Read。**
- [x] **Step 5: 实现统一范围校验，使用 `dataLength > (TOTAL_SIZE - address)`，禁止跨 `0xFF` 回绕。**
- [x] **Step 6: 将新 Driver 源文件/头文件目录加入 `OTA_APP.uvprojx` 的现有 Platform/BSP 组织，不修改 `.uvoptx` 作为功能依赖。**
- [x] **Step 7: 执行 normal build。**
  - Run: `05_Tools/Scripts/build_app.bat`
  - Expected: build succeeds; no missing include/source symbols.
- [x] **Step 8: 提交本任务，并将 Commit 写入 `handoff.md`。**

### Task 3: Implement page-aware write and ACK polling

**Files:**
- Modify: `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/at24c02/platform_at24c02.c`

**Interfaces:**
- Consumes: Task 1/2 interfaces.
- Produces: completed `platform_at24c02_write()` with automatic page splitting and bounded ready wait.

- [x] **Step 1: 增加私有 `platform_at24c02_page_write()`，单次只允许当前 8 Byte Page 内 `1..8` Byte。**
  - Build one local buffer: `[wordAddress][data...]`, maximum 9 Byte.
  - Send with `platform_i2c_write()`.
- [x] **Step 2: 增加私有 `platform_at24c02_wait_ready()`。**
  - `probe == OK` → ready.
  - `probe == NOT_FOUND` → delay 1 ms and retry.
  - any other error → return immediately.
  - 10 ms budget exhausted → `PLATFORM_ERR_TIMEOUT`.
- [x] **Step 3: 实现公共 `write()` 的 Page Split：**
```text
pageRemaining = 8 - (address % 8)
chunk = min(remaining, pageRemaining)
```
  - Full request range must be validated before first write.
  - After every page write, call `wait_ready()` before advancing.
- [x] **Step 4: 代码审查以下边界：**
  - `write(0xFF, len=1)` valid.
  - `write(0xFC, len=4)` valid.
  - `write(0xFC, len=5)` rejected before bus write.
  - `write(0x06, len=10)` splits `2 + 8`.
- [x] **Step 5: 执行 normal build 和 clean rebuild。**
  - Run normal: `05_Tools/Scripts/build_app.bat`
  - Run clean/rebuild through the repository's existing Keil build tool entry according to its supported argument/command.
  - Expected: both succeed with no new errors.
- [x] **Step 6: 提交本任务，并将 Commit 写入 `handoff.md`。**

### Task 4: Add S03 hardware verification entry using RTT + EasyLogger

**Files:**
- Modify only the existing Application composition/test entry that currently hosts board bring-up tests; inspect the current repository before editing and keep destructive EEPROM test code isolated from normal runtime behavior.
- Modify: `03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx` only if an added test source file is necessary.
- Create or Modify: S03-specific test source only if this matches current test organization; do not introduce a new test framework.

**Interfaces:**
- Consumes: Software I2C GPIO BSP constructors, `platform_i2c_init()`, AT24C02 API, RTT + EasyLogger.
- Produces: reproducible S03 board-test path with clear logs.

- [x] **Step 1: 组装 PB6/PB7 Software I2C 对象并调用 `platform_i2c_init()`。**
- [x] **Step 2: 使用 `PROJECT_AT24C02_I2C_ADDRESS` 调用 `platform_at24c02_init()`，日志输出 probe 结果和错误码。**
- [x] **Step 3: 增加单字节、页内、跨页和非页对齐跨页的 write/read/compare 测试。**
  - Cross-page required case: start `0x06`, length `10`, expected split `2 + 8`.
- [x] **Step 4: 增加边界测试。**
  - `0xFF, len=1` must pass.
  - `0xFC, len=5` must return parameter error and must not corrupt target data.
- [x] **Step 5: RTT/EasyLogger 输出每个 testcase 的 PASS/FAIL；失败至少打印 test name/address/length/error，数据不一致时打印 expected/actual。**
- [x] **Step 6: 完成 Reset Persistence 模式：写入固定测试标记后复位，再次启动时只读并确认保持。**
- [x] **Step 7: Build 并烧录真实板，记录 RTT 日志。**
- [x] **Step 8: 提交测试入口变化，并将 Commit 写入 `handoff.md`。**

### Task 5: Perform physical persistence verification and prepare verification handoff

**Files:**
- Create during Verification Role: `04_Test/Reports/Stages/S03_EEPROM_Storage/verification.md`
- Modify during implementation handoff: `00_Project/03_Stages/S03_EEPROM_Storage/handoff.md`
- Modify stage status files only according to `00_Project/WORKFLOW.md`.

**Interfaces:**
- Consumes: completed implementation and S03 board-test firmware.
- Produces: implementation handoff and later verification evidence.

- [x] **Step 1: 在真实板上执行初始化/probe、single-byte、in-page、cross-page、unaligned、boundary 测试，并保存 RTT 关键日志。**
- [x] **Step 2: 执行 Reset Persistence，确认 Reset 后 EEPROM 数据保持。**
- [x] **Step 3: 执行真正断电再上电测试，确认 Power-cycle Persistence。**
- [x] **Step 4: 执行最终 Keil normal build + clean rebuild，确认无新增错误。**
- [x] **Step 5: 更新 `handoff.md` 的 Implementation Output，记录 changed files、commits、板测结果、已知问题和任何偏差。**
- [x] **Step 6: 将阶段推进到 `READY_FOR_VERIFICATION`；不要由 Implementation Role 自行填写 PASS Verification 或关闭 Stage。**

## Final Verification

Verification Role 必须独立核对：

```text
Software I2C probe semantics         PASS
AT24C02 init/probe                   PASS
Single-byte read/write               PASS
In-page write/read                   PASS
Cross-page 0x06 + 10 bytes           PASS
Unaligned cross-page                 PASS
0xFF last-byte access                PASS
Out-of-range rejection               PASS
ACK polling bounded timeout design   PASS
Reset persistence                    PASS
Power-cycle persistence              PASS
Keil normal build                    PASS
Keil clean/rebuild                   PASS
RTT/EasyLogger evidence              PASS
Scope boundary / no OTA semantics    PASS
```

Verification evidence belongs in `04_Test/Reports/Stages/S03_EEPROM_Storage/verification.md`; build success must not substitute for real hardware persistence evidence.

## Completion Condition

Implementation tasks are complete when code is committed, normal/clean builds pass, hardware test entry is ready, `handoff.md` contains implementation output, and stage state is `READY_FOR_VERIFICATION`.

S03 itself is only closed after independent Verification and Review pass according to `00_Project/WORKFLOW.md`.
