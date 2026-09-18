/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_at24c02.c
 * @brief Bootloader AT24C02 读写驱动实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "boot_at24c02.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_AT24C02_ACK_POLL_TIMEOUT_MS (10U)
//******************************** Defines **********************************//

//******************************** Private Functions ************************//
static boot_driver_status_t boot_at24c02_validate(
    const boot_at24c02_t *eeprom)
{
    if (eeprom == NULL) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if ((eeprom->initialized == 0U) || (eeprom->i2c == NULL)) {
        return BOOT_DRIVER_ERR_NOT_INITIALIZED;
    }

    return BOOT_DRIVER_OK;
}

static boot_driver_status_t boot_at24c02_validate_range(
    uint32_t address,
    uint16_t length)
{
    if ((length == 0U) || (address >= BOOT_AT24C02_TOTAL_SIZE_BYTES) ||
        ((uint32_t)length > (BOOT_AT24C02_TOTAL_SIZE_BYTES - address))) {
        return BOOT_DRIVER_ERR_RANGE;
    }

    return BOOT_DRIVER_OK;
}

static boot_driver_status_t boot_at24c02_wait_ready(boot_at24c02_t *eeprom)
{
    uint32_t elapsedMs = 0U;
    boot_driver_status_t result;

    while (elapsedMs < BOOT_AT24C02_ACK_POLL_TIMEOUT_MS) {
        result = boot_soft_i2c_probe(eeprom->i2c, eeprom->address);
        if (result == BOOT_DRIVER_OK) {
            return BOOT_DRIVER_OK;
        }
        if (result != BOOT_DRIVER_ERR_NOT_FOUND) {
            return result;
        }
        HAL_Delay(1U);
        elapsedMs++;
    }

    return BOOT_DRIVER_ERR_TIMEOUT;
}

static boot_driver_status_t boot_at24c02_page_write(
    boot_at24c02_t *eeprom,
    uint32_t address,
    const uint8_t *data,
    uint16_t length)
{
    uint8_t writeData[BOOT_AT24C02_PAGE_SIZE_BYTES + 1U];
    uint16_t index;

    if ((address % BOOT_AT24C02_PAGE_SIZE_BYTES) + length >
        BOOT_AT24C02_PAGE_SIZE_BYTES) {
        return BOOT_DRIVER_ERR_INVALID;
    }
    writeData[0] = (uint8_t)address;
    for (index = 0U; index < length; index++) {
        writeData[index + 1U] = data[index];
    }

    return boot_soft_i2c_write(eeprom->i2c, eeprom->address, writeData,
                               (uint16_t)(length + 1U));
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
boot_driver_status_t boot_at24c02_init(
    boot_at24c02_t *eeprom,
    boot_soft_i2c_t *i2c,
    uint8_t address)
{
    boot_driver_status_t result;

    if ((eeprom == NULL) || (i2c == NULL)) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if (eeprom->initialized != 0U) {
        return BOOT_DRIVER_ERR_INVALID;
    }
    if ((address < 0x50U) || (address > 0x57U)) {
        return BOOT_DRIVER_ERR_INVALID;
    }
    eeprom->i2c = i2c;
    eeprom->address = address;
    result = boot_soft_i2c_probe(i2c, address);
    if (result != BOOT_DRIVER_OK) {
        eeprom->i2c = NULL;
        return result;
    }
    eeprom->initialized = 1U;
    return BOOT_DRIVER_OK;
}

boot_driver_status_t boot_at24c02_read(
    boot_at24c02_t *eeprom,
    uint32_t address,
    uint8_t *data,
    uint16_t length)
{
    uint8_t wordAddress;
    boot_driver_status_t result = boot_at24c02_validate(eeprom);

    if (data == NULL) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    result = boot_at24c02_validate_range(address, length);
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    wordAddress = (uint8_t)address;
    return boot_soft_i2c_write_read(eeprom->i2c, eeprom->address,
                                    &wordAddress, 1U, data, length);
}

boot_driver_status_t boot_at24c02_write(
    boot_at24c02_t *eeprom,
    uint32_t address,
    const uint8_t *data,
    uint16_t length)
{
    uint32_t currentAddress = address;
    const uint8_t *currentData = data;
    uint16_t remaining = length;
    uint16_t pageOffset;
    uint16_t pageRemaining;
    uint16_t chunkLength;
    boot_driver_status_t result = boot_at24c02_validate(eeprom);

    if (data == NULL) {
        return BOOT_DRIVER_ERR_NULL;
    }
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    result = boot_at24c02_validate_range(address, length);
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    while (remaining > 0U) {
        pageOffset = (uint16_t)(currentAddress % BOOT_AT24C02_PAGE_SIZE_BYTES);
        pageRemaining = (uint16_t)(BOOT_AT24C02_PAGE_SIZE_BYTES - pageOffset);
        chunkLength = (remaining < pageRemaining) ? remaining : pageRemaining;
        result = boot_at24c02_page_write(eeprom, currentAddress, currentData, chunkLength);
        if (result != BOOT_DRIVER_OK) {
            return result;
        }
        result = boot_at24c02_wait_ready(eeprom);
        if (result != BOOT_DRIVER_OK) {
            return result;
        }
        currentAddress += chunkLength;
        currentData += chunkLength;
        remaining -= chunkLength;
    }

    return BOOT_DRIVER_OK;
}
