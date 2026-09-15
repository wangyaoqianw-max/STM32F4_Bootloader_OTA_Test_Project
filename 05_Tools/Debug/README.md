# Debug Tools

保存日志解析、调试辅助和故障分析工具，不保存一次性调试输出。

当前文件：

```text
GDB/runtime_snapshot_resume.gdb  运行态快照后继续运行
GDB/runtime_snapshot_halt.gdb    运行态快照后保持暂停
GDB/test_gdb_automation.ps1      脚本合同和失败路径测试
CmBacktrace/test_cm_backtrace_integration.ps1  CmBacktrace 工程接入契约测试
```

CmBacktrace 源码位于 Application 工程的 `05_Vendors/CmBacktrace`，本目录
只保存其工程接入契约测试。Fault 注入和 MCU Fault 现场自动采集仍未实现。

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

输出日志：

~~~text
06_Output/Logs/OTA_APP_gdb_server.log
06_Output/Logs/OTA_APP_gdb_snapshot.log
~~~
