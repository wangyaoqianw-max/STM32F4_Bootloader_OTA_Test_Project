/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_bsp_w25q64.c
 * @brief W25Q64JV Platform BSP 静态构造实现
 * @author YaoQian Wang
 * @date 2026-09-12
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_bsp_w25q64.h"

#include "platform_bsp_gpio.h"
#include "project_config.h"
//******************************** Includes *********************************//

//******************************** Functions ********************************//
platform_error_t platform_bsp_w25q64_construct_flash(
    platform_w25q64_t *flash)
{
    platform_w25q64_t constructed = PLATFORM_W25Q64_INITIALIZER;
    platform_error_t result = PLATFORM_ERR_OK;

    if (flash == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if ((flash->initialized == PLATFORM_TRUE) ||
        (flash->cs.initialized == PLATFORM_TRUE) ||
        (flash->spiDevice.initialized == PLATFORM_TRUE)) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    result = platform_bsp_gpio_construct_flash_cs(&constructed.cs);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    constructed.spiConfig.mode = PLATFORM_SPI_MODE_0;
    constructed.spiConfig.bitOrder = PLATFORM_SPI_BIT_ORDER_MSB_FIRST;
    constructed.spiConfig.dataBits = 8U;
    constructed.spiConfig.maxClockHz = PROJECT_FLASH_SPI_MAX_CLOCK_HZ;
    constructed.csActiveLevel = PLATFORM_GPIO_LEVEL_LOW;

    *flash = constructed;
    return PLATFORM_ERR_OK;
}
//******************************** Functions ********************************//
