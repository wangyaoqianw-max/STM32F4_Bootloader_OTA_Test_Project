/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_soft_i2c.h
 * @brief Bootloader Soft-I2C 最小同步接口。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

#ifndef BOOT_SOFT_I2C_H
#define BOOT_SOFT_I2C_H

//******************************** Includes *********************************//
#include "boot_driver_status.h"
#include "main.h"
//******************************** Includes *********************************//

//******************************** Types ***********************************//
typedef struct
{
    GPIO_TypeDef *sclPort;
    uint16_t sclPin;
    GPIO_TypeDef *sdaPort;
    uint16_t sdaPin;
    uint8_t initialized;
} boot_soft_i2c_t;
//******************************** Types ***********************************//

//******************************** Functions ********************************//
boot_driver_status_t boot_soft_i2c_init(
    boot_soft_i2c_t *i2c,
    GPIO_TypeDef *sclPort,
    uint16_t sclPin,
    GPIO_TypeDef *sdaPort,
    uint16_t sdaPin);
boot_driver_status_t boot_soft_i2c_probe(
    boot_soft_i2c_t *i2c,
    uint8_t address);
boot_driver_status_t boot_soft_i2c_write(
    boot_soft_i2c_t *i2c,
    uint8_t address,
    const uint8_t *data,
    uint16_t length);
boot_driver_status_t boot_soft_i2c_read(
    boot_soft_i2c_t *i2c,
    uint8_t address,
    uint8_t *data,
    uint16_t length);
boot_driver_status_t boot_soft_i2c_write_read(
    boot_soft_i2c_t *i2c,
    uint8_t address,
    const uint8_t *txData,
    uint16_t txLength,
    uint8_t *rxData,
    uint16_t rxLength);
//******************************** Functions ********************************//

#endif
