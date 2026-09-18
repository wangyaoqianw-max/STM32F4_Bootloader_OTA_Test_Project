/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_at24c02.h
 * @brief Bootloader AT24C02 读写驱动接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_AT24C02_H
#define BOOT_AT24C02_H

//******************************** Includes *********************************//
#include "boot_soft_i2c.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_AT24C02_TOTAL_SIZE_BYTES (256U)
#define BOOT_AT24C02_PAGE_SIZE_BYTES  (8U)
#define BOOT_AT24C02_ADDRESS          (0x50U)
//******************************** Defines **********************************//

//******************************** Types ***********************************//
typedef struct
{
    boot_soft_i2c_t *i2c;
    uint8_t address;
    uint8_t initialized;
} boot_at24c02_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
boot_driver_status_t boot_at24c02_init(
    boot_at24c02_t *eeprom,
    boot_soft_i2c_t *i2c,
    uint8_t address);
boot_driver_status_t boot_at24c02_read(
    boot_at24c02_t *eeprom,
    uint32_t address,
    uint8_t *data,
    uint16_t length);
boot_driver_status_t boot_at24c02_write(
    boot_at24c02_t *eeprom,
    uint32_t address,
    const uint8_t *data,
    uint16_t length);
//******************************** Functions ********************************//

#endif
