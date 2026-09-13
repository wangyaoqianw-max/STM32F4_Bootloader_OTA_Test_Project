# S03 EEPROM Storage Handoff

## Metadata

- Stage: `S03_EEPROM_Storage`
- Status: `READY_FOR_REVIEW`
- Branch: `codex/s03-eeprom-storage`
- Baseline Commit: `b590b3cad3c04292c41130b78cfb737d3898dd30`
- Design Commit: `a2a77a6a01d3219f8a1a095ce922b5a81cb6d771`
- Plan Commit: `b67a7b7c1375522b1c74fcc5290ffb10ce5a7bb8`
- Implementation Commits: `13b1147` (Task 1), `82feca9` (Task 2), `8c45967` (Task 3), `48618ae` (Task 4), `93c93b6` (board-test cleanup)
- Verification Commit: `Not created yet`
- Review Commit: `Not created yet`

## Implementation Input

### Goal

实现并验证 AT24C02 EEPROM Raw Driver，建立 Application 可复用的小容量掉电存储基础能力：Software I2C 地址探测、AT24C02 初始化、随机/顺序读取、8 Byte Page 自动拆分写入、ACK Polling、边界保护和真实硬件持久性验证。

### Required Reading

按顺序读取：

1. `AGENTS.md`
2. `README.md`
3. `PROJECT_CONTEXT.md`
4. `00_Project/WORKFLOW.md`
5. `00_Project/02_Roadmap/development_roadmap.md`
6. `00_Project/03_Stages/S03_EEPROM_Storage/design.md`
7. `00_Project/03_Stages/S03_EEPROM_Storage/implementation_plan.md`
8. `02_Hardware/Hardware_Software_Interface/AT24C02_硬件软件接口参考.md`
9. `03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/i2c/platform_i2c.h/.c`
10. `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/w25q64/platform_w25q64.h/.c`（编码风格参考）
11. `03_Firmware/Application/OTA_APP/00_Config/project_config.h`
12. `03_Firmware/Application/OTA_APP/04_Impl/impl_bsp/impl_platform_bsp_gpio.c`
13. `03_Firmware/Application/OTA_APP/Core/Src/gpio.c`

### Confirmed Hardware / Software Facts

- U8 = AT24C02，256 Byte；
- Page Size = 8 Byte；
- Word Address = 8 bit；
- A0/A1/A2 = GND → 7-bit address = `0x50`；
- WP = GND → 永久允许写；
- PB6 = SCL，PB7 = SDA；
- SCL/SDA 外部各有 4.7 kΩ 上拉至 `FLASH_VCC`；
- PB6/PB7 当前生成配置为 Open Drain + `GPIO_NOPULL` + initial HIGH；
- Software I2C 当前半周期 5 us，名义约 100 kHz；
- AT24C02 `tWR` 最大 5 ms；
- Platform I2C API 使用 7-bit address；
- I2C Bus 可能为共享总线，AT24C02 Driver 不得 deinit 底层 Bus。

### Frozen Design Decisions

- 新增通用 `platform_i2c_probe(i2c, address)`，只执行一次 `START → SLA+W → ACK/NACK → STOP`；
- probe ACK → `PLATFORM_ERR_OK`；地址 NACK → `PLATFORM_ERR_NOT_FOUND`；其它错误原样传播；
- AT24C02 只暴露 `init/deinit/read/write` 四个公共 API；
- 公共地址参数使用 `uint32_t`，内部校验后转换为 8-bit Word Address；
- `read()` 使用 `platform_i2c_write_read()`，读取不按 8 Byte Page 拆分；
- `write()` 自动按 8 Byte Page 拆分，上层不处理 Page；
- 私有 Page Write 使用最大 9 Byte 临时缓冲 `[wordAddress][data...]`；
- 每个 Page Write 后使用 ACK Polling；轮询间隔 1 ms、软件保护窗口 10 ms；
- `NOT_FOUND` 只在写后 `wait_ready()` 中解释为 Busy；其它 probe 错误立即返回；
- 本板地址 `0x50` 放入 `project_config.h`，不增加只为保存一个地址的 AT24C02 BSP Wrapper；
- 不引入通用 NVM、Storage Device、Device Manager；架构统一工作延期到主项目完成后的专项重构。

### Allowed Changes

按照 implementation plan，可修改：

- `03_Platform/platform_mcu/i2c/platform_i2c.h/.c`；
- 新增 `03_Platform/platform_bsp/at24c02/platform_at24c02.h/.c`；
- `00_Config/project_config.h`；
- `MDK-ARM/OTA_APP.uvprojx`；
- 必要的 Application/S03 板测入口；
- 当前阶段 `handoff.md`、状态同步文档；
- Verification Role 后续可创建 `04_Test/Reports/Stages/S03_EEPROM_Storage/verification.md`。

### Prohibited Changes

- 不修改 S02 已关闭结论；
- 不修改 Vendor 原始库；
- 不把 AT24C02 Driver 直接绑定 HAL GPIO；
- 不将 Firmware Metadata / OTA 状态写入 Raw Driver；
- 不引入 CRC/双副本/Sequence/Journal；
- 不实现 Device Manager、Storage Device 或通用 NVM Manager；
- 不增加 Hardware I2C 或 RTOS 互斥；
- 不把 W25Q64 当作 S03 EEPROM 替代实现；
- 不用伪造数据字节的普通 I2C write 代替 probe；
- 不把固定 5 ms delay 作为唯一写完成判定；
- 不在缺少真实硬件证据时宣称 S03 PASS/CLOSED。

### Acceptance Criteria

1. `platform_i2c_probe()` 能准确区分 ACK、地址 NACK 和真实总线错误；
2. AT24C02 init 通过 probe 后才置 initialized；
3. read/write 地址和长度边界正确；
4. Page Write 和跨页拆分正确；
5. ACK Polling 有界，不无限等待；
6. 单字节、页内、跨页、非页对齐写入均能正确读回；
7. `0xFF` 最后字节合法，越界请求在事务前拒绝；
8. Reset Persistence 通过；
9. Power-cycle Persistence 通过；
10. Keil normal build + clean/rebuild 通过；
11. RTT + EasyLogger 提供可诊断板测证据；
12. 无范围外架构扩张。

### Required Verification

板测统一通过 SEGGER RTT + EasyLogger 输出，至少包括：

```text
Init/Probe
Single Byte
In-page Write
Cross-page Write (0x06 + 10 Byte → 2 + 8)
Unaligned Cross-page
0xFF Last Byte
Out-of-range Reject
Reset Persistence
Power-cycle Persistence
```

失败日志至少包含 testcase、address、length、error；compare failure 增加 expected / actual。

## Implementation Output

- Status: `READY_FOR_VERIFICATION`

### Completed Work

- Task 1 completed: added the generic single-attempt Software I2C address probe.
- The probe reuses the existing transaction start, address-send, cleanup and STOP helpers.
- Address NACK returns `PLATFORM_ERR_NOT_FOUND`; no data byte is transmitted.
- Task 2 completed: added the AT24C02 Raw Driver lifecycle and Random/Sequential Read path.
- Added the board address configuration `PROJECT_AT24C02_I2C_ADDRESS (0x50U)`.
- Added the missing Keil project entries for Software I2C, its microsecond delay implementation and the AT24C02 driver.
- Task 3 completed: added 8 Byte Page Write splitting and bounded ACK Polling.
- Full request ranges are validated before the first physical write; each page write uses a maximum 9 Byte buffer.
- ACK Polling retries only `PLATFORM_ERR_NOT_FOUND` at 1 ms intervals and returns `PLATFORM_ERR_TIMEOUT` after 10 ms.
- Task 4 completed: added an isolated S03 RTT + EasyLogger board-test entry for hardware verification.
- After board testing, the temporary test source was moved to `04_Test/Board/S03_EEPROM_Storage` and removed from the production Application startup and Keil `OTA_APP` target.
- The retained board-test source covers single-byte, in-page, `0x06 + 10 Byte` cross-page, unaligned cross-page, `0xFF`, out-of-range preservation and persistence-marker checks.
- User-provided RTT evidence shows successful init/probe, all read/write and boundary tests, `power-cycle-persistence: PASS`, `automated suite: PASS error=0`, and board-test result `0`.
- The direct `reset-persistence: PASS` line was not retained because RTT was reconnected after power-up; the user confirmed the Reset and real power-cycle sequence.
- The production Application no longer contains or invokes the destructive EEPROM board test.

### Changed Files

- `03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/i2c/platform_i2c.h`
- `03_Firmware/Application/OTA_APP/03_Platform/platform_mcu/i2c/platform_i2c.c`
- `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/at24c02/platform_at24c02.h`
- `03_Firmware/Application/OTA_APP/03_Platform/platform_bsp/at24c02/platform_at24c02.c`
- `03_Firmware/Application/OTA_APP/00_Config/project_config.h`
- `03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx`
- `04_Test/Board/S03_EEPROM_Storage/app_s03_eeprom_test.h`
- `04_Test/Board/S03_EEPROM_Storage/app_s03_eeprom_test.c`
- `04_Test/Reports/Stages/S03_EEPROM_Storage/verification.md`
- Stage context files updated for branch `codex/s03-eeprom-storage` and status `READY_FOR_REVIEW`.

### Deviations From Plan

- 初始板测入口暂放在 `03_Firmware/Application/OTA_APP/06_Test` 并临时接入 Keil 工程；板测完成后按仓库统一约定迁移至 `04_Test/Board/S03_EEPROM_Storage`，并从生产工程移除。

### Verification Results

- `git diff --check`: PASS before cleanup commit.
- Keil build with temporary board-test enable: PASS, 0 errors, 0 warnings; this temporary configuration was used only for board testing.
- User-provided RTT board evidence: init/probe, read/write, cross-page, unaligned, last-byte, out-of-range and persistence checks completed with PASS; `power-cycle-persistence` and the automated suite are PASS.
- Final production build after removing the test entry: 0 errors; 8 warnings are pre-existing in GPIO, W25Q64, FreeRTOS Adapter and Vendor EasyLogger sources; no S03 test source was compiled.
- XML/project audit after cleanup: PASS; formal AT24C02 and Software I2C sources remain in the Keil target, while the board-test source is absent.
- Code verification: PASS for build and static implementation checks.
- Hardware verification: PASS based on user-provided real-board RTT evidence, with the missing direct Reset Persistence line documented above.
- Verification Role report: `04_Test/Reports/Stages/S03_EEPROM_Storage/verification.md`, status `PASS`.

### Known Issues

Implementation and requested board testing are complete. Verification is recorded as `PASS`; the destructive board-test source is retained only under `04_Test/Board/S03_EEPROM_Storage` and is not part of the production Application or Keil target. Review Role remains the next gate.

### Review Focus

实现完成后重点检查：

- probe 是否真的不发送数据；
- NACK 语义是否只在正确上下文解释；
- 全请求范围是否在第一笔写前完成校验；
- Page Split 是否处理非对齐首尾页；
- ACK Polling 是否有界；
- Driver 是否错误拥有/释放共享 I2C Bus；
- destructive test 是否已退出正常生产启动路径；
- RTT 板测记录中的 Reset Persistence 直接日志是否需要补充归档。
