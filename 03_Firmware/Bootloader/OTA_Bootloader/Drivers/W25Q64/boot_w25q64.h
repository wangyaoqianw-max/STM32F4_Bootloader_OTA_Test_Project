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
typedef struct
{
    boot_spi_bus_t *bus;
    GPIO_TypeDef *csPort;
    uint16_t csPin;
    uint8_t manufacturerId;
    uint8_t memoryType;
    uint8_t capacityId;
    uint8_t initialized;
} boot_w25q64_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
boot_driver_status_t boot_w25q64_init(
    boot_w25q64_t *flash,
    boot_spi_bus_t *bus,
    GPIO_TypeDef *csPort,
    uint16_t csPin);
boot_driver_status_t boot_w25q64_read(
    boot_w25q64_t *flash,
    uint32_t address,
    uint8_t *data,
    uint32_t length);
//******************************** Functions ********************************//

#endif
