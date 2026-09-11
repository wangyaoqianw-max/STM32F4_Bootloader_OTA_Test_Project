# Keil 工程与构建输出规范

## 1. 目的与适用范围

本文规定 STM32CubeMX、Keil MDK、Git 和 Agent 在本仓库中的构建边界，适用于：

```text
03_Firmware/Application/
03_Firmware/Bootloader/
```

核心原则：源码和构建配置是长期维护的工程输入；能够通过 Clean + Rebuild 恢复的文件是临时构建输出。两者不得混放或按同一方式管理。

## 2. 当前目录约定

Application 是默认主工程，Bootloader 仅在项目需要时启用。每个实际 Keil 工程内部采用相同约定：

```text
03_Firmware/<Application|Bootloader>/
├── 00_Config/
├── 01_APP/
├── 02_Service/
├── 03_Platform/
├── 04_Impl/
├── 05_Vendors/
├── Core/
├── Drivers/
├── Middlewares/
├── MDK-ARM/
│   ├── <project>.uvprojx
│   ├── Objects/              # 编译、链接和普通固件输出
│   └── Listings/             # Map 和 Listing 输出
└── <project>.ioc
```

`Objects/` 和 `Listings/` 中禁止保存人工维护的源码、配置、脚本、验证报告或发布说明。两个目录必须能够在关闭相关工具后安全删除并通过重建恢复。

固件公共架构和规范放在 `03_Firmware/00_Doc/`；项目测试与正式验证证据放在 `04_Test/`；本地生成的固件、升级包和日志放在 `06_Output/`。不得在每个 Keil 工程中复制这些仓库级职责。

## 3. Git 管理边界

### 3.1 应进入 Git 的工程输入

通常包括：

- 项目自研 `.c`、`.h`；
- `.ioc`、`.uvprojx`；
- 人工维护的 Scatter File、链接脚本和配置；
- 必须版本化的启动代码、Vendor 依赖和补丁；
- 构建、测试、打包脚本和正式工程文档。

判断标准是：删除后能否仅依靠仓库内容和确定的工具步骤恢复。不能可靠恢复的内容属于工程输入。

### 3.2 不进入 Git 的普通构建输出

`MDK-ARM/Objects/`、`MDK-ARM/Listings/` 和 `06_Output/` 下的普通生成物由根 `.gitignore` 按目录排除。

不使用覆盖整个仓库的 `*.hex`、`*.bin`、`*.map` 等扩展名规则代替目录规则。这样当 Keil 输出目录配置漂移时，`git status` 会暴露落在错误位置的生成物。

已经被 Git 跟踪的生成物不会因为新增 `.gitignore` 自动解除跟踪。处理前必须核对精确路径，确认其中没有人工维护文件，再通过 Git Index 移除；不得误删 `.uvprojx`、`.ioc`、Scatter File 或源码。

### 3.3 Build Artifact 与正式交付物

`Objects/` 中的日常 `.axf`、`.hex`、`.bin` 和 `.map` 属于可丢弃的 Build Artifact。

需要交付的固件应从经过验证的 Commit 或 Tag 执行 Clean Rebuild 后产生，并与版本、构建配置、校验值和验证报告建立对应关系。临时交付文件可放入 `06_Output/`，长期发布优先使用正式制品库或 GitHub Release；Git 仓库内只保存需要长期追溯的发布说明和验证证据。

## 4. Keil 输出配置

创建或导入 Keil 工程时，在 `Options for Target` 中设置：

```text
Output Directory  = .\Objects\
Listing Directory = .\Listings\
```

对应 `.uvprojx` 中通常表现为：

```xml
<OutputDirectory>Objects\</OutputDirectory>
<ListingPath>Listings\</ListingPath>
```

一个工程存在多个 Target 时，应逐个核对。不得只创建目录而保留 Target 指向其他输出位置。

初始化或修改构建配置后至少验证：

1. Clean Targets；
2. Rebuild all target files；
3. 输出只进入当前工程的 `MDK-ARM/Objects/` 和 `MDK-ARM/Listings/`；
4. `git status --short` 不出现构建生成物；
5. 实际构建命令、工具链版本和结果写入当前阶段验证报告。

## 5. 构建期间的协作边界

Keil 构建开始前，应保存参与构建的源码、`.uvprojx`、`.ioc`、Scatter File 和相关配置。

构建期间：

- 不修改本次构建使用的源码和配置；
- 不执行会移动、清理、切换或重写这些文件的 Git 操作；
- 不删除、重命名或扫描式改写 `Objects/`、`Listings/`；
- 可以进行不影响本次构建输入与输出的只读检查。

构建完成后再记录结果、更新验证报告并执行提交。Agent 无法操作真实硬件时，只能记录代码构建结果，不能据此声明硬件验证通过。

## 6. Keil I/O 错误分流

出现以下类型信息时：

```text
couldn't write file '*.o'
I/O error writing file '*.o'
Invalid argument
```

如果失败对象随机变化、对应源码没有编译错误、相同代码此前可以构建，优先按构建环境或文件 I/O 问题诊断。不得仅凭这类信息修改 `.c` 文件来规避失败。

只有编译器同时给出能够稳定定位到源码、语法、类型或符号的诊断时，才进入代码错误分析。

## 7. I/O 错误排查顺序

按以下顺序执行并记录实际结果：

1. 区分编译诊断和输出写入失败，确认对应源码是否存在真实 Error。
2. 核对当前 Target 的 `OutputDirectory`、`ListingPath` 和实际落盘位置。
3. 执行 Clean Targets，再执行 Rebuild。
4. 结束 Keil Debug Session、RTT/J-Link Viewer，以及可能占用输出文件的 Git、Agent、索引或打包操作。
5. 关闭 Keil；确认 `Objects/`、`Listings/` 内无人工文件后，删除这两个可再生目录，重新打开工程并 Rebuild。
6. 检查工程路径长度、特殊字符、目录权限、磁盘空间、文件系统状态和文件句柄竞争。
7. 检查 Windows Defender 或其他实时扫描程序。确需对照测试时，仅考虑排除具体的 `Objects/`、`Listings/`，不要排除整个仓库、`.git` 或磁盘。
8. 仍然失败时，记录 Keil、Compiler、Windows、文件系统和稳定复现步骤，再判断工具链兼容性或主机故障。

删除输出目录、调整安全软件或修改系统设置前，应遵守仓库安全规则并确认操作范围。

## 8. 阶段验证记录

构建验证至少记录：

```text
Source Commit / Tag
Keil 与 Compiler 版本
Target 名称
Build 类型：Build / Clean Rebuild
实际 Error / Warning 数量
固件文件及校验值（需要交付时）
代码验证：PASS / FAIL / NOT_APPLICABLE
硬件验证：PASS / PENDING / FAIL / NOT_APPLICABLE
```

Keil I/O 故障应记录为构建环境失败及其处理过程。问题恢复后必须重新执行所需构建，不能把一次失败构建中的局部编译结果当作完整代码验证通过。

## 9. 新工程检查表

```text
[ ] .ioc、.uvprojx、源码和人工配置均在 Git 管理范围内
[ ] 每个 Target 的 OutputDirectory 为 Objects\
[ ] 每个 Target 的 ListingPath 为 Listings\
[ ] Objects/ 和 Listings/ 不含人工维护文件
[ ] Clean Rebuild 完成并记录实际结果
[ ] git status 不出现构建生成物
[ ] 测试证据写入 04_Test/Reports
[ ] 普通输出与正式发布制品已经区分
```
