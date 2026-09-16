# S05B Tools Toolkit Reuse Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将当前 `05_Tools` 从项目专用脚本集合重构为配置驱动、职责分离、入口稳定、可扩展和可跨工程复用的 PC 工具框架，同时保持现有 Build / Flash / RTT / GDB / Fault / Firmware / Ymodem 能力不退化。

**Architecture:** 采用 `Config + Core + Adapters + Workflows + Project Tests + Legacy Wrappers`。Core 只承载配置、路径、进程、日志、锁和清理；Adapter 按 `Build/Keil`、`Probe/JLink`、`Debug/GDB` 变化轴拆分；Workflow 表达 build/flash/run/rtt/snapshot/fault 等开发动作；`toolkit.bat` 作为统一 Human / Agent Router，旧 `Scripts/*.bat` 仅保留兼容转发。

**Tech Stack:** Windows CMD/BAT、Windows PowerShell 5.1+、Python 3、Keil uVision 5、SEGGER J-Link Commander / RTT Logger / GDB Server、GNU Arm GDB。

**Spec:** `00_Project/03_Stages/S05B_Toolkit_Reuse/design.md`

## Global Constraints

- 不修改 `03_Firmware` 生产代码、Flash Layout、Firmware Image Contract、RTOS 接口或 MCU 侧 Ymodem 实现。
- 第一版只支持已验证的 `STM32 + Keil + J-Link + GNU Arm GDB` 工具组合，不实现 GCC/CMake、OpenOCD、GD32 Adapter。
- 不修改系统 `PATH`，不自动安装第三方工具。
- `toolchain.local.bat` 与 `project.local.bat` 为本机文件，不得提交；`project.defaults.bat` 为工程事实，必须提交。
- 通用 Core / Adapter / Workflow 不允许硬编码 `OTA_APP`、`STM32F411CE`、固定 COM 口、S04 专用符号或本机绝对路径。
- 旧 `Scripts` 入口在 S05B 结束时继续可用，但不得与新 Workflow 保留两套核心业务实现。
- 保持 S05A 已验证 GDB 合同：`halt -> detach -> quit` 后 MCU 保持暂停；`resume -> continue& -> disconnect -> quit` 后 MCU 继续运行；禁止 GDB `load`。
- 同一时刻只能有一个工具持有 J-Link；只清理当前 Workflow 自己启动的进程。
- 只创建实际使用的目录和文件；不为了“架构完整”新增空目录。
- 每个 Task 完成后运行其指定验证并独立提交；验证失败时不得继续后续迁移。

---

## Planned File Map

新建或正式化：

```text
05_Tools/
├─ Config/
│  ├─ toolchain.local.example.bat
│  ├─ project.defaults.bat
│  └─ project.local.example.bat
├─ Core/
│  ├─ Config.ps1
│  ├─ Path.ps1
│  ├─ Process.ps1
│  ├─ Logging.ps1
│  ├─ Lock.ps1
│  └─ Toolkit.Core.psm1
├─ Adapters/
│  ├─ Build/Keil/keil_build.ps1
│  ├─ Probe/JLink/jlink_flash.ps1
│  ├─ Probe/JLink/jlink_rtt.ps1
│  └─ Debug/GDB/gdb_session.ps1
├─ Workflows/
│  ├─ Application/build.ps1
│  ├─ Application/flash.ps1
│  ├─ Application/rtt.ps1
│  ├─ Application/run.ps1
│  ├─ Debug/snapshot.ps1
│  └─ Debug/fault_capture.ps1
├─ Contracts/
│  ├─ Core/test_core.ps1
│  ├─ Application/test_application_workflows.ps1
│  ├─ Debug/test_debug_workflows.ps1
│  └─ Compatibility/test_legacy_entries.ps1
├─ Tests/S04_Persistence/
├─ toolkit.ps1
├─ toolkit.bat
└─ README.md
```

现有 `Firmware/`、`Ymodem/`、`TeraTerm/` 第一轮保留原路径；现有 `Debug/GDB/*.gdb`、CmBacktrace 契约测试可以继续保留，只有 S04 专用资产按 Task 6 收口。

---

### Task 1: 建立三层配置合同与 Core 基础模块

**Files:**
- Modify: `.gitignore`
- Modify: `05_Tools/Config/toolchain.local.example.bat`
- Create: `05_Tools/Config/project.defaults.bat`
- Create: `05_Tools/Config/project.local.example.bat`
- Create: `05_Tools/Core/Config.ps1`
- Create: `05_Tools/Core/Path.ps1`
- Create: `05_Tools/Core/Logging.ps1`
- Create: `05_Tools/Core/Toolkit.Core.psm1`
- Create: `05_Tools/Contracts/Core/test_core.ps1`

**Interfaces:**
- Produces: `Import-ToolkitConfiguration`, `Resolve-ToolkitProjectPath`, `Assert-ToolkitRequiredValue`, `New-ToolkitLogDirectory`。
- `Import-ToolkitConfiguration -ToolsRoot <path>` 返回键值对象，加载顺序必须为 `project.defaults -> project.local override -> toolchain.local`。

- [ ] **Step 1: 先写配置合同测试**

测试至少覆盖：

```powershell
$config = Import-ToolkitConfiguration -ToolsRoot $fixtureRoot
Assert-Equal $config.PROJECT_KEIL_TARGET "FixtureTarget"
Assert-Equal $config.JLINK_SPEED "4000"
Assert-Equal $config.KEIL_UV4 "C:\fake\UV4.exe"
```

同时验证缺少 `toolchain.local.bat`、缺少必需变量、相对路径解析和本机绝对路径不出现在 committed example 中。

- [ ] **Step 2: 运行测试并确认当前结构失败**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Contracts\Core\test_core.ps1
```

Expected: FAIL，原因应为新 Core/config 文件尚不存在。

- [ ] **Step 3: 拆分配置**

`toolchain.local.example.bat` 只保留：

```bat
set "KEIL_UV4="
set "JLINK_EXE="
set "JLINK_GDB_SERVER="
set "JLINK_RTT_LOGGER="
set "ARM_GDB="
set "PYTHON_EXE="
set "TERA_TERM_EXE="
```

`project.defaults.bat` 写入当前仓库工程事实：Keil project/target、AXF/HEX/BIN 相对路径、输出/日志目录、`STM32F411CE`、`SWD` 等。

`project.local.example.bat` 只提供 `SERIAL_PORT`、`GDB_PORT`、`JLINK_SPEED`、`JLINK_RTT_CHANNEL` 等机器覆盖项。

`.gitignore` 增加：

```text
05_Tools/Config/project.local.bat
```

- [ ] **Step 4: 实现最小 Core Config/Path/Logging**

`Import-ToolkitConfiguration` 通过 `cmd.exe /d /s /c` 调用三个 BAT 配置并解析最终环境，不把本机路径复制进脚本；`Resolve-ToolkitProjectPath` 只负责 Project Root + 相对路径解析。

- [ ] **Step 5: 重新运行 Core 合同测试**

Expected: PASS。

- [ ] **Step 6: 提交**

```bash
git add .gitignore 05_Tools/Config 05_Tools/Core 05_Tools/Contracts/Core
git commit -m "refactor(tools): establish reusable configuration core"
```

---

### Task 2: 抽取 Process / Lock 公共能力

**Files:**
- Create: `05_Tools/Core/Process.ps1`
- Create: `05_Tools/Core/Lock.ps1`
- Modify: `05_Tools/Core/Toolkit.Core.psm1`
- Modify: `05_Tools/Contracts/Core/test_core.ps1`

**Interfaces:**
- Produces: `Invoke-ToolkitProcess`, `Stop-ToolkitOwnedProcess`, `Test-ToolkitTcpPort`, `Enter-ToolkitLock`, `Exit-ToolkitLock`。
- `Invoke-ToolkitProcess` 必须返回至少 `ExitCode / Stdout / Stderr / TimedOut / ProcessId`，并只清理自己创建的进程。

- [ ] **Step 1: 将现有 GDB 脚本中的重复行为写成失败测试**

覆盖：进程正常退出、非零退出、超时 kill、TCP port probe、owned-process cleanup、锁冲突拒绝。

- [ ] **Step 2: 运行测试确认失败**

Expected: FAIL，因为 Process/Lock API 尚不存在。

- [ ] **Step 3: 从现有 `gdb_runtime_snapshot.ps1` 的稳定实现抽取最小公共逻辑**

不要改变其已验证语义；只抽取：参数转义、captured process、timeout、stdout/stderr、TCP ready、owned process cleanup、lock file。

- [ ] **Step 4: 运行 Core 合同测试**

Expected: PASS，且测试结束后无残留测试进程和 lock file。

- [ ] **Step 5: 提交**

```bash
git add 05_Tools/Core 05_Tools/Contracts/Core
git commit -m "refactor(tools): extract process and lock primitives"
```

---

### Task 3: 建立 Build / Probe Adapter 与 Application Workflows

**Files:**
- Create: `05_Tools/Adapters/Build/Keil/keil_build.ps1`
- Create: `05_Tools/Adapters/Probe/JLink/jlink_flash.ps1`
- Create: `05_Tools/Adapters/Probe/JLink/jlink_rtt.ps1`
- Create: `05_Tools/Workflows/Application/build.ps1`
- Create: `05_Tools/Workflows/Application/flash.ps1`
- Create: `05_Tools/Workflows/Application/rtt.ps1`
- Create: `05_Tools/Workflows/Application/run.ps1`
- Create: `05_Tools/Contracts/Application/test_application_workflows.ps1`

**Interfaces:**
- `keil_build.ps1` consumes effective config and returns adapter result;不得写死 `OTA_APP`。
- `jlink_flash.ps1 -Mode run|prepare` preserves current J-Link command semantics。
- `jlink_rtt.ps1` owns RTT Logger invocation but not Application orchestration。
- `run.ps1` orchestration fixed as `Build -> Flash(run) -> RTT Capture`。

- [ ] **Step 1: 写 Adapter/Workflow Host Contract Test**

使用 temporary fake executable 记录 argv，不连接真实 J-Link。验证工程文件、Target、HEX、Device、IF、Speed、RTT channel 均来自 Config。

- [ ] **Step 2: 运行测试确认失败**

Expected: FAIL because adapters/workflows do not exist。

- [ ] **Step 3: 迁移现有 `build_app.bat` 与 `flash_app.bat` 核心逻辑**

保留现有行为：Keil exit `0/1/2/3` 判定；Flash `run|prepare`；J-Link temporary command file 必须在 finally/cleanup 删除。

- [ ] **Step 4: 迁移 RTT 与 run cycle**

`run.ps1` 只能调用三个 Workflow/Adapter 接口，不复制 Keil/J-Link 参数拼装。

- [ ] **Step 5: 运行 Host Contract Test**

Expected: PASS；扫描新文件确认不存在 `OTA_APP`、固定 `COM9`、本机绝对路径。

- [ ] **Step 6: 使用当前真实配置执行 Application smoke test**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Workflows\Application\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Workflows\Application\flash.ps1 -Mode run
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Workflows\Application\rtt.ps1 -Seconds 10
```

若执行环境没有 USB/J-Link，只记录 hardware smoke 为 `PENDING`，不得伪造 PASS；Host contract 必须先 PASS。

- [ ] **Step 7: 提交**

```bash
git add 05_Tools/Adapters/Build 05_Tools/Adapters/Probe 05_Tools/Workflows/Application 05_Tools/Contracts/Application
git commit -m "refactor(tools): add application workflows and adapters"
```

---

### Task 4: 重构 GDB Adapter 与 Debug Workflows

**Files:**
- Create: `05_Tools/Adapters/Debug/GDB/gdb_session.ps1`
- Create: `05_Tools/Workflows/Debug/snapshot.ps1`
- Create: `05_Tools/Workflows/Debug/fault_capture.ps1`
- Create: `05_Tools/Contracts/Debug/test_debug_workflows.ps1`
- Modify only after tests pass: `05_Tools/Scripts/gdb_runtime_snapshot.ps1`
- Modify only after tests pass: `05_Tools/Scripts/gdb_fault_capture.ps1`

**Interfaces:**
- GDB Adapter owns GDB Server startup/readiness/client lifecycle, not snapshot/fault business rules。
- Snapshot Workflow owns `resume|halt` selection and associated `.gdb` command file。
- Fault Workflow owns `capture|trigger` selection and fault evidence collection order。

- [ ] **Step 1: 写 GDB 合同测试**

必须检查：

```text
resume script contains continue&
resume script does not contain detach
halt script contains detach
halt script does not contain continue&
all scripts reject GDB load
one workflow owns exactly one GDB Server
failure path cleans only owned PIDs
```

- [ ] **Step 2: 运行现有 S05A 契约测试作为迁移基线**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Debug\GDB\test_gdb_automation.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Debug\test_tool_sequence.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Debug\CmBacktrace\test_fault_diagnostics.ps1
```

Expected: baseline PASS before migration。

- [ ] **Step 3: 将进程生命周期改为调用 Core**

删除 `gdb_runtime_snapshot.ps1` / `gdb_fault_capture.ps1` 中重复的 process/log/TCP cleanup 实现；Debug Workflow 只保留调试语义。

- [ ] **Step 4: 运行新旧全部 Debug Contract Test**

Expected: PASS；禁止 `load`、resume/halt 退出合同保持不变。

- [ ] **Step 5: 有真实板卡时回归 snapshot**

```text
snapshot halt   -> MCU remains halted
snapshot resume -> MCU continues running
```

重新连接验证 `uwTick` 行为；无板卡环境则明确留给 Verification Role。

- [ ] **Step 6: 提交**

```bash
git add 05_Tools/Adapters/Debug 05_Tools/Workflows/Debug 05_Tools/Contracts/Debug 05_Tools/Scripts/gdb_*.ps1
git commit -m "refactor(tools): separate gdb adapter from debug workflows"
```

---

### Task 5: 增加统一 toolkit Router 并切换 Legacy Entries

**Files:**
- Create: `05_Tools/toolkit.ps1`
- Create: `05_Tools/toolkit.bat`
- Modify: `05_Tools/Scripts/build_app.bat`
- Modify: `05_Tools/Scripts/flash_app.bat`
- Modify: `05_Tools/Scripts/rtt_capture.bat`
- Modify: `05_Tools/Scripts/run_app_cycle.bat`
- Modify: `05_Tools/Scripts/gdb_runtime_snapshot.bat`
- Modify: `05_Tools/Scripts/gdb_fault_capture.bat`
- Create: `05_Tools/Contracts/Compatibility/test_legacy_entries.ps1`

**Interfaces:**
- Public commands:

```text
toolkit.bat build
toolkit.bat flash [run|prepare]
toolkit.bat run [rtt_seconds]
toolkit.bat rtt [seconds]
toolkit.bat snapshot [halt|resume]
toolkit.bat fault [capture|trigger]
```

- Stable toolkit exit classes:

```text
0  SUCCESS
10 CONFIG_ERROR
20 BUILD_ERROR
30 PROBE_ERROR
40 DEBUG_ERROR
50 TRANSFER_ERROR
60 TEST_ERROR
```

- [ ] **Step 1: 写 Router/Legacy Contract Test**

验证命令路由、未知命令返回 config/usage error、legacy wrapper 不再含 Keil/J-Link/GDB 核心命令。

- [ ] **Step 2: 实现 `toolkit.ps1` Router 和 `.bat` shim**

`toolkit.bat` 只定位自身目录并调用 PowerShell Router；Router 只做参数校验、Workflow 选择和稳定 Exit Code 映射。

- [ ] **Step 3: 将旧 BAT 改为薄包装**

示例：

```bat
@echo off
call "%~dp0..\toolkit.bat" build
exit /b %ERRORLEVEL%
```

对应参数必须原样透传，不能在 wrapper 重新拼装工具参数。

- [ ] **Step 4: 运行 Compatibility Contract Test**

Expected: 新旧入口调用同一 Workflow，返回语义一致。

- [ ] **Step 5: 提交**

```bash
git add 05_Tools/toolkit.* 05_Tools/Scripts 05_Tools/Contracts/Compatibility
git commit -m "refactor(tools): add unified toolkit router"
```

---

### Task 6: 收口 S04 Persistence 项目测试扩展

**Files:**
- Create/Move: `05_Tools/Tests/S04_Persistence/`
- Move implementation from: `05_Tools/Scripts/s04_persistence_test.py`
- Move S04-specific GDB asset from: `05_Tools/Debug/GDB/s04_reset_persistence.gdb`
- Modify: `05_Tools/Scripts/s04_persistence_test.bat` to compatibility wrapper
- Keep source-of-truth parser at: `04_Test/Host/S04_Firmware_Image_Storage/s04_persistence_log.py`

**Interfaces:**
- Project extension may consume Core Process/Lock/Logging/GDB Adapter。
- Project extension owns `S04_PERSISTENCE_AXF`、S04 symbols、snapshot field rules and retry policy。
- Generic Application/Debug Workflow must not reference S04 identifiers。

- [ ] **Step 1: 写边界检查**

搜索 `Core/Adapters/Workflows`，S04-specific symbol/path 必须为 0 matches。

- [ ] **Step 2: 迁移 runner 与 GDB asset，更新内部引用**

不要移动已关闭 S04 阶段的 Host parser；Test Extension 通过明确路径调用它。

- [ ] **Step 3: 保留 BAT 兼容入口并运行现有 S04 Host Test**

```powershell
python -B -m unittest discover -s .\04_Test\Host\S04_Firmware_Image_Storage -p "test_*.py" -v
```

Expected: PASS。

- [ ] **Step 4: 提交**

```bash
git add 05_Tools/Tests 05_Tools/Scripts/s04_persistence_test.bat 05_Tools/Debug/GDB 04_Test/Host/S04_Firmware_Image_Storage
git commit -m "refactor(tools): isolate s04 persistence extension"
```

---

### Task 7: 接入 Firmware / Ymodem 能力并更新工具文档

**Files:**
- Modify as needed: `05_Tools/toolkit.ps1`
- Modify as needed: `05_Tools/Scripts/send_ymodem.bat`
- Modify as needed: `05_Tools/Scripts/send_ymodem_python.bat`
- Modify: `05_Tools/README.md`
- Modify: `05_Tools/Scripts/README.md`
- Delete only if still empty/placeholders: `05_Tools/CI/README.md`, `05_Tools/Packaging/README.md`

**Interfaces:**
- `toolkit.bat firmware pack ...` forwards to existing `Firmware/pack_firmware.py`。
- `toolkit.bat ymodem ...` forwards to existing Python/TeraTerm sender without重新实现协议。

- [ ] **Step 1: 运行现有 Firmware / Ymodem Host Test baseline**

```powershell
python -B -m unittest discover -s .\05_Tools\Firmware -p "test_*.py" -v
python -B -m unittest discover -s .\05_Tools\Ymodem\tests -v
```

- [ ] **Step 2: 将两个能力接到 Router，但不抽象 Transport Adapter**

保持当前 Python Sender `--json` 行为和 Tera Term 真实板入口。

- [ ] **Step 3: 更新 README**

必须说明：目录职责、三层配置、统一入口、Legacy 兼容、Exit Code、J-Link ownership、哪些功能需要真实 USB/板卡、哪些只是 Host Test。

- [ ] **Step 4: 清理纯占位目录**

只有 `CI/` 或 `Packaging/` 仍无任何可执行职责时才删除；如果实施过程中产生真实能力则保留。

- [ ] **Step 5: 重新运行 Firmware / Ymodem Host Test**

Expected: PASS。

- [ ] **Step 6: 提交**

```bash
git add -A 05_Tools
git commit -m "docs(tools): expose reusable firmware and transport entries"
```

---

### Task 8: 全量回归、第二工程复用演练与 Verification Handoff

**Files:**
- Modify: `00_Project/03_Stages/S05B_Toolkit_Reuse/handoff.md`
- Modify: `00_Project/05_Status/current_status.md`
- Modify: `PROJECT_CONTEXT.md`
- Formal verification output later: `04_Test/Reports/Stages/S05B_Toolkit_Reuse/verification.md`

**Interfaces:**
- Implementation Role 结束状态：`READY_FOR_VERIFICATION`。
- 正式 PASS/CLOSED 只能由 Verification / Review Role 根据证据决定。

- [ ] **Step 1: 全量 Host/Contract Regression**

依次运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Contracts\Core\test_core.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Contracts\Application\test_application_workflows.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Contracts\Debug\test_debug_workflows.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Contracts\Compatibility\test_legacy_entries.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Debug\GDB\test_gdb_automation.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Debug\test_tool_sequence.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Debug\CmBacktrace\test_cm_backtrace_integration.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Debug\CmBacktrace\test_fault_diagnostics.ps1
python -B -m unittest discover -s .\05_Tools\Firmware -p "test_*.py" -v
python -B -m unittest discover -s .\05_Tools\Ymodem\tests -v
python -B -m unittest discover -s .\04_Test\Host\S04_Firmware_Image_Storage -p "test_*.py" -v
git diff --check
```

任何一项 FAIL 都先修复，不进入复用演练。

- [ ] **Step 2: 当前工程新入口 smoke test**

```bat
05_Tools\toolkit.bat build
05_Tools\toolkit.bat flash run
05_Tools\toolkit.bat rtt 10
05_Tools\toolkit.bat snapshot resume
```

没有真实 USB/J-Link 的执行环境只允许记录 Host PASS / Board PENDING。

- [ ] **Step 3: 第二工程复用演练**

首选仓库：`wangyaoqianw-max/stm32f4_DMA_UART_ring_RTOS`。

执行原则：不修改第二工程生产代码，不修改通用 Core/Adapter/Workflow。将当前 `05_Tools` 复制到临时目录，只替换配置：

```powershell
Get-ChildItem <second-project-root> -Recurse -Filter *.uvprojx
```

根据实际 Keil project/target/output 生成该临时副本的 `project.defaults.bat`，复制本机 `toolchain.local.bat`，然后至少执行：

```bat
toolkit.bat build
```

若同一开发板可安全复用，再执行 `flash run` / `rtt`。关键验收：为第二工程适配时没有编辑 `Core/Adapters/Workflows/toolkit.ps1`。

- [ ] **Step 4: 检查硬编码与本机信息泄漏**

```powershell
Get-ChildItem .\05_Tools\Core,.\05_Tools\Adapters,.\05_Tools\Workflows -Recurse -File |
  Select-String -Pattern 'OTA_APP|COM9|wangyaoqian|Program Files|S04_PERSISTENCE'
```

只允许出现在明确的项目 Test / 配置 / 文档边界中；通用实现中必须清零。

- [ ] **Step 5: 更新 handoff/context/status**

记录实现 Commit、Host Test 结果、板测是否 PENDING、第二工程复用结果和未完成项。状态进入：

```text
READY_FOR_VERIFICATION
```

- [ ] **Step 6: 提交 Implementation Handoff**

```bash
git add 00_Project/03_Stages/S05B_Toolkit_Reuse/handoff.md 00_Project/05_Status/current_status.md PROJECT_CONTEXT.md
git commit -m "docs(s05b): hand off toolkit reuse for verification"
```

---

## Stop Conditions

出现以下任一情况必须停止当前 Task，不得通过修改验收条件绕过：

1. 新 Workflow 需要修改生产固件才能运行；
2. 为第二工程复用必须编辑通用 Core/Adapter/Workflow 中的工程专用字符串；
3. GDB `resume/halt` 行为与 S05A 已验证合同冲突；
4. 旧入口与新入口输出/退出语义出现无法解释的差异；
5. J-Link ownership 无法保证单 owner 或失败后留下已启动子进程；
6. 迁移导致 Firmware/Ymodem/S04 Host Test 回归失败。

## Verification Gate After Implementation

Implementation Role 完成后不直接关闭 S05B。Verification Role 必须独立记录：

- Host / Contract 全量测试；
- 当前工程 Build / Flash / RTT / GDB 真实能力；
- Legacy Entry compatibility；
- 第二同类工程配置替换复用证据；
- 本机路径/临时代码/构建缓存未提交；
- 未执行的硬件项明确标记为 `PENDING`，不能由 Host PASS 替代。
