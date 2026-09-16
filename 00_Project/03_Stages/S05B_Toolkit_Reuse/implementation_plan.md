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

## Planned File Map

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

现有 `Firmware/`、`Ymodem/`、`TeraTerm/` 第一轮保留原路径；现有 `Debug/GDB/*.gdb`、CmBacktrace 契约测试继续保留，只有 S04 专用资产按 Task 6 收口。

### Task 1: 建立三层配置合同与 Core 基础模块

**Files:** `.gitignore`、`05_Tools/Config/*`、`05_Tools/Core/{Config,Path,Logging}.ps1`、`Toolkit.Core.psm1`、`05_Tools/Contracts/Core/test_core.ps1`。

**Interfaces:** `Import-ToolkitConfiguration -ToolsRoot $toolsRoot`、`Resolve-ToolkitProjectPath`、`Assert-ToolkitRequiredValue`、`New-ToolkitLogDirectory`。加载顺序固定为 `project.defaults -> project.local override -> toolchain.local`。

- [ ] **Step 1: 先写配置合同测试**

```powershell
$config = Import-ToolkitConfiguration -ToolsRoot $fixtureRoot
Assert-Equal $config.PROJECT_KEIL_TARGET "FixtureTarget"
Assert-Equal $config.JLINK_SPEED "4000"
Assert-Equal $config.KEIL_UV4 "C:\fake\UV4.exe"
```

同时覆盖缺少 machine config、缺少必需变量、相对路径解析、local override 优先级。

- [ ] **Step 2: 运行测试并确认失败**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Contracts\Core\test_core.ps1
```

Expected: FAIL，因为 Core/config 尚未建立。

- [ ] **Step 3: 拆分配置**

`toolchain.local.example.bat` 只保留 `KEIL_UV4/JLINK_EXE/JLINK_GDB_SERVER/JLINK_RTT_LOGGER/ARM_GDB/PYTHON_EXE/TERA_TERM_EXE`；`project.defaults.bat` 保存当前仓库 Keil project/target、AXF/HEX/BIN 相对路径、输出/日志目录、`STM32F411CE`、`SWD`；`project.local.example.bat` 只提供 `SERIAL_PORT/GDB_PORT/JLINK_SPEED/JLINK_RTT_CHANNEL` 覆盖项；`.gitignore` 增加 `05_Tools/Config/project.local.bat`。

- [ ] **Step 4: 实现 Config/Path/Logging**

`Import-ToolkitConfiguration` 通过 `cmd.exe /d /s /c` 调用三个 BAT 配置并解析最终环境；`Resolve-ToolkitProjectPath` 只负责 Project Root + 相对路径解析。

- [ ] **Step 5: 重跑测试，Expected: PASS**

- [ ] **Step 6: 提交**

```bash
git add .gitignore 05_Tools/Config 05_Tools/Core 05_Tools/Contracts/Core
git commit -m "refactor(tools): establish reusable configuration core"
```

### Task 2: 抽取 Process / Lock 公共能力

**Files:** `05_Tools/Core/Process.ps1`、`Lock.ps1`、`Toolkit.Core.psm1`、`Contracts/Core/test_core.ps1`。

**Interfaces:** `Invoke-ToolkitProcess`、`Stop-ToolkitOwnedProcess`、`Test-ToolkitTcpPort`、`Enter-ToolkitLock`、`Exit-ToolkitLock`。Process result 至少包含 `ExitCode/Stdout/Stderr/TimedOut/ProcessId`。

- [ ] **Step 1: 写失败测试**：正常退出、非零退出、超时 kill、TCP probe、owned cleanup、lock conflict。
- [ ] **Step 2: 确认测试 FAIL**。
- [ ] **Step 3: 从当前 `gdb_runtime_snapshot.ps1` 抽取参数转义、captured process、timeout、stdout/stderr、TCP ready、owned cleanup、lock file；不改变已验证 GDB 语义。
- [ ] **Step 4: Core test PASS，且无残留测试进程/lock file**。
- [ ] **Step 5: 提交**：`git commit -m "refactor(tools): extract process and lock primitives"`。

### Task 3: 建立 Build / Probe Adapter 与 Application Workflows

**Files:** `Adapters/Build/Keil/keil_build.ps1`、`Adapters/Probe/JLink/{jlink_flash,jlink_rtt}.ps1`、`Workflows/Application/{build,flash,rtt,run}.ps1`、`Contracts/Application/test_application_workflows.ps1`。

**Interfaces:** Keil Adapter 只接受 effective config；J-Link flash 支持 `run|prepare`；RTT Adapter 只负责 Logger；`run.ps1` 固定编排 `Build -> Flash(run) -> RTT`。

- [ ] **Step 1: 使用 temporary fake executable 写 Host Contract Test**，验证 project/target/HEX/device/IF/speed/channel 全部来自 Config。
- [ ] **Step 2: 确认测试 FAIL**。
- [ ] **Step 3: 迁移 `build_app.bat` / `flash_app.bat` 核心逻辑**，保留 Keil `0/1/2/3` 判定和 J-Link `run|prepare`；temporary command file 必须 cleanup。
- [ ] **Step 4: 迁移 RTT 与 run cycle**，不得复制 Adapter 参数拼装。
- [ ] **Step 5: Host Contract PASS；扫描通用实现不得出现 `OTA_APP`、固定 `COM9`、本机绝对路径**。
- [ ] **Step 6: 有本机硬件时运行 smoke**：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Workflows\Application\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Workflows\Application\flash.ps1 -Mode run
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Workflows\Application\rtt.ps1 -Seconds 10
```

无 USB/J-Link 时只记录 hardware smoke `PENDING`。

- [ ] **Step 7: 提交**：`git commit -m "refactor(tools): add application workflows and adapters"`。

### Task 4: 重构 GDB Adapter 与 Debug Workflows

**Files:** `Adapters/Debug/GDB/gdb_session.ps1`、`Workflows/Debug/{snapshot,fault_capture}.ps1`、`Contracts/Debug/test_debug_workflows.ps1`，随后修改 `Scripts/gdb_runtime_snapshot.ps1` 与 `gdb_fault_capture.ps1`。

**Interfaces:** GDB Adapter 只拥有 Server readiness/client lifecycle；Snapshot Workflow 拥有 `resume|halt`；Fault Workflow 拥有 `capture|trigger` 与证据顺序。

- [ ] **Step 1: 写合同测试**：resume 必须含 `continue&` 且无 `detach`；halt 必须含 `detach` 且无 `continue&`；所有脚本禁止 `load`；单 Workflow 单 GDB Server owner；失败只清理 owned PID。
- [ ] **Step 2: 迁移前先跑现有基线**：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Debug\GDB\test_gdb_automation.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Debug\test_tool_sequence.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\05_Tools\Debug\CmBacktrace\test_fault_diagnostics.ps1
```

- [ ] **Step 3: 删除两个旧 PS1 内重复的 process/log/TCP cleanup，改为调用 Core**。
- [ ] **Step 4: 新旧 Debug Contract 全部 PASS**。
- [ ] **Step 5: 有真实板卡时回归 `snapshot halt` 与 `snapshot resume`，重新连接检查 `uwTick`；无板卡则交 Verification Role**。
- [ ] **Step 6: 提交**：`git commit -m "refactor(tools): separate gdb adapter from debug workflows"`。

### Task 5: 增加统一 toolkit Router 并切换 Legacy Entries

**Files:** 新建 `05_Tools/toolkit.ps1`、`toolkit.bat`、`Contracts/Compatibility/test_legacy_entries.ps1`；修改现有 build/flash/rtt/run/snapshot/fault BAT 入口。

**Public Commands:**

```text
toolkit.bat build
toolkit.bat flash [run|prepare]
toolkit.bat run [rtt_seconds]
toolkit.bat rtt [seconds]
toolkit.bat snapshot [halt|resume]
toolkit.bat fault [capture|trigger]
```

**Exit Classes:** `0 SUCCESS / 10 CONFIG_ERROR / 20 BUILD_ERROR / 30 PROBE_ERROR / 40 DEBUG_ERROR / 50 TRANSFER_ERROR / 60 TEST_ERROR`。

- [ ] **Step 1: 写 Router/Legacy Contract Test**：未知命令必须失败；legacy wrapper 不再出现 Keil/J-Link/GDB 核心命令。
- [ ] **Step 2: `toolkit.bat` 只定位自身并调用 `toolkit.ps1`；PowerShell Router 只负责参数校验、Workflow 路由、Exit Code 映射**。
- [ ] **Step 3: 旧 BAT 全部改成参数透传薄包装**，例如：

```bat
@echo off
call "%~dp0..\toolkit.bat" build
exit /b %ERRORLEVEL%
```

- [ ] **Step 4: Compatibility Contract PASS**。
- [ ] **Step 5: 提交**：`git commit -m "refactor(tools): add unified toolkit router"`。

### Task 6: 收口 S04 Persistence 项目测试扩展

**Files:** 新建 `05_Tools/Tests/S04_Persistence/`；移动 `Scripts/s04_persistence_test.py` 与 `Debug/GDB/s04_reset_persistence.gdb`；`Scripts/s04_persistence_test.bat` 改为兼容 wrapper；`04_Test/Host/S04_Firmware_Image_Storage/s04_persistence_log.py` 保持原位置。

- [ ] **Step 1: 边界测试**：`Core/Adapters/Workflows` 中 S04-specific symbol/path 必须 0 matches。
- [ ] **Step 2: 迁移 runner/GDB asset 并更新引用**；不移动已关闭 S04 阶段 Host parser。
- [ ] **Step 3: 保留 BAT 兼容入口并运行：

```powershell
python -B -m unittest discover -s .\04_Test\Host\S04_Firmware_Image_Storage -p "test_*.py" -v
```

Expected: PASS。

- [ ] **Step 4: 提交**：`git commit -m "refactor(tools): isolate s04 persistence extension"`。

### Task 7: 接入 Firmware / Ymodem 能力并更新工具文档

**Files:** 修改 `05_Tools/toolkit.ps1`，为 Firmware Pack 与 Ymodem 增加明确路由；修改 `Scripts/send_ymodem.bat` 和 `send_ymodem_python.bat` 为统一入口兼容 wrapper；更新 `05_Tools/README.md`、`Scripts/README.md`；若 `CI/`、`Packaging/` 仍只有占位 README 且没有执行职责则删除。

- [ ] **Step 1: 迁移前基线**：

```powershell
python -B -m unittest discover -s .\05_Tools\Firmware -p "test_*.py" -v
python -B -m unittest discover -s .\05_Tools\Ymodem\tests -v
```

- [ ] **Step 2: 增加 `toolkit.bat firmware pack ...` 和 `toolkit.bat ymodem ...` 路由**，只转发现有工具，不重写协议。
- [ ] **Step 3: 两个 Ymodem Legacy BAT 改为参数透传 wrapper**，保留 Python `--json` 和 Tera Term 入口语义。
- [ ] **Step 4: README 记录目录职责、三层配置、统一入口、Exit Code、J-Link ownership、Host/Board 能力边界**。
- [ ] **Step 5: 若 CI/Packaging 仍为空占位则删除；否则保留实际职责**。
- [ ] **Step 6: Firmware/Ymodem Host Test 再次 PASS**。
- [ ] **Step 7: 提交**：`git commit -m "docs(tools): expose reusable firmware and transport entries"`。

### Task 8: 全量回归、第二工程复用演练与 Verification Handoff

**Files:** 实施结束后更新 `handoff.md`、`current_status.md`、`PROJECT_CONTEXT.md`；正式 Verification Role 输出 `04_Test/Reports/Stages/S05B_Toolkit_Reuse/verification.md`。

- [ ] **Step 1: 全量 Host/Contract Regression**

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

任何一项 FAIL 都先修复。

- [ ] **Step 2: 当前工程新入口 smoke**

```bat
05_Tools\toolkit.bat build
05_Tools\toolkit.bat flash run
05_Tools\toolkit.bat rtt 10
05_Tools\toolkit.bat snapshot resume
```

无真实 USB/J-Link 时只记录 Host PASS / Board PENDING。

- [ ] **Step 3: 第二工程复用演练**

首选仓库固定为 `wangyaoqianw-max/stm32f4_DMA_UART_ring_RTOS`。为避免依赖本机已有 clone，使用临时目录：

```powershell
$reuseRoot = Join-Path $env:TEMP "s05b-reuse-stm32f4_DMA_UART_ring_RTOS"
if (Test-Path $reuseRoot) { Remove-Item $reuseRoot -Recurse -Force }
git clone https://github.com/wangyaoqianw-max/stm32f4_DMA_UART_ring_RTOS.git $reuseRoot
Get-ChildItem $reuseRoot -Recurse -Filter *.uvprojx
```

将当前 `05_Tools` 复制到该临时 clone，仅根据找到的 Keil 工程、target 和 output 修改临时副本中的 `project.defaults.bat`，并复制本机 `toolchain.local.bat`。至少执行：

```bat
toolkit.bat build
```

若同一板卡连接条件安全，再执行 `flash run` / `rtt`。关键验收：不得编辑临时副本的 `Core/Adapters/Workflows/toolkit.ps1` 来适配第二工程。

- [ ] **Step 4: 硬编码/本机信息泄漏检查**

```powershell
Get-ChildItem .\05_Tools\Core,.\05_Tools\Adapters,.\05_Tools\Workflows -Recurse -File |
  Select-String -Pattern 'OTA_APP|COM9|wangyaoqian|Program Files|S04_PERSISTENCE'
```

通用实现必须 0 matches；工程事实只允许存在于 Config/Tests/文档边界。

- [ ] **Step 5: 更新 handoff/context/status**，记录 Implementation Commit、Host Test、Board Test 是否 PENDING、第二工程复用结果，状态进入 `READY_FOR_VERIFICATION`。
- [ ] **Step 6: 提交**：`git commit -m "docs(s05b): hand off toolkit reuse for verification"`。

## Stop Conditions

1. 新 Workflow 需要修改生产固件才能运行；
2. 第二工程复用必须编辑通用 Core/Adapter/Workflow 中的工程专用字符串；
3. GDB `resume/halt` 与 S05A 合同冲突；
4. Legacy 与新入口出现无法解释的语义差异；
5. J-Link 无法保证单 owner 或失败后残留 owned child process；
6. Firmware/Ymodem/S04 Host Test 回归失败。

## Verification Gate After Implementation

Implementation Role 完成后不得直接关闭 S05B。Verification Role 必须独立记录 Host/Contract 全量测试、当前工程 Build/Flash/RTT/GDB、Legacy compatibility、第二工程复用证据、未提交本机路径/临时代码/缓存，以及所有未执行硬件项的 `PENDING` 状态。
