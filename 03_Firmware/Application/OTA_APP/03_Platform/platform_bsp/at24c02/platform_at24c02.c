/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file platform_at24c02.c
 * @brief AT24C02 Platform Raw Driver 读路径与生命周期实现
 * @author YaoQian Wang
 * @date 2026-09-13
 * @version V1.0
 *
 *****************************************************************************/

//******************************** Includes *********************************//
#include "platform_at24c02.h"

#include "platform_def.h"

#include <stddef.h>
//******************************** Includes *********************************//

//******************************** Defines *********************************//
#define PLATFORM_AT24C02_WORD_ADDRESS_SIZE_BYTES    (1U)
//******************************** Defines *********************************//

//******************************** Declaring *********************************//
static platform_error_t platform_at24c02_validate_initialized(
    const platform_at24c02_t *eeprom);
static platform_error_t platform_at24c02_validate_range(
    uint32_t address,
    platform_size_t dataLength);
static void platform_at24c02_clear_binding(
    platform_at24c02_t *eeprom);
//******************************** Declaring *********************************//

//******************************** Private Functions *************************//
static platform_error_t platform_at24c02_validate_initialized(
    const platform_at24c02_t *eeprom)
{
    if (eeprom == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (eeprom->initialized != PLATFORM_TRUE) {
        return PLATFORM_ERR_NOT_INITIALIZED;
    }

    return PLATFORM_ERR_OK;
}

/* 校验完整 EEPROM 地址范围，避免通过 address + dataLength 产生溢出。 */
static platform_error_t platform_at24c02_validate_range(
    uint32_t address,
    platform_size_t dataLength)
{
    if (dataLength == 0U) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (address >= PLATFORM_AT24C02_TOTAL_SIZE_BYTES) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    if (dataLength > (PLATFORM_AT24C02_TOTAL_SIZE_BYTES - address)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    return PLATFORM_ERR_OK;
}

static void platform_at24c02_clear_binding(
    platform_at24c02_t *eeprom)
{
    eeprom->i2c = NULL;
    eeprom->deviceAddress = 0U;
    eeprom->initialized = PLATFORM_FALSE;
}
//******************************** Private Functions *************************//

//******************************** Functions *********************************//
platform_error_t platform_at24c02_init(
    platform_at24c02_t *eeprom,
    platform_i2c_t *i2c,
    uint8_t deviceAddress)
{
    platform_error_t result = PLATFORM_ERR_OK;

    if (eeprom == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (i2c == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (eeprom->initialized == PLATFORM_TRUE) {
        return PLATFORM_ERR_ALREADY_INITIALIZED;
    }

    if ((deviceAddress < PLATFORM_AT24C02_I2C_ADDRESS_MIN) ||
        (deviceAddress > PLATFORM_AT24C02_I2C_ADDRESS_MAX)) {
        return PLATFORM_ERR_INVALID_PARAM;
    }

    eeprom->i2c = i2c;
    eeprom->deviceAddress = deviceAddress;
    eeprom->initialized = PLATFORM_FALSE;

    result = platform_i2c_probe(i2c, deviceAddress);
    if (result != PLATFORM_ERR_OK) {
        platform_at24c02_clear_binding(eeprom);
        return result;
    }

    eeprom->initialized = PLATFORM_TRUE;
    return PLATFORM_ERR_OK;
}

platform_error_t platform_at24c02_deinit(
    platform_at24c02_t *eeprom)
{
    platform_error_t result = platform_at24c02_validate_initialized(eeprom);

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    platform_at24c02_clear_binding(eeprom);
    return PLATFORM_ERR_OK;
}

platform_error_t platform_at24c02_read(
    platform_at24c02_t *eeprom,
    uint32_t address,
    uint8_t *data,
    platform_size_t dataLength)
{
    platform_error_t result = platform_at24c02_validate_initialized(eeprom);
    uint8_t wordAddress = 0U;

    if (data == NULL) {
        return PLATFORM_ERR_NULL_POINTER;
    }

    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    result = platform_at24c02_validate_range(address, dataLength);
    if (result != PLATFORM_ERR_OK) {
        return result;
    }

    wordAddress = (uint8_t)address;
    return platform_i2c_write_read(eeprom->i2c,
                                   eeprom->deviceAddress,
                                   &wordAddress,
                                   PLATFORM_AT24C02_WORD_ADDRESS_SIZE_BYTES,
                                   data,
                                   (uint16_t)dataLength);
}
//******************************** Functions *********************************//
