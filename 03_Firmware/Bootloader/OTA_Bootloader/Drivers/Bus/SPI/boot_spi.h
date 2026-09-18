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
typedef struct
{
    SPI_HandleTypeDef *handle;
    uint8_t initialized;
} boot_spi_bus_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
boot_driver_status_t boot_spi_init(
    boot_spi_bus_t *bus,
    SPI_HandleTypeDef *handle);
boot_driver_status_t boot_spi_write(
    boot_spi_bus_t *bus,
    const uint8_t *data,
    uint16_t length);
boot_driver_status_t boot_spi_read(
    boot_spi_bus_t *bus,
    uint8_t *data,
    uint16_t length);
//******************************** Functions ********************************//

#endif
