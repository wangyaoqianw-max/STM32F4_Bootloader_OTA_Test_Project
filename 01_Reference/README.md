# Reference

本目录保存项目依赖的外部原始资料。项目自行形成的软件设计结论不放在这里；从资料中确认出的结构化事实统一登记到工程准备工作簿。

工程准备阶段先将原始资料保存到相应分类目录，再在以下位置建立索引：

```text
00_Project/00_Preparation/Engineering_Preparation.xlsx
└── 01_资料_References
```

本目录建议保持为原始资料仓库：

```text
01_Reference/
├── Datasheets/
├── Reference_Manuals/
├── Application_Notes/
├── Protocols/
└── Other/
```

优先收集官方 Datasheet、Reference Manual、Errata、Application Note、协议规范、开发板官方说明等资料，并在工作簿中记录文档版本、来源组织、URL、仓库路径和权威性。

无法可靠确认来源、版本或适用性的资料，应在工作簿中标记为 `未确认 / UNKNOWN`；仅作为临时参考的内容可以标记为 `临时假设 / ASSUMED`，不得直接作为冻结设计依据。
