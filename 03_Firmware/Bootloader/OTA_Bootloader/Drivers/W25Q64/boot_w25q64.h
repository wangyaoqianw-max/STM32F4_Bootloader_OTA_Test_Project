/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_w25q64.h
 * @brief Bootloader W25Q64 只读驱动接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_W25Q64_H
#define BOOT_W25Q64_H

//******************************** Includes *********************************//
#include "boot_spi.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_W25Q64_TOTAL_SIZE_BYTES (0x800000UL)
//******************************** Defines **********************************//

//******************************** Types ***********************************//
/**
 * @brief W25Q64 只读设备绑定及 JEDEC 身份。
 * @note S09 不在此模块提供任何写、擦除或 Write Enable API。
 */
typedef struct
{
    /** 已初始化的 SPI 总线；由调用者拥有。 */
    boot_spi_bus_t *bus;
    /** Flash 片选 GPIO 端口。 */
    GPIO_TypeDef *csPort;
    /** Flash 片选 GPIO 引脚。 */
    uint16_t csPin;
    /** JEDEC 厂商 ID。 */
    uint8_t manufacturerId;
    /** JEDEC 存储器类型。 */
    uint8_t memoryType;
    /** JEDEC 容量 ID。 */
    uint8_t capacityId;
    /** 非零表示 JEDEC 校验已通过。 */
    uint8_t initialized;
} boot_w25q64_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
/**
 * @brief 绑定 W25Q64、读取并校验 JEDEC ID。
 * @param[out] flash : W25Q64 对象；不得为空。
 * @param[in] bus : 已初始化 SPI 总线；驱动不重复初始化。
 * @param[in] csPort : 片选 GPIO 端口。
 * @param[in] csPin : 片选 GPIO 引脚。
 * @return 驱动状态；非 EF/40/17 时返回 VERIFY。
 */
boot_driver_status_t boot_w25q64_init(
    boot_w25q64_t *flash,
    boot_spi_bus_t *bus,
    GPIO_TypeDef *csPort,
    uint16_t csPin);
/**
 * @brief 从 W25Q64 连续读取数据。
 * @param[in] flash : 已通过 JEDEC 校验的对象。
 * @param[in] address : 24-bit Flash 起始地址。
 * @param[out] data : 接收 Buffer；不得为空。
 * @param[in] length : 读取字节数，必须位于 8 MiB 设备范围内。
 * @return 驱动状态；本接口不改变 W25Q64 内容。
 */
boot_driver_status_t boot_w25q64_read(
    boot_w25q64_t *flash,
    uint32_t address,
    uint8_t *data,
    uint32_t length);
//******************************** Functions ********************************//

#endif
