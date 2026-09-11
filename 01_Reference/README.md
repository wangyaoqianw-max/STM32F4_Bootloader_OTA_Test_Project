# Reference

本目录保存项目依赖的外部原始资料。项目自行形成的软件设计结论不放在这里；从资料中确认出的板级事实应提炼到 `02_Hardware` 的对应文档。

工程准备阶段先填写 `reference_index.md`，再将原始资料保存到相应分类目录。

```text
01_Reference/
├── reference_index.md
├── Datasheets/
├── Reference_Manuals/
├── Application_Notes/
├── Protocols/
└── Other/
```

优先收集官方 Datasheet、Reference Manual、Errata、Application Note、协议规范、开发板原理图或官方说明，并记录版本和来源。

无法确认来源、版本或适用性的资料必须标记为 `UNVERIFIED` 或 `MISSING`，不得直接作为冻结设计依据。
