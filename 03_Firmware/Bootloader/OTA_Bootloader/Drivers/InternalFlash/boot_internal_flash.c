/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file boot_internal_flash.c
 * @brief Bootloader Application Flash Region 专用实现。
 * @author YaoQian Wang
 * @date 2026-09-18
 * @version V1.0
 *
 ******************************************************************************/

//******************************** Includes *********************************//
#include <stddef.h>

#include "boot_internal_flash.h"
#include "memory_layout.h"
#include "stm32f4xx_hal.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define BOOT_INTERNAL_FLASH_WORD_SIZE (4U)
#define BOOT_INTERNAL_FLASH_FIRST_SECTOR (FLASH_SECTOR_4)
#define BOOT_INTERNAL_FLASH_SECTOR_COUNT (4U)
//******************************** Defines **********************************//

//******************************** Private Functions ************************//
static boot_driver_status_t boot_internal_flash_validate_range(
    uint32_t appOffset,
    uint32_t length)
{
    if ((length == 0U) || (appOffset >= APP_FLASH_SIZE) ||
        (length > (APP_FLASH_SIZE - appOffset))) {
        return BOOT_DRIVER_ERR_RANGE;
    }

    return BOOT_DRIVER_OK;
}

static uint32_t boot_internal_flash_read_word(uint32_t address)
{
    volatile const uint8_t *source = (volatile const uint8_t *)address;

    return (uint32_t)source[0] |
           ((uint32_t)source[1] << 8U) |
           ((uint32_t)source[2] << 16U) |
           ((uint32_t)source[3] << 24U);
}

static boot_driver_status_t boot_internal_flash_map_hal_status(
    HAL_StatusTypeDef status)
{
    return (status == HAL_OK) ? BOOT_DRIVER_OK : BOOT_DRIVER_ERR_HAL;
}

static void boot_internal_flash_clear_flags(void)
{
    __HAL_FLASH_CLEAR_FLAG(
        FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
        FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
}
//******************************** Private Functions ************************//

//******************************** Functions ********************************//
boot_driver_status_t boot_internal_flash_erase_app(void)
{
    FLASH_EraseInitTypeDef eraseConfig = {0};
    uint32_t sectorError = 0U;
    boot_driver_status_t result;
    HAL_StatusTypeDef lockResult;

    result = boot_internal_flash_map_hal_status(HAL_FLASH_Unlock());
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    boot_internal_flash_clear_flags();
    eraseConfig.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseConfig.Sector = BOOT_INTERNAL_FLASH_FIRST_SECTOR;
    eraseConfig.NbSectors = BOOT_INTERNAL_FLASH_SECTOR_COUNT;
    eraseConfig.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    result = boot_internal_flash_map_hal_status(
        HAL_FLASHEx_Erase(&eraseConfig, &sectorError));
    lockResult = HAL_FLASH_Lock();
    if ((result == BOOT_DRIVER_OK) && (lockResult != HAL_OK)) {
        return BOOT_DRIVER_ERR_HAL;
    }

    return result;
}

boot_driver_status_t boot_internal_flash_read(
    uint32_t appOffset,
    uint8_t *data,
    uint32_t length)
{
    volatile const uint8_t *source;
    uint32_t index;
    boot_driver_status_t result;

    if (data == NULL) {
        return BOOT_DRIVER_ERR_NULL;
    }
    result = boot_internal_flash_validate_range(appOffset, length);
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    source = (volatile const uint8_t *)(APP_BASE_ADDR + appOffset);
    for (index = 0U; index < length; index++) {
        data[index] = source[index];
    }

    return BOOT_DRIVER_OK;
}

boot_driver_status_t boot_internal_flash_write(
    uint32_t appOffset,
    const uint8_t *data,
    uint32_t length)
{
    uint32_t currentOffset;
    uint32_t blockOffset;
    uint32_t blockEnd;
    uint32_t copyStart;
    uint32_t copyEnd;
    uint32_t index;
    uint32_t word;
    volatile const uint8_t *source;
    boot_driver_status_t result;
    HAL_StatusTypeDef lockResult;

    if (data == NULL) {
        return BOOT_DRIVER_ERR_NULL;
    }
    result = boot_internal_flash_validate_range(appOffset, length);
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    result = boot_internal_flash_map_hal_status(HAL_FLASH_Unlock());
    if (result != BOOT_DRIVER_OK) {
        return result;
    }
    boot_internal_flash_clear_flags();
    currentOffset = appOffset & ~(BOOT_INTERNAL_FLASH_WORD_SIZE - 1U);
    while (currentOffset < (appOffset + length)) {
        blockOffset = currentOffset;
        blockEnd = currentOffset + BOOT_INTERNAL_FLASH_WORD_SIZE;
        copyStart = (appOffset > blockOffset) ? appOffset : blockOffset;
        copyEnd = ((appOffset + length) < blockEnd) ?
                  (appOffset + length) : blockEnd;
        word = boot_internal_flash_read_word(APP_BASE_ADDR + blockOffset);
        source = data + (copyStart - appOffset);
        for (index = copyStart; index < copyEnd; index++) {
            ((uint8_t *)&word)[index - blockOffset] = source[index - copyStart];
        }
        result = boot_internal_flash_map_hal_status(
            HAL_FLASH_Program(
                FLASH_TYPEPROGRAM_WORD,
                APP_BASE_ADDR + blockOffset,
                word));
        if (result != BOOT_DRIVER_OK) {
            break;
        }
        if (boot_internal_flash_read_word(APP_BASE_ADDR + blockOffset) != word) {
            result = BOOT_DRIVER_ERR_VERIFY;
            break;
        }
        currentOffset += BOOT_INTERNAL_FLASH_WORD_SIZE;
    }
    lockResult = HAL_FLASH_Lock();
    if ((result == BOOT_DRIVER_OK) && (lockResult != HAL_OK)) {
        result = BOOT_DRIVER_ERR_HAL;
    }

    return result;
}
