# 00_Config

本目录只保存当前 Application 基础工程实际使用的、编译期确定的静态配置；修改后需要重新编译才能生效。

`project_config.h` 当前只定义 Status LED 和 Software I2C 基础运行所需的静态参数。

后续阶段新增模块时，只能把已经进入对应 Build 且有实际调用方的参数加入配置。

以下内容不属于静态配置，必须保留在实际运行模块中：

- 对象实例或对象指针
- Storage 地址
- 运行时 Context、State、Statistics
- HAL、DMA 或 USART Handle
- Impl 映射
- Callback
- RTOS native handle
