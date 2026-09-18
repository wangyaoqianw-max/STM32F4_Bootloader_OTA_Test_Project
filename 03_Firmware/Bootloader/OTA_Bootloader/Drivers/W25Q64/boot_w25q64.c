/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_w25q64.c
 * @brief Bootloader W25Q64 只读驱动实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "boot_w25q64.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_W25Q64_CMD_READ_JEDEC_ID (0x9FU)
#define BOOT_W25Q64_CMD_READ_DATA     (0x03U)
#define BOOT_W25Q64_CHUNK_SIZE        (0xFFFFU)
//******************************** Defines **********************************//

//******************************** Private Functions ************************//
static boot_driver_status_t boot_w25q64_validate(
    const boot_w25q64_t *flash)
{
    if (flash == NULL) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if ((flash->initialized == 0U) || (flash->bus == NULL)) {
        return BOOT_DRIVER_ERR_NOT_INITIALIZED;
    }

    return BOOT_DRIVER_OK;
}

static void boot_w25q64_cs(boot_w25q64_t *flash, GPIO_PinState state)
{
    HAL_GPIO_WritePin(flash->csPort, flash->csPin, state);
}

static boot_driver_status_t boot_w25q64_read_jedec(
    boot_w25q64_t *flash,
    uint8_t jedec[3])
{
    const uint8_t command = BOOT_W25Q64_CMD_READ_JEDEC_ID;
    boot_driver_status_t result;

    boot_w25q64_cs(flash, GPIO_PIN_RESET);
    result = boot_spi_write(flash->bus, &command, 1U);
    if (result == BOOT_DRIVER_OK) {
        result = boot_spi_read(flash->bus, jedec, 3U);
    }
    boot_w25q64_cs(flash, GPIO_PIN_SET);
    return result;
}

static boot_driver_status_t boot_w25q64_read_chunk(
    boot_w25q64_t *flash,
    uint32_t address,
    uint8_t *data,
    uint16_t length)
{
    uint8_t command[4] = {
        BOOT_W25Q64_CMD_READ_DATA,
        (uint8_t)(address >> 16U),
        (uint8_t)(address >> 8U),
        (uint8_t)address
    };
    boot_driver_status_t result;

    boot_w25q64_cs(flash, GPIO_PIN_RESET);
    result = boot_spi_write(flash->bus, command, sizeof(command));
    if (result == BOOT_DRIVER_OK) {
        result = boot_spi_read(flash->bus, data, length);
    }
    boot_w25q64_cs(flash, GPIO_PIN_SET);
    return result;
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
boot_driver_status_t boot_w25q64_init(
    boot_w25q64_t *flash,
    boot_spi_bus_t *bus,
    GPIO_TypeDef *csPort,
    uint16_t csPin)
{
    uint8_t jedec[3] = {0U};
    boot_driver_status_t result;

    if ((flash == NULL) || (bus == NULL) || (csPort == NULL)) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if (flash->initialized != 0U) {
        return BOOT_DRIVER_ERR_INVALID;
    }
    if ((bus->initialized == 0U) || (bus->handle == NULL)) {
        return BOOT_DRIVER_ERR_NOT_INITIALIZED;
    }

    flash->bus = bus;
    flash->csPort = csPort;
    flash->csPin = csPin;
    flash->manufacturerId = 0U;
    flash->memoryType = 0U;
    flash->capacityId = 0U;
    boot_w25q64_cs(flash, GPIO_PIN_SET);
    result = boot_w25q64_read_jedec(flash, jedec);
    if (result != BOOT_DRIVER_OK) {
        flash->bus = NULL;
        flash->csPort = NULL;
        return result;
    }
    if ((jedec[0] != 0xEFU) || (jedec[1] != 0x40U) || (jedec[2] != 0x17U)) {
        flash->bus = NULL;
        flash->csPort = NULL;
        return BOOT_DRIVER_ERR_VERIFY;
    }
    flash->manufacturerId = jedec[0];
    flash->memoryType = jedec[1];
    flash->capacityId = jedec[2];
    flash->initialized = 1U;
    return BOOT_DRIVER_OK;
}

boot_driver_status_t boot_w25q64_read(
    boot_w25q64_t *flash,
    uint32_t address,
    uint8_t *data,
    uint32_t length)
{
    uint32_t currentAddress = address;
    uint8_t *currentData = data;
    uint32_t remaining = length;
    uint16_t chunkLength;
    boot_driver_status_t result = boot_w25q64_validate(flash);

    if (data == NULL) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    if ((length == 0U) || (address >= BOOT_W25Q64_TOTAL_SIZE_BYTES) ||
        (length > (BOOT_W25Q64_TOTAL_SIZE_BYTES - address))) {
        return BOOT_DRIVER_ERR_RANGE;
    }

    while (remaining > 0U) {
        chunkLength = (remaining > BOOT_W25Q64_CHUNK_SIZE) ?
                      (uint16_t)BOOT_W25Q64_CHUNK_SIZE : (uint16_t)remaining;
        result = boot_w25q64_read_chunk(flash, currentAddress, currentData, chunkLength);
        if (result != BOOT_DRIVER_OK) {
            return result;
        }
        currentAddress += chunkLength;
        currentData += chunkLength;
        remaining -= chunkLength;
    }

    return BOOT_DRIVER_OK;
}
