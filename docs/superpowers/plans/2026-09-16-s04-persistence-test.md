# S04 Reset and Power-cycle Persistence Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增一个只读 S04 Persistence 板测入口和主机自动化，使 Reset Persistence 与 Power-cycle Persistence 能在不改写 Slot 或 Metadata 的前提下完成真实板级回归。

**Architecture:** 测试固件在每次启动后初始化现有 Storage Service，只读验证 Slot B、读取 Metadata 双副本并周期性输出固定格式快照。主机解析器负责从 RTT/GDB 证据中提取并比对事件前后的快照；MCU Reset 由 GDB 发出，Power-cycle 由用户操作电源键。测试入口通过编译期开关临时接入 Application，默认关闭并保持生产路径不变。

**Tech Stack:** C99、Keil MDK/ARMCC、FreeRTOS、现有 Platform/Service Firmware、SEGGER RTT、Windows Batch、Python unittest。

**Spec:** `00_Project/03_Stages/S04_Firmware_Image_Storage/design.md`；延期回归范围见 `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`。

## Global Constraints

- 测试入口只允许读取 W25Q64 和 AT24C02，不得擦除、写入、提交或破坏任何持久化数据。
- Slot B 固定使用 S04 合同地址和 `firmware_storage_validate_image()` 完整校验。
- Metadata 必须通过现有 `firmware_storage_load_metadata()` 选择有效副本，并额外读取 Copy A/B 验证有效性与序列号。
- GDB 和 RTT Logger 不得同时占用同一个 J-Link Probe。
- `PROJECT_ENABLE_S04_PERSISTENCE_BOARD_TEST` 默认值必须为 `0U`。
- 测试代码保留在 `04_Test/Board`；正式 Application 默认不启动该入口。
- 代码验证和真实硬件验证必须分别记录，不能以编译或 Host Test 代替板测。

---

### Task 1: 定义只读 Persistence 快照接口

**Files:**
- Create: `04_Test/Board/S04_Firmware_Image_Storage/app_s04_persistence_test.h`
- Create: `04_Test/Board/S04_Firmware_Image_Storage/app_s04_persistence_test.c`
- Create: `04_Test/Host/S04_Firmware_Image_Storage/s04_persistence_log.py`
- Create: `04_Test/Host/S04_Firmware_Image_Storage/test_s04_persistence_log.py`

**Interfaces:**
- Consumes: `firmware_storage_validate_image()`、`firmware_storage_load_metadata()`、`firmware_metadata_decode_committed()`、现有 W25Q64/AT24C02 BSP 初始化接口和 `service_log`。
- Produces: `platform_error_t app_s04_persistence_test_run(void)`；每次启动及每秒输出一条 `[S04-PERSIST] SNAPSHOT ...` 记录。

- [x] **Step 1: Write the failing Host parser test for a stable snapshot.**
  Create a Python parser test with one literal valid log line and one changed-sequence log line. The test must require the parser to return the selected copy, sequence, image CRC, and slot state from the valid line, and reject a changed post-event snapshot.

- [x] **Step 2: Run the parser test and confirm the expected missing-parser failure.**
  Run:
  ```powershell
  python -B -m unittest discover -s .\04_Test\Host\S04_Firmware_Image_Storage -p "test_*.py" -v
  ```
  Expected: FAIL because the new parser module does not exist yet.

- [x] **Step 3: Implement the minimal host parser.**
  Implement `parse_snapshot_line(line)`, `compare_snapshots(before, after)`, and `evaluate_snapshots(lines)` with no dependency on the board or J-Link. Parse only the fixed `[S04-PERSIST] SNAPSHOT` contract and return a structured result with `status`, `reason`, `before`, and `after`.

- [x] **Step 4: Implement the minimal board snapshot contract.**
  Initialize the existing Storage SPI, W25Q64, Software I2C and AT24C02 exactly as the S04 board test does. Do not call `firmware_storage_erase_slot()`, `firmware_storage_write_payload()`, `firmware_storage_write_header()` or `firmware_storage_commit_metadata()`.

  Each snapshot must include:
  ```text
  [S04-PERSIST] SNAPSHOT image_validation=<value> image_size=<value> image_crc=0x<8 hex digits> metadata_a_valid=<0|1> metadata_b_valid=<0|1> selected_copy=<0|1|2> sequence=<value> active_slot=<value> confirmed_slot=<value> slot_a_state=<value> slot_b_state=<value> confirmed_version=<major>.<minor>.<patch>
  ```

  Keep the test task blocked with `platform_time_delay_ms(1000U)` between reads after the first snapshot so the host listener can capture a baseline before the user resets or power-cycles the board.

- [x] **Step 5: Run the parser test and confirm it passes.**
  Run the same unittest command. Expected: all parser tests PASS, with changed sequence or image CRC reported as a mismatch rather than accepted.

- [x] **Step 6: Commit the board snapshot contract and parser test.**
  ```bash
  git add 04_Test/Board/S04_Firmware_Image_Storage 04_Test/Host/S04_Firmware_Image_Storage
  git commit -m "test(s04): add read-only persistence snapshot"
  ```

### Task 2: Add host listener and Reset/Power-cycle orchestration

**Files:**
- Create: `05_Tools/Scripts/s04_persistence_test.py`
- Create: `05_Tools/Scripts/s04_persistence_test.bat`
- Modify: `05_Tools/Scripts/README.md`
- Modify: `05_Tools/README.md`

**Interfaces:**
- Consumes: RTT Logger output, S04 snapshot lines, local toolchain paths, and `ARM_GDB`/`JLINK_GDB_SERVER` settings.
- Produces: `06_Output/Logs/S04_reset_persistence.log`, `06_Output/Logs/S04_power_cycle_persistence.log`, and an explicit PASS/FAIL result.

- [x] **Step 1: Implement the listener lifecycle.**
  The Python controller must start RTT Logger before the event, preserve segmented listener logs, detect the first baseline snapshot, and keep retrying the Logger after target loss or stalled output until the post-power-on snapshot is captured or the bounded timeout expires. It must never run Flash or erase commands.

- [x] **Step 2: Implement Reset mode with GDB ownership.**
  Start J-Link GDB Server and ARM GDB first, load symbols with `file` only, issue `monitor reset`, then `continue&`. After the post-reset snapshot is observable, execute `disconnect` and `quit`; only after J-Link is released may the script start RTT Logger for buffered evidence. No GDB `load` command is permitted.

- [x] **Step 3: Implement Power-cycle mode with manual user handoff.**
  Start the RTT listener and print a ready marker after a baseline snapshot. Keep the listener active while the user turns power off and on. Recover Logger exit or no-data stalls, compare the first valid post-power-on snapshot, and return PASS/FAIL.

- [x] **Step 4: Run Host tests and static script checks.**
  Run the unittest command above and the existing tool contract checks:
  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Debug\GDB\test_gdb_automation.ps1
  powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Debug\CmBacktrace\test_fault_diagnostics.ps1
  ```
  Expected: all tests PASS; no script may contain a persistence-mode `loadfile`, erase, or metadata commit command.

- [x] **Step 5: Commit host automation.**
  ```bash
  git add 05_Tools/Scripts 05_Tools/README.md 05_Tools/Scripts/README.md 04_Test/Host/S04_Firmware_Image_Storage
  git commit -m "test(s04): automate persistence regression capture"
  ```

### Task 3: Temporarily integrate the board test into Keil

**Files:**
- Modify: `03_Firmware/Application/OTA_APP/00_Config/project_config.h`
- Modify: `03_Firmware/Application/OTA_APP/01_APP/app_main.c`
- Modify: `03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx`

**Interfaces:**
- Consumes: `app_s04_persistence_test_run()` and the compile-time switch.
- Produces: a buildable temporary test firmware with the test switch enabled locally for board verification and default disabled in the committed source.

- [x] **Step 1: Add the default-off switch and exclusive test selection.**
  Define `PROJECT_ENABLE_S04_PERSISTENCE_BOARD_TEST` as `0U` when absent. Ensure S04 Persistence and S05 YMODEM test switches cannot both be enabled, and keep the normal LED path unchanged when both are disabled.

- [x] **Step 2: Add the two Persistence source files to the Keil test group.**
  Keep the files under `04_Test/Board`; do not move them into production Application directories. The `.uvprojx` path must point to the board-test source files and the test group name must identify them as temporary S04 test assets.

- [x] **Step 3: Build with the default switch disabled.**
  Run:
  ```powershell
  .\05_Tools\Scripts\build_app.bat
  ```
  Expected: normal OTA_APP build succeeds without starting the persistence test.

- [x] **Step 4: Enable the temporary switch only for the board-test build and rebuild.**
  Use the existing Keil project configuration mechanism, record the exact switch value in the test log, and confirm the generated AXF/HEX is the persistence-test image. Do not commit a default-on switch.

- [x] **Step 5: Commit Keil integration.**
  ```bash
  git add 03_Firmware/Application/OTA_APP/00_Config/project_config.h 03_Firmware/Application/OTA_APP/01_APP/app_main.c 03_Firmware/Application/OTA_APP/MDK-ARM/OTA_APP.uvprojx
  git commit -m "test(s04): integrate temporary persistence board target"
  ```

### Task 4: Execute hardware regressions and record evidence

**Files:**
- Modify: `04_Test/Reports/Stages/S04_Firmware_Image_Storage/verification.md`
- Modify: `00_Project/03_Stages/S04_Firmware_Image_Storage/handoff.md`
- Modify: `PROJECT_CONTEXT.md`
- Modify: `00_Project/05_Status/current_status.md`

**Interfaces:**
- Consumes: board-test firmware, J-Link GDB/RTT automation, and user power-button operation.
- Produces: separate Reset Persistence and Power-cycle Persistence evidence with exact log paths and a final status.

- [x] **Step 1: Establish a valid persistent baseline once.**
  Use the already verified S04/S05 image and Metadata state, or execute the destructive provisioning flow before this test. After the baseline snapshot is captured, do not build, flash, erase, send YMODEM, or commit Metadata during either persistence experiment.

- [x] **Step 2: Execute Reset Persistence.**
  Start the GDB-controlled test flow, verify the baseline snapshot, issue `monitor reset` and `continue&`, wait for the post-reset snapshot, release GDB with `disconnect`/`quit`, then collect RTT evidence. PASS requires all snapshot fields to match.

- [x] **Step 3: Execute Power-cycle Persistence.**
  Start the RTT listener, wait for the ready marker, have the user switch board power off, wait 3–5 seconds, switch power on, and allow the bounded capture window to collect the first post-power-on snapshot. PASS requires all snapshot fields to match.

- [x] **Step 4: Verify no write occurred during both tests.**
  Confirm logs contain no erase/program/metadata-commit operation and that the test source calls only read/validate APIs.

- [x] **Step 5: Record results and clean up the temporary target.**
  Mark each item `PASS`, `FAIL`, or `PENDING` based on actual evidence. Restore the default test switch to `0U`; remove the temporary test files from the formal Keil target while retaining them under `04_Test/Board` if the project needs future reruns.

- [x] **Step 6: Run final verification and commit.**
  Run `git diff --check`, Host tests, Keil normal build, and the applicable hardware capture review. Update the reports with code/hardware status separately, then commit the documentation and cleanup as one documentation/test-state change.
