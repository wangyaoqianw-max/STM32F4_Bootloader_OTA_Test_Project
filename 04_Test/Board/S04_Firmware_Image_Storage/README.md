# S04 Persistence Board Test

本目录保存 Reset / Power-cycle Persistence 的只读板测固件入口。测试入口不属于正式
Application，当前正式 `OTA_APP.uvprojx` 不包含本目录文件，也不会在正常启动路径调用它。

## 临时生成测试 AXF

如需重新板测，使用本目录的两个源文件，临时完成以下接入：

1. 在 `project_config.h` 增加默认关闭的 `PROJECT_ENABLE_S04_PERSISTENCE_BOARD_TEST`，
   本地板测时仅在测试构建中置为 `1U`，并与 S05 YMODEM 测试入口互斥。
2. 在 `01_APP/task/app_main_task.c` 中临时选择 `app_s04_persistence_test_run()` 作为启动入口。
3. 在 `OTA_APP.uvprojx` 的临时测试组加入本目录的 `.c` 文件，并添加本目录为 Include Path。
4. 用 Keil 构建测试镜像；确认 AXF 中存在 `app_s04_persistence_test_run` 符号。
5. 将测试 AXF 复制到正式工程之外的本地路径，并在未提交的
   `05_Tools\Config\toolchain.local.bat` 中设置：

   ```bat
   set "S04_PERSISTENCE_AXF=E:\Temp\OTA_APP_S04_Persistence.axf"
   ```

6. 恢复 `project_config.h`、`01_APP/task/app_main_task.c` 和 `OTA_APP.uvprojx`；提交前确认正式工程不再
   引用本目录。

`05_Tools\Scripts\s04_persistence_test.bat` 只接受通过符号检查的 S04 测试 AXF，不会把
   普通 Application AXF 当作板测固件。测试顺序仍为：先烧录并运行测试 AXF，再关闭其他
   J-Link 客户端，最后启动 Reset 或 Power-cycle 自动化脚本。

## 测试约束

- 只调用现有 Storage Service 的读取和校验接口；
- 不擦除、不写入 Slot，不提交 Metadata；
- RTT 输出固定格式快照，Host Parser 比较事件前后的持久化字段；
- Power-cycle 测试需由操作者在 `READY` 后操作开发板电源。
