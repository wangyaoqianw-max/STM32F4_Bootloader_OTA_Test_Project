/******************************************************************************
 * Copyright (C) 2026 YaoQian Wang
 *
 * All Rights Reserved.
 *
 * @file Dev_Inf.c
 * @brief W25Q64JV 的 STM32CubeProgrammer StorageInfo
 * @author YaoQian Wang
 * @date 2026-09-21
 * @version V1.0
 ******************************************************************************/

#include "Dev_Inf.h"

struct StorageInfo const StorageInfo = {
    "W25Q64_STM32F411",
    SPI_FLASH,
    0x90000000UL,
    0x00800000UL,
    0x00000100UL,
    0xFFU,
    {
        {0x00000800UL, 0x00001000UL},
        {0x00000000UL, 0x00000000UL},
        {0x00000000UL, 0x00000000UL},
        {0x00000000UL, 0x00000000UL},
        {0x00000000UL, 0x00000000UL},
        {0x00000000UL, 0x00000000UL},
        {0x00000000UL, 0x00000000UL},
        {0x00000000UL, 0x00000000UL},
        {0x00000000UL, 0x00000000UL},
        {0x00000000UL, 0x00000000UL}
    }
};
