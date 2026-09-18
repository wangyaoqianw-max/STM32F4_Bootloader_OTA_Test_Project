# S09 Factory Restore Board Test

该目录提供 `toolkit.bat factory restore` 使用的临时 Application board test。它不加入正式 `OTA_APP.uvprojx`，由 Toolkit 在执行期间临时替换 `app_system.c` 并加入工程，流程结束后恢复工程文件。

测试固件使用正式 Application 的 Firmware Storage、Metadata V2、YMODEM Receiver 和 OTA Firmware Sink：

```text
擦除 Slot A/B
→ YMODEM 接收 compact v1.0 .img 到 Slot A
→ 验证 Slot A
→ 提交两次 Metadata V2 baseline
→ 回读 Slot A / Slot B / Metadata
```

Factory Restore 是 destructive operation，会重置 External Flash 两个 Slot 与 Metadata。Toolkit 只有在显式确认后才会执行，随后再烧录正式 v1.0 Application 到 Internal APP。

该测试入口只允许由统一 Toolkit workflow 使用，不属于正式 Application 启动链。
