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

## 构建约定

Keil 工程的普通构建输出统一放在各工程自己的：

```text
MDK-ARM/Objects/
MDK-ARM/Listings/
```

源码、构建配置、普通构建输出和正式交付物的边界，以及 Keil I/O 错误排查顺序，见：

```text
03_Firmware/00_Doc/Standards/Keil工程与构建输出规范.md
```
