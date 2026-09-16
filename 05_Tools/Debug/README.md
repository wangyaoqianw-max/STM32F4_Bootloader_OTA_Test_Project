# Debug Tools

保存日志解析、调试辅助和故障分析工具，不保存一次性调试输出。

当前文件：

```text
GDB/runtime_snapshot_resume.gdb  运行态快照后继续运行
GDB/runtime_snapshot_halt.gdb    运行态快照后保持暂停
GDB/s04_reset_persistence.gdb    复位前后读取 S04 持久性快照
GDB/fault_capture.gdb             已发生 Fault 的只读现场采集
GDB/fault_trigger_capture.gdb     GDB 已启动后复位并触发受控 Fault
GDB/test_gdb_automation.ps1      脚本合同和失败路径测试
CmBacktrace/test_cm_backtrace_integration.ps1  CmBacktrace 工程接入契约测试
CmBacktrace/test_fault_diagnostics.ps1          Fault 诊断契约测试
test_tool_sequence.ps1             工具调用顺序契约测试
```

CmBacktrace 源码位于 Application 工程的 `05_Vendors/CmBacktrace`，本目录只保存工程接入、Fault
诊断和工具调用顺序契约测试。Fault 原始输出通过 SEGGER RTT 最小接口写入，不依赖 EasyLogger、
Mutex、Queue 或阻塞 UART。

## GDB Runtime Snapshot

S05A 当前只覆盖 GDB 在线调试、运行态快照和退出生命周期，不执行 GDB
load，不隐式烧录 Flash。

依赖由 05_Tools/Config/toolchain.local.bat 提供：

~~~text
ARM_GDB          = arm-none-eabi-gdb.exe
JLINK_GDB_SERVER = JLinkGDBServerCL.exe
~~~

默认目标连接参数：

~~~text
Device    = STM32F411CE
Interface = SWD
Speed     = 4000 kHz
GDB Port  = 2331
~~~

运行入口：

~~~bat
05_Tools\Scripts\gdb_runtime_snapshot.bat resume
05_Tools\Scripts\gdb_runtime_snapshot.bat halt
~~~

退出合同：

~~~text
resume: snapshot -> continue& -> disconnect -> quit
halt:   snapshot -> detach -> quit
~~~

Resume 模式不得使用 -batch，也不得在 continue& 后使用 detach。
Halt 模式保持 MCU 暂停。两个模式都不执行 load。

## GDB Fault Capture

已发生 Fault 的采集：

~~~bat
05_Tools\Scripts\gdb_fault_capture.bat capture
~~~

测试固件的完整触发与采集：

~~~bat
05_Tools\Scripts\flash_app.bat prepare
05_Tools\Scripts\gdb_fault_capture.bat trigger
~~~

`prepare` 只烧录并保持 MCU Halt；`trigger` 随后先启动 GDB Server/GDB 客户端并设置
`diagnostics_fault_capture_stop` 断点，再通过已建立的 GDB 会话执行 `monitor reset` 和 `continue&`。
这样需要在复位前打开的 GDB 工具已经处于工作状态。命中断点表示工程 Fault Handler 已保存现场，
随后执行 `halt-and-detach`，不会执行 `load` 或再次恢复 MCU。

J-Link 同一时刻只能由一个工具占用。RTT Logger 不能与 GDB Server 并行连接，因此脚本在 GDB
释放 Probe 后再读取 RTT 缓冲中的 CmBacktrace 输出。

## S04 Reset / Power-cycle Persistence

`GDB/s04_reset_persistence.gdb` 只负责连接临时测试固件、执行复位和释放 Probe，执行顺序为：

```text
target extended-remote -> monitor reset -> continue& -> disconnect -> quit
```

快照由 RTT Logger 采集并由 Host Parser 比对；GDB 脚本不执行 `load`，也不修改 Slot B 或 Metadata。电源循环由
`Scripts/s04_persistence_test.bat power-cycle` 启动 RTT Logger；看到 `READY` 后由操作者完成
断电/上电。Power-cycle 模式从 AXF 解析 `_SEGGER_RTT` 固定地址，Logger 退出或 5 秒无新数据
时自动按 1/2/4 秒退避重连，并在输出日志中记录原因。当前串口模块为 `COM9`，但 S04
持久性证据来自 RTT Channel 0。

输出日志：

~~~text
06_Output/Logs/OTA_APP_gdb_server.log
06_Output/Logs/OTA_APP_gdb_snapshot.log
06_Output/Logs/OTA_APP_fault_gdb.log
06_Output/Logs/OTA_APP_fault_rtt.log
~~~
