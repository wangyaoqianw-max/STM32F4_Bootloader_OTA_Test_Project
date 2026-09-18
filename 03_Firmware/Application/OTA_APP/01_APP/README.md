# Application App Layer

`01_APP` 按启动、长期 Task、运行时装配和跨 Task 合同划分物理目录：

```text
system/
  Application Bootstrap、Startup Context、Startup Barrier
task/
  appMainTask、otaWorker、displayTask 等长期 RTOS 执行上下文
runtime/
  Application 级 Platform / Service 依赖装配与运行时资源
contract/
  跨 Task / 模块共享的紧凑数据合同、通知位和 Display Event
```

`system/` 不拥有业务 Task 的私有硬件资源；私有初始化仍由对应 Task 在自己的执行上下文中完成。共享 IPC 由 `app_system_bootstrap()` 在创建长期 Task 前集中创建。
