# Firmware

本目录是模板的主要开发区域。修改前读取本目录 `AGENTS.md`、当前阶段文档和相关固件设计。

```text
03_Firmware/
├── 00_Doc/
├── Application/
├── Bootloader/
└── Shared/
```

- Application：默认 RTOS 主工程；
- Bootloader：按项目需要启用；
- Shared：仅保存已经存在两个使用方的稳定代码；
- 00_Doc：固件架构、接口、模块、RTOS、Memory 和规范。
