/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_spi.h
 * @brief Bootloader SPI2 同步总线最小接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_SPI_H
#define BOOT_SPI_H

//******************************** Includes *********************************//
#include "boot_driver_status.h"
#include "spi.h"
//******************************** Includes *********************************//

//******************************** Types ***********************************//
/**
 * @brief 已由 CubeMX/HAL 初始化的 SPI 总线绑定。
 * @note 总线不拥有 SPI Handle，也不负责具体设备片选。
 */
typedef struct
{
    /** HAL SPI Handle；由 Core 层拥有。 */
    SPI_HandleTypeDef *handle;
    /** 非零表示已完成 Bootloader 总线绑定。 */
    uint8_t initialized;
} boot_spi_bus_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
/**
 * @brief 绑定已初始化的 SPI Handle。
 * @param[out] bus : Bootloader SPI 总线对象；不得为空。
 * @param[in] handle : 已由 MX_SPI2_Init 初始化的 HAL Handle；不得为空。
 * @return 驱动状态；本函数不重复初始化 HAL 外设。
 */
boot_driver_status_t boot_spi_init(
    boot_spi_bus_t *bus,
    SPI_HandleTypeDef *handle);
/**
 * @brief 同步发送一段 SPI 数据。
 * @param[in] bus : 已初始化 SPI 总线。
 * @param[in] data : 发送 Buffer；length 非零时不得为空。
 * @param[in] length : 发送字节数，受 HAL uint16_t 接口限制。
 * @return 驱动状态。
 */
boot_driver_status_t boot_spi_write(
    boot_spi_bus_t *bus,
    const uint8_t *data,
    uint16_t length);
/**
 * @brief 同步接收一段 SPI 数据。
 * @param[in] bus : 已初始化 SPI 总线。
 * @param[out] data : 接收 Buffer；不得为空。
 * @param[in] length : 接收字节数，受 HAL uint16_t 接口限制。
 * @return 驱动状态。
 */
boot_driver_status_t boot_spi_read(
    boot_spi_bus_t *bus,
    uint8_t *data,
    uint16_t length);
//******************************** Functions ********************************//

#endif
