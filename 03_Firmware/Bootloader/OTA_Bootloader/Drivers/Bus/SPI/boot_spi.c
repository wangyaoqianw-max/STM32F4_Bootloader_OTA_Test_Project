/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_spi.c
 * @brief Bootloader SPI2 同步总线最小实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "boot_spi.h"
//******************************** Includes *********************************//

//******************************** Private Functions ************************//
static boot_driver_status_t boot_spi_validate(
    const boot_spi_bus_t *bus)
{
    if (bus == NULL) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if ((bus->initialized == 0U) || (bus->handle == NULL)) {
        return BOOT_DRIVER_ERR_NOT_INITIALIZED;
    }

    return BOOT_DRIVER_OK;
}

static boot_driver_status_t boot_spi_map_hal_status(HAL_StatusTypeDef status)
{
    switch (status) {
        case HAL_OK:
            return BOOT_DRIVER_OK;
        case HAL_BUSY:
            return BOOT_DRIVER_ERR_BUSY;
        case HAL_TIMEOUT:
            return BOOT_DRIVER_ERR_TIMEOUT;
        default:
            return BOOT_DRIVER_ERR_HAL;
    }
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
/* 只绑定 CubeMX 已完成初始化的 Handle，避免 Bootloader 重复配置 SPI2。 */
boot_driver_status_t boot_spi_init(
    boot_spi_bus_t *bus,
    SPI_HandleTypeDef *handle)
{
    if ((bus == NULL) || (handle == NULL)) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if (bus->initialized != 0U) {
        return BOOT_DRIVER_ERR_INVALID;
    }

    bus->handle = handle;
    bus->initialized = 1U;
    return BOOT_DRIVER_OK;
}

boot_driver_status_t boot_spi_write(
    boot_spi_bus_t *bus,
    const uint8_t *data,
    uint16_t length)
{
    boot_driver_status_t result = boot_spi_validate(bus);

    if ((data == NULL) && (length != 0U)) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    if (length == 0U) {
        return BOOT_DRIVER_ERR_INVALID;
    }

    return boot_spi_map_hal_status(
        HAL_SPI_Transmit(bus->handle, (uint8_t *)data, length, 1000U));
}

boot_driver_status_t boot_spi_read(
    boot_spi_bus_t *bus,
    uint8_t *data,
    uint16_t length)
{
    boot_driver_status_t result = boot_spi_validate(bus);

    if (data == NULL) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    if (length == 0U) {
        return BOOT_DRIVER_ERR_INVALID;
    }

    return boot_spi_map_hal_status(
        HAL_SPI_Receive(bus->handle, data, length, 1000U));
}
//******************************** Functions ********************************//
